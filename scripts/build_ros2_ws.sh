#!/bin/bash
# build_ros2_ws.sh — ROS2 工作区编译脚本
#
# 参考: data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh
#
# 用法:
#   bash build_ros2_ws.sh [--clean] [--verbose] [--with-sdk] [package ...]
#
# 选项:
#   --clean        清除 build/install/log 目录后重新编译
#   --verbose, -v  显示详细编译输出 (colcon console_direct+)
#   --with-sdk     在编译 ROS2 前先执行 SDK 构建脚本
#   package ...    仅编译指定包（及其依赖）
#   -h, --help     显示帮助信息
#
# P1 增强包（暂时屏蔽，已在各包目录放置 COLCON_IGNORE）：
#   wheeltec_robot_kcf     目标跟踪 (~50MOPS)
#   wheeltec_robot_urdf    机器人 URDF 模型
#   wheeltec_rviz2         RViz 可视化配置
#   aruco_ros              ArUco 标记检测 (~100MOPS)
#   usb_cam-ros2           USB 摄像头驱动
#   web_video_server-ros2  网络视频流 (~50MOPS)
# 解屏蔽：删除对应包目录下的 COLCON_IGNORE 文件

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
ROS_WS="${ROOT_DIR}/src/ros2_ws"
REPORT_FILE="${ROOT_DIR}/output/ros_compile_test_report.txt"

CLEAN_BUILD=0
VERBOSE=0
RUN_SDK_BUILD=0
PACKAGES=()

for arg in "$@"; do
  case "${arg}" in
    --clean)        CLEAN_BUILD=1 ;;
    --verbose|-v)   VERBOSE=1 ;;
    --with-sdk)     RUN_SDK_BUILD=1 ;;
    -h|--help)
      grep '^#' "$0" | head -30 | sed 's/^# \{0,2\}//'
      exit 0
      ;;
    -*)
      echo "[build_ros2_ws.sh] 未知选项: ${arg}" >&2
      exit 1
      ;;
    *)
      PACKAGES+=("${arg}")
      ;;
  esac
done

echo "========================================="
echo "[build_ros2_ws.sh] ROS2 工作区编译"
echo "========================================="
echo "工作区:    ${ROS_WS}"
echo "清理构建:  ${CLEAN_BUILD}"
echo "详细输出:  ${VERBOSE}"
echo "预先SDK:   ${RUN_SDK_BUILD}"
if [[ ${#PACKAGES[@]} -gt 0 ]]; then
  echo "指定包:    ${PACKAGES[*]}"
else
  echo "编译范围:  全工作区（跳过 COLCON_IGNORE 标记的 P1 包）"
fi
echo "========================================="

# 检查工作区
if [[ ! -d "${ROS_WS}" ]]; then
  echo "[build_ros2_ws.sh] 错误: 工作区目录不存在: ${ROS_WS}" >&2
  exit 1
fi

# 检查 ROS2 环境
if [[ ! -f /opt/ros/jazzy/setup.bash ]]; then
  echo "[build_ros2_ws.sh] 错误: 未找到 ROS2 Jazzy: /opt/ros/jazzy/setup.bash" >&2
  echo "[build_ros2_ws.sh] 请在 Docker 容器内运行或安装 ROS2 Jazzy" >&2
  exit 1
fi

# （可选）先构建 SDK
if [[ ${RUN_SDK_BUILD} -eq 1 ]]; then
  echo "[build_ros2_ws.sh] 先执行 SDK 构建..."
  bash "${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh"
fi

# 清理旧产物
if [[ ${CLEAN_BUILD} -eq 1 ]]; then
  echo "[build_ros2_ws.sh] 清理旧构建目录..."
  rm -rf "${ROS_WS}/build" "${ROS_WS}/install" "${ROS_WS}/log"
  echo "[build_ros2_ws.sh] 清理完成"
fi

# 加载 ROS2 环境
echo "[build_ros2_ws.sh] 加载 ROS2 Jazzy 环境..."
set +u
source /opt/ros/jazzy/setup.bash
set -u

cd "${ROS_WS}"

# 组装 colcon 构建参数
COLCON_ARGS="--symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release"

if [[ ${#PACKAGES[@]} -gt 0 ]]; then
  echo "[build_ros2_ws.sh] 仅构建指定包: ${PACKAGES[*]}"
  COLCON_ARGS="${COLCON_ARGS} --packages-up-to ${PACKAGES[*]}"
fi

if [[ ${VERBOSE} -eq 1 ]]; then
  COLCON_ARGS="${COLCON_ARGS} --event-handlers console_direct+"
fi

# 显示受 COLCON_IGNORE 屏蔽的 P1 包列表
echo ""
echo "[build_ros2_ws.sh] P1 包屏蔽状态（COLCON_IGNORE）："
P1_PKGS=(
  "wheeltec_robot_kcf"
  "wheeltec_robot_urdf"
  "wheeltec_rviz2"
  "aruco_ros-humble-devel"
  "usb_cam-ros2"
  "web_video_server-ros2"
)
for pkg in "${P1_PKGS[@]}"; do
  if [[ -f "${ROS_WS}/src/${pkg}/COLCON_IGNORE" ]]; then
    echo "  [屏蔽] ${pkg}"
  else
    echo "  [启用] ${pkg}"
  fi
done
echo ""

# 执行 colcon 构建
echo "[build_ros2_ws.sh] 开始 colcon 构建..."
colcon build ${COLCON_ARGS}
BUILD_RESULT=$?

# 加载安装环境，统计结果
if [[ ${BUILD_RESULT} -eq 0 ]]; then
  set +u
  source install/setup.bash 2>/dev/null || true
  set -u
  PKG_COUNT=$(colcon list --names-only 2>/dev/null | wc -l || echo "?")
  PKG_LIST=$(colcon list --names-only 2>/dev/null | tr '\n' ' ' | sed 's/[[:space:]]\+$//' || echo "")

  mkdir -p "$(dirname "${REPORT_FILE}")"
  cat > "${REPORT_FILE}" <<REPORT
[A1 ROS2 编译报告]
生成时间: $(date '+%Y-%m-%d %H:%M:%S')

1) 编译结论
- 编译结果: 成功
- ROS2 版本: Jazzy
- 构建类型: Release

2) 编译统计
- 构建包数量: ${PKG_COUNT}
- 构建包列表: ${PKG_LIST}

3) P1 包屏蔽状态（COLCON_IGNORE）
- wheeltec_robot_kcf      [屏蔽] 目标跟踪，~50MOPS，Sprint4 启用
- wheeltec_robot_urdf     [屏蔽] URDF 模型，Sprint3 启用
- wheeltec_rviz2          [屏蔽] RViz 配置，Sprint3 启用
- aruco_ros               [屏蔽] ArUco 检测，~100MOPS，Sprint4 启用
- usb_cam-ros2            [屏蔽] USB 摄像头，Sprint1 完成后评估
- web_video_server-ros2   [屏蔽] 网络视频流，~50MOPS，Sprint4 启用

4) 解屏蔽方法
- 删除对应包目录下的 COLCON_IGNORE 文件
- 重新运行: bash scripts/build_ros2_ws.sh
REPORT
  echo "[build_ros2_ws.sh] 报告已写入: ${REPORT_FILE}"
fi

echo "========================================="
if [[ ${BUILD_RESULT} -eq 0 ]]; then
  echo "[build_ros2_ws.sh] ✓ 编译成功"
else
  echo "[build_ros2_ws.sh] ✗ 编译失败 (exit=${BUILD_RESULT})"
fi
echo "========================================="
exit ${BUILD_RESULT}
