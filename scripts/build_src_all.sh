#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_DIR="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"
SDK_EVB_IMAGE="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"

echo "[build_src_all.sh] root=${ROOT_DIR}"
echo "[build_src_all.sh] sdk=${SDK_DIR}"
echo "[build_src_all.sh] ros=${ROS_DIR}"

if [ ! -d "${SDK_DIR}" ]; then
  echo "[build_src_all.sh] ERROR: SDK directory not found: ${SDK_DIR}" >&2
  exit 1
fi

if [ ! -x "${SDK_DIR}/scripts/a1_sc132gs_build.sh" ]; then
  echo "[build_src_all.sh] ERROR: SDK build script not executable: ${SDK_DIR}/scripts/a1_sc132gs_build.sh" >&2
  exit 1
fi

echo "[build_src_all.sh] Step 1: build SmartSens SDK and demo"
bash "${SDK_DIR}/scripts/a1_sc132gs_build.sh"

if [ -d "${ROS_DIR}" ]; then
  echo "[build_src_all.sh] Step 2: build ROS2 workspace"
  pushd "${ROS_DIR}" >/dev/null
  rm -rf build install log
  set +u
  source /opt/ros/jazzy/setup.bash
  set -u
  colcon build --symlink-install
  popd >/dev/null
else
  echo "[build_src_all.sh] WARN: ROS workspace not found: ${ROS_DIR}" >&2
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[build_src_all.sh] Step 3: collect EVB artifacts"
if [ -f "${SDK_EVB_IMAGE}" ]; then
  cp -v "${SDK_EVB_IMAGE}" "${ARTIFACT_DIR}/"
else
  echo "[build_src_all.sh] WARN: expected EVB image not found: ${SDK_EVB_IMAGE}" >&2
fi

find "${SDK_DIR}/output/images" -type f \( -name "*.evb" -o -name "*evb" -o -name "*EVB*" \) -exec cp -v {} "${ARTIFACT_DIR}/" \; || true
find "${ROS_DIR}" -type f -name "*.evb" -exec cp -v {} "${ARTIFACT_DIR}/" \; || true

echo "[build_src_all.sh] Done"
