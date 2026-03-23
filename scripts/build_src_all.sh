#!/usr/bin/env bash
set -euo pipefail

# Script: build_src_all.sh
# Purpose: Build complete project (SDK + ROS2)
# Supports: Docker environment with SmartSens SDK
# Updated: 2025-03-24

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"
ROS_DIR="${ROOT_DIR}/src/ros2_ws"
ARTIFACT_DIR="${ROOT_DIR}/output/evb"

SKIP_SDK=0
CLEAN_BUILD=0
VERBOSE=0

# Parse arguments
while [ $# -gt 0 ]; do
  case "$1" in
    --skip-sdk)
      SKIP_SDK=1
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
    --help|-h)
      cat << 'EOF'
Usage: build_src_all.sh [OPTIONS]

Options:
  --skip-sdk          Skip SDK build, only build ROS2
  --clean             Clean build directories before building
  --verbose, -v       Enable verbose output
  --help, -h          Show this help message

Examples:
  # Full build (SDK + ROS2)
  ./build_src_all.sh

  # ROS2 only (skip SDK)
  ./build_src_all.sh --skip-sdk

  # Clean full build
  ./build_src_all.sh --clean

  # ROS2 clean build
  ./build_src_all.sh --skip-sdk --clean
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
echo "[build_src_all.sh] Project Build Script"
echo "========================================="
echo "Root directory: ${ROOT_DIR}"
echo "SDK directory:  ${SDK_DIR}"
echo "ROS directory:  ${ROS_DIR}"
echo "Skip SDK:       ${SKIP_SDK}"
echo "Clean build:    ${CLEAN_BUILD}"
echo "Verbose:        ${VERBOSE}"
echo "========================================="

# Create symlink for Docker SDK path
mkdir -p /app 2>/dev/null || true
ln -sfn "${ROOT_DIR}/data" /app/smartsens_sdk 2>/dev/null || true

# Validation
if [ "${SKIP_SDK}" -eq 0 ]; then
  if [ ! -d "${SDK_DIR}" ]; then
    echo "[build_src_all.sh] ERROR: SDK directory not found: ${SDK_DIR}" >&2
    exit 1
  fi

  if [ ! -x "${SDK_DIR}/scripts/a1_sc132gs_build.sh" ]; then
    echo "[build_src_all.sh] ERROR: SDK build script not executable" >&2
    echo "  Path: ${SDK_DIR}/scripts/a1_sc132gs_build.sh" >&2
    exit 1
  fi
fi

if [ ! -d "${ROS_DIR}" ]; then
  echo "[build_src_all.sh] ERROR: ROS workspace not found: ${ROS_DIR}" >&2
  exit 1
fi

# Step 1: Build SDK (optional)
if [ "${SKIP_SDK}" -eq 0 ]; then
  echo ""
  echo "[build_src_all.sh] Step 1: Building SmartSens SDK and demo..."
  if bash "${SDK_DIR}/scripts/a1_sc132gs_build.sh"; then
    echo "[build_src_all.sh] ✓ SDK build complete"
  else
    echo "[build_src_all.sh] ✗ SDK build failed" >&2
    exit 1
  fi
else
  echo ""
  echo "[build_src_all.sh] Step 1: Skipping SDK build"
fi

# Step 2: Build ROS2 workspace
echo ""
echo "[build_src_all.sh] Step 2: Building ROS2 workspace..."
BUILD_ROS_ARGS="--skip-sdk"
if [ "${CLEAN_BUILD}" -eq 1 ]; then
  BUILD_ROS_ARGS="${BUILD_ROS_ARGS} --clean"
fi
if [ "${VERBOSE}" -eq 1 ]; then
  BUILD_ROS_ARGS="${BUILD_ROS_ARGS} --verbose"
fi

if bash "${ROOT_DIR}/scripts/build_ros2_ws.sh" ${BUILD_ROS_ARGS}; then
  echo "[build_src_all.sh] ✓ ROS2 build complete"
else
  echo "[build_src_all.sh] ✗ ROS2 build failed" >&2
  exit 1
fi

# Step 3: Collect artifacts (optional)
if [ -x "${ROOT_DIR}/scripts/collect_evb_artifacts.sh" ]; then
  echo ""
  echo "[build_src_all.sh] Step 3: Collecting EVB artifacts..."
  if bash "${ROOT_DIR}/scripts/collect_evb_artifacts.sh"; then
    echo "[build_src_all.sh] ✓ Artifacts collected"
  else
    echo "[build_src_all.sh] ⚠ Artifact collection failed" >&2
  fi
fi

echo ""
echo "========================================="
echo "[build_src_all.sh] ✓ Build COMPLETE"
echo "========================================="

