# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository shape

This repo is not single app. It is A1 vision robot stack with four main first-party layers:

- **Board-side AI demo** — `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
  - Runs on SmartSens A1 board.
  - Owns inference pipeline, OSD, `A1_TEST` CLI/debug path, and some UART/chassis integration.
- **SDK / firmware packaging** — `data/A1_SDK_SC132GS/smartsens_sdk/` plus root `scripts/`
  - Vendor SDK tree plus repo-level build wrappers.
  - Produces final EVB image, not only app binary.
- **ROS2 workspace** — `src/ros2_ws/`
  - Separate Jazzy workspace for chassis control and later ROS integration.
  - Not same thing as default board-side runtime path.
- **Windows host tools** — `tools/aurora/`, `tools/PC/`, `tools/A1/`
  - Aurora: camera preview, capture, COM13 terminal, manual `A1_TEST` checks.
  - PC: direct PC → STM32 serial debugging.
  - A1: COM13 → `A1_TEST` → STM32 relay control.

Prefer edits in `scripts/`, `tools/`, `src/ros2_ws/`, `docs/`, and `.../ssne_ai_demo/`. Treat rest of `smartsens_sdk/`, `third_party/ultralytics/`, and vendor ROS packages as imported/vendor-heavy code.

## Common commands

Run from repo root unless noted.

Read `README.md`, `tools/aurora/README.md`, `src/ros2_ws/README.md`, `docs/03_编译与烧录.md`, `docs/06_程序概览.md`, and `docs/07_架构设计.md` before changing build or integration behavior.

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
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

### Docker wrapper build

```bash
bash scripts/build_docker.sh
bash scripts/build_docker.sh --skip-ros
bash scripts/build_docker.sh --clean
```

### ROS2 workspace build

Requires `/opt/ros/jazzy/setup.bash`.

```bash
bash scripts/build_ros2_ws.sh --clean
bash scripts/build_ros2_ws.sh
bash scripts/build_ros2_ws.sh --verbose
bash scripts/build_ros2_ws.sh --with-sdk
bash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot
```

### Incremental builds

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
bash scripts/build_incremental.sh sdk m1_sdk_lib
bash scripts/build_incremental.sh sdk linux
bash scripts/build_incremental.sh ros wheeltec_multi
bash scripts/build_incremental.sh ros --clean turn_on_wheeltec_robot
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

Note: docs and scripts are not fully consistent on Docker container naming. `docker exec A1_Builder ...` is used by README/docs for full-image builds, while `scripts/build_docker.sh` internally uses container name `dev`.

## Build and runtime architecture
### 1. Firmware build flow

Authoritative image build is `scripts/build_complete_evb.sh`.

Flow:
1. optionally rebuild SDK base layers
2. rebuild `ssne_ai_demo`
3. optionally build ROS2 workspace
4. rerun SDK packaging so newest app goes into initramfs / `zImage`
5. collect outputs in `output/evb/<timestamp>/`

Important consequence: `ssne_ai_demo` alone is not final deployable artifact. Final flashable output is `zImage.smartsens-m1-evb`.

### 2. Board app vs ROS2 split

`ssne_ai_demo` is current board-side runtime path. `src/ros2_ws/` is separate integration path for ROS nodes and chassis work. Do not assume ROS2 packages are part of default board boot/runtime flow.

`scripts/build_ros2_ws.sh` only scans `src/ros2_ws/src/`. Several heavier packages are intentionally disabled by `COLCON_IGNORE` and should stay that way unless task is specifically about enabling them:

- `wheeltec_robot_kcf`
- `wheeltec_robot_urdf`
- `wheeltec_rviz2`
- `aruco_ros-humble-devel`
- `usb_cam-ros2`
- `web_video_server-ros2`

### 3. Windows tool structure

`tools/aurora/aurora_companion.py` is video / COM13 entrypoint. It ties together:

- `qt_camera_bridge.py` — QtMultimedia camera path
- `serial_terminal.py` — shared `A1_TEST` serial terminal
- `templates/companion_ui.html` — Aurora Web UI

`tools/PC/pc_tool.py` is direct STM32 entrypoint. It registers `pc_chassis.py` only. Default communication port is `COM17`.

`tools/A1/a1_tool.py` is COM13 relay entrypoint. It registers A1 relay and serial-terminal routes for COM13 → `A1_TEST` → STM32.

Current accepted camera init flow from docs: start `Aurora.exe` first, then let Companion take over.

### 4. Hardware/control paths

Keep these two debug/control paths separate:

- **Direct STM32 path**: PC serial port → STM32 UART
- **Relay-through-A1 path**: PC COM13 → A1 `A1_TEST` CLI → A1 UART0 → STM32 UART3

Many Windows tool bugs are really confusion between these two paths.

## Existing repo guidance worth keeping in mind

- Read `README.md`, `tools/aurora/README.md`, and `src/ros2_ws/README.md` before changing build or integration behavior.
- `docs/03_编译与烧录.md` is best reference for end-to-end build/flash flow.
- `docs/06_程序概览.md` and `docs/07_架构设计.md` are best big-picture architecture summaries.
- `docs/13_贡献指南.md` is useful when touching repo-level structure or workflow.
- Do not treat `output/` as source of truth.
- `build_complete_evb.sh --app-only` assumes SDK build cache already exists from at least one prior full build.
