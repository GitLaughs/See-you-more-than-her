#!/bin/sh

set -u

echo "[APP] starting ssne_ai_demo..."
echo "[APP] cwd=$(pwd)"

if [ ! -x ./ssne_ai_demo ]; then
    chmod +x ./ssne_ai_demo || { echo "[APP] chmod ssne_ai_demo failed"; exit 1; }
fi

MODEL_PATH="./app_assets/models/test.m1model"
if [ ! -f "${MODEL_PATH}" ]; then
    echo "[APP] missing A1 5-class model: expected ./app_assets/models/test.m1model"
    exit 1
fi

echo "[APP] model=${MODEL_PATH}"

if [ ! -f ./app_assets/background.ssbmp ] || [ ! -f ./app_assets/background_colorLUT.sscl ]; then
    echo "[APP] missing OSD assets"
    exit 1
fi

./ssne_ai_demo
status=$?
echo "[APP] ssne_ai_demo exited status=${status}"
exit "${status}"
