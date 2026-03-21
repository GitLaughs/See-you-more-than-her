#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_DIR="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"
SDK_EVB_IMAGE="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"

mkdir -p "${ARTIFACT_DIR}"

echo "[collect_evb_artifacts.sh] Collecting SDK EVB image"
if [ -f "${SDK_EVB_IMAGE}" ]; then
  cp -v "${SDK_EVB_IMAGE}" "${ARTIFACT_DIR}/"
else
  echo "[collect_evb_artifacts.sh] WARN: expected EVB image not found: ${SDK_EVB_IMAGE}" >&2
fi

if [ -d "${SDK_DIR}/output/images" ]; then
  find "${SDK_DIR}/output/images" -type f \( -name "*.evb" -o -name "*evb" -o -name "*EVB*" \) -exec cp -v {} "${ARTIFACT_DIR}/" \; || true
fi

if [ -d "${ROS_DIR}" ]; then
  find "${ROS_DIR}" -type f -name "*.evb" -exec cp -v {} "${ARTIFACT_DIR}/" \; || true
fi

echo "[collect_evb_artifacts.sh] Done"
