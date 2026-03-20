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

mkdir -p /app
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk

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

echo "[build_src_all.sh] Step 2: build ROS2 workspace"
bash "${ROOT_DIR}/scripts/build_ros2_ws.sh" --clean

echo "[build_src_all.sh] Step 3: collect EVB artifacts"
bash "${ROOT_DIR}/scripts/collect_evb_artifacts.sh"

echo "[build_src_all.sh] Done"
