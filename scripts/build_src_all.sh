#!/bin/bash
# build_src_all.sh — 全量一键构建脚本（SDK + ROS2）
#
# 参考: data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh
#
# 用法:
#   bash build_src_all.sh [--skip-sdk] [--clean] [--verbose]
#
# 选项:
#   --skip-sdk    跳过 SDK 构建，仅构建 ROS2（SDK 已构建时使用）
#   --clean       清除 ROS2 build/install/log 后重新编译
#   --verbose,-v  显示详细输出
#   -h, --help    显示帮助信息
#
# 构建流程:
#   [1] SmartSens SDK 基础库（可选）
#   [2] ROS2 工作区（colcon build，跳过 P1 COLCON_IGNORE 包）
#   [3] 收集 EVB 产物到 output/evb/

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_WS="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"

SKIP_SDK=0
CLEAN_BUILD=0
VERBOSE=0

for arg in "$@"; do
  case "${arg}" in
    --skip-sdk)   SKIP_SDK=1 ;;
    --clean)      CLEAN_BUILD=1 ;;
    --verbose|-v) VERBOSE=1 ;;
    -h|--help)
      grep '^#' "$0" | head -25 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    *)
      echo "[build_src_all.sh] 未知选项: ${arg}" >&2
      exit 1
      ;;
  esac
done

echo "========================================="
echo "[build_src_all.sh] 全量构建 (SDK + ROS2)"
echo "========================================="
echo "根目录:    ${ROOT_DIR}"
echo "SDK 目录:  ${SDK_DIR}"
echo "ROS 目录:  ${ROS_WS}"
echo "跳过SDK:   ${SKIP_SDK}"
echo "清理构建:  ${CLEAN_BUILD}"
echo "详细输出:  ${VERBOSE}"
echo "========================================="

# 建立 Docker 内路径软链接（兼容容器环境）
mkdir -p /app 2>/dev/null || true
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk 2>/dev/null || true

# 检查目录
if [[ ${SKIP_SDK} -eq 0 ]]; then
  if [[ ! -d "${SDK_DIR}" ]]; then
    echo "[build_src_all.sh] 错误: SDK 目录不存在: ${SDK_DIR}" >&2
    exit 1
  fi
  if [[ ! -x "${SDK_DIR}/scripts/a1_sc132gs_build.sh" ]]; then
    echo "[build_src_all.sh] 错误: SDK 构建脚本不可执行" >&2
    echo "  路径: ${SDK_DIR}/scripts/a1_sc132gs_build.sh" >&2
    exit 1
  fi
fi

if [[ ! -d "${ROS_WS}" ]]; then
  echo "[build_src_all.sh] 错误: ROS 工作区不存在: ${ROS_WS}" >&2
  exit 1
fi

# Step 1：构建 SDK
if [[ ${SKIP_SDK} -eq 0 ]]; then
  echo ""
  echo "[build_src_all.sh] Step 1/3: SmartSens SDK 构建..."
  cd "${SDK_DIR}"
  bash scripts/a1_sc132gs_build.sh
  echo "[build_src_all.sh] ✓ SDK 构建完成"
else
  echo ""
  echo "[build_src_all.sh] Step 1/3: SDK 构建已跳过 (--skip-sdk)"
fi

# Step 2：构建 ROS2 工作区
echo ""
echo "[build_src_all.sh] Step 2/3: ROS2 工作区构建..."
ROS_ARGS=""
[[ ${CLEAN_BUILD} -eq 1 ]] && ROS_ARGS="${ROS_ARGS} --clean"
[[ ${VERBOSE}     -eq 1 ]] && ROS_ARGS="${ROS_ARGS} --verbose"

bash "${SCRIPT_DIR}/build_ros2_ws.sh" ${ROS_ARGS}
echo "[build_src_all.sh] ✓ ROS2 构建完成"

# Step 3：收集 EVB 产物
echo ""
echo "[build_src_all.sh] Step 3/3: 收集 EVB 产物..."
if [[ -x "${SCRIPT_DIR}/collect_evb_artifacts.sh" ]]; then
  bash "${SCRIPT_DIR}/collect_evb_artifacts.sh"
  echo "[build_src_all.sh] ✓ 产物收集完成"
else
  echo "[build_src_all.sh] ⚠ 产物收集脚本不存在或不可执行，已跳过"
fi

echo ""
echo "========================================="
echo "[build_src_all.sh] ✓ 全量构建完成"
echo "========================================="

