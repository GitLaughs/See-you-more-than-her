#!/bin/bash
# 中期提交「技术数据（代码类）」打包脚本
# 用法: bash scripts/create_submission_zip.sh
# 输出: 技术数据_代码类_A1视觉机器人_20260505.zip

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATE_STAMP="20260505"
ZIP_NAME="技术数据_代码类_A1视觉机器人_${DATE_STAMP}.zip"
BUILD_DIR="${ROOT}/output/submission_build"
PACK_DIR="${BUILD_DIR}/技术数据_代码类_A1视觉机器人_${DATE_STAMP}"

echo "=== 清理上次打包 ==="
rm -rf "${BUILD_DIR}"
mkdir -p "${PACK_DIR}"

copy_dir() {
  local src="$1"
  local dst="$2"
  if [ ! -d "$src" ]; then
    echo "  SKIP (not found): $src"
    return
  fi
  mkdir -p "$(dirname "$dst")"
  cp -r "$src" "$dst"
}

echo "=== 复制源代码 ==="

# --- demo-rps ---
echo "  demo-rps/"
copy_dir "${ROOT}/demo-rps" "${PACK_DIR}/demo-rps"

# --- tools/aurora ---
echo "  tools/aurora/"
copy_dir "${ROOT}/tools/aurora" "${PACK_DIR}/tools/aurora"
# 删除状态文件
rm -f "${PACK_DIR}/tools/aurora/.a1_camera_device" \
      "${PACK_DIR}/tools/aurora/.a1_camera_source" \
      "${PACK_DIR}/tools/aurora/.a1_detect_model" \
      "${PACK_DIR}/tools/aurora/.qt_bridge_owner.json"

# --- tools/A1 ---
echo "  tools/A1/"
copy_dir "${ROOT}/tools/A1" "${PACK_DIR}/tools/A1"

# --- tools/PC ---
echo "  tools/PC/"
copy_dir "${ROOT}/tools/PC" "${PACK_DIR}/tools/PC"

# --- tools/convert ---
echo "  tools/convert/"
copy_dir "${ROOT}/tools/convert" "${PACK_DIR}/tools/convert"

# --- tools/yolo ---
echo "  tools/yolo/"
copy_dir "${ROOT}/tools/yolo" "${PACK_DIR}/tools/yolo"
rm -rf "${PACK_DIR}/tools/yolo/runs"

# --- tools/video ---
echo "  tools/video/"
copy_dir "${ROOT}/tools/video" "${PACK_DIR}/tools/video"

# --- scripts ---
echo "  scripts/"
copy_dir "${ROOT}/scripts" "${PACK_DIR}/scripts"

# --- docker ---
echo "  docker/"
copy_dir "${ROOT}/docker" "${PACK_DIR}/docker"

# --- docs ---
echo "  docs/"
copy_dir "${ROOT}/docs" "${PACK_DIR}/docs"

# --- models ---
echo "  models/"
copy_dir "${ROOT}/models" "${PACK_DIR}/models"

# --- 板端应用源码 (仅 ssne_ai_demo) ---
echo "  data/A1_SDK_SC132GS/.../ssne_ai_demo/"
SDK_APP="data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo"
mkdir -p "$(dirname "${PACK_DIR}/${SDK_APP}")"
copy_dir "${ROOT}/${SDK_APP}" "${PACK_DIR}/${SDK_APP}"

# --- YOLO 训练配置 ---
echo "  data/yolov8_dataset/"
copy_dir "${ROOT}/data/yolov8_dataset" "${PACK_DIR}/data/yolov8_dataset"

# --- 根目录文件 ---
echo "  README.md .gitignore CLAUDE.md"
cp "${ROOT}/README.md" "${PACK_DIR}/"
cp "${ROOT}/.gitignore" "${PACK_DIR}/"
cp "${ROOT}/CLAUDE.md" "${PACK_DIR}/"

echo "=== 清理残留 ==="
find "${PACK_DIR}" -type d -name '__pycache__' -exec rm -rf {} + 2>/dev/null || true
find "${PACK_DIR}" -type f -name '*.pyc' -delete 2>/dev/null || true
find "${PACK_DIR}" -type d -name '.pytest_cache' -exec rm -rf {} + 2>/dev/null || true
find "${PACK_DIR}" -type f -name '.DS_Store' -delete 2>/dev/null || true

echo "=== 文件清单 ==="
FILE_COUNT=$(find "${PACK_DIR}" -type f | wc -l)
echo "${FILE_COUNT} 个文件"
du -sh "${PACK_DIR}"

echo "=== 创建压缩包 ==="
cd "${BUILD_DIR}"
zip -r "${ZIP_NAME}" "$(basename "${PACK_DIR}")"
mv "${ZIP_NAME}" "${ROOT}/output/"
cd "${ROOT}"

echo "=== 完成 ==="
ls -lh "${ROOT}/output/${ZIP_NAME}"
echo ""
echo "压缩包: output/${ZIP_NAME}"
