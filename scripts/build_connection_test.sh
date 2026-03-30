#!/bin/bash
# build_connection_test.sh — 完整 EVB 镜像构建 (含连接测试程序)
#
# 用途：一键编译完整 EVB 固件（SDK + 连接测试 + ROS2 + zImage 内核打包）
#       产物可直接烧录到 A1 开发板运行连接测试。
#
# 用法（在 A1_Builder 容器内执行）:
#   bash /app/scripts/build_connection_test.sh [--clean] [--skip-ros] [--verbose]
#
# 参数:
#   --clean      清除 Buildroot 缓存（完全重建，耗时较长）
#   --skip-ros   跳过 ROS2 工作区编译
#   --verbose    显示详细编译输出
#
# 构建流程:
#   [1] SDK 基础库（build_release_sdk.sh 第一次）
#   [2] ssne_connection_test（交叉编译连接测试程序，植入 rootfs）
#   [3] ssne_face_drive_demo（人脸检测 + 底盘控制，保留完整功能）
#   [4] ROS2 工作区（可跳过）
#   [5] 重新打包 zImage（build_release_sdk.sh 第二次，将最新应用打入 initramfs）
#   [6] 产物保存到 output/evb/<YYYYMMDD_HHMMSS>/

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
SRC_DIR="${ROOT_DIR}/src/a1_connection_test"
ROS_WS="${ROOT_DIR}/src/ros2_ws"
OUTPUT_BASE="${ROOT_DIR}/output/evb"
LOG_DIR="${ROOT_DIR}/output/logs"

# Buildroot 工具链路径（SDK 编译后可用）
TOOLCHAIN_DIR="${SDK_DIR}/output/host"
CROSS_COMPILE="${TOOLCHAIN_DIR}/bin/arm-linux-"
SYSROOT="${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot"

# SDK 头文件和库文件路径
M1_SDK_INC="${SDK_DIR}/output/opt/m1_sdk/usr/include"
M1_SDK_LIB="${SDK_DIR}/output/opt/m1_sdk/usr/lib"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
CLEAN_BUILD=0
SKIP_ROS=0
VERBOSE=0

for arg in "$@"; do
  case "${arg}" in
    --clean)      CLEAN_BUILD=1 ;;
    --skip-ros)   SKIP_ROS=1 ;;
    --verbose|-v) VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -30 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    *)
      echo "[连接测试构建] 未知选项: ${arg}" >&2
      exit 1
      ;;
  esac
done

# ─── 辅助函数 ─────────────────────────────────────────────────────────────────
log()  { echo "[连接测试构建] $*"; }
step() { echo; echo "══════════════════════════════════════════════════════"; \
         echo "  $*"; \
         echo "══════════════════════════════════════════════════════"; }
fail() { echo "[连接测试构建] 失败: $*" >&2; exit 1; }
elapsed() { echo "$(( ($SECONDS - START_TIME) / 60 )) 分钟"; }

# ─── 产物目录（每次构建独立时间戳）────────────────────────────────────────────
BUILD_TS=$(date +%Y%m%d_%H%M%S)
ARTIFACT_DIR="${OUTPUT_BASE}/${BUILD_TS}"
mkdir -p "${ARTIFACT_DIR}" "${LOG_DIR}"

# 更新 latest 软链接
ln -sfn "${ARTIFACT_DIR}" "${OUTPUT_BASE}/latest"

# ─── 开始信息 ────────────────────────────────────────────────────────────────
START_TIME=$SECONDS
echo "========================================="
echo " 完整 EVB 镜像构建 (含连接测试)"
echo "========================================="
echo "构建时间:    ${BUILD_TS}"
echo "SDK 目录:    ${SDK_DIR}"
echo "测试源码:    ${SRC_DIR}"
echo "产物目录:    ${ARTIFACT_DIR}"
echo "========================================="

[[ -d "${SDK_DIR}" ]] || fail "SDK 目录不存在: ${SDK_DIR}"
[[ -d "${SRC_DIR}" ]] || fail "测试源码目录不存在: ${SRC_DIR}"

# ═════════════════════════════════════════════════════════════════════════════
# Step 1：SDK 基础库（首次）
# ═════════════════════════════════════════════════════════════════════════════
step "Step 1/5: SDK 基础库编译 (build_release_sdk.sh)"
cd "${SDK_DIR}"

if [[ ${CLEAN_BUILD} -eq 1 ]]; then
  log "--clean 模式：清除 Buildroot 缓存..."
  rm -rf output/build output/.config 2>/dev/null || true
fi

log "执行 build_release_sdk.sh ..."
bash scripts/build_release_sdk.sh 2>&1 | tee "${LOG_DIR}/sdk_pass1.log" \
  || fail "SDK 基础库编译失败，日志：${LOG_DIR}/sdk_pass1.log"
log "✓ SDK 基础库编译成功"

# ═════════════════════════════════════════════════════════════════════════════
# Step 2：交叉编译 ssne_connection_test 并植入 rootfs
# ═════════════════════════════════════════════════════════════════════════════
step "Step 2/5: ssne_connection_test（A1 ↔ STM32 连接测试）"

# 前置检查
[[ -f "${CROSS_COMPILE}gcc" ]] \
  || fail "交叉编译工具链未找到: ${CROSS_COMPILE}gcc"
[[ -f "${M1_SDK_LIB}/libgpio.so" ]] \
  || fail "SDK 库文件未找到: ${M1_SDK_LIB}/libgpio.so"
[[ -f "${M1_SDK_INC}/smartsoc/gpio_api.h" ]] \
  || fail "SDK 头文件未找到: ${M1_SDK_INC}/smartsoc/gpio_api.h"

# CMake 交叉编译
BUILD_DIR="${SRC_DIR}/build"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

log "CMake 配置..."
cmake "${SRC_DIR}" \
  -DCMAKE_C_COMPILER="${CROSS_COMPILE}gcc" \
  -DCMAKE_CXX_COMPILER="${CROSS_COMPILE}g++" \
  -DCMAKE_SYSROOT="${SYSROOT}" \
  -DM1_SDK_INC_DIR="${M1_SDK_INC}" \
  -DM1_SDK_LIB_DIR="${M1_SDK_LIB}" \
  2>&1 | tee "${LOG_DIR}/connection_test_cmake.log"

log "编译中..."
make 2>&1 | tee "${LOG_DIR}/connection_test_make.log" \
  || fail "ssne_connection_test 编译失败，日志：${LOG_DIR}/connection_test_make.log"

[[ -f "${BUILD_DIR}/ssne_connection_test" ]] \
  || fail "编译产物未找到: ${BUILD_DIR}/ssne_connection_test"

# 植入 Buildroot target rootfs（这样 zImage 重新打包时会包含测试程序）
TARGET_DIR="${SDK_DIR}/output/target"
mkdir -p "${TARGET_DIR}/app_demo/scripts"
cp "${BUILD_DIR}/ssne_connection_test" "${TARGET_DIR}/app_demo/ssne_connection_test"
chmod +x "${TARGET_DIR}/app_demo/ssne_connection_test"
cp "${SRC_DIR}/scripts/run.sh" "${TARGET_DIR}/app_demo/scripts/run_connection_test.sh"
chmod +x "${TARGET_DIR}/app_demo/scripts/run_connection_test.sh"

log "✓ ssne_connection_test 编译成功，已植入 rootfs"

# ═════════════════════════════════════════════════════════════════════════════
# Step 3：编译 ssne_face_drive_demo（保留完整应用功能）
# ═════════════════════════════════════════════════════════════════════════════
step "Step 3/5: ssne_face_drive_demo（人脸检测 + UART 底盘控制）"
cd "${SDK_DIR}"
rm -rf output/build/ssne_face_drive_demo/
make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_face_drive_demo \
    2>&1 | tee "${LOG_DIR}/demo.log" \
  || fail "ssne_face_drive_demo 编译失败，日志：${LOG_DIR}/demo.log"
log "✓ ssne_face_drive_demo 编译成功"

# ═════════════════════════════════════════════════════════════════════════════
# Step 4：ROS2（可选）
# ═════════════════════════════════════════════════════════════════════════════
if [[ ${SKIP_ROS} -eq 0 ]]; then
  step "Step 4/5: ROS2 工作区编译"
  if [[ -f /opt/ros/jazzy/setup.bash ]]; then
    bash "${SCRIPT_DIR}/build_ros2_ws.sh" \
        2>&1 | tee "${LOG_DIR}/ros2.log" \
      || log "⚠ ROS2 编译有错误，继续打包 EVB..."
    log "✓ ROS2 编译完成"
  else
    log "⚠ 未找到 ROS2 Jazzy，跳过 ROS2 编译"
  fi
else
  log "Step 4/5: ROS2 编译已跳过 (--skip-ros)"
fi

# ═════════════════════════════════════════════════════════════════════════════
# Step 5：重新打包 zImage（将最新应用打入 initramfs）
# ═════════════════════════════════════════════════════════════════════════════
step "Step 5/5: 重新打包 zImage（将最新应用写入 initramfs）"
cd "${SDK_DIR}"
bash scripts/build_release_sdk.sh 2>&1 | tee "${LOG_DIR}/sdk_pass2.log" \
  || fail "zImage 重新打包失败，日志：${LOG_DIR}/sdk_pass2.log"
log "✓ zImage 重新打包成功"

# ─── 收集产物到时间戳目录 ─────────────────────────────────────────────────────
step "收集产物 → ${ARTIFACT_DIR}/"
EVB_SRC="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_SRC="${SDK_DIR}/output/target/app_demo/ssne_face_drive_demo"
TEST_SRC="${SDK_DIR}/output/target/app_demo/ssne_connection_test"

[[ -f "${EVB_SRC}" ]] || fail "未找到 zImage：${EVB_SRC}"
cp "${EVB_SRC}" "${ARTIFACT_DIR}/zImage.smartsens-m1-evb"
log "  ✓ zImage.smartsens-m1-evb           ($(du -sh "${ARTIFACT_DIR}/zImage.smartsens-m1-evb" | cut -f1))"

if [[ -f "${TEST_SRC}" ]]; then
  cp "${TEST_SRC}" "${ARTIFACT_DIR}/ssne_connection_test"
  log "  ✓ ssne_connection_test              ($(du -sh "${ARTIFACT_DIR}/ssne_connection_test" | cut -f1))"
else
  log "  ⚠ ssne_connection_test 未找到，跳过"
fi

if [[ -f "${DEMO_SRC}" ]]; then
  cp "${DEMO_SRC}" "${ARTIFACT_DIR}/ssne_face_drive_demo"
  log "  ✓ ssne_face_drive_demo              ($(du -sh "${ARTIFACT_DIR}/ssne_face_drive_demo" | cut -f1))"
else
  log "  ⚠ ssne_face_drive_demo 未找到，跳过"
fi

# ─── 构建总结 ────────────────────────────────────────────────────────────────
step "构建完成"
echo "产物目录: ${ARTIFACT_DIR}"
echo "软链接:   ${OUTPUT_BASE}/latest -> ${ARTIFACT_DIR}"
echo ""
ls -lh "${ARTIFACT_DIR}/"
echo ""
echo "包含组件:"
echo "  - zImage.smartsens-m1-evb    完整内核 + rootfs (含 SDK 基础库)"
echo "  - ssne_connection_test       A1 ↔ STM32 连接测试程序"
echo "  - ssne_face_drive_demo       人脸检测 + 底盘控制主程序"
echo "  - ROS2 Jazzy                 $(if [[ ${SKIP_ROS} -eq 0 ]]; then echo '已编译'; else echo '已跳过'; fi)"
echo ""
echo "烧录命令:"
echo "  cd tools/aurora && .\\launch.ps1 --flash ${ARTIFACT_DIR}/zImage.smartsens-m1-evb"
echo ""
echo "板端连接测试:"
echo "  ssh root@<A1_IP>"
echo "  /app_demo/scripts/run_connection_test.sh"
echo ""
echo "板端人脸检测主程序:"
echo "  ssh root@<A1_IP>"
echo "  /app_demo/scripts/run.sh"
echo ""
log "总耗时: $(elapsed)"
log "✓ 完整 EVB 构建完成（含连接测试）！"
