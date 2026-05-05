#!/bin/sh

set -u

echo "[APP] starting ssne_ai_demo..."
echo "[APP] cwd=$(pwd)"

if [ ! -x ./ssne_ai_demo ]; then
    chmod +x ./ssne_ai_demo || { echo "[APP] chmod ssne_ai_demo failed"; exit 1; }
fi

if [ ! -f ./app_assets/models/model_rps.m1model ]; then
    echo "[APP] missing RPS model"
    exit 1
fi

if [ ! -f ./app_assets/background.ssbmp ] || [ ! -f ./app_assets/background_colorLUT.sscl ]; then
    echo "[APP] missing OSD assets"
    exit 1
fi

./ssne_ai_demo
status=$?
echo "[APP] ssne_ai_demo exited status=${status}"
exit "${status}"
