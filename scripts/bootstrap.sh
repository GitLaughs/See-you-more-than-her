#!/bin/bash
# bootstrap.sh — 首次克隆后的一键初始化脚本
#
# 用途：帮助新成员从零开始完成本地开发环境的完整初始化。
#       包括克隆 SmartSens 官方 SDK、加载 Docker 基础镜像、构建并启动容器。
#
# 用法（在项目根目录，Linux/WSL/Git-Bash 中执行）：
#   bash scripts/bootstrap.sh [选项]
#
# 选项：
#   --sdk-only       仅克隆 SDK，不操作 Docker
#   --docker-only    仅处理 Docker（跳过 SDK 克隆）
#   --load-image <path>   指定 a1-sdk-builder-latest.tar 文件路径
#   --skip-build     跳过 docker build（仅在已有最终镜像时使用）
#   -h, --help       显示帮助
#
# 前提条件：
#   - Git 已安装，且能访问 git.smartsenstech.ai
#   - Docker 已安装并运行
#   - 已获取 a1-sdk-builder-latest.tar（SmartSens SDK 基础镜像）
#
# 运行后，执行完整 EVB 构建：
#   docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_ROOT="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk"

normalize_sdk_line_endings() {
  [[ -d "${SDK_ROOT}" ]] || return 0

  local changed=()
  local path rel
  local roots=()

  for path in \
    "${SDK_ROOT}/scripts" \
    "${SDK_ROOT}/smart_software" \
    "${SDK_ROOT}/support/scripts" \
    "${SDK_ROOT}/support/download" \
    "${SDK_ROOT}/support/dependencies" \
    "${SDK_ROOT}/support/kconfig" \
    "${SDK_ROOT}/support/misc"; do
    [[ -d "${path}" ]] && roots+=("${path}")
  done
  (( ${#roots[@]} > 0 )) || return 0

  while IFS= read -r -d '' path; do
    grep -Iq . "${path}" || continue
    grep -q $'\r' "${path}" || continue
    sed -i 's/\r$//' "${path}"
    rel=${path#"${SDK_ROOT}/"}
    changed+=("${rel}")
  done < <(find "${roots[@]}" -type f -print0)

  if (( ${#changed[@]} > 0 )); then
    echo "[bootstrap] 规范化 ${#changed[@]} 个 SDK 文本构建控制文件为 LF"
    local limit=${#changed[@]}
    (( limit > 10 )) && limit=10
    local i
    for (( i=0; i<limit; i++ )); do
      echo "[bootstrap]   ${changed[i]}"
    done
    (( ${#changed[@]} > limit )) && echo "[bootstrap]   ..."
  fi
}

# ─── 颜色输出 ────────────────────────────────────────────────────────────────
RED='\033[0;31m'; YELLOW='\033[1;33m'; GREEN='\033[0;32m'; NC='\033[0m'
log()  { echo -e "${GREEN}[bootstrap]${NC} $*"; }
warn() { echo -e "${YELLOW}[bootstrap] ⚠${NC}  $*"; }
fail() { echo -e "${RED}[bootstrap] ✗${NC} $*" >&2; exit 1; }

# ─── 参数解析 ────────────────────────────────────────────────────────────────
SDK_ONLY=0
DOCKER_ONLY=0
LOAD_IMAGE_PATH=""
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk-only)       SDK_ONLY=1; shift ;;
    --docker-only)    DOCKER_ONLY=1; shift ;;
    --load-image)     LOAD_IMAGE_PATH="$2"; shift 2 ;;
    --skip-build)     SKIP_BUILD=1; shift ;;
    -h|--help)
      grep '^#' "$0" | head -28 | sed 's/^# \{0,2\}//'
      exit 0 ;;
    *) fail "未知选项: $1" ;;
  esac
done

echo "============================================================"
echo "  A1 Vision Robot Stack — 首次初始化"
echo "============================================================"
echo "项目根目录: ${ROOT_DIR}"
echo ""

# ════════════════════════════════════════════════════════════════
# STEP 1：克隆 SmartSens 官方 SDK
# ════════════════════════════════════════════════════════════════
SDK_TARGET="${ROOT_DIR}/data/A1_SDK_SC132GS"
SDK_MARKER="${SDK_TARGET}/smartsens_sdk/scripts/build_release_sdk.sh"
SDK_GIT_URL="https://git.smartsenstech.ai/Smartsens/A1_SDK_SC132GS.git"

if [[ ${DOCKER_ONLY} -eq 0 ]]; then
  echo "── Step 1: SmartSens 官方 SDK ──────────────────────────────"

  if [[ -f "${SDK_MARKER}" ]]; then
    log "SDK 已存在：${SDK_TARGET}"
    log "  跳过克隆（如需重新克隆，请先删除该目录）"
  else
    log "正在克隆 SDK → ${SDK_TARGET}"
    log "  来源: ${SDK_GIT_URL}"
    log "  注意: --depth 1 仅拉取最新版本，节省空间和时间（约 500MB~2GB）"
    echo ""

    mkdir -p "${ROOT_DIR}/data"

    # 如果目录存在但不完整，清理后重新克隆
    if [[ -d "${SDK_TARGET}" ]]; then
      warn "发现不完整的 SDK 目录，清理中..."
      rm -rf "${SDK_TARGET}"
    fi

    git clone --depth 1 "${SDK_GIT_URL}" "${SDK_TARGET}" \
      || fail "SDK 克隆失败。请检查：
  1. 是否能访问 git.smartsenstech.ai（VPN/内网）
  2. Git 账号权限（需要 SmartSens 开发者账号）
  3. 网络连接是否正常"

    log "✓ SDK 克隆成功"
  fi

  normalize_sdk_line_endings

  # 验证 SDK 结构
  echo ""
  log "验证 SDK 目录结构..."
  for check_path in \
    "${SDK_TARGET}/smartsens_sdk" \
    "${SDK_TARGET}/smartsens_sdk/scripts/build_release_sdk.sh" \
    "${SDK_TARGET}/smartsens_sdk/smart_software"; do
    if [[ -e "${check_path}" ]]; then
      log "  ✓ ${check_path##${ROOT_DIR}/}"
    else
      fail "SDK 结构不完整，缺少: ${check_path##${ROOT_DIR}/}
  请尝试重新克隆或联系项目负责人。"
    fi
  done
  echo ""
fi

if [[ ${SDK_ONLY} -eq 1 ]]; then
  log "✓ SDK 初始化完成（--sdk-only 模式，跳过 Docker）"
  exit 0
fi

# ════════════════════════════════════════════════════════════════
# STEP 2：Docker 环境检查
# ════════════════════════════════════════════════════════════════
echo "── Step 2: Docker 环境检查 ─────────────────────────────────"

if ! command -v docker &>/dev/null; then
  fail "Docker 未安装。请先安装 Docker Desktop（Windows/Mac）或 Docker Engine（Linux）。"
fi

if ! docker info &>/dev/null; then
  fail "Docker 未运行。请先启动 Docker Desktop 或 Docker 守护进程。"
fi

log "✓ Docker 就绪：$(docker version --format '{{.Server.Version}}' 2>/dev/null || echo '版本未知')"
echo ""

# ════════════════════════════════════════════════════════════════
# STEP 3：获取/验证 Docker 基础镜像
# ════════════════════════════════════════════════════════════════
echo "── Step 3: SmartSens SDK Docker 基础镜像 ───────────────────"

BASE_IMAGE="a1-sdk-builder:latest"

if docker image inspect "${BASE_IMAGE}" &>/dev/null; then
  log "✓ 基础镜像已存在：${BASE_IMAGE}"
else
  warn "基础镜像 ${BASE_IMAGE} 不存在"

  # 尝试从指定 tar 加载
  if [[ -n "${LOAD_IMAGE_PATH}" ]]; then
    if [[ ! -f "${LOAD_IMAGE_PATH}" ]]; then
      fail "指定的镜像文件不存在: ${LOAD_IMAGE_PATH}"
    fi
    log "从 tar 加载基础镜像: ${LOAD_IMAGE_PATH}"
    docker load -i "${LOAD_IMAGE_PATH}" \
      || fail "docker load 失败，请确认 tar 文件完整"
    log "✓ 基础镜像加载成功"

  # 尝试在常见位置自动查找 tar
  elif [[ -f "${ROOT_DIR}/a1-sdk-builder-latest.tar" ]]; then
    warn "在项目根目录发现 a1-sdk-builder-latest.tar，正在加载..."
    docker load -i "${ROOT_DIR}/a1-sdk-builder-latest.tar" \
      || fail "docker load 失败"
    log "✓ 基础镜像加载成功"

  else
    fail "缺少 SmartSens SDK Docker 基础镜像。
  
  请通过以下方式之一获取：
    A) 向项目负责人索取 a1-sdk-builder-latest.tar，然后：
       docker load -i /path/to/a1-sdk-builder-latest.tar
    
    B) 再次运行本脚本并指定路径：
       bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
  
  获取后请重新运行: bash scripts/bootstrap.sh"
  fi
fi
echo ""

# ════════════════════════════════════════════════════════════════
# STEP 4：构建最终 Docker 镜像
# ════════════════════════════════════════════════════════════════
echo "── Step 4: 构建 A1_Builder 镜像 ────────────────────────"

if [[ ${SKIP_BUILD} -eq 1 ]]; then
  log "跳过 docker build（--skip-build）"
else
  log "从 docker/Dockerfile 构建最终镜像（约 10-20 分钟，需要网络）..."
  log "此步骤会在 SmartSens SDK 基础上准备 A1 构建环境"
  echo ""
  docker build \
    -f "${ROOT_DIR}/docker/Dockerfile" \
    -t "a1-sdk-builder:latest" \
    "${ROOT_DIR}" \
    || fail "docker build 失败。
  常见原因：
  1. 网络问题（依赖包无法下载） — 检查代理或镜像源配置
  2. 磁盘空间不足（需要约 10GB）
  3. 基础镜像与 Dockerfile 不兼容"

  log "✓ 镜像构建成功"
fi
echo ""

# ════════════════════════════════════════════════════════════════
# STEP 5：启动 Docker 容器
# ════════════════════════════════════════════════════════════════
echo "── Step 5: 启动 A1_Builder 容器 ────────────────────────────"

# 清理旧容器（如果存在）
if docker ps -a --format '{{.Names}}' | grep -q '^A1_Builder$'; then
  warn "发现已有 A1_Builder 容器，先停止并删除..."
  docker rm -f A1_Builder 2>/dev/null || true
fi

log "启动容器..."
docker compose -f "${ROOT_DIR}/docker/docker-compose.yml" up -d \
  || fail "容器启动失败。请检查 docker-compose.yml 配置。"

# 等待容器就绪
sleep 2
if ! docker ps --format '{{.Names}}' | grep -q '^A1_Builder$'; then
  fail "容器启动后未检测到 A1_Builder 运行"
fi
log "✓ 容器 A1_Builder 已运行"
echo ""

# ════════════════════════════════════════════════════════════════
# STEP 6：容器内环境验证
# ════════════════════════════════════════════════════════════════
echo "── Step 6: 验证容器内环境 ──────────────────────────────────"

verify() {
  local desc="$1"; local cmd="$2"
  if docker exec A1_Builder bash -lc "${cmd}" &>/dev/null; then
    log "  ✓ ${desc}"
  else
    warn "  ✗ ${desc} — 请手动确认"
  fi
}

verify "SDK 外层目录挂载" "test -d /app/data/A1_SDK_SC132GS/smartsens_sdk"
verify "SDK 内层构建根"   "test -f /app/data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/scripts/build_release_sdk.sh"
verify "构建脚本挂载"     "test -f /app/scripts/build_complete_evb.sh"
echo ""

# ════════════════════════════════════════════════════════════════
# 完成
# ════════════════════════════════════════════════════════════════
echo "============================================================"
log "✓ 初始化完成！"
echo ""
echo "下一步操作："
echo ""
echo "  # 完整 EVB 构建："
echo "  docker exec A1_Builder bash -lc \"bash /app/scripts/build_complete_evb.sh\""
echo ""
echo "  # 只重建 app 并重新打包镜像："
echo "  docker exec A1_Builder bash -lc \"bash /app/scripts/build_complete_evb.sh --app-only\""
echo ""
echo "  # 查看构建产物："
echo "  ls output/evb/latest/"
echo ""
echo "  详细说明: docs/协作配置指南.md"
echo "============================================================"
