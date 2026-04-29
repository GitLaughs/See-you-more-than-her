#!/bin/bash
# build_complete_evb.sh — 完整 EVB 镜像构建脚本
#
# 用途：一键编译并打包完整的 EVB 固件（内核 + rootfs + 最新应用）
#       每次构建产物自动保存到带时间戳的独立目录。
#
# 用法（在 A1_Builder 容器内执行）:
#   bash build_complete_evb.sh [--clean] [--skip-ros] [--app-only] [--verbose]
#
# 参数:
#   --clean      清除 Buildroot 缓存（完全重建，耗时较长）
#   --skip-ros   跳过 ROS2 工作区编译
#   --app-only   快速模式：仅重编 ssne_ai_demo + 重打包 zImage（跳过 SDK 基础库，
#                需先至少完整构建过一次；约 5-10 分钟）
#   --verbose    显示详细编译输出
#
# 构建流程（完整模式）:
#   [1] SDK 基础库（build_release_sdk.sh 第一次）
#   [2] ssne_ai_demo（YOLOv8 检测 + 底盘控制）
#   [3] ROS2 工作区（可跳过，或 --app-only 时强制跳过）
#   [4] 重新打包 zImage（build_release_sdk.sh 第二次，将最新应用打入 initramfs）
#   [5] 产物保存到 output/evb/<YYYYMMDD_HHMMSS>/
#
# 产物（每次构建均包含）:
#   ssne_ai_demo              ARM ELF 应用二进制
#   zImage.smartsens-m1-evb  包含最新应用的完整内核镜像（可直接烧录）

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
SMART_SOFTWARE_OVERLAY_DIR="${ROOT_DIR}/output/smart_software_newsdk"
USE_REFERENCE_OVERLAY="${USE_REFERENCE_OVERLAY:-0}"
ROS_WS="${ROOT_DIR}/src/ros2_ws"
OUTPUT_BASE="${ROOT_DIR}/output/evb"
LOG_DIR="${ROOT_DIR}/output/logs"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
CLEAN_BUILD=0
SKIP_ROS=0
APP_ONLY=0
VERBOSE=0

for arg in "$@"; do
  case "${arg}" in
    --clean)      CLEAN_BUILD=1 ;;
    --skip-ros)   SKIP_ROS=1 ;;
    --app-only)   APP_ONLY=1; SKIP_ROS=1 ;;
    --verbose|-v) VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -30 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    *)
      echo "[EVB构建] 未知选项: ${arg}" >&2
      exit 1
      ;;
  esac
done

# ─── 辅助函数 ─────────────────────────────────────────────────────────────────
log()  { echo "[EVB构建] $*"; }
step() { echo; echo "══════════════════════════════════════════════════════"; \
         echo "  $*"; \
         echo "══════════════════════════════════════════════════════"; }
fail() { echo "[EVB构建] 失败: $*" >&2; exit 1; }
elapsed() { echo "$(( ($SECONDS - START_TIME) / 60 )) 分钟"; }

repair_libzlib_build_dir() {
  local zlib_dir="${SDK_DIR}/output/build/libzlib-1.2.12"
  if [[ -d "${zlib_dir}" && ! -d "${zlib_dir}/objs" ]]; then
    mkdir -p "${zlib_dir}/objs"
    log "已修复 libzlib 构建目录: ${zlib_dir}/objs"
  fi
}

run_sdk_build_with_retry() {
  local log_file="$1"

  set +e
  bash scripts/build_release_sdk.sh 2>&1 | tee "${log_file}"
  local build_status=${PIPESTATUS[0]}
  set -e

  if [[ ${build_status} -eq 0 ]]; then
    return 0
  fi

  if grep -q "can't create objs/.*No such file or directory" "${log_file}"; then
    log "检测到 libzlib 缺少 objs/ 目录，执行一次兼容修复后重试..."
    repair_libzlib_build_dir
    bash scripts/build_release_sdk.sh 2>&1 | tee "${log_file}"
    return ${PIPESTATUS[0]}
  fi

  return ${build_status}
}

sync_smart_software_overlay() {
  local overlay_dir="${SMART_SOFTWARE_OVERLAY_DIR}"
  local target_dir="${SDK_DIR}/smart_software"
  local item=""

  if [[ "${USE_REFERENCE_OVERLAY}" != "1" ]]; then
    log "默认跳过参考 overlay 同步；如需使用 output/smart_software_newsdk，请显式设置 USE_REFERENCE_OVERLAY=1"
    return 0
  fi

  [[ -d "${overlay_dir}" ]] || {
    log "未找到参考 overlay，继续使用 SDK 内置 smart_software"
    return 0
  }
  [[ -d "${target_dir}" ]] || fail "目标 smart_software 目录不存在: ${target_dir}"

  step "同步参考 overlay → ${target_dir}"
  for item in Config.in external.desc external.mk local.mk board configs package; do
    if [[ -e "${overlay_dir}/${item}" ]]; then
      rm -rf "${target_dir:?}/${item}"
      cp -a "${overlay_dir}/${item}" "${target_dir}/${item}"
      log "  ✓ 已同步 ${item}"
    fi
  done

  if [[ -d "${overlay_dir}/src/app_demo" ]]; then
    mkdir -p "${target_dir}/src"
    rm -rf "${target_dir}/src/app_demo"
    cp -a "${overlay_dir}/src/app_demo" "${target_dir}/src/app_demo"
    log "  ✓ 已同步 src/app_demo"
  fi
}

# ─── 产物目录（每次构建独立时间戳）────────────────────────────────────────────
BUILD_TS=$(date +%Y%m%d_%H%M%S)
ARTIFACT_DIR="${OUTPUT_BASE}/${BUILD_TS}"
mkdir -p "${ARTIFACT_DIR}" "${LOG_DIR}"

# 更新 latest 软链接
rm -rf "${OUTPUT_BASE}/latest" 2>/dev/null || true
if ! ln -sfn "${ARTIFACT_DIR}" "${OUTPUT_BASE}/latest" 2>/dev/null; then
  printf '%s\n' "${ARTIFACT_DIR}" > "${OUTPUT_BASE}/latest.txt"
  log "⚠ 当前环境不支持 latest 软链接，已写入 ${OUTPUT_BASE}/latest.txt"
fi

# ─── 开始信息 ────────────────────────────────────────────────────────────────
START_TIME=$SECONDS
echo "========================================="
echo " 完整 EVB 镜像构建"
echo "========================================="
echo "构建时间:    ${BUILD_TS}"
echo "SDK 目录:    ${SDK_DIR}"
echo "产物目录:    ${ARTIFACT_DIR}"
[[ ${APP_ONLY} -eq 1 ]] && echo "模式:        --app-only（跳过 SDK 基础库，仅重编 Demo + zImage）"
echo "========================================="

[[ -d "${SDK_DIR}" ]] || fail "SDK 目录不存在: ${SDK_DIR}"
sync_smart_software_overlay

# ─── Step 1：SDK 基础库（首次） ──────────────────────────────────────────────
if [[ ${APP_ONLY} -eq 1 ]]; then
  step "Step 1/4: SDK 基础库 [已跳过 --app-only 模式]"
  # 验证上次完整构建的缓存存在
  [[ -d "${SDK_DIR}/output/build" ]] \
    || fail "--app-only 模式要求先完整构建过一次，未找到 output/build/，请先不加 --app-only 运行一次"
  log "✓ 检测到已有构建缓存，跳过 SDK 基础库"
else
  step "Step 1/4: SDK 基础库编译 (build_release_sdk.sh)"
  cd "${SDK_DIR}"

  if [[ ${CLEAN_BUILD} -eq 1 ]]; then
    log "--clean 模式：清除 Buildroot 缓存..."
    rm -rf output/build output/.config 2>/dev/null || true
  fi

  log "执行 build_release_sdk.sh ..."
  run_sdk_build_with_retry "${LOG_DIR}/sdk_pass1.log" \
    || fail "SDK 基础库编译失败，日志：${LOG_DIR}/sdk_pass1.log"
  log "✓ SDK 基础库编译成功"
fi

# ─── Step 2：编译 ssne_ai_demo ───────────────────────────────────────────────
step "Step 2/4: ssne_ai_demo（YOLOv8 检测 + OSD标签 + UART 底盘控制）"
cd "${SDK_DIR}"
rm -rf output/build/ssne_ai_demo/
make BR2_EXTERNAL=./smart_software ssne_ai_demo \
    2>&1 | tee "${LOG_DIR}/demo.log" \
  || fail "ssne_ai_demo 编译失败，日志：${LOG_DIR}/demo.log"
log "✓ ssne_ai_demo 编译成功"

# ─── Step 3：ROS2（可选）─────────────────────────────────────────────────────
if [[ ${SKIP_ROS} -eq 0 ]]; then
  step "Step 3/4: ROS2 工作区编译"
  if [[ -f /opt/ros/jazzy/setup.bash ]]; then
    bash "${SCRIPT_DIR}/build_ros2_ws.sh" \
        2>&1 | tee "${LOG_DIR}/ros2.log" \
      || log "⚠ ROS2 编译有错误，继续打包 EVB..."
    log "✓ ROS2 编译完成"
  else
    log "⚠ 未找到 ROS2 Jazzy，跳过 ROS2 编译"
  fi
else
  log "Step 3/4: ROS2 编译已跳过 (--skip-ros)"
fi

# ─── Step 4：重新打包 zImage（将最新应用打入 initramfs）────────────────────
step "Step 4/4: 重新打包 zImage（将最新应用写入 initramfs）"
cd "${SDK_DIR}"
run_sdk_build_with_retry "${LOG_DIR}/sdk_pass2.log" \
  || fail "zImage 重新打包失败，日志：${LOG_DIR}/sdk_pass2.log"
log "✓ zImage 重新打包成功"

# ─── 收集产物到时间戳目录 ─────────────────────────────────────────────────────
step "收集产物 → ${ARTIFACT_DIR}/"
EVB_SRC="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_SRC="${SDK_DIR}/output/target/app_demo/ssne_ai_demo"

# 收集 zImage（必须存在）
if [[ -f "${EVB_SRC}" ]]; then
  cp "${EVB_SRC}"  "${ARTIFACT_DIR}/zImage.smartsens-m1-evb"
  log "  ✓ zImage.smartsens-m1-evb  ($(du -sh "${ARTIFACT_DIR}/zImage.smartsens-m1-evb" | cut -f1))"
else
  fail "未找到 zImage：${EVB_SRC}（Step 4 是否失败？）"
fi

# 收集 ssne_ai_demo（必须存在）
if [[ -f "${DEMO_SRC}" ]]; then
  cp "${DEMO_SRC}" "${ARTIFACT_DIR}/ssne_ai_demo"
  log "  ✓ ssne_ai_demo              ($(du -sh "${ARTIFACT_DIR}/ssne_ai_demo" | cut -f1))"
else
  fail "未找到 Demo 二进制：${DEMO_SRC}（Step 2 是否失败？）"
fi

# ─── 构建总结 ────────────────────────────────────────────────────────────────
step "构建完成"
echo "产物目录: ${ARTIFACT_DIR}"
echo "软链接:   ${OUTPUT_BASE}/latest -> ${ARTIFACT_DIR}"
echo ""
ls -lh "${ARTIFACT_DIR}/"
echo ""
echo "Windows 测试入口:"
echo "  cd tools/aurora && .\\launch.ps1 -Mode a1"
echo "烧录请使用 Aurora-2.0.0-ciciec.16/Aurora.exe 打开 ${ARTIFACT_DIR}/zImage.smartsens-m1-evb"
echo ""
echo "板端验证:"
echo "  ssh root@<A1_IP>  &&  /app_demo/scripts/run.sh"
echo ""
log "总耗时: $(elapsed)"
log "✓ 完整 EVB 构建完成！"
