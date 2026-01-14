#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
CACHE_DIR=${ROOT_DIR}/cache


check_and_download() {
    local FILE_PATH=$1
    local DOWNLOAD_URL=$2
    local MD5_STANDARD=$3
    local FILE_DIR=$(dirname ${FILE_PATH})
    if [[ -f "${FILE_PATH}" && $(md5sum "${FILE_PATH}"|awk '{print $1}') == "${MD5_STANDARD}" ]]; then
        echo "${FILE_PATH} exist and md5 check ok"
    else
        mkdir -p ${FILE_DIR} && wget -c ${DOWNLOAD_URL} -P ${FILE_DIR}/
    fi
}

KERNEL_FILE="${CACHE_DIR}/linux-5.15.24.tar.gz"
KERNEL_URL="https://mirrors.tuna.tsinghua.edu.cn/kernel/v5.x/linux-5.15.24.tar.gz"
KERNEL_MD5="b17b06b0141fa35b59fa44ef19fc6daa"
check_and_download ${KERNEL_FILE} ${KERNEL_URL} ${KERNEL_MD5}

TOOLCHAIN_FILE="${CACHE_DIR}/glibc-ssp-cpp.tar.gz"
TOOLCHAIN_URL="https://gitee.com/alayi/buildroot_a1/releases/download/r1/glibc-ssp-cpp.tar.gz"
TOOLCHAIN_MD5="0cd33a84afb820bfafb77ac6d7c6aaaf"
check_and_download ${TOOLCHAIN_FILE} ${TOOLCHAIN_URL} ${TOOLCHAIN_MD5}

PACKAGE_FILE="${CACHE_DIR}/package.tar.gz"
PACKAGE_URL="https://gitee.com/alayi/buildroot_a1/releases/download/r1/package.tar.gz"
PACKAGE_MD5="7ded48bdab1fa5e9561232474df899f1"
check_and_download ${PACKAGE_FILE} ${PACKAGE_URL} ${PACKAGE_MD5}
