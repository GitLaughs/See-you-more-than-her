#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DOCKER_DIR="${ROOT_DIR}/docker"
COMPOSE_FILE="${DOCKER_DIR}/docker-compose.yml"
SERVICE_NAME="dev"
CONTAINER_NAME="A1_Builder"
BUILD_CMD="/app/scripts/build_complete_evb.sh"
CLEAN_BUILD=0
APP_ONLY=0

usage() {
  cat <<'EOF'
用法: build_docker.sh [选项]

选项:
  --app-only          只重建 ssne_ai_demo 并重新打包 EVB 镜像
  --clean             清理构建缓存后再执行
  --help, -h          显示帮助信息
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app-only)
      APP_ONLY=1
      shift
      ;;
    --clean)
      CLEAN_BUILD=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "未知选项: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${COMPOSE_FILE}" ]]; then
  echo "[build_docker.sh] 缺少 docker-compose.yml: ${COMPOSE_FILE}" >&2
  exit 1
fi

BUILD_ARGS=()
if [[ ${APP_ONLY} -eq 1 ]]; then
  BUILD_ARGS+=(--app-only)
fi
if [[ ${CLEAN_BUILD} -eq 1 ]]; then
  BUILD_ARGS+=(--clean)
fi

if ! docker ps --format '{{.Names}}' | grep -q '^A1_Builder$'; then
  docker compose -f "${COMPOSE_FILE}" up -d "${SERVICE_NAME}"
fi

docker exec "${CONTAINER_NAME}" bash -lc "bash ${BUILD_CMD} ${BUILD_ARGS[*]}"
