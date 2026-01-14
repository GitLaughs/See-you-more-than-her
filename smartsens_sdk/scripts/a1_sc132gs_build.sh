#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
CACHE_DIR=${ROOT_DIR}/cache
SERVER_OUTPUT_DIR=/home/a1_output

## 修改 app_config.json 中的图片尺寸参数
#CONFIG_FILE="${ROOT_DIR}/smart_software/src/app_demo/ped_detection/ssne_ai_demo/app_assets/app_config.json"
#if [ -f "$CONFIG_FILE" ]; then
#    # 使用 sed 命令修改 JSON 文件中的 img_height 和 img_width
#    sed -i 's/"img_height": [0-9]\+/"img_height": 1280/g' "$CONFIG_FILE"
#    sed -i 's/"img_width": [0-9]\+/"img_width": 720/g' "$CONFIG_FILE"
#    echo "已更新 app_config.json 中的图片尺寸参数"
#else
#    echo "警告: 配置文件 $CONFIG_FILE 不存在"
#fi


bash ${SCRIPT_DIR}/build_release_sdk.sh
bash ${SCRIPT_DIR}/build_app.sh
bash ${SCRIPT_DIR}/build_release_sdk.sh

if [ -d "$SERVER_OUTPUT_DIR" ]; then
    echo "copy zImage to $SERVER_OUTPUT_DIR"
    cp -f ${ROOT_DIR}/output/images/zImage.smartsens-m1-evb ${SERVER_OUTPUT_DIR}/
fi


