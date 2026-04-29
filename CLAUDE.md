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
- **Windows host tools** — `tools/aurora/`
  - Aurora: camera preview, serial debugging, STM32 control, ROS bridge.

Prefer edits in `scripts/`, `tools/`, `src/ros2_ws/`, `docs/`, and `.../ssne_ai_demo/`. Treat rest of `smartsens_sdk/`, `third_party/ultralytics/`, and vendor ROS packages as imported/vendor-heavy code.

## Common commands

Run from repo root unless noted.

### Bootstrap / container

```bash
bash scripts/bootstrap.sh
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
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

### Aurora host tool

From `tools/aurora/`:

```powershell
pip install -r requirements.txt
.\launch.ps1
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
.\launch.ps1 -Port 5802
.\launch.ps1 -ListenHost 0.0.0.0
```

### Aurora verification

```bash
python -m pytest tools/aurora/tests -q
python -m pytest tools/aurora/tests/test_aurora_startup.py -q
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py tools/aurora/chassis_comm.py tools/aurora/ros_bridge.py
```

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

### 3. Aurora structure

`tools/aurora/aurora_companion.py` is Windows orchestration entrypoint. It ties together:

- `qt_camera_bridge.py` — QtMultimedia camera path
- `serial_terminal.py` — shared `A1_TEST` serial terminal
- `relay_comm.py` — PC → A1_TEST → STM32 relay path
- `chassis_comm.py` — direct PC → STM32 serial control
- `ros_bridge.py` — ROS environment detection and bridge routes
- `templates/companion_ui.html` — single-page UI

Aurora changes often span Flask routes, helper modules, and template JavaScript together.

Current accepted camera init flow from docs: start `Aurora.exe` first, then let Companion take over.

### 4. Hardware/control paths

Keep these two debug/control paths separate:

- **Direct STM32 path**: PC serial port → STM32 UART
- **Relay-through-A1 path**: PC COM13 → A1 `A1_TEST` CLI → A1 UART0 → STM32 UART3

Many Aurora bugs are really confusion between these two paths.

## Existing repo guidance worth keeping in mind

- Read `README.md`, `tools/aurora/README.md`, and `src/ros2_ws/README.md` before changing build or integration behavior.
- Do not treat `output/` as source of truth.
- `build_complete_evb.sh --app-only` assumes SDK build cache already exists from at least one prior full build.
