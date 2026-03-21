#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ROS_DIR="${ROOT_DIR}/src/ros2_ws"

clean_build=0
packages=()

while [ $# -gt 0 ]; do
  case "$1" in
    --clean)
      clean_build=1
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [--clean] [package ...]"
      echo "  With no package arguments, build the full ROS2 workspace."
      echo "  With package arguments, build only the selected packages and their dependencies."
      exit 0
      ;;
    --)
      shift
      while [ $# -gt 0 ]; do
        packages+=("$1")
        shift
      done
      ;;
    *)
      packages+=("$1")
      shift
      ;;
  esac
done

if [ ! -d "${ROS_DIR}" ]; then
  echo "[build_ros2_ws.sh] ERROR: ROS workspace not found: ${ROS_DIR}" >&2
  exit 1
fi

if [ ! -f /opt/ros/jazzy/setup.bash ]; then
  echo "[build_ros2_ws.sh] WARN: ROS2 Jazzy not found at /opt/ros/jazzy/setup.bash, skipping ROS2 build" >&2
  exit 0
fi

pushd "${ROS_DIR}" >/dev/null
if [ "${clean_build}" -eq 1 ]; then
  rm -rf build install log
fi

set +u
source /opt/ros/jazzy/setup.bash
set -u

if [ "${#packages[@]}" -gt 0 ]; then
  echo "[build_ros2_ws.sh] Building selected packages: ${packages[*]}"
  colcon build --symlink-install --packages-up-to "${packages[@]}"
else
  echo "[build_ros2_ws.sh] Building full ROS2 workspace"
  colcon build --symlink-install
fi

popd >/dev/null
