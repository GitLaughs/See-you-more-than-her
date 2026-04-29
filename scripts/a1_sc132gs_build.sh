#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"

if [[ ! -d "${SDK_DIR}" ]]; then
  echo "[a1_sc132gs_build] SDK directory not found: ${SDK_DIR}" >&2
  exit 1
fi

cd "${SDK_DIR}"
exec bash scripts/a1_sc132gs_build.sh "$@"
