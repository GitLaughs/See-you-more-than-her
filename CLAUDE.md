# CLAUDE.md

Guidance for Claude Code (claude.ai/code) in this repo.

## Repository shape

Repo = A1 vision robot stack, not single app. Four first-party layers:

- **Board-side AI demo** — `data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
  - Runs on SmartSens A1 board.
  - Owns inference pipeline, OSD, `A1_TEST` CLI/debug path, UART/chassis integration.
- **SDK / firmware packaging** — `data/A1_SDK_SC132GS/smartsens_sdk/` plus root `scripts/`
  - Vendor SDK tree plus repo build wrappers.
  - Produces final EVB image, not only app binary.
- **Windows host tools** — `tools/aurora/`, `tools/PC/`, `tools/A1/`
  - Aurora: camera preview, capture, COM13 terminal, manual `A1_TEST` checks.
  - PC: direct PC → STM32 serial debug.
  - A1: COM13 → `A1_TEST` → STM32 relay control.

Prefer edits in `scripts/`, `tools/`, `docs/`, and `.../ssne_ai_demo/`. Treat rest of `smartsens_sdk/`, `third_party/ultralytics/`, and vendor-heavy packages as imported/vendor-heavy code.

## Common commands

Run from repo root unless noted.

Read `README.md`, `tools/aurora/README.md`, `docs/03_编译与烧录.md`, `docs/06_程序概览.md`, and `docs/07_架构设计.md` before build/integration behavior changes.

### Bootstrap / container

```bash
bash scripts/bootstrap.sh
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
bash scripts/bootstrap.sh --sdk-only
bash scripts/bootstrap.sh --docker-only --skip-build
```

```bash
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
docker compose -f docker/docker-compose.yml up -d
```

### Full firmware / EVB build

Run inside `A1_Builder` container:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

### Docker wrapper build

```bash
bash scripts/build_docker.sh
bash scripts/build_docker.sh --clean
```

### Incremental builds

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
bash scripts/build_incremental.sh sdk m1_sdk_lib
bash scripts/build_incremental.sh sdk linux
```

### Board-side runtime check

```bash
ssh root@<A1_IP>
/app_demo/scripts/run.sh
```

### Windows host tools

Aurora video / COM13 terminal:

```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
.\launch.ps1 -Port 6201
.\launch.ps1 -ListenHost 0.0.0.0
.\launch.ps1 -Device 0
```

PC direct STM32 tool:

```powershell
cd tools/PC
.\launch.ps1
```

A1 relay tool:

```powershell
cd tools/A1
.\launch.ps1
```

Default ports: Aurora `6201`, PC `6202`, A1 `6203`.

### Windows tool verification

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py
```

Note: docs/scripts container names differ. `docker exec A1_Builder ...` used by README/docs for full-image builds; `scripts/build_docker.sh` uses service `dev`.

## Build and runtime architecture
### 1. Firmware build flow

Authoritative image build: `scripts/build_complete_evb.sh`.

Flow:
1. optionally rebuild SDK base layers
2. rebuild `ssne_ai_demo`
3. rerun SDK packaging so newest app enters final `zImage`
4. collect outputs in `output/evb/<timestamp>/`

Consequence: `ssne_ai_demo` alone is not deployable artifact. Final flashable output: `zImage.smartsens-m1-evb`.

### 2. Board app and SDK packaging

`ssne_ai_demo` is current board runtime path. Active build root: `data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/`. `scripts/build_complete_evb.sh` rebuilds app, reruns SDK packaging, emits flashable `zImage.smartsens-m1-evb`.

### 3. Windows tool structure

`tools/aurora/aurora_companion.py` = video / COM13 entrypoint. Ties together:

- `qt_camera_bridge.py` — QtMultimedia camera path
- `serial_terminal.py` — shared `A1_TEST` serial terminal
- `templates/companion_ui.html` — Aurora Web UI

`tools/PC/pc_tool.py` = direct STM32 entrypoint. Registers `pc_chassis.py` only. Default communication port: `COM17`.

`tools/A1/a1_tool.py` = COM13 relay entrypoint. Registers A1 relay and serial-terminal routes for COM13 → `A1_TEST` → STM32.

Accepted camera init flow from docs: start `Aurora.exe` first, then Companion takes over.

### 4. Hardware/control paths

Keep debug/control paths separate:

- **Direct STM32 path**: PC serial port → STM32 UART
- **Relay-through-A1 path**: PC COM13 → A1 `A1_TEST` CLI → A1 UART0 → STM32 UART3

Many Windows tool bugs = confusion between these paths.

## Existing repo guidance worth keeping in mind

- Read `README.md` and `tools/aurora/README.md` before build/integration changes.
- `docs/03_编译与烧录.md` = best end-to-end build/flash reference.
- `docs/06_程序概览.md` and `docs/07_架构设计.md` = best architecture summaries.
- `docs/13_贡献指南.md` useful for repo structure/workflow changes.
- Do not treat `output/` as source of truth.
- `build_complete_evb.sh --app-only` assumes SDK build cache exists from one prior full build.

## Tooling and workflow pitfalls

- With `Read` tool, omit `pages` for source/text files. Never pass empty `pages`; `pages` only for PDFs and empty values break reads.
- Treat Docker container edits as temporary unless copied back here. Prefer repo edits first, then build through `docker exec A1_Builder ...`; if inspecting/patching inside `/app`, mirror same change to matching repo path before commit.
- For board OSD issues, add stdout evidence around `VISUALIZER::Initialize`, `DrawBitmap`, `osd_add_texture_layer`, and `osd_flush_texture_layer` before claiming behavior. Screenshots alone cannot distinguish app-not-running, stale flashed image, OSD API failure, or Aurora preview path.
- `data/A1_SDK_SC132GS/smartsens_sdk/` = upstream repo root; actual SDK build root nested at `.../smartsens_sdk/smartsens_sdk/`.
- For `git.smartsenstech.ai` clones/fetches, disable proxy env/config for command if Git routes via `127.0.0.1` and disconnects.
- After replacing official SDK contents on Windows, normalize CRLF SDK Buildroot control files and executable scripts to LF before first container build; not only `scripts/*.sh`.
- Docker should bind-mount host `data/A1_SDK_SC132GS` to `/app/data/A1_SDK_SC132GS`; build scripts target nested SDK root under mount.
- Outer `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/` can become stale after repo replacement; active demo/build path is nested SDK root, not outer mirror.
- `scripts/build_complete_evb.sh --app-only` should fail fast when nested SDK baseline artifact missing; run full build once to seed cache.

## Board OSD interaction guidance

- RPS OSD pattern: use `.ssbmp` assets in `app_assets/`, draw via `VISUALIZER::DrawBitmap`, keep background on layer 2, transient state/animation on layers 3/4, clear transient layers on state changes.
- Product OSD states planned: person -> hello bubble; forward gesture -> car-forward animation; stop gesture -> car-stop animation; obstacle -> avoidance alert + detour animation.
- OSD asset sizing suggestion: status bubbles ~360x120, car action animations ~320x180, obstacle alerts ~480x160, detour animations ~480x270; design for 640x480 semantic input but place with board OSD absolute coordinates.
- Keep training/inference semantics separate from OSD pixels: YOLO input stays 640x480, OSD bitmap positions/sizes follow display layer coordinates.
