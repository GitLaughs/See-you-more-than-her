#!/usr/bin/env bash
set -euo pipefail

# 脚本: build_docker.sh
# 功能: 在 Docker 容器中执行项目构建
# 依赖: Docker 和 docker-compose
# 更新: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DOCKER_DIR="${ROOT_DIR}/docker"
COMPOSE_FILE="${DOCKER_DIR}/docker-compose.yml"

CONTAINER_NAME="a1-builder"
SKIP_SDK=0
CLEAN_BUILD=0
VERBOSE=0
BUILD_CMD="build_src_all.sh"

# 解析参数
while [ $# -gt 0 ]; do
  case "$1" in
    --skip-sdk)
      SKIP_SDK=1
      BUILD_CMD="build_ros2_ws.sh"
      shift
      ;;
    --clean)
      CLEAN_BUILD=1
      shift
      ;;
    --verbose|-v)
      VERBOSE=1
      shift
      ;;
    --ros-only)
      SKIP_SDK=1
      BUILD_CMD="build_ros2_ws.sh"
      shift
      ;;
    --help|-h)
      cat << 'EOF'
用法: build_docker.sh [选项]

选项:
  --ros-only          仅构建 ROS2（跳过 SDK）
  --skip-sdk          同 --ros-only
  --clean             清理构建目录
  --verbose, -v       启用详细输出
  --help, -h          显示帮助信息

示例:
  # 在 Docker 中全量构建
  ./build_docker.sh

  # 仅构建 ROS2
  ./build_docker.sh --ros-only

  # 清理后全量构建
  ./build_docker.sh --clean
EOF
      exit 0
      ;;
    *)
      echo "未知选项: $1" >&2
      exit 1
      ;;
  esac
done

echo "========================================="
echo "[build_docker.sh] Docker 构建脚本"
echo "========================================="
echo "项目根目录: ${ROOT_DIR}"
echo "Docker 目录: ${DOCKER_DIR}"
echo "容器名:     ${CONTAINER_NAME}"
echo "构建命令:   ${BUILD_CMD}"
echo "跳过SDK:     ${SKIP_SDK}"
echo "清理构建:   ${CLEAN_BUILD}"
echo "详细输出:   ${VERBOSE}"
echo "========================================="

# 检查前置条件
if [ ! -f "${COMPOSE_FILE}" ]; then
  echo "[build_docker.sh] 错误: docker-compose.yml 未找到" >&2
  echo "  期望路径: ${COMPOSE_FILE}" >&2
  exit 1
fi

if ! command -v docker-compose &> /dev/null; then
  echo "[build_docker.sh] 错误: docker-compose 未安装" >&2
  echo "  请先安装 Docker 和 docker-compose" >&2
  exit 1
fi

# 构建命令参数
BUILD_ARGS=""
if [ "${SKIP_SDK}" -eq 1 ]; then
  BUILD_ARGS="${BUILD_ARGS} --skip-sdk"
fi
if [ "${CLEAN_BUILD}" -eq 1 ]; then
  BUILD_ARGS="${BUILD_ARGS} --clean"
fi
if [ "${VERBOSE}" -eq 1 ]; then
  BUILD_ARGS="${BUILD_ARGS} --verbose"
fi

# 启动 Docker 环境
echo ""
echo "[build_docker.sh] Step 1: 启动 Docker 容器..."
cd "${DOCKER_DIR}"
docker-compose up -d "${CONTAINER_NAME}" || {
  echo "[build_docker.sh] 错误: Docker 容器启动失败" >&2
  exit 1
}

echo "[build_docker.sh] ✓ 容器已启动"

# 在容器内执行构建脚本
echo ""
echo "[build_docker.sh] Step 2: 在容器内执行构建..."
echo "  命令: /app/scripts/${BUILD_CMD} ${BUILD_ARGS}"
echo ""

docker-compose exec -T "${CONTAINER_NAME}" \
  bash "/app/scripts/${BUILD_CMD}" ${BUILD_ARGS}

BUILD_RESULT=$?

# 清理
echo ""
echo "[build_docker.sh] Step 3: 清理容器..."
docker-compose down

echo ""
echo "========================================="
if [ ${BUILD_RESULT} -eq 0 ]; then
  echo "[build_docker.sh] ✓ Docker 构建成功"
  echo "========================================="
  exit 0
else
  echo "[build_docker.sh] ✗ Docker 构建失败"
  echo "========================================="
  exit ${BUILD_RESULT}
fi
