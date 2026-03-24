#!/bin/bash
# build_complete_evb.sh — 完整 EVB 镜像构建脚本（一体化编译）
#
# 用途：直接生成完整的 EVB 镜像文件，包含内核 + rootfs + 所有应用
#
# 用法（在 A1_Builder 容器内执行）:
#   bash build_complete_evb.sh [--clean] [--skip-ros] [--verbose]
#
# 构建流程:
#   [1] SDK 基础库（build_release_sdk.sh）
#   [2] ssne_face_drive_demo（人脸检测 + 底盘控制）
#   [3] ROS2 工作区（可选，--skip-ros 跳过）
#   [4] 重新生成 zImage（包含最新应用）
#   [5] 收集 zImage.smartsens-m1-evb 到 output/evb/

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_WS="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"
LOG_DIR="${ROOT_DIR}/output/logs"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
CLEAN_BUILD=0
SKIP_ROS=0
VERBOSE=0

for arg in "$@"; do
  case "${arg}" in
    --clean)        CLEAN_BUILD=1 ;;
    --skip-ros)     SKIP_ROS=1 ;;
    --verbose|-v)   VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -20 | sed 's/^# \{0,2\}//'
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
elapsed() { local t=$(($SECONDS / 60)); echo "耗时 ${t} 分钟"; }

# ─── 环境准备 ─────────────────────────────────────────────────────────────────
START_TIME=$SECONDS
echo "========================================="
echo " 完整 EVB 镜像构建"
echo "========================================="
echo "主目录:      ${ROOT_DIR}"
echo "SDK 目录:    ${SDK_DIR}"
echo "工作区:      ${ROS_WS}"
echo "产物目录:    ${ARTIFACT_DIR}"
echo "========================================="

mkdir -p "${LOG_DIR}" "${ARTIFACT_DIR}" 2>/dev/null || true

[[ -d "${SDK_DIR}" ]] || fail "SDK 目录不存在: ${SDK_DIR}"

# ─── Step 1：SDK 基础库 ────────────────────────────────────────────────────────
step "Step 1/5: SDK 基础库编译 (build_release_sdk.sh)"
cd "${SDK_DIR}"

if [[ ${CLEAN_BUILD} -eq 1 ]]; then
  log "清除 Buildroot 缓存..."
  rm -rf output/build output/.config 2>/dev/null || true
fi

log "执行 build_release_sdk.sh..."
if bash scripts/build_release_sdk.sh 2>&1 | tee "${LOG_DIR}/evb_sdk.log"; then
  log "✓ SDK 基础库编译成功"
else
  fail "SDK 编译失败，查看 ${LOG_DIR}/evb_sdk.log"
fi

# ─── Step 2：编译 ssne_face_drive_demo ─────────────────────────────────────────
step "Step 2/5: ssne_face_drive_demo 编译 (人脸检测 + UART 底盘控制)"
log "清除旧构建..."
rm -rf output/build/ssne_face_drive_demo/
log "构建 ssne_face_drive_demo..."

if make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_face_drive_demo \
    2>&1 | tee "${LOG_DIR}/evb_demo.log"; then
  log "✓ Demo 编译成功"
else
  fail "Demo 编译失败，查看 ${LOG_DIR}/evb_demo.log"
fi

# ─── Step 3：ROS2 工作区编译（可选）──────────────────────────────────────────
if [[ ${SKIP_ROS} -eq 0 ]]; then
  step "Step 3/5: ROS2 工作区编译"
  cd "${ROS_WS}"

  log "清理旧构建..."
  rm -rf build install log 2>/dev/null || true

  log "使用 rosdep 安装依赖..."
  rosdep install --from-paths src --ignore-src -r -y 2>&1 | tail -20

  log "执行 colcon build..."
  if colcon build --symlink-install 2>&1 | tee "${LOG_DIR}/evb_ros2.log" | tail -50; then
    log "✓ ROS2 编译成功"
  else
    log "⚠ ROS2 编译有问题，但继续生成 EVB..."
  fi
else
  log "Step 3/5: ROS2 编译已跳过 (--skip-ros)"
  log "✓ ROS2 编译跳过"
fi

# ─── Step 4：重新生成 zImage（包含最新应用）─────────────────────────────────
step "Step 4/5: 重新生成 zImage (包含最新应用)"
cd "${SDK_DIR}"

log "运行 build_release_sdk.sh 重新打包 zImage..."
if bash scripts/build_release_sdk.sh 2>&1 | tee -a "${LOG_DIR}/evb_sdk.log"; then
  log "✓ zImage 重新生成成功"
else
  fail "zImage 重新生成失败"
fi

# ─── Step 5：收集和验证产物 ──────────────────────────────────────────────────
step "Step 5/5: 收集产物"
EVB_KERNEL="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
EVB_TARGET="${ARTIFACT_DIR}/zImage.smartsens-m1-evb"

if [[ ! -f "${EVB_KERNEL}" ]]; then
  fail "zImage.smartsens-m1-evb 未生成: ${EVB_KERNEL}"
fi

log "复制 zImage..."
cp -f "${EVB_KERNEL}" "${EVB_TARGET}" || fail "复制 zImage 失败"

log "生成版本备份..."
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
EVBX_BACKUP="${ARTIFACT_DIR}/zImage.smartsens-m1-evb.v-complete-${TIMESTAMP}"
cp -f "${EVB_TARGET}" "${EVBX_BACKUP}"

log "复制 Demo 二进制..."
DEMO_SRC="${SDK_DIR}/output/target/app_demo/ssne_face_drive_demo"
DEMO_DST="${ARTIFACT_DIR}/ssne_face_drive_demo"
if [[ -f "${DEMO_SRC}" ]]; then
  cp -f "${DEMO_SRC}" "${DEMO_DST}"
  log "✓ Demo 复制成功"
else
  log "⚠ Demo 二进制未找到"
fi

# ─── 验证和总结 ────────────────────────────────────────────────────────────
step "构建完成"
echo "产物位置:"
echo "  - zImage 镜像:  ${EVB_TARGET}"
echo "  - 版本备份:     ${EVBX_BACKUP}"
if [[ -f "${DEMO_DST}" ]]; then
  echo "  - Demo 二进制:  ${DEMO_DST}"
fi
echo ""
echo "烧录说明:"
echo "  1. 连接 A1 开发板的 Type-C (SPI 接口)"
echo "  2. 使用 Aurora 伴侣工具烧录:"
echo "     cd tools/aurora && .\launch.ps1 --flash ${EVB_TARGET}"
echo "  3. 或使用命令行烧录工具"
echo ""
echo "板端验证:"
echo "  ssh root@<A1_IP>"
echo "  /app_demo/scripts/run.sh"
echo ""

log "总耗时: $(elapsed)"
log "✓ 完整 EVB 构建完成！"
