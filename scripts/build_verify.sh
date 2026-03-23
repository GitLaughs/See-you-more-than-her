#!/usr/bin/env bash
set -euo pipefail

# Script: build_verify.sh
# Purpose: Verify ROS2 workspace configuration and dependencies
# Can be run before actual build to catch issues early
# Updated: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROS_DIR="${ROOT_DIR}/src/ros2_ws"

echo "========================================="
echo "[build_verify.sh] ROS2 Workspace Verification"
echo "========================================="

# 1. Check workspace structure
echo ""
echo "[1] Checking workspace structure..."
if [ ! -d "${ROS_DIR}" ]; then
  echo "  ✗ Workspace directory not found: ${ROS_DIR}"
  exit 1
fi
echo "  ✓ Workspace exists: ${ROS_DIR}"

if [ ! -f "${ROS_DIR}/package.xml" ]; then
  echo "  ✗ Workspace package.xml not found"
  exit 1
fi
echo "  ✓ Workspace package.xml found"

# 2. Count packages
echo ""
echo "[2] Checking ROS packages..."
SRC_DIR="${ROS_DIR}/src"
if [ ! -d "${SRC_DIR}" ]; then
  echo "  ✗ Source directory not found: ${SRC_DIR}"
  exit 1
fi

PKG_COUNT=$(find "${SRC_DIR}" -maxdepth 2 -name "package.xml" | wc -l)
echo "  ✓ Found ${PKG_COUNT} packages"

# List packages
echo ""
echo "[3] Package list:"
find "${SRC_DIR}" -maxdepth 2 -name "package.xml" | while read pkg_xml; do
  pkg_dir=$(dirname "$pkg_xml")
  pkg_name=$(basename "$pkg_dir")
  format=$(grep -oP '(?<=format=")[^"]+' "$pkg_xml" 2>/dev/null || echo "unknown")
  echo "    • ${pkg_name} (format=${format})"
done

# 4. Check ROS2 installation
echo ""
echo "[4] Checking ROS2 environment..."
if [ ! -f /opt/ros/jazzy/setup.bash ]; then
  echo "  ⚠ ROS2 Jazzy not found at /opt/ros/jazzy/setup.bash"
  echo "  Note: Build will fail if ROS2 is not available"
  echo "  This is expected when running outside Docker container"
else
  echo "  ✓ ROS2 Jazzy found"
  
  # Try to source and check
  set +u
  source /opt/ros/jazzy/setup.bash
  set -u
  
  if command -v colcon &> /dev/null; then
    COLCON_VERSION=$(colcon --version 2>/dev/null || echo "unknown")
    echo "  ✓ colcon available: ${COLCON_VERSION}"
  else
    echo "  ✗ colcon not found in PATH"
  fi
fi

# 5. Check common tools
echo ""
echo "[5] Checking build tools..."
TOOLS=("cmake" "make" "g++")
for tool in "${TOOLS[@]}"; do
  if command -v "$tool" &> /dev/null; then
    VERSION=$($tool --version 2>&1 | head -1)
    echo "  ✓ ${tool}: ${VERSION}"
  else
    echo "  ⚠ ${tool}: not found"
  fi
done

# 6. Check key packages
echo ""
echo "[6] Checking key package.xml files..."
KEY_PACKAGES=(
  "wheeltec_robot_msg"
  "turn_on_wheeltec_robot"
  "wheeltec_multi"
)

for pkg in "${KEY_PACKAGES[@]}"; do
  pkg_xml="${SRC_DIR}/${pkg}/package.xml"
  if [ -f "$pkg_xml" ]; then
    # Check if it has jazzy reference
    if grep -q "jazzy" "$pkg_xml"; then
      echo "  ✓ ${pkg} (jazzy)"
    elif grep -q "humble" "$pkg_xml"; then
      echo "  ⚠ ${pkg} (still has humble)"
    else
      echo "  ✓ ${pkg}"
    fi
  else
    echo "  ✗ ${pkg} (not found)"
  fi
done

echo ""
echo "========================================="
echo "[build_verify.sh] Verification COMPLETE"
echo "========================================="
echo ""
echo "To build the workspace, run:"
echo "  ./scripts/build_ros2_ws.sh"
echo ""
echo "For clean build, run:"
echo "  ./scripts/build_ros2_ws.sh --clean"
echo ""
