#!/usr/bin/env bash
set -euo pipefail

# 脚本: build_verify.sh
# 功能: 验证 ROS2 工作区配置和依赖项
# 说明: 在正式构建前运行，可提前发现配置问题
# 更新: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROS_DIR="${ROOT_DIR}/src/ros2_ws"

echo "========================================="
echo "[build_verify.sh] ROS2 工作区预检"
echo "========================================="

# 1. 检查工作区结构
echo ""
echo "[1] 检查工作区结构..."
if [ ! -d "${ROS_DIR}" ]; then
  echo "  ✗ 工作区目录不存在: ${ROS_DIR}"
  exit 1
fi
echo "  ✓ 工作区存在: ${ROS_DIR}"

if [ ! -f "${ROS_DIR}/package.xml" ]; then
  echo "  ✗ 工作区 package.xml 未找到"
  exit 1
fi
echo "  ✓ 工作区 package.xml 已找到"

# 2. 统计包数量
echo ""
echo "[2] 检查 ROS 包..."
SRC_DIR="${ROS_DIR}/src"
if [ ! -d "${SRC_DIR}" ]; then
  echo "  ✗ 源码目录不存在: ${SRC_DIR}"
  exit 1
fi

PKG_COUNT=$(find "${SRC_DIR}" -maxdepth 2 -name "package.xml" | wc -l)
echo "  ✓ 发现 ${PKG_COUNT} 个包"

# 3. 列出所有包
echo ""
echo "[3] 包列表:"
find "${SRC_DIR}" -maxdepth 2 -name "package.xml" | while read pkg_xml; do
  pkg_dir=$(dirname "$pkg_xml")
  pkg_name=$(basename "$pkg_dir")
  format=$(grep -oP '(?<=format=")[^"]+' "$pkg_xml" 2>/dev/null || echo "unknown")
  echo "    • ${pkg_name} (format=${format})"
done

# 4. 检查 ROS2 安装
echo ""
echo "[4] 检查 ROS2 环境..."
if [ ! -f /opt/ros/jazzy/setup.bash ]; then
  echo "  ⚠ 未找到 ROS2 Jazzy: /opt/ros/jazzy/setup.bash"
  echo "  注意: 如果 ROS2 不可用，构建将失败"
  echo "  在 Docker 容器外运行时属正常现象"
else
  echo "  ✓ ROS2 Jazzy 已安装"
  
  # 尝试加载环境并检查
  set +u
  source /opt/ros/jazzy/setup.bash
  set -u
  
  if command -v colcon &> /dev/null; then
    COLCON_VERSION=$(colcon --version 2>/dev/null || echo "unknown")
    echo "  ✓ colcon 可用: ${COLCON_VERSION}"
  else
    echo "  ✗ colcon 未在 PATH 中找到"
  fi
fi

# 5. 检查构建工具
echo ""
echo "[5] 检查构建工具..."
TOOLS=("cmake" "make" "g++")
for tool in "${TOOLS[@]}"; do
  if command -v "$tool" &> /dev/null; then
    VERSION=$($tool --version 2>&1 | head -1)
    echo "  ✓ ${tool}: ${VERSION}"
  else
    echo "  ⚠ ${tool}: 未找到"
  fi
done

# 6. 检查关键包
echo ""
echo "[6] 检查关键 package.xml 文件..."
KEY_PACKAGES=(
  "wheeltec_robot_msg"
  "turn_on_wheeltec_robot"
  "wheeltec_multi"
)

for pkg in "${KEY_PACKAGES[@]}"; do
  pkg_xml="${SRC_DIR}/${pkg}/package.xml"
  if [ -f "$pkg_xml" ]; then
    if grep -q "jazzy" "$pkg_xml"; then
      echo "  ✓ ${pkg} (jazzy)"
    elif grep -q "humble" "$pkg_xml"; then
      echo "  ⚠ ${pkg} (仍含 humble 引用)"
    else
      echo "  ✓ ${pkg}"
    fi
  else
    echo "  ✗ ${pkg} (未找到)"
  fi
done

echo ""
echo "========================================="
echo "[build_verify.sh] 预检完成"
echo "========================================="
echo ""
echo "构建工作区请运行:"
echo "  ./scripts/build_ros2_ws.sh"
echo ""
echo "清理后全量构建:"
echo "  ./scripts/build_ros2_ws.sh --clean"
echo ""
