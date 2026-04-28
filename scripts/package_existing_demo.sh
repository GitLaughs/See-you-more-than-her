#!/bin/bash
# package_existing_demo.sh — 打包已编译好的 ssne_ai_demo 成 EVB
#
# 用途：在用户已在容器内单独构建好 ssne_ai_demo 后，将其打包成完整的 EVB 镜像
#
# 用法（在 A1_Builder 容器内执行）:
#   bash package_existing_demo.sh [--verbose]
#
# 流程:
#   [1] 验证 ssne_ai_demo 已构建并位于 SDK 的 output/target/app_demo/ssne_ai_demo
#   [2] 重新打包 zImage（将最新应用写入 initramfs）
#   [3] 产物保存到 output/evb/<YYYYMMDD_HHMMSS>/

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
OUTPUT_BASE="${ROOT_DIR}/output/evb"
LOG_DIR="${ROOT_DIR}/output/logs"

# 用户已构建好的 demo 位置（在 SDK 的 target 目录）
SDK_TARGET="${SDK_DIR}/output/target/app_demo"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
VERBOSE=0
for arg in "$@"; do
  case "${arg}" in
    --verbose|-v) VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -20 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    *)
      echo "[打包] 未知选项: ${arg}" >&2
      exit 1
      ;;
  esac
done

# ─── 辅助函数 ─────────────────────────────────────────────────────────────────
log()  { echo "[打包] $*"; }
step() { echo; echo "══════════════════════════════════════════════════════"; \
         echo "  $*"; \
         echo "══════════════════════════════════════════════════════"; }
fail() { echo "[打包] 失败: $*" >&2; exit 1; }
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
echo " 打包已编译的 ssne_ai_demo 成 EVB"
echo "========================================="
echo "构建时间:    ${BUILD_TS}"
echo "SDK 目录:    ${SDK_DIR}"
echo "Demo 源:     ${SDK_TARGET}"
echo "产物目录:    ${ARTIFACT_DIR}"
echo "========================================="

[[ -d "${SDK_DIR}" ]] || fail "SDK 目录不存在: ${SDK_DIR}"

# ─── Step 1：验证已构建的 demo ────────────────────────────────────────
step "Step 1/2: 验证已构建的 ssne_ai_demo"

# 检查源文件是否存在
DEMO_BIN="${SDK_TARGET}/ssne_ai_demo"
if [[ ! -f "${DEMO_BIN}" ]]; then
  fail "未找到已编译的 ssne_ai_demo: ${DEMO_BIN}"
fi

log "✓ 找到 ssne_ai_demo: ${DEMO_BIN} ($(du -sh "${DEMO_BIN}" | cut -f1))"
log "✓ app_demo 目录结构已在 SDK 的 target 目录中"

# ─── Step 2：重新打包 zImage（将最新应用写入 initramfs）────────────────────
step "Step 2/2: 重新打包 zImage（将最新应用写入 initramfs）"
cd "${SDK_DIR}"
run_sdk_build_with_retry "${LOG_DIR}/package_sdk.log" \
  || fail "zImage 重新打包失败，日志：${LOG_DIR}/package_sdk.log"
log "✓ zImage 重新打包成功"

# ─── Step 3：收集产物到时间戳目录 ─────────────────────────────────────────────
step "收集产物 → ${ARTIFACT_DIR}/"
EVB_SRC="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_SRC="${SDK_DIR}/output/target/app_demo/ssne_ai_demo"

# 收集 zImage（必须存在）
if [[ -f "${EVB_SRC}" ]]; then
  cp "${EVB_SRC}"  "${ARTIFACT_DIR}/zImage.smartsens-m1-evb"
  log "  ✓ zImage.smartsens-m1-evb  ($(du -sh "${ARTIFACT_DIR}/zImage.smartsens-m1-evb" | cut -f1))"
else
  fail "未找到 zImage：${EVB_SRC}（Step 2 是否失败？）"
fi

# 收集 ssne_ai_demo（必须存在）
if [[ -f "${DEMO_SRC}" ]]; then
  cp "${DEMO_SRC}" "${ARTIFACT_DIR}/ssne_ai_demo"
  log "  ✓ ssne_ai_demo              ($(du -sh "${ARTIFACT_DIR}/ssne_ai_demo" | cut -f1))"
else
  fail "未找到 Demo 二进制：${DEMO_SRC}（Step 1 是否失败？）"
fi

# 可选：复制整个 app_demo 目录
if [[ -d "${SDK_DIR}/output/target/app_demo" ]]; then
  cp -a "${SDK_DIR}/output/target/app_demo" "${ARTIFACT_DIR}/"
  log "  ✓ app_demo/                 (资源目录)"
fi

# ─── 构建总结 ────────────────────────────────────────────────────────────────
step "打包完成"
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
log "✓ EVB 打包完成！"
