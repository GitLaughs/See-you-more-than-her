#!/usr/bin/env bash
set -euo pipefail

# Script: build_docker.sh
# Purpose: Build project inside Docker container
# Requires: Docker and docker-compose
# Updated: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DOCKER_DIR="${ROOT_DIR}/docker"
COMPOSE_FILE="${DOCKER_DIR}/docker-compose.yml"

CONTAINER_NAME="a1-builder"
SKIP_SDK=0
CLEAN_BUILD=0
VERBOSE=0
BUILD_CMD="build_src_all.sh"

# Parse arguments
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
Usage: build_docker.sh [OPTIONS]

Options:
  --ros-only          Build only ROS2 (skip SDK)
  --skip-sdk          Same as --ros-only
  --clean             Clean build directories
  --verbose, -v       Enable verbose output
  --help, -h          Show this help message

Examples:
  # Full build in Docker
  ./build_docker.sh

  # ROS2 only build in Docker
  ./build_docker.sh --ros-only

  # Clean full build
  ./build_docker.sh --clean
EOF
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

echo "========================================="
echo "[build_docker.sh] Docker Build Script"
echo "========================================="
echo "Project root: ${ROOT_DIR}"
echo "Docker dir:   ${DOCKER_DIR}"
echo "Container:    ${CONTAINER_NAME}"
echo "Build cmd:    ${BUILD_CMD}"
echo "Skip SDK:     ${SKIP_SDK}"
echo "Clean:        ${CLEAN_BUILD}"
echo "Verbose:      ${VERBOSE}"
echo "========================================="

# Check prerequisites
if [ ! -f "${COMPOSE_FILE}" ]; then
  echo "[build_docker.sh] ERROR: docker-compose.yml not found" >&2
  echo "  Expected: ${COMPOSE_FILE}" >&2
  exit 1
fi

if ! command -v docker-compose &> /dev/null; then
  echo "[build_docker.sh] ERROR: docker-compose not found in PATH" >&2
  echo "  Please install Docker and docker-compose" >&2
  exit 1
fi

# Build command with options
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

# Start Docker environment
echo ""
echo "[build_docker.sh] Step 1: Starting Docker container..."
cd "${DOCKER_DIR}"
docker-compose up -d "${CONTAINER_NAME}" || {
  echo "[build_docker.sh] ERROR: Failed to start Docker container" >&2
  exit 1
}

echo "[build_docker.sh] ✓ Container started"

# Run build script inside container
echo ""
echo "[build_docker.sh] Step 2: Running build script in container..."
echo "  Command: /app/scripts/${BUILD_CMD} ${BUILD_ARGS}"
echo ""

docker-compose exec -T "${CONTAINER_NAME}" \
  bash "/app/scripts/${BUILD_CMD}" ${BUILD_ARGS}

BUILD_RESULT=$?

# Clean up
echo ""
echo "[build_docker.sh] Step 3: Cleaning up..."
docker-compose down

echo ""
echo "========================================="
if [ ${BUILD_RESULT} -eq 0 ]; then
  echo "[build_docker.sh] ✓ Docker build SUCCESS"
  echo "========================================="
  exit 0
else
  echo "[build_docker.sh] ✗ Docker build FAILED"
  echo "========================================="
  exit ${BUILD_RESULT}
fi
