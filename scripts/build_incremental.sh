#!/bin/bash
# build_incremental.sh — 增量构建脚本（开发迭代用）
#
# 参考: data/A1_SDK_SC132GS/smartsens_sdk/scripts/build_app.sh
#
# 说明: 本脚本只做单步增量编译，供开发迭代使用。
#       生产发布请使用 build_complete_evb.sh（每次均生成带时间戳的完整 EVB 固件）。
#
# 用法:
#   build_incremental.sh sdk [ssne_face_drive_demo|m1_sdk_lib|linux|full]
#   build_incremental.sh ros [--clean] [--verbose] [package ...]
#
# 示例:
#   build_incremental.sh sdk ssne_face_drive_demo
#   build_incremental.sh sdk m1_sdk_lib
#   build_incremental.sh ros turn_on_wheeltec_robot wheeltec_multi
#   build_incremental.sh ros --clean

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"

mkdir -p /app 2>/dev/null || true
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk 2>/dev/null || true

usage() {
  grep '^#' "$0" | head -20 | sed 's/^# \{0,2\}//'
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

mode="$1"
shift

case "${mode}" in
  sdk)
    target="${1:-ssne_ai_demo}"
    cd "${SDK_DIR}"
    case "${target}" in
      ssne_ai_demo|demo)
        echo "[build_incremental.sh] 构建 ssne_ai_demo（人脸检测 + OSD标签 + 底盘控制）"
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
        echo "[build_incremental.sh] 完整 SDK 构建"
        bash "${SDK_DIR}/scripts/a1_sc132gs_build.sh"
        ;;
      *)
        usage
        exit 1
        ;;
    esac
    ;;
  ros)
    # 传递所有剩余参数给 build_ros2_ws.sh（支持 --clean/--verbose/包名）
    bash "${SCRIPT_DIR}/build_ros2_ws.sh" "$@"
    ;;
  *)
    usage
    exit 1
    ;;
esac
