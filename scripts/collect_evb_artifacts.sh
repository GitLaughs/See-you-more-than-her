#!/usr/bin/env bash
set -euo pipefail

# 脚本: collect_evb_artifacts.sh
# 功能: 收集构建产物到 output/evb/ 目录

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"
SDK_EVB_IMAGE="${SDK_DIR}/output/images/zImage.smartsens-m1-evb"
DEMO_BIN="${SDK_DIR}/output/target/app_demo/ssne_face_drive_demo"

mkdir -p "${ARTIFACT_DIR}"

echo "[收集] EVB 固件..."
if [ -f "${SDK_EVB_IMAGE}" ]; then
  cp -v "${SDK_EVB_IMAGE}" "${ARTIFACT_DIR}/"
else
  echo "[收集] ⚠ EVB 固件未找到: ${SDK_EVB_IMAGE}" >&2
fi

echo "[收集] Demo 可执行文件..."
if [ -f "${DEMO_BIN}" ]; then
  cp -v "${DEMO_BIN}" "${ARTIFACT_DIR}/"
else
  echo "[收集] ⚠ Demo 未找到: ${DEMO_BIN}" >&2
fi

echo "[收集] ✓ 产物收集完成"
