#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_ROOT="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
SDK_BUILD_APP="${SDK_ROOT}/scripts/build_app.sh"
SDK_RELEASE="${SDK_ROOT}/scripts/build_release_sdk.sh"
SDK_ARTIFACT="${SDK_ROOT}/output/images/zImage.smartsens-m1-evb"
OUTPUT_ROOT="${ROOT_DIR}/output/evb"
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
RUN_APP_ONLY=0
RUN_CLEAN=0

usage() {
  cat <<'EOF'
用法: build_complete_evb.sh [选项]

选项:
  --app-only          只重建 ssne_ai_demo 并重新打包 EVB 镜像
  --clean             先清理脚本管理的构建缓存
  --help, -h          显示帮助信息
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app-only)
      RUN_APP_ONLY=1
      shift
      ;;
    --clean)
      RUN_CLEAN=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "[build_complete_evb.sh] 未知选项: $1" >&2
      exit 1
      ;;
  esac
done

fail() {
  echo "[build_complete_evb.sh] $1" >&2
  exit 1
}

normalize_sdk_line_endings() {
  local changed=()
  local candidates=()
  local path rel

  for path in \
    "${SDK_ROOT}/scripts" \
    "${SDK_ROOT}/support/scripts" \
    "${SDK_ROOT}/support/download" \
    "${SDK_ROOT}/support/dependencies" \
    "${SDK_ROOT}/support/kconfig" \
    "${SDK_ROOT}/support/gnuconfig" \
    "${SDK_ROOT}/support/misc"; do
    [[ -d "${path}" ]] && while IFS= read -r -d '' rel; do candidates+=("${rel}"); done < <(find "${path}" -type f -print0)
  done

  for path in \
    "${SDK_ROOT}/Config.in" \
    "${SDK_ROOT}/Config.in.legacy" \
    "${SDK_ROOT}/Config.in.host" \
    "${SDK_ROOT}/Makefile" \
    "${SDK_ROOT}/package/Config.in" \
    "${SDK_ROOT}/package/Config.in.host" \
    "${SDK_ROOT}/toolchain/Config.in" \
    "${SDK_ROOT}/smart_software/Config.in" \
    "${SDK_ROOT}/smart_software/external.desc" \
    "${SDK_ROOT}/smart_software/external.mk" \
    "${SDK_ROOT}/smart_software/local.mk"; do
    [[ -f "${path}" ]] && candidates+=("${path}")
  done

  for path in \
    "${SDK_ROOT}/arch" \
    "${SDK_ROOT}/board" \
    "${SDK_ROOT}/boot" \
    "${SDK_ROOT}/configs" \
    "${SDK_ROOT}/fs" \
    "${SDK_ROOT}/linux" \
    "${SDK_ROOT}/package" \
    "${SDK_ROOT}/system" \
    "${SDK_ROOT}/toolchain" \
    "${SDK_ROOT}/utils" \
    "${SDK_ROOT}/smart_software/board" \
    "${SDK_ROOT}/smart_software/configs" \
    "${SDK_ROOT}/smart_software/package" \
    "${SDK_ROOT}/smart_software/src/app_demo/face_detection/ssne_ai_demo"; do
    [[ -d "${path}" ]] && while IFS= read -r -d '' rel; do candidates+=("${rel}"); done < <(
      find "${path}" -type f \( \
        -name '*.sh' -o \
        -name '*.mk' -o \
        -name 'Makefile' -o \
        -name 'CMakeLists.txt' -o \
        -name 'Config.in' -o \
        -name 'Config.in.*' -o \
        -name 'Config.*.in' -o \
        -name '*.in' -o \
        -name '*.config' -o \
        -name '*_defconfig' -o \
        -name 'external.desc' -o \
        -name 'rcS' -o \
        -name 'rcK' \
      \) -print0
    )
  done

  [[ -d "${SDK_ROOT}/output/build" ]] && while IFS= read -r -d '' rel; do candidates+=("${rel}"); done < <(
    find "${SDK_ROOT}/output/build" -type f \( \
      -name 'config.guess' -o \
      -name 'config.sub' \
    \) -print0
  )

  for path in "${candidates[@]}"; do
    grep -Iq . "${path}" || continue
    grep -q $'\r' "${path}" || continue
    sed -i 's/\r$//' "${path}"
    rel=${path#"${SDK_ROOT}/"}
    changed+=("${rel}")
  done

  if (( ${#changed[@]} > 0 )); then
    echo "[build_complete_evb.sh] 规范化 ${#changed[@]} 个 SDK 文本构建控制文件为 LF"
    local limit=${#changed[@]}
    (( limit > 10 )) && limit=10
    local i
    for (( i=0; i<limit; i++ )); do
      echo "[build_complete_evb.sh]   ${changed[i]}"
    done
    if (( ${#changed[@]} > limit )); then
      echo "[build_complete_evb.sh]   ..."
    fi
  fi
}

if [[ ! -d "${SDK_ROOT}" ]]; then
  fail "缺少 SDK 构建根: ${SDK_ROOT}"
fi
if [[ ! -f "${SDK_BUILD_APP}" ]]; then
  fail "缺少 build_app.sh: ${SDK_BUILD_APP}"
fi
if [[ ! -f "${SDK_RELEASE}" ]]; then
  fail "缺少 build_release_sdk.sh: ${SDK_RELEASE}"
fi
if [[ -d /app && ! -d /app/data/A1_SDK_SC132GS/smartsens_sdk ]]; then
  fail "容器内 /app/data/A1_SDK_SC132GS/smartsens_sdk 不存在，说明 bind mount 未生效"
fi

normalize_sdk_line_endings

if [[ ${RUN_CLEAN} -eq 1 ]]; then
  rm -rf "${SDK_ROOT}/output/build/ssne_ai_demo" "${OUTPUT_ROOT}/latest"
fi

mkdir -p "${OUTPUT_ROOT}/${TIMESTAMP}" "${OUTPUT_ROOT}/latest"
cd "${SDK_ROOT}"

if [[ ${RUN_APP_ONLY} -eq 0 ]]; then
  echo "[build_complete_evb.sh] 先执行 SDK 发布构建，确保基础缓存存在"
  bash "${SDK_RELEASE}"
else
  if [[ ! -f "${SDK_ARTIFACT}" ]]; then
    fail "--app-only 需要已有基础 SDK 产物: ${SDK_ARTIFACT}"
  fi
fi

echo "[build_complete_evb.sh] 重建 ssne_ai_demo"
bash "${SDK_BUILD_APP}"

echo "[build_complete_evb.sh] 重新打包最终 EVB 镜像"
bash "${SDK_RELEASE}"

if [[ ! -f "${SDK_ARTIFACT}" ]]; then
  fail "构建完成后未找到产物: ${SDK_ARTIFACT}"
fi

cp -f "${SDK_ARTIFACT}" "${OUTPUT_ROOT}/${TIMESTAMP}/zImage.smartsens-m1-evb"
cp -f "${SDK_ARTIFACT}" "${OUTPUT_ROOT}/latest/zImage.smartsens-m1-evb"

echo "[build_complete_evb.sh] 产物: ${OUTPUT_ROOT}/${TIMESTAMP}/zImage.smartsens-m1-evb"
echo "[build_complete_evb.sh] 最新: ${OUTPUT_ROOT}/latest/zImage.smartsens-m1-evb"
