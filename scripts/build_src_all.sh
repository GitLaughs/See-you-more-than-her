#!/bin/bash
# build_src_all.sh — 全量一键构建脚本（SDK + Demo + ROS2）
#
# 参考: data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh
#       data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh
#
# 用法（在 A1_Builder 容器内执行）:
#   bash build_src_all.sh [--skip-sdk] [--skip-demo] [--skip-ros] [--skip-collect] [--clean] [--verbose]
#
# 选项:
#   --skip-sdk      跳过 SDK 基础库构建（SDK 已构建时使用）
#   --skip-demo     跳过 ssne_ai_demo / ssne_vision_demo 构建
#   --skip-ros      跳过 ROS2 工作区构建
#   --skip-collect  跳过 EVB 产物收集
#   --clean         清除 ROS2 build/install/log 后重新编译
#   --verbose,-v    显示详细输出
#   -h, --help      显示帮助信息
#
# 构建流程:
#   [1] SmartSens SDK 基础库（Buildroot defconfig + m1_sdk_lib）
#   [2] ssne_ai_demo（SCRFD 人脸检测 Demo）
#   [3] ssne_vision_demo（YOLOv8+OSD+雷达综合 Demo）
#   [4] ROS2 工作区（colcon build，跳过 P1 COLCON_IGNORE 包）
#   [5] 收集 EVB 产物到 output/evb/
#
# P1 增强包（已通过 COLCON_IGNORE 暂时屏蔽）：
#   wheeltec_robot_kcf, wheeltec_robot_urdf, wheeltec_rviz2,
#   aruco_ros, usb_cam-ros2, web_video_server-ros2

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_WS="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"
LOG_DIR="${ROOT_DIR}/output/logs"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
SKIP_SDK=0
SKIP_DEMO=0
SKIP_ROS=0
SKIP_COLLECT=0
CLEAN_BUILD=0
VERBOSE=0

for arg in "$@"; do
  case "${arg}" in
    --skip-sdk)     SKIP_SDK=1 ;;
    --skip-demo)    SKIP_DEMO=1 ;;
    --skip-ros)     SKIP_ROS=1 ;;
    --skip-collect) SKIP_COLLECT=1 ;;
    --clean)        CLEAN_BUILD=1 ;;
    --verbose|-v)   VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -30 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    *)
      echo "[build_src_all.sh] 未知选项: ${arg}" >&2
      exit 1
      ;;
  esac
done

# ─── 辅助函数 ─────────────────────────────────────────────────────────────────
log()  { echo "[build_src_all.sh] $*"; }
step() { echo; echo "══════════════════════════════════════════════════════"; \
         echo "  $*"; \
         echo "══════════════════════════════════════════════════════"; }
fail() { echo "[build_src_all.sh] 失败: $*" >&2; exit 1; }

# ─── 环境准备 ─────────────────────────────────────────────────────────────────
echo "========================================="
echo "[build_src_all.sh] 全量构建 (SDK + Demo + ROS2)"
echo "========================================="
echo "根目录:      ${ROOT_DIR}"
echo "SDK 目录:    ${SDK_DIR}"
echo "ROS 目录:    ${ROS_WS}"
echo "跳过SDK:     ${SKIP_SDK}"
echo "跳过Demo:    ${SKIP_DEMO}"
echo "跳过ROS:     ${SKIP_ROS}"
echo "跳过收集:    ${SKIP_COLLECT}"
echo "清理构建:    ${CLEAN_BUILD}"
echo "详细输出:    ${VERBOSE}"
echo "========================================="

# 建立 Docker 内路径软链接（兼容容器环境）
mkdir -p /app 2>/dev/null || true
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk 2>/dev/null || true
mkdir -p "${LOG_DIR}" 2>/dev/null || true

# 检查目录
if [[ ${SKIP_SDK} -eq 0 || ${SKIP_DEMO} -eq 0 ]]; then
  [[ -d "${SDK_DIR}" ]] || fail "SDK 目录不存在: ${SDK_DIR}"
fi

if [[ ${SKIP_ROS} -eq 0 ]]; then
  [[ -d "${ROS_WS}" ]] || fail "ROS 工作区不存在: ${ROS_WS}"
fi

# ─── Step 1：构建 SDK 基础库 ──────────────────────────────────────────────────
if [[ ${SKIP_SDK} -eq 0 ]]; then
  step "Step 1/6: SDK 基础库编译 (SmartSens M1 SDK)"
  cd "${SDK_DIR}"

  if [[ ! -f output/.config ]]; then
    log "应用 defconfig: smartsens_m1pro_release_defconfig"
    make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg smartsens_m1pro_release_defconfig \
        2>&1 | tee "${LOG_DIR}/sdk_defconfig.log"
  else
    log "已有 .config，跳过 defconfig"
  fi

  log "构建 SDK 基础库 (m1_sdk_lib)..."
  make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg m1_sdk_lib-rebuild \
      2>&1 | tee "${LOG_DIR}/sdk_lib.log" \
      || fail "SDK 基础库构建失败，请查看 ${LOG_DIR}/sdk_lib.log"
  log "✓ SDK 基础库构建完成"
else
  log "Step 1/6: SDK 基础库构建已跳过 (--skip-sdk)"
fi

# ─── Step 2：构建 ssne_ai_demo（人脸检测 Demo）────────────────────────────────
if [[ ${SKIP_DEMO} -eq 0 ]]; then
  step "Step 2/6: ssne_ai_demo 编译 (SCRFD 人脸检测)"
  cd "${SDK_DIR}"
  log "清除旧构建..."
  rm -rf output/build/ssne_ai_demo/
  log "构建 ssne_ai_demo..."
  make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo \
      2>&1 | tee "${LOG_DIR}/ssne_ai_demo.log" \
      || fail "ssne_ai_demo 构建失败，请查看 ${LOG_DIR}/ssne_ai_demo.log"
  log "✓ ssne_ai_demo 构建完成"
else
  log "Step 2/6: ssne_ai_demo 构建已跳过 (--skip-demo)"
fi

# ─── Step 3：构建 ssne_vision_demo（综合视觉 Demo）────────────────────────────
if [[ ${SKIP_DEMO} -eq 0 ]]; then
  step "Step 3/6: ssne_vision_demo 编译 (YOLOv8+OSD+雷达+调试接口)"
  cd "${SDK_DIR}"
  log "清除旧构建..."
  rm -rf output/build/ssne_vision_demo/
  log "构建 ssne_vision_demo..."
  make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_vision_demo \
      2>&1 | tee "${LOG_DIR}/ssne_vision_demo.log" \
      || fail "ssne_vision_demo 构建失败，请查看 ${LOG_DIR}/ssne_vision_demo.log"
  log "✓ ssne_vision_demo 构建完成"
else
  log "Step 3/6: ssne_vision_demo 构建已跳过 (--skip-demo)"
fi

# ─── Step 3.5：构建 ssne_face_drive_demo（人脸驱动兼容性测试）─────────────────
if [[ ${SKIP_DEMO} -eq 0 ]]; then
  step "Step 3.5/6: ssne_face_drive_demo 编译 (人脸检测+底盘控制兼容性测试)"
  cd "${SDK_DIR}"
  log "清除旧构建..."
  rm -rf output/build/ssne_face_drive_demo/
  log "构建 ssne_face_drive_demo..."
  make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_face_drive_demo \
      2>&1 | tee "${LOG_DIR}/ssne_face_drive_demo.log" \
      || fail "ssne_face_drive_demo 构建失败，请查看 ${LOG_DIR}/ssne_face_drive_demo.log"
  log "✓ ssne_face_drive_demo 构建完成"
else
  log "Step 3.5/6: ssne_face_drive_demo 构建已跳过 (--skip-demo)"
fi

# ─── Step 4：构建 ROS2 工作区 ─────────────────────────────────────────────────
if [[ ${SKIP_ROS} -eq 0 ]]; then
  step "Step 4/6: ROS2 工作区编译 (colcon build)"
  if [[ -f /opt/ros/jazzy/setup.bash ]]; then
    ROS_ARGS=""
    [[ ${CLEAN_BUILD} -eq 1 ]] && ROS_ARGS="${ROS_ARGS} --clean"
    [[ ${VERBOSE}     -eq 1 ]] && ROS_ARGS="${ROS_ARGS} --verbose"
    log "构建 ROS2 工作区 (Jazzy)..."
    bash "${SCRIPT_DIR}/build_ros2_ws.sh" ${ROS_ARGS} \
        2>&1 | tee "${LOG_DIR}/ros2_ws.log" \
        || log "⚠ ROS2 构建有错误（非致命），请查看 ${LOG_DIR}/ros2_ws.log"
    log "✓ ROS2 构建完成"
  else
    log "⚠ 未找到 /opt/ros/jazzy/setup.bash，跳过 ROS2 构建"
  fi
else
  log "Step 4/6: ROS2 工作区构建已跳过 (--skip-ros)"
fi

# ─── Step 5：收集 EVB 产物 ────────────────────────────────────────────────────
if [[ ${SKIP_COLLECT} -eq 0 ]]; then
  step "Step 5/6: 收集 EVB 产物"
  if [[ -x "${SCRIPT_DIR}/collect_evb_artifacts.sh" ]]; then
    bash "${SCRIPT_DIR}/collect_evb_artifacts.sh" \
        2>&1 | tee "${LOG_DIR}/collect.log" \
        || log "⚠ 产物收集有警告"
    log "✓ 产物收集完成"
  else
    log "⚠ 产物收集脚本不存在或不可执行，已跳过"
  fi
else
  log "Step 5/6: 产物收集已跳过 (--skip-collect)"
fi

# ─── 构建摘要 ─────────────────────────────────────────────────────────────────
step "构建完成摘要"

EVB_IMAGE="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_BIN="${SDK_DIR}/output/target/app_demo/ssne_ai_demo"
VISION_BIN="${SDK_DIR}/output/target/app_demo/ssne_vision_demo"
FACE_DRIVE_BIN="${SDK_DIR}/output/target/app_demo/ssne_face_drive_demo"

check_file() {
  if [[ -f "$1" ]]; then
    log "  ✓ $(ls -lh "$1" | awk '{print $5, $9}')"
  else
    log "  ✗ 未找到: $1"
  fi
}

check_file "${EVB_IMAGE}"
check_file "${DEMO_BIN}"
check_file "${VISION_BIN}"
check_file "${FACE_DRIVE_BIN}"

log ""
log "日志保存于: ${LOG_DIR}/"
log ""
log "========================================="
log "✓ 全量构建完成"
log "========================================="

