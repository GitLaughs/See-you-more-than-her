#!/usr/bin/env bash
set -euo pipefail

# Script: build_ros2_ws.sh
# Purpose: Build ROS2 workspace with improved path handling
# Supports: Local environment and Docker environment
# Updated: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROS_DIR="${ROOT_DIR}/src/ros2_ws"
BUILD_DIR="${ROS_DIR}/build"
INSTALL_DIR="${ROS_DIR}/install"
LOG_DIR="${ROS_DIR}/log"

# Default options
CLEAN_BUILD=0
PACKAGES=()
VERBOSE=0

# Parse arguments
while [ $# -gt 0 ]; do
  case "$1" in
    --clean)
      CLEAN_BUILD=1
      shift
      ;;
    --verbose|-v)
      VERBOSE=1
      shift
      ;;
    --help|-h)
      cat << 'EOF'
Usage: build_ros2_ws.sh [OPTIONS] [PACKAGES...]

Options:
  --clean              Remove build/install/log directories before building
  --verbose, -v        Enable verbose output
  --help, -h          Show this help message

Examples:
  # Build entire ROS2 workspace
  ./build_ros2_ws.sh

  # Clean build
  ./build_ros2_ws.sh --clean

  # Build specific packages
  ./build_ros2_ws.sh turn_on_wheeltec_robot wheeltec_multi

  # Build packages and their dependencies
  ./build_ros2_ws.sh --clean turn_on_wheeltec_robot
EOF
      exit 0
      ;;
    --)
      shift
      while [ $# -gt 0 ]; do
        PACKAGES+=("$1")
        shift
      done
      ;;
    *)
      PACKAGES+=("$1")
      shift
      ;;
  esac
done

# Validation
if [ ! -d "${ROS_DIR}" ]; then
  echo "[build_ros2_ws.sh] ERROR: ROS workspace not found: ${ROS_DIR}" >&2
  exit 1
fi

if [ ! -f /opt/ros/jazzy/setup.bash ]; then
  echo "[build_ros2_ws.sh] ERROR: ROS2 Jazzy not installed at /opt/ros/jazzy/setup.bash" >&2
  echo "[build_ros2_ws.sh] Please install ROS2 Jazzy or run in Docker environment" >&2
  exit 1
fi

# Log build info
echo "========================================="
echo "[build_ros2_ws.sh] ROS2 Workspace Build"
echo "========================================="
echo "Root:     ${ROOT_DIR}"
echo "Workspace: ${ROS_DIR}"
echo "Build dir: ${BUILD_DIR}"
echo "Clean:     ${CLEAN_BUILD}"
echo "Verbose:   ${VERBOSE}"
if [ ${#PACKAGES[@]} -gt 0 ]; then
  echo "Packages:  ${PACKAGES[*]}"
else
  echo "Packages:  ALL"
fi
echo "========================================="

# Enter workspace directory
pushd "${ROS_DIR}" >/dev/null

# Clean if requested
if [ "${CLEAN_BUILD}" -eq 1 ]; then
  echo "[build_ros2_ws.sh] Cleaning build artifacts..."
  rm -rf "${BUILD_DIR}" "${INSTALL_DIR}" "${LOG_DIR}"
  echo "[build_ros2_ws.sh] Clean complete"
fi

# Setup ROS2 environment
echo "[build_ros2_ws.sh] Setting up ROS2 Jazzy environment..."
set +u
source /opt/ros/jazzy/setup.bash
set -u

# Run colcon build
echo "[build_ros2_ws.sh] Starting colcon build..."
if [ "${#PACKAGES[@]}" -gt 0 ]; then
  echo "[build_ros2_ws.sh] Building selected packages: ${PACKAGES[*]}"
  COLCON_ARGS="--symlink-install --packages-up-to ${PACKAGES[*]}"
else
  echo "[build_ros2_ws.sh] Building full workspace"
  COLCON_ARGS="--symlink-install"
fi

if [ "${VERBOSE}" -eq 1 ]; then
  COLCON_ARGS="${COLCON_ARGS} --event-handlers console_direct+"
fi

colcon build ${COLCON_ARGS}
BUILD_RESULT=$?

popd >/dev/null

# Report results
echo "========================================="
if [ ${BUILD_RESULT} -eq 0 ]; then
  echo "[build_ros2_ws.sh] ✓ Build SUCCESS"
  echo "========================================="
  exit 0
else
  echo "[build_ros2_ws.sh] ✗ Build FAILED (exit code: ${BUILD_RESULT})"
  echo "========================================="
  exit ${BUILD_RESULT}
fi
