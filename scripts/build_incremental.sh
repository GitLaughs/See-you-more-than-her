#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"

mkdir -p /app
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk

usage() {
  cat <<'EOF'
Usage:
  build_incremental.sh sdk [ssne_ai_demo|ssne_vision_demo|m1_sdk_lib|linux|full]
  build_incremental.sh ros [--clean] [package ...]
  build_incremental.sh collect

Examples:
  build_incremental.sh sdk ssne_ai_demo
  build_incremental.sh sdk ssne_vision_demo
  build_incremental.sh sdk m1_sdk_lib
  build_incremental.sh ros --clean robot_navigation_ros2 ncnn_ros2
  build_incremental.sh collect
EOF
}

if [ $# -lt 1 ]; then
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
        echo "[build_incremental.sh] Building ssne_ai_demo (face detection)"
        rm -rf output/build/ssne_ai_demo/
        make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo
        ;;
      ssne_vision_demo|vision)
        echo "[build_incremental.sh] Building ssne_vision_demo (YOLOv8+OSD+lidar)"
        rm -rf output/build/ssne_vision_demo/
        make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_vision_demo
        ;;
      m1_sdk_lib|lib)
        echo "[build_incremental.sh] Rebuilding SDK library only"
        make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg m1_sdk_lib-rebuild
        ;;
      linux|kernel)
        echo "[build_incremental.sh] Rebuilding kernel with initramfs"
        make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg linux-rebuild-with-initramfs
        ;;
      full)
        echo "[build_incremental.sh] Running full SDK build"
        bash scripts/a1_sc132gs_build.sh
        ;;
      *)
        usage
        exit 1
        ;;
    esac
    ;;
  ros)
    bash "${ROOT_DIR}/scripts/build_ros2_ws.sh" "$@"
    ;;
  collect)
    bash "${ROOT_DIR}/scripts/collect_evb_artifacts.sh"
    ;;
  *)
    usage
    exit 1
    ;;
esac
