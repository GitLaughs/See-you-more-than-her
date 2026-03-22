#!/usr/bin/env bash
# build_vision_stack.sh — 全栈构建脚本
# 依次完成：SDK 基础库 → ssne_ai_demo → ssne_vision_demo → ROS2 工作区 → 收集产物
#
# 用法（在 A1_Builder 容器内执行）：
#   bash /app/src/scripts/build_vision_stack.sh [--skip-sdk] [--skip-ros] [--skip-collect]
#
# 选项：
#   --skip-sdk      跳过 SDK 基础库构建（SDK 库已存在时使用）
#   --skip-demo     跳过 SSNE Demo 构建（只构建 ROS）
#   --skip-ros      跳过 ROS2 工作区构建
#   --skip-collect  跳过产物收集

set -euo pipefail

# ─── 路径配置 ─────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
SRC_DIR="${ROOT_DIR}/src"
ROS_WS="${SRC_DIR}/ros2_ws"
OUTPUT_DIR="${ROOT_DIR}/output"
LOG_DIR="${OUTPUT_DIR}/logs"

# ─── 参数解析 ─────────────────────────────────────────────────────────────────
SKIP_SDK=0
SKIP_DEMO=0
SKIP_ROS=0
SKIP_COLLECT=0

for arg in "$@"; do
    case "${arg}" in
        --skip-sdk)     SKIP_SDK=1 ;;
        --skip-demo)    SKIP_DEMO=1 ;;
        --skip-ros)     SKIP_ROS=1 ;;
        --skip-collect) SKIP_COLLECT=1 ;;
        -h|--help)
            grep '^#' "$0" | head -15 | sed 's/^# \{0,2\}//'
            exit 0
            ;;
        *)
            echo "[build_vision_stack.sh] ERROR: Unknown option: ${arg}" >&2
            exit 1
            ;;
    esac
done

# ─── 辅助函数 ─────────────────────────────────────────────────────────────────
log()  { echo "[build_vision_stack.sh] $*"; }
step() { echo; echo "══════════════════════════════════════════════════════"; \
         echo "  $*"; \
         echo "══════════════════════════════════════════════════════"; }
fail() { echo "[build_vision_stack.sh] FAILED: $*" >&2; exit 1; }

# ─── 环境检测 ─────────────────────────────────────────────────────────────────
log "root=${ROOT_DIR}"
log "sdk =${SDK_DIR}"
log "src =${SRC_DIR}"

mkdir -p /app
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk
mkdir -p "${LOG_DIR}"

[[ -d "${SDK_DIR}" ]] || fail "SDK directory not found: ${SDK_DIR}"

# ─── Step 1：SDK 基础库 ────────────────────────────────────────────────────────
if [[ ${SKIP_SDK} -eq 0 ]]; then
    step "Step 1/5: SDK 基础库编译 (SmartSens M1 SDK)"
    cd "${SDK_DIR}"

    if [[ ! -f output/.config ]]; then
        log "Applying defconfig: smartsens_m1pro_release_defconfig"
        make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg smartsens_m1pro_release_defconfig \
            2>&1 | tee "${LOG_DIR}/sdk_defconfig.log"
    else
        log "Found existing .config, skipping defconfig"
    fi

    log "Building SDK base libraries (m1_sdk_lib)"
    make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg m1_sdk_lib-rebuild \
        2>&1 | tee "${LOG_DIR}/sdk_lib.log" \
        || fail "SDK base library build failed. Check ${LOG_DIR}/sdk_lib.log"
    log "Step 1 done."
else
    log "Step 1/5: SDK 基础库构建已跳过 (--skip-sdk)"
fi

# ─── Step 2：ssne_ai_demo（人脸检测 Demo）─────────────────────────────────────
if [[ ${SKIP_DEMO} -eq 0 ]]; then
    step "Step 2/5: ssne_ai_demo 编译 (SCRFD 人脸检测)"
    cd "${SDK_DIR}"
    log "Cleaning previous build..."
    rm -rf output/build/ssne_ai_demo/
    log "Building ssne_ai_demo..."
    make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo \
        2>&1 | tee "${LOG_DIR}/ssne_ai_demo.log" \
        || fail "ssne_ai_demo build failed. Check ${LOG_DIR}/ssne_ai_demo.log"
    log "Step 2 done."
else
    log "Step 2/5: ssne_ai_demo 构建已跳过 (--skip-demo)"
fi

# ─── Step 3：ssne_vision_demo（综合视觉 Demo）────────────────────────────────
if [[ ${SKIP_DEMO} -eq 0 ]]; then
    step "Step 3/5: ssne_vision_demo 编译 (YOLOv8+OSD+雷达+调试接口)"
    cd "${SDK_DIR}"
    log "Cleaning previous build..."
    rm -rf output/build/ssne_vision_demo/
    log "Building ssne_vision_demo..."
    make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_vision_demo \
        2>&1 | tee "${LOG_DIR}/ssne_vision_demo.log" \
        || fail "ssne_vision_demo build failed. Check ${LOG_DIR}/ssne_vision_demo.log"
    log "Step 3 done."
else
    log "Step 3/5: ssne_vision_demo 构建已跳过 (--skip-demo)"
fi

# ─── Step 4：ROS2 工作区 ───────────────────────────────────────────────────────
if [[ ${SKIP_ROS} -eq 0 ]]; then
    step "Step 4/5: ROS2 工作区编译 (colcon build)"
    if [[ -f /opt/ros/jazzy/setup.bash ]]; then
        log "Sourcing ROS2 Jazzy..."
        bash "${ROOT_DIR}/scripts/build_ros2_ws.sh" \
            2>&1 | tee "${LOG_DIR}/ros2_ws.log" \
            || {
                log "WARN: ROS2 build had errors (non-fatal). Check ${LOG_DIR}/ros2_ws.log"
            }
        log "Step 4 done."
    else
        log "WARN: /opt/ros/jazzy/setup.bash not found, skipping ROS2 build"
    fi
else
    log "Step 4/5: ROS2 工作区构建已跳过 (--skip-ros)"
fi

# ─── Step 5：收集 EVB 产物 ─────────────────────────────────────────────────────
if [[ ${SKIP_COLLECT} -eq 0 ]]; then
    step "Step 5/5: 收集 EVB 产物"
    bash "${ROOT_DIR}/scripts/collect_evb_artifacts.sh" \
        2>&1 | tee "${LOG_DIR}/collect.log" \
        || log "WARN: artifact collection had warnings"
    log "Step 5 done."
else
    log "Step 5/5: 产物收集已跳过 (--skip-collect)"
fi

# ─── 构建摘要 ─────────────────────────────────────────────────────────────────
step "构建完成摘要"

EVB_IMAGE="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_BIN="${SDK_DIR}/output/target/app_demo/ssne_ai_demo"
VISION_BIN="${SDK_DIR}/output/target/app_demo/ssne_vision_demo"

check_file() {
    if [[ -f "$1" ]]; then
        log "  ✓ $(ls -lh "$1" | awk '{print $5, $9}')"
    else
        log "  ✗ NOT FOUND: $1"
    fi
}

check_file "${EVB_IMAGE}"
check_file "${DEMO_BIN}"
check_file "${VISION_BIN}"

log ""
log "Logs saved to: ${LOG_DIR}/"
log ""
log "build_vision_stack.sh 完成"
