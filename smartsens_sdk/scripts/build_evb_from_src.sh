#!/usr/bin/env bash
set -euo pipefail

echo "[build_evb_from_src.sh] Starting full build (SDK + ROS2)"

# 1) Run SDK build script if exists
if [ -x ./smartsens_sdk/scripts/a1_sc132gs_build.sh ]; then
  echo "[build_evb_from_src.sh] Running SDK build script"
  ./smartsens_sdk/scripts/a1_sc132gs_build.sh || true
else
  echo "[build_evb_from_src.sh] SDK build script not found or not executable: smartsens_sdk/scripts/a1_sc132gs_build.sh"
fi

# 2) Build ROS2 workspace (assumes /opt/ros/jazzy is available in container)
if [ -d ./src/ros2_ws ]; then
  echo "[build_evb_from_src.sh] Building ROS2 workspace"
  source /opt/ros/jazzy/setup.bash || true
  pushd src/ros2_ws
  colcon build --event-handlers console_direct+ || true
  popd
else
  echo "[build_evb_from_src.sh] No ROS2 workspace at src/ros2_ws"
fi

# 3) Collect EVB artifacts into output/images
mkdir -p smartsens_sdk/output/images
echo "[build_evb_from_src.sh] Searching for .evb files"
found=0
while IFS= read -r -d '' file; do
  echo "[build_evb_from_src.sh] Found: $file"
  cp -v "$file" smartsens_sdk/output/images/ || true
  found=1
done < <(find . -type f -name "*.evb" -print0 || true)

if [ "$found" -eq 0 ]; then
  echo "[build_evb_from_src.sh] No .evb files found. Check SDK build output (smartsens_sdk/output/images/) or vendor docs."
else
  echo "[build_evb_from_src.sh] Copied EVB artifacts to smartsens_sdk/output/images/"
fi

echo "[build_evb_from_src.sh] Done"
