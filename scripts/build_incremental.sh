#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk"

usage() {
  cat <<'EOF'
用法:
  build_incremental.sh sdk [ssne_ai_demo|m1_sdk_lib|linux|full]

示例:
  build_incremental.sh sdk ssne_ai_demo
  build_incremental.sh sdk m1_sdk_lib
  build_incremental.sh sdk linux
  build_incremental.sh sdk full
EOF
}

if [[ $# -lt 2 || "$1" != "sdk" ]]; then
  usage
  exit 1
fi

target="$2"
cd "${SDK_DIR}"

case "${target}" in
  ssne_ai_demo|demo)
    echo "[build_incremental.sh] 构建 ssne_ai_demo"
    rm -rf output/build/ssne_ai_demo/
    make BR2_EXTERNAL=./smart_software ssne_ai_demo
    ;;
  m1_sdk_lib|lib)
    echo "[build_incremental.sh] 重新构建 SDK 基础库"
    make BR2_EXTERNAL=./smart_software m1_sdk_lib-rebuild
    ;;
  linux|kernel)
    echo "[build_incremental.sh] 重新构建内核 (with initramfs)"
    make BR2_EXTERNAL=./smart_software linux-rebuild-with-initramfs
    ;;
  full)
    echo "[build_incremental.sh] 走完整 EVB 打包路径"
    bash "${SCRIPT_DIR}/build_complete_evb.sh"
    ;;
  *)
    usage
    exit 1
    ;;
esac
