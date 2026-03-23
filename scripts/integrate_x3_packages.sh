#!/bin/bash
# 集成 x3_src_250401 中精选的 ROS 包到 src/ros2_ws/src
# 用法: bash scripts/integrate_x3_packages.sh [x3_src_路径] [--clean]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
X3_SRC="${1:-${PROJECT_ROOT}/x3_src_250401/src}"
ROS_WS="${PROJECT_ROOT}/src/ros2_ws/src"
CLEAN_MODE=0

# 解析参数
while [ $# -gt 0 ]; do
    case "$1" in
        --clean)
            CLEAN_MODE=1
            shift
            ;;
        *)
            X3_SRC="$1"
            shift
            ;;
    esac
done

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warn() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# 检查路径
if [ ! -d "$X3_SRC" ]; then
    print_error "x3_src 目录不存在: $X3_SRC"
    exit 1
fi

if [ ! -d "$ROS_WS" ]; then
    print_error "ROS 工作区不存在: $ROS_WS"
    exit 1
fi

print_header "x3_src ROS 包集成工具"
echo "X3_SRC: $X3_SRC"
echo "ROS_WS: $ROS_WS"
echo "清理模式: $CLEAN_MODE"
echo ""

# P0 核心包 (必须)
P0_PACKAGES=(
    "turn_on_wheeltec_robot"
    "wheeltec_robot_msg"
    "wheeltec_multi"
    "navigation2-humble"
    "wheeltec_robot_nav2"
    "wheeltec_robot_rrt2"
    "nav2_waypoint_cycle"
    "wheeltec_rrt_msg"
    "openslam_gmapping"
    "slam_gmapping"
    "wheeltec_lidar_ros2"
    "wheeltec_imu"
    "wheeltec_gps"
    "wheeltec_joy"
)

# P1 增强包
P1_PACKAGES=(
    "wheeltec_robot_kcf"
    "aruco_ros-humble-devel"
    "web_video_server-ros2"
    "wheeltec_rviz2"
    "wheeltec_robot_urdf"
    "wheeltec_robot_keyboard"
    "usb_cam-ros2"
    "ros2_astra_camera-master"
    "wheeltec_path_follow"
)

# 复制函数
copy_packages() {
    local priority=$1
    local packages=("${@:2}")
    local count=0
    local skipped=0
    
    print_header "复制 $priority 包 (${#packages[@]} 个)"
    
    for pkg in "${packages[@]}"; do
        if [ -d "$X3_SRC/$pkg" ]; then
            if [ -d "$ROS_WS/$pkg" ]; then
                if [ "$CLEAN_MODE" -eq 1 ]; then
                    rm -rf "$ROS_WS/$pkg"
                    print_warn "$pkg 已存在，覆盖中..."
                else
                    print_warn "$pkg 已存在，跳过"
                    ((skipped++))
                    continue
                fi
            fi
            
            cp -r "$X3_SRC/$pkg" "$ROS_WS/$pkg"
            print_success "$pkg ($(du -sh "$ROS_WS/$pkg" | cut -f1))"
            ((count++))
        else
            print_warn "$pkg 不存在于 x3_src"
            ((skipped++))
        fi
    done
    
    echo "  完成: $count 个包, 跳过: $skipped 个"
    return $count
}

# 执行复制
P0_COUNT=0
P1_COUNT=0

copy_packages "P0 核心" "${P0_PACKAGES[@]}"
P0_COUNT=$?

echo ""

copy_packages "P1 增强" "${P1_PACKAGES[@]}"
P1_COUNT=$?

# 统计
print_header "集成总结"
echo "P0 包: $P0_COUNT 个"
echo "P1 包: $P1_COUNT 个"
echo "总计: $((P0_COUNT + P1_COUNT)) 个包"

# 计算大小
TOTAL_SIZE=$(du -sh "$ROS_WS" | cut -f1)
echo "总大小: $TOTAL_SIZE"

# 版本适配提示
print_header "后续步骤"
echo "1. 版本适配 (Humble → Jazzy):"
echo "   bash scripts/adapt_ros_versions.sh"
echo ""
echo "2. 依赖检查:"
echo "   cd src/ros2_ws"
echo "   rosdep install --from-paths src --ignore-src -r -y"
echo ""
echo "3. 编译验证:"
echo "   colcon build --symlink-install"
echo ""
echo "4. 查看更多信息:"
echo "   cat docs/X3_PACKAGE_INTEGRATION.md"

print_success "集成完成！"
