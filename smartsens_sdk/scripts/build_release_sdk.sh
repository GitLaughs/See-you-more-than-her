#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
CACHE_DIR=${ROOT_DIR}/cache

bash ${SCRIPT_DIR}/build_dl.sh

echo ">> checking toolchain"
if [[ -d "${ROOT_DIR}/smart_software/toolchain/glibc-ssp-cpp" ]]; then
    echo "toolchain exist"
else
    mkdir -p ${ROOT_DIR}/smart_software/toolchain
    cd ${ROOT_DIR}/smart_software/toolchain/
    tar -xvf ${CACHE_DIR}/glibc-ssp-cpp.tar.gz
    cd ${ROOT_DIR}
fi
echo ">> checking toolchain done!"

echo ">> checking package"
if [[ -d "${ROOT_DIR}/package" ]]; then
    echo "package exist"
else
    cd ${ROOT_DIR}
    tar -xvf ${CACHE_DIR}/package.tar.gz
    cd ${ROOT_DIR}
fi
echo ">> checking package done!"

echo ">> checking kernel src"
if [[ -d "${ROOT_DIR}/smart_software/src/linux-5.15.24" ]]; then
    echo "linux-5.15.24 exist"
else
    cd ${ROOT_DIR}/smart_software/src
    tar -xvf ${CACHE_DIR}/linux-5.15.24.tar.gz
    cd linux-5.15.24
    patch -p1 < ${ROOT_DIR}/patch/0001-Add-support-of-A1.patch
    cd ${ROOT_DIR}
fi
echo ">> checking kernel src done!"

export LD_LIBRARY_PATH=
DIR_EXTERNAL="BR2_EXTERNAL=./smart_software"
M1MAKE="make ${DIR_EXTERNAL}"
${M1MAKE} smartsens_m1pro_release_defconfig

make -j32
