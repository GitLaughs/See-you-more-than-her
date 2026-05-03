# SDK Reset and Demo Replacement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve current main-tree chassis controller code for future rewrite, replace dirty local SDK with upstream official contents, overlay `demo-rps` `ssne_ai_demo`, restore a non-ROS EVB build path with working `--app-only`, and make Docker use the host-mounted `data/A1_SDK_SC132GS` tree as the only SDK source.

**Architecture:** Work only in the repository root tree, not in Claude worktrees. Treat `data/A1_SDK_SC132GS` on the host as the single source of truth, bind-mount that exact tree into Docker, rebuild the EVB image through a new repo-level `scripts/build_complete_evb.sh`, and remove stale ROS-facing operational paths from wrapper scripts and entry docs.

**Tech Stack:** Bash, Python 3, Git, Docker Compose, Markdown, SmartSens SDK build scripts (`build_app.sh`, `build_release_sdk.sh`), existing `ssne_ai_demo` C++ sources.

---

## File structure

- Create: `memories/repo/chassis_controller_backup.md`
  - Generated markdown backup of the current main-tree `chassis_controller` implementation, direct call sites, build wiring, and restore notes.
- Create: `scripts/build_complete_evb.sh`
  - New repo-level non-ROS EVB build entrypoint that rebuilds `ssne_ai_demo`, repackages the SDK image, and copies the final artifact into `output/evb/`.
- Modify: `docker/docker-compose.yml`
  - Mount only `data/A1_SDK_SC132GS` into `/app/data/A1_SDK_SC132GS` so host edits sync directly into the container.
- Modify: `scripts/build_docker.sh`
  - Remove ROS flags, keep `A1_Builder` startup simple, and route builds through the new `build_complete_evb.sh`.
- Modify: `scripts/build_incremental.sh`
  - Remove `ros` mode and keep only SDK-side targets.
- Modify: `scripts/bootstrap.sh`
  - Remove ROS verification/output hints and validate the exact SDK mount + new build command.
- Modify: `README.md`
  - Update repo shape, build commands, and build-flow text to the non-ROS path.
- Modify: `CLAUDE.md`
  - Update repo guidance to match non-ROS operational reality.
- Modify: `docs/01_快速上手.md`
  - Remove `--skip-ros` and ROS entry guidance from first-run flow.
- Modify: `docs/03_编译与烧录.md`
  - Remove ROS build branch and document the new build chain.
- Modify: `docs/04_容器操作.md`
  - Fix container mount table and remove ROS container workflows from the operational doc.
- Modify: `docs/06_程序概览.md`
  - Remove ROS as an active development entrypoint.
- Modify: `docs/07_架构设计.md`
  - Update architecture text to reflect board app + SDK packaging + Windows tools only.
- Modify: `docs/11_常见问题.md`
  - Remove ROS troubleshooting and keep only active paths.
- Modify: `docs/12_项目规划.md`
  - Remove ROS future direction from current project direction.
- Modify: `docs/13_贡献指南.md`
  - Remove ROS as a recommended modification area or validation target.

### Task 1: Generate chassis controller backup

**Files:**
- Create: `memories/repo/chassis_controller_backup.md`
- Read from: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp`
- Read from: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp`
- Read from: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
- Read from: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt`
- Read from: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/README.md`

- [ ] **Step 1: Verify backup source files exist in the main tree**

```bash
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp" && \
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp" && \
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp" && \
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt" && \
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/README.md"
```

Expected: all five paths print successfully.

- [ ] **Step 2: Create the repo-local backup directory**

```bash
ls "E:/See-you-more-than-her" && mkdir -p "E:/See-you-more-than-her/memories/repo"
```

Expected: `memories/repo` exists under the repository root.

- [ ] **Step 3: Generate the markdown backup from current main-tree files**

```bash
python - <<'PY'
from pathlib import Path
import re

root = Path(r"E:/See-you-more-than-her")
demo = root / "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo"
out = root / "memories/repo/chassis_controller_backup.md"

hpp = demo / "include/chassis_controller.hpp"
cpp = demo / "src/chassis_controller.cpp"
main = demo / "demo_rps_game.cpp"
cmake = demo / "CMakeLists.txt"
readme = demo / "README.md"

main_lines = main.read_text(encoding="utf-8").splitlines()
cmake_text = cmake.read_text(encoding="utf-8")
readme_text = readme.read_text(encoding="utf-8")

patterns = [
    r"#include \"include/chassis_controller.hpp\"",
    r"ChassisController\* g_chassis = nullptr;",
    r"g_chassis->SendVelocity\(vx, 0, 0\);",
    r"g_chassis->SendVelocity\(0, 0, 0\);",
    r"ChassisController chassis;",
    r"chassis.ReadTelemetry\(chassis_state\);",
    r"chassis.SendVelocity\(vx, vy, vz\);",
    r"chassis.SendVelocity\(0, 0, 0\);",
]

used = set()
blocks = []
for pat in patterns:
    rx = re.compile(pat)
    for idx, line in enumerate(main_lines):
        if idx in used:
            continue
        if rx.search(line):
            start = max(0, idx - 3)
            end = min(len(main_lines), idx + 4)
            for i in range(start, end):
                used.add(i)
            snippet = "\n".join(f"{i+1}: {main_lines[i]}" for i in range(start, end))
            blocks.append(snippet)
            break

content = f'''# Chassis Controller Backup

## Source boundary
- Main-tree source only
- No `.claude/worktrees/**`
- No `output/**`
- No container-only copies

## Restore destination
Restore into:
`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

## File: include/chassis_controller.hpp
```cpp
{hpp.read_text(encoding="utf-8").rstrip()}
```

## File: src/chassis_controller.cpp
```cpp
{cpp.read_text(encoding="utf-8").rstrip()}
```

## Direct references from demo_rps_game.cpp
```cpp
{chr(10).join(blocks).rstrip()}
```

## Build wiring from CMakeLists.txt
```cmake
{cmake_text.rstrip()}
```

## Integration notes from README.md
```md
{readme_text.rstrip()}
```
'''

out.write_text(content, encoding="utf-8", newline="\n")
print(out)
PY
```

Expected: prints `E:/See-you-more-than-her/memories/repo/chassis_controller_backup.md`.

- [ ] **Step 4: Verify the backup file contains source blocks and reference sections**

```bash
grep -n "^## File: include/chassis_controller.hpp\|^## File: src/chassis_controller.cpp\|^## Direct references from demo_rps_game.cpp\|^## Build wiring from CMakeLists.txt\|^## Integration notes from README.md" "E:/See-you-more-than-her/memories/repo/chassis_controller_backup.md"
```

Expected: five matching section headers.

### Task 2: Remove old worktrees and replace SDK/demo sources

**Files:**
- Remove external state: `.claude/worktrees/**`
- Replace directory: `data/A1_SDK_SC132GS/smartsens_sdk/`
- Replace directory: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

- [ ] **Step 1: List current worktrees and confirm only the root tree should remain**

```bash
git worktree list --porcelain
```

Expected: one `worktree` entry for the repository root plus zero or more `.claude/worktrees/...` entries.

- [ ] **Step 2: Remove all Claude worktrees for this repository**

```bash
git worktree list --porcelain | python - <<'PY' | while read -r wt; do
import sys
from pathlib import Path
for line in sys.stdin:
    if line.startswith('worktree '):
        path = line.split(' ', 1)[1].strip()
        if '/.claude/worktrees/' in path.replace('\\', '/'):
            print(path)
PY
  git worktree remove --force "$wt"
done && git worktree prune
```

Expected: command exits cleanly; rerunning `git worktree list` shows only the root worktree.

- [ ] **Step 3: Export credentials into the shell session without writing them into repo files**

```bash
export SMARTSENS_USER='<user-supplied>'
export SMARTSENS_PASS='<password-supplied>'
export SDK_REMOTE="https://${SMARTSENS_USER}:${SMARTSENS_PASS}@git.smartsenstech.ai/Smartsens/A1_SDK_SC132GS.git"
export DEMO_REMOTE="https://${SMARTSENS_USER}:${SMARTSENS_PASS}@git.smartsenstech.ai/hhy/demo-rps.git"
```

Expected: `echo "$SDK_REMOTE" | sed 's#://.*@#://***:***@#'` prints a masked URL.

- [ ] **Step 4: Replace the local `smartsens_sdk` tree from upstream official SDK**

```bash
SDK_PARENT="E:/See-you-more-than-her/data/A1_SDK_SC132GS"
SDK_DIR="${SDK_PARENT}/smartsens_sdk"
ls "$SDK_PARENT" && rm -rf "$SDK_DIR" && git clone --depth 1 "$SDK_REMOTE" "$SDK_DIR"
```

Expected: clone completes and `test -d "$SDK_DIR/.git"` succeeds.

- [ ] **Step 5: Replace `ssne_ai_demo` from `demo-rps` with layout detection**

```bash
TMP_DIR="$(mktemp -d)"
git clone --depth 1 "$DEMO_REMOTE" "$TMP_DIR/demo-rps"
python - <<'PY'
from pathlib import Path
import shutil
import sys

tmp = Path(Path.cwd()) / Path("${TMP_DIR}")
repo = tmp / "demo-rps"
candidates = [
    repo / "ssne_ai_demo",
    repo / "smart_software/src/app_demo/face_detection/ssne_ai_demo",
]
source = next((p for p in candidates if p.is_dir()), None)
if source is None:
    raise SystemExit("demo-rps does not contain ssne_ai_demo in either expected layout")

target = Path(r"E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo")
if target.exists():
    shutil.rmtree(target)
shutil.copytree(source, target)
print(source)
print(target)
PY
rm -rf "$TMP_DIR"
```

Expected: prints both source and target directories and leaves a fresh local target tree.

- [ ] **Step 6: Verify official SDK root and overlaid demo exist where expected**

```bash
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh" && \
ls "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo" && \
grep -n "chassis_controller\|demo_rps_game" "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/README.md"
```

Expected: SDK build script exists; demo directory exists; README resolves to demo-specific content.

### Task 3: Update Docker mount and local wrapper scripts

**Files:**
- Modify: `docker/docker-compose.yml`
- Modify: `scripts/build_docker.sh`
- Modify: `scripts/build_incremental.sh`
- Modify: `scripts/bootstrap.sh`

- [ ] **Step 1: Replace `docker/docker-compose.yml` with the exact bind-mount layout**

```yaml
services:
  dev:
    image: a1-sdk-builder:latest
    container_name: A1_Builder
    command: ["/bin/bash", "-lc", "sleep infinity"]
    restart: unless-stopped
    stdin_open: true
    tty: true
    volumes:
      - ../data/A1_SDK_SC132GS:/app/data/A1_SDK_SC132GS
      - ../src:/app/src
      - ../scripts:/app/scripts
      - ../models:/app/models
      - ../output:/app/output
    working_dir: /app
    ports:
      - "8080:8080"
```

- [ ] **Step 2: Replace `scripts/build_docker.sh` with a non-ROS wrapper**

```bash
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
```

- [ ] **Step 3: Replace `scripts/build_incremental.sh` with SDK-only targets**

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
SDK_DIR="${ROOT_DIR}/data/A1_SDK_SC132GS/smartsens_sdk"

usage() {
  cat <<'EOF'
用法:
  build_incremental.sh sdk [ssne_ai_demo|m1_sdk_lib|linux|full]

示例:
  build_incremental.sh sdk ssne_ai_demo
  build_incremental.sh sdk m1_sdk_lib
  build_incremental.sh sdk linux
  build_incremental.sh sdk full
EOF
}

if [[ $# -lt 2 || "$1" != "sdk" ]]; then
  usage
  exit 1
fi

target="$2"
cd "${SDK_DIR}"

case "${target}" in
  ssne_ai_demo|demo)
    echo "[build_incremental.sh] 构建 ssne_ai_demo"
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
    echo "[build_incremental.sh] 走完整 EVB 打包路径"
    bash "${SCRIPT_DIR}/build_complete_evb.sh"
    ;;
  *)
    usage
    exit 1
    ;;
esac
```

- [ ] **Step 4: Update `scripts/bootstrap.sh` verification and next-step text**

```bash
python - <<'PY'
from pathlib import Path
path = Path(r"E:/See-you-more-than-her/scripts/bootstrap.sh")
text = path.read_text(encoding="utf-8")
text = text.replace('verify "SDK 目录挂载"   "test -d /app/data/A1_SDK_SC132GS/smartsens_sdk"\nverify "构建脚本挂载"   "test -f /app/scripts/build_complete_evb.sh"\nverify "ROS2 Jazzy"    "test -f /opt/ros/jazzy/setup.bash"\nverify "colcon 工具"   "which colcon"\nverify "buildroot_pkg" "test -f /app/src/buildroot_pkg/external.desc"\n', 'verify "SDK 目录挂载"   "test -d /app/data/A1_SDK_SC132GS/smartsens_sdk"\nverify "构建脚本挂载"   "test -f /app/scripts/build_complete_evb.sh"\nverify "SDK 发布脚本"   "test -f /app/data/A1_SDK_SC132GS/smartsens_sdk/scripts/build_release_sdk.sh"\n')
text = text.replace('echo "  # 完整 EVB 构建（含 ROS2 底盘包，约 12 分钟）："\necho "  docker exec A1_Builder bash -lc \\\"bash /app/scripts/build_complete_evb.sh\\\""\necho ""\necho "  # 快速构建（跳过 ROS2，约 8 分钟）："\necho "  docker exec A1_Builder bash -lc \\\"bash /app/scripts/build_complete_evb.sh --skip-ros\\\""\n', 'echo "  # 完整 EVB 构建："\necho "  docker exec A1_Builder bash -lc \\\"bash /app/scripts/build_complete_evb.sh\\\""\necho ""\necho "  # 只重建 app 并重新打包镜像："\necho "  docker exec A1_Builder bash -lc \\\"bash /app/scripts/build_complete_evb.sh --app-only\\\""\n')
path.write_text(text, encoding="utf-8", newline="\n")
print(path)
PY
```

- [ ] **Step 5: Verify the updated wrapper files no longer mention ROS or `--skip-ros`**

```bash
grep -n "skip-ros\|ROS2\|build_ros2_ws" "E:/See-you-more-than-her/scripts/build_docker.sh" "E:/See-you-more-than-her/scripts/build_incremental.sh" "E:/See-you-more-than-her/scripts/bootstrap.sh"
```

Expected: no matches.

### Task 4: Create the new `build_complete_evb.sh`

**Files:**
- Create: `scripts/build_complete_evb.sh`
- Reference: `data/A1_SDK_SC132GS/smartsens_sdk/scripts/build_app.sh`
- Reference: `data/A1_SDK_SC132GS/smartsens_sdk/scripts/build_release_sdk.sh`

- [ ] **Step 1: Write the full script with `default`, `--app-only`, and `--clean` modes**

```bash
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

if [[ ! -d "${SDK_ROOT}" ]]; then
  fail "缺少 SDK 目录: ${SDK_ROOT}"
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
```

- [ ] **Step 2: Save the script with LF endings and make it executable**

```bash
python - <<'PY'
from pathlib import Path
path = Path(r"E:/See-you-more-than-her/scripts/build_complete_evb.sh")
text = path.read_text(encoding="utf-8")
path.write_text(text, encoding="utf-8", newline="\n")
print(path)
PY
chmod +x "E:/See-you-more-than-her/scripts/build_complete_evb.sh"
```

Expected: file exists and `head -1` prints `#!/usr/bin/env bash`.

- [ ] **Step 3: Verify the script exposes only the three supported modes**

```bash
grep -n "app-only\|clean\|skip-ros\|ROS2" "E:/See-you-more-than-her/scripts/build_complete_evb.sh"
```

Expected: matches only `app-only` and `clean`; no `skip-ros` or `ROS2` references.

### Task 5: Rewrite active docs and repo guidance to the non-ROS flow

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`
- Modify: `docs/01_快速上手.md`
- Modify: `docs/03_编译与烧录.md`
- Modify: `docs/04_容器操作.md`
- Modify: `docs/06_程序概览.md`
- Modify: `docs/07_架构设计.md`
- Modify: `docs/11_常见问题.md`
- Modify: `docs/12_项目规划.md`
- Modify: `docs/13_贡献指南.md`

- [ ] **Step 1: Apply the exact README updates**

```bash
python - <<'PY'
from pathlib import Path
path = Path(r"E:/See-you-more-than-her/README.md")
text = path.read_text(encoding="utf-8")
text = text.replace('- ROS2 工作区：`src/ros2_ws/`\n', '')
text = text.replace('docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"\n', '')
text = text.replace('`scripts/build_complete_evb.sh` 会依次重建 Demo、可选构建 ROS2、再重新打包 SDK 镜像，因此最终可烧录产物是 `output/evb/<timestamp>/zImage.smartsens-m1-evb`（同时更新软链接 `output/evb/latest`），不是单独的 `ssne_ai_demo`。', '`scripts/build_complete_evb.sh` 会重建 `ssne_ai_demo`、重新打包 SDK 镜像，并把最终可烧录产物落到 `output/evb/<timestamp>/zImage.smartsens-m1-evb`（同时更新 `output/evb/latest/zImage.smartsens-m1-evb`），不是单独的 `ssne_ai_demo`。')
text = text.replace('- ROS2 工作区：`scripts/build_ros2_ws.sh`\n', '')
text = text.replace('- `src/ros2_ws/` 是独立工作区和后续集成路径，不等于默认板端运行栈\n- `scripts/build_ros2_ws.sh` 只扫描 `src/ros2_ws/src/` 下的包\n- 部分可选包刻意保留 `COLCON_IGNORE`\n', '')
text = text.replace('- 改 ROS2：优先做包级构建\n', '')
path.write_text(text, encoding="utf-8", newline="\n")
print(path)
PY
```

- [ ] **Step 2: Apply the exact `CLAUDE.md` operational updates**

```bash
python - <<'PY'
from pathlib import Path
path = Path(r"E:/See-you-more-than-her/CLAUDE.md")
text = path.read_text(encoding="utf-8")
text = text.replace('- **ROS2 workspace** — `src/ros2_ws/`\n  - Separate Jazzy workspace for chassis control and later ROS integration.\n  - Not same thing as default board-side runtime path.\n', '')
text = text.replace('Prefer edits in `scripts/`, `tools/`, `src/ros2_ws/`, `docs/`, and `.../ssne_ai_demo/`.', 'Prefer edits in `scripts/`, `tools/`, `docs/`, and `.../ssne_ai_demo/`.')
text = text.replace('Read `README.md`, `tools/aurora/README.md`, `src/ros2_ws/README.md`, `docs/03_编译与烧录.md`, `docs/06_程序概览.md`, and `docs/07_架构设计.md` before changing build or integration behavior.', 'Read `README.md`, `tools/aurora/README.md`, `docs/03_编译与烧录.md`, `docs/06_程序概览.md`, and `docs/07_架构设计.md` before changing build or integration behavior.')
text = text.replace('docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"\n', '')
text = text.replace('bash scripts/build_docker.sh --skip-ros\n', '')
text = text.replace('### ROS2 workspace build\n\nRequires `/opt/ros/jazzy/setup.bash`.\n\n```bash\nbash scripts/build_ros2_ws.sh --clean\nbash scripts/build_ros2_ws.sh\nbash scripts/build_ros2_ws.sh --verbose\nbash scripts/build_ros2_ws.sh --with-sdk\nbash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot\n```\n\n', '')
text = text.replace('bash scripts/build_incremental.sh ros wheeltec_multi\nbash scripts/build_incremental.sh ros --clean turn_on_wheeltec_robot\n', '')
text = text.replace('3. optionally build ROS2 workspace\n4. rerun SDK packaging so newest app goes into initramfs / `zImage`\n5. collect outputs in `output/evb/<timestamp>/`\n', '3. rerun SDK packaging so newest app goes into final `zImage`\n4. collect outputs in `output/evb/<timestamp>/`\n')
text = text.replace('### 2. Board app vs ROS2 split\n\n`ssne_ai_demo` is current board-side runtime path. `src/ros2_ws/` is separate integration path for ROS nodes and chassis work. Do not assume ROS2 packages are part of default board boot/runtime flow.\n\n`scripts/build_ros2_ws.sh` only scans `src/ros2_ws/src/`. Several heavier packages are intentionally disabled by `COLCON_IGNORE` and should stay that way unless task is specifically about enabling them:\n\n- `wheeltec_robot_kcf`\n- `wheeltec_robot_urdf`\n- `wheeltec_rviz2`\n- `aruco_ros-humble-devel`\n- `usb_cam-ros2`\n- `web_video_server-ros2`\n\n### 3. Windows tool structure\n', '### 2. Board app and SDK packaging\n\n`ssne_ai_demo` is current board-side runtime path. `scripts/build_complete_evb.sh` rebuilds the app, reruns SDK packaging, and emits the final flashable `zImage.smartsens-m1-evb`.\n\n### 3. Windows tool structure\n')
text = text.replace('- Read `README.md`, `tools/aurora/README.md`, and `src/ros2_ws/README.md` before changing build or integration behavior.\n', '- Read `README.md` and `tools/aurora/README.md` before changing build or integration behavior.\n')
path.write_text(text, encoding="utf-8", newline="\n")
print(path)
PY
```

- [ ] **Step 3: Apply the exact entry-doc updates for `docs/01`, `docs/03`, and `docs/04`**

```bash
python - <<'PY'
from pathlib import Path
updates = {
    Path(r"E:/See-you-more-than-her/docs/01_快速上手.md"): [
        ('- ROS2：`src/ros2_ws/`\n', ''),
        ('3. `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"`\n', '3. `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"`\n'),
        ('### ROS2 不是默认板端运行栈\n- `src/ros2_ws/` 是独立工作区和后续集成路径\n- `scripts/build_ros2_ws.sh` 只扫描 `src/ros2_ws/src/` 下的包\n- 一些可选包故意保留 `COLCON_IGNORE`\n\n', ''),
        ('- 改 ROS2：先看 `src/ros2_ws/`\n', ''),
        ('- 改 ROS2：优先做包级构建\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/03_编译与烧录.md"): [
        ('docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"\n', ''),
        ('`scripts/build_complete_evb.sh` 会依次重建 Demo、可选构建 ROS2、再重新打包 SDK 镜像，所以最终可烧录产物是：', '`scripts/build_complete_evb.sh` 会重建 Demo、重新打包 SDK 镜像，所以最终可烧录产物是：'),
        ('## ROS2 单独构建\n\n```bash\nbash scripts/build_ros2_ws.sh\nbash scripts/build_ros2_ws.sh --clean\nbash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot\n```\n\n说明：\n- 只扫描 `src/ros2_ws/src/` 下的包\n- 一些可选包故意保留 `COLCON_IGNORE`\n- 不要把 ROS2 工作区等同于默认板端运行栈\n\n', ''),
        ('### 改了 ROS2 包\n1. `bash scripts/build_ros2_ws.sh <package-name>`\n2. 需要把 ROS2 内容重新带进整包镜像时，再跑 `build_complete_evb.sh`\n\n', ''),
        ('### ROS2 包编不过\n先确认包在 `src/ros2_ws/src/` 下，且没有 `COLCON_IGNORE`。\n\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/04_容器操作.md"): [
        ('| `/app/smartsens_sdk` | SmartSens SDK | `./data` |\n| `/app/src` | C++ 源码 + ROS2 工作区 | `./src` |\n', '| `/app/data/A1_SDK_SC132GS` | SmartSens SDK 根目录 | `./data/A1_SDK_SC132GS` |\n| `/app/src` | 其余源码目录 | `./src` |\n'),
        ('# 完整 EVB 构建（推荐，跳过 ROS2 更快）\ndocker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros 2>&1 | tee /app/output/build.log"\n', '# 完整 EVB 构建\ndocker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh 2>&1 | tee /app/output/build.log"\n'),
        ('# step 3：编译 ssne_ai_demo（人脸检测 + 底盘控制）\ndocker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && rm -rf output/build/ssne_ai_demo && make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo"\n', '# step 3：编译 ssne_ai_demo（人脸检测 + 底盘控制）\ndocker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && rm -rf output/build/ssne_ai_demo && make BR2_EXTERNAL=./smart_software ssne_ai_demo"\n'),
        ('# 只重编 ROS2 工作区\ndocker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh ros"\n', ''),
        ('## 5. ROS2 工作区构建\n\n首次构建前安装依赖：\n\n```powershell\ndocker exec A1_Builder bash -lc "apt-get update && apt-get install -y ros-jazzy-camera-info-manager ros-jazzy-cv-bridge ros-jazzy-image-geometry ros-jazzy-image-publisher ros-jazzy-image-transport ros-jazzy-message-filters ros-jazzy-tf2-msgs ros-jazzy-tf2-sensor-msgs ros-jazzy-tf2-ros ros-jazzy-rclcpp-components ros-jazzy-class-loader ros-jazzy-vision-opencv libusb-1.0-0-dev libuvc-dev libgflags-dev libgoogle-glog-dev nlohmann-json3-dev"\n```\n\n全量构建 ROS2：\n\n```powershell\ndocker exec A1_Builder bash -lc "cd /app/src/ros2_ws && rm -rf build install log && set +u && source /opt/ros/jazzy/setup.bash && set -u && colcon build --symlink-install"\n```\n\n', ''),
        ('# ROS2 包列表\ndocker exec A1_Builder bash -lc "cd /app/src/ros2_ws && set +u && source /opt/ros/jazzy/setup.bash && source install/setup.bash && set -u && ros2 pkg list | grep -E \'base_control|ncnn|rplidar\'"\n', ''),
    ],
}
for path, repls in updates.items():
    text = path.read_text(encoding='utf-8')
    for old, new in repls:
        text = text.replace(old, new)
    path.write_text(text, encoding='utf-8', newline='\n')
    print(path)
PY
```

- [ ] **Step 4: Apply the exact operational-doc updates for `docs/06`, `docs/07`, `docs/11`, `docs/12`, and `docs/13`**

```bash
python - <<'PY'
from pathlib import Path
updates = {
    Path(r"E:/See-you-more-than-her/docs/06_程序概览.md"): [
        ('- `src/ros2_ws/`：ROS2 Jazzy 工作区\n', ''),
        ('### ROS2 工作区\n`src/ros2_ws/` 是独立工作区和后续集成路径，不等于默认板端运行栈。`scripts/build_ros2_ws.sh` 只扫描 `src/ros2_ws/src/`，部分可选包故意保留 `COLCON_IGNORE`。\n\n', ''),
        ('- 改机器人节点、colcon 构建、ROS 包启停：先看 `src/ros2_ws/`\n', ''),
        ('- ROS2 构建：`scripts/build_ros2_ws.sh`\n', ''),
        ('- `src/ros2_ws/`\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/07_架构设计.md"): [
        ('- ROS2：提供独立工作区和后续集成路径，不等于默认板端运行栈\n', ''),
        ('`scripts/build_complete_evb.sh` 会重建 Demo、可选构建 ROS2、再重新打包 SDK 镜像，所以最终部署单位是 `output/evb/<timestamp>/zImage.smartsens-m1-evb`（同时更新 `output/evb/latest` 软链接）。', '`scripts/build_complete_evb.sh` 会重建 Demo、再重新打包 SDK 镜像，所以最终部署单位是 `output/evb/<timestamp>/zImage.smartsens-m1-evb`（同时更新 `output/evb/latest/zImage.smartsens-m1-evb`）。'),
        ('### ROS2 集成层\n`src/ros2_ws/` 提供底盘控制和后续集成能力。它是独立工作区，不应被描述成默认已经跑在板端的完整导航栈。\n\n', ''),
        ('- ROS 驱动参考资料里的协议描述属于 ROS / STM32 集成背景\n- 不应把 ROS 路径里的协议值直接写成板端默认控制栈的现状\n', ''),
        ('## ROS2 工作区边界\n- `scripts/build_ros2_ws.sh` 只扫描 `src/ros2_ws/src/`\n- 一些可选包故意保留 `COLCON_IGNORE`\n- 不要为了“看起来完整”就把这些包当成默认启用\n\n', ''),
        ('- ROS2 已存在工作区，不代表它已经是默认板端运行栈\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/11_常见问题.md"): [
        ('## ROS2 包编不过\n先确认包在 `src/ros2_ws/src/` 下，且没有 `COLCON_IGNORE`。\n\n可以先做窄范围构建：\n\n```bash\nbash scripts/build_ros2_ws.sh <package-name>\n```\n\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/12_项目规划.md"): [
        ('- `src/ros2_ws/` 已存在 Jazzy 工作区和底盘相关包，但不是默认板端运行路径\n', ''),
        ('- ROS2 工作区：`src/ros2_ws/`\n', ''),
        ('- 文档、脚本、Aurora、ROS2 工作区和 `ssne_ai_demo` 属于优先维护区域\n', '- 文档、脚本、Aurora 和 `ssne_ai_demo` 属于优先维护区域\n'),
        ('### ROS2 包级联调与启用范围梳理\n先明确哪些包属于默认路径，哪些继续保持 `COLCON_IGNORE`，避免入口文档和实际工作区状态脱节。\n\n', ''),
    ],
    Path(r"E:/See-you-more-than-her/docs/13_贡献指南.md"): [
        ('- ROS2：先看 `src/ros2_ws/`\n', ''),
        ('- `src/ros2_ws/`\n', ''),
        ('- `src/ros2_ws/src/aruco_ros-humble-devel/`\n- `src/ros2_ws/src/usb_cam-ros2/`\n- `src/ros2_ws/src/web_video_server-ros2/`\n', ''),
        ('- ROS2：优先做包级构建\n', ''),
    ],
}
for path, repls in updates.items():
    text = path.read_text(encoding='utf-8')
    for old, new in repls:
        text = text.replace(old, new)
    path.write_text(text, encoding='utf-8', newline='\n')
    print(path)
PY
```

- [ ] **Step 5: Verify active docs no longer mention `--skip-ros` or `build_ros2_ws.sh`**

```bash
grep -R -n "--skip-ros\|build_ros2_ws\.sh\|src/ros2_ws" "E:/See-you-more-than-her/README.md" "E:/See-you-more-than-her/CLAUDE.md" "E:/See-you-more-than-her/docs/01_快速上手.md" "E:/See-you-more-than-her/docs/03_编译与烧录.md" "E:/See-you-more-than-her/docs/04_容器操作.md" "E:/See-you-more-than-her/docs/06_程序概览.md" "E:/See-you-more-than-her/docs/07_架构设计.md" "E:/See-you-more-than-her/docs/11_常见问题.md" "E:/See-you-more-than-her/docs/12_项目规划.md" "E:/See-you-more-than-her/docs/13_贡献指南.md"
```

Expected: no matches.

### Task 6: Verify source replacement, mount behavior, and build entrypoints

**Files:**
- Verify: `memories/repo/chassis_controller_backup.md`
- Verify: `docker/docker-compose.yml`
- Verify: `scripts/build_complete_evb.sh`
- Verify: `output/evb/`

- [ ] **Step 1: Validate the new Docker config syntax and start the container**

```bash
docker compose -f "E:/See-you-more-than-her/docker/docker-compose.yml" config && docker compose -f "E:/See-you-more-than-her/docker/docker-compose.yml" up -d
```

Expected: `config` succeeds and `A1_Builder` starts.

- [ ] **Step 2: Verify the exact SDK mount is visible inside the container**

```bash
docker exec A1_Builder bash -lc "test -d /app/data/A1_SDK_SC132GS/smartsens_sdk && ls /app/data/A1_SDK_SC132GS/smartsens_sdk/scripts"
```

Expected: exits 0 and lists the SDK script directory.

- [ ] **Step 3: Verify host-to-container sync with a temporary sentinel file**

```bash
SENTINEL="E:/See-you-more-than-her/data/A1_SDK_SC132GS/.mount-check-$(date +%s)"
: > "$SENTINEL"
docker exec A1_Builder bash -lc "test -f /app/data/A1_SDK_SC132GS/$(basename "$SENTINEL")"
rm -f "$SENTINEL"
```

Expected: `test -f` succeeds before the sentinel is removed.

- [ ] **Step 4: Run the app-only build path first**

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: either completes successfully and copies `zImage.smartsens-m1-evb` into `output/evb/latest/`, or fails with the explicit prerequisite message that no baseline SDK artifact exists yet.

- [ ] **Step 5: If app-only reports missing baseline artifact, run the full EVB build path**

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

Expected: completes and prints the timestamped artifact path under `output/evb/`.

- [ ] **Step 6: Verify the final artifact paths exist on the host**

```bash
ls "E:/See-you-more-than-her/output/evb/latest/zImage.smartsens-m1-evb" && ls -d "E:/See-you-more-than-her/output/evb/"*
```

Expected: latest artifact exists and at least one timestamped output directory exists.

## Self-review checklist

### Spec coverage
- Backup boundary and file contents: Task 1
- Worktree removal: Task 2
- Official SDK replacement: Task 2
- `demo-rps` overlay: Task 2
- Docker bind-mount source of truth: Task 3 + Task 6
- Non-ROS `build_complete_evb.sh` with `--app-only`: Task 4 + Task 6
- Remove stale ROS-facing wrappers/docs: Task 3 + Task 5

### Placeholder scan
- No `TODO`
- No `TBD`
- No “update accordingly” steps
- Every command block names exact files and exact commands

### Type and command consistency
- Backup file path is consistently `memories/repo/chassis_controller_backup.md`
- SDK path is consistently `data/A1_SDK_SC132GS/smartsens_sdk`
- Docker mount path is consistently `/app/data/A1_SDK_SC132GS`
- Build entrypoint is consistently `scripts/build_complete_evb.sh`
- Supported flags are consistently `--app-only` and `--clean`

Plan complete and saved to `docs/superpowers/plans/2026-05-02-sdk-reset-and-demo-replacement.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
