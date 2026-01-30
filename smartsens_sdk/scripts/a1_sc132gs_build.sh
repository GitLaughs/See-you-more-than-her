#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
CACHE_DIR=${ROOT_DIR}/cache
SERVER_OUTPUT_DIR=/home/a1_output
ZIMAGE=${ROOT_DIR}/output/build/linux-custom/arch/arm/boot/zImage.smartsens-m1-evb

if [[ -f ${ZIMAGE} ]]; then
    echo "The SDK has been successfully compiled, skip first build_release_sdk.sh"
else
    echo "The SDK has not been successfully compiled, do build_release_sdk.sh first"
    bash ${SCRIPT_DIR}/build_release_sdk.sh
fi
bash ${SCRIPT_DIR}/build_app.sh
bash ${SCRIPT_DIR}/build_release_sdk.sh

if [ -d "$SERVER_OUTPUT_DIR" ]; then
    echo "copy zImage to $SERVER_OUTPUT_DIR"
    cp -f ${ROOT_DIR}/output/images/zImage.smartsens-m1-evb ${SERVER_OUTPUT_DIR}/
fi


