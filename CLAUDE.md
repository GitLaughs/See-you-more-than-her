# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository overview

This repo is an embedded robotics stack for the SmartSens A1 board, not a single app. The main first-party areas are:

1. **Board-side AI demo**
   - Primary area: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
   - Runs on the A1 board and owns SSNE/NPU inference, OSD overlay, A1_TEST CLI/protocol handling, and UART chassis control.

2. **SDK / firmware packaging layer**
   - Primary area: `data/A1_SDK_SC132GS/smartsens_sdk/`
   - Buildroot/vendor tree used to build the board image and package the latest app binary into the final `zImage`.

3. **ROS2 workspace**
   - Primary area: `src/ros2_ws/`
   - ROS2 Jazzy workspace for chassis control and later navigation / SLAM integration.

4. **Windows Aurora tooling**
   - Primary area: `tools/aurora/`
   - Flask + PySide6 desktop-side tooling for camera preview, A1 serial debugging, STM32 direct control, and ROS-assisted control.

5. **STM32 integration reference**
   - Primary area: `src/stm32_akm_driver/`
   - WHEELTEC chassis protocol and firmware-side integration context.

## First-party vs vendor code

Prefer first-party integration code unless the task explicitly requires vendor changes.

Good starting points:
- `scripts/`
- `tools/aurora/`
- `src/ros2_ws/`
- `docs/`
- `data/A1_SDK_SC132GS/.../ssne_ai_demo/`

Treat these areas as imported/vendor-heavy and edit carefully:
- `data/A1_SDK_SC132GS/smartsens_sdk/` outside the app/integration overlay
- `src/ros2_ws/src/aruco_ros-humble-devel/`
- `src/ros2_ws/src/usb_cam-ros2/`
- `src/ros2_ws/src/web_video_server-ros2/`
- `third_party/ultralytics/`
- `WHEELTEC_C50X_2025.12.26/`
- `output/` generated artifacts

## Common commands

Run commands from the repo root unless noted otherwise.

### Initial setup

```bash
bash scripts/bootstrap.sh
```

Load the required SmartSens base image tar during bootstrap:

```bash
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
```

### Build Docker image / start container

```bash
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
docker compose -f docker/docker-compose.yml up -d
```

### Full EVB firmware build

Run inside the `A1_Builder` container. Outputs go to `output/evb/<timestamp>/`.

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

Skip ROS2 for a faster build:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
```

Fast app-only rebuild after at least one successful full build:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Clean rebuild:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

### Docker wrapper build

```bash
bash scripts/build_docker.sh
bash scripts/build_docker.sh --skip-ros
bash scripts/build_docker.sh --clean
```

### ROS2 workspace build

Requires ROS2 Jazzy (`/opt/ros/jazzy/setup.bash`). The script only scans packages under `src/ros2_ws/src`.

```bash
bash scripts/build_ros2_ws.sh --clean
bash scripts/build_ros2_ws.sh
bash scripts/build_ros2_ws.sh --verbose
```

Build only selected packages:

```bash
bash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot
```

### Incremental development builds

SDK demo only:

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
```

SDK base libraries:

```bash
bash scripts/build_incremental.sh sdk m1_sdk_lib
```

Kernel / initramfs repack:

```bash
bash scripts/build_incremental.sh sdk linux
```

ROS-only incremental build:

```bash
bash scripts/build_incremental.sh ros wheeltec_multi
```

### Board-side runtime verification

```bash
ssh root@<A1_IP>
/app_demo/scripts/run.sh
```

### ROS2 runtime

```bash
source src/ros2_ws/install/setup.bash
ros2 run wheeltec_multi wheeltec_multi_node
```

### Windows Aurora tooling

From `tools/aurora/`:

```powershell
pip install -r requirements.txt
.\launch.ps1
```

Optional launch modes:

```powershell
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
```

The current default Aurora companion UI port is `http://127.0.0.1:5801`.

### Aurora Python tests

From the repo root:

```bash
python -m pytest tools/aurora/tests -q
python -m pytest tools/aurora/tests/test_aurora_startup.py -q
```

## Testing and verification

There is no single documented root test suite.

Verification is mainly done through:
- successful EVB builds via `scripts/build_complete_evb.sh`
- successful ROS2 package builds via `scripts/build_ros2_ws.sh`
- board-side runtime checks with `/app_demo/scripts/run.sh`
- Windows-side Aurora manual verification through `tools/aurora/launch.ps1`
- Aurora Python tests under `tools/aurora/tests/`

For ROS2-scoped changes, the narrowest documented verification is a package-targeted build:

```bash
bash scripts/build_ros2_ws.sh <package-name>
```

For the Python Aurora tooling, a cheap syntax check is:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py
```

Run the Aurora pytest suite from the repo root:

```bash
python -m pytest tools/aurora/tests -q
```

Run a single Aurora test file:

```bash
python -m pytest tools/aurora/tests/test_aurora_startup.py -q
```

The repo also contains some package-local pytest/ament-style tests, for example under `src/ros2_ws/src/wheeltec_robot_keyboard/test/`, but there is no project-level script here that standardizes running them across the whole repo.

## Architecture notes

### Build flow

The main firmware build flow is driven by `scripts/build_complete_evb.sh`:

1. optionally rebuild SDK base layers
2. rebuild `ssne_ai_demo`
3. optionally build the ROS2 workspace
4. rerun SDK packaging so the latest app is embedded into initramfs / `zImage`
5. copy artifacts into `output/evb/<timestamp>/`

Important consequence: a freshly built `ssne_ai_demo` binary is not the final deployable artifact by itself. The deployable image is the repackaged `zImage.smartsens-m1-evb`.

### App vs SDK boundary

Most day-to-day feature work belongs in the app/integration layer rather than deep inside the vendor SDK tree.

- App behavior lives primarily in `ssne_ai_demo/`
- Packaging/build orchestration lives in `scripts/` plus the SDK build scripts
- `scripts/build_incremental.sh` is for targeted iteration
- `scripts/build_complete_evb.sh` is the authoritative full-image path

### ROS2 workspace boundaries

`scripts/build_ros2_ws.sh` assumes:
- ROS2 Jazzy
- workspace root at `src/ros2_ws`
- package discovery only under `src/ros2_ws/src`
- some optional packages are intentionally disabled with `COLCON_IGNORE`

Packages explicitly treated as disabled-by-default include:
- `wheeltec_robot_kcf`
- `wheeltec_robot_urdf`
- `wheeltec_rviz2`
- `aruco_ros-humble-devel`
- `usb_cam-ros2`
- `web_video_server-ros2`

Do not remove `COLCON_IGNORE` files unless the task is specifically about enabling those packages.

### Aurora tool structure

`tools/aurora/aurora_companion.py` is the Windows entrypoint and orchestration layer. It composes several modules:
- `qt_camera_bridge.py` for camera capture via QtMultimedia / PySide6
- `serial_terminal.py` for shared A1_TEST serial terminal support
- `relay_comm.py` for PC → COM13 → A1_TEST → STM32 flows
- `chassis_comm.py` for direct STM32 serial control flows
- `ros_bridge.py` for ROS-backed control and status integration
- `templates/companion_ui.html` for the single-page web UI
- `tests/` for pytest coverage of startup, bridge lifecycle, serial terminal, ROS bridge routes, OSD config, and UI layout expectations

In practice, Aurora features are split across Flask routes, camera/serial helpers, and the template JavaScript. UI changes often require coordinated edits in multiple files.

### Hardware / communication model

At a high level:
- the A1 board captures camera frames and runs inference
- detections / control logic can drive UART communication to the STM32 chassis controller
- the PC-side Aurora tool can either talk to STM32 directly or go through the A1 debug serial path (`A1_TEST` over COM13)
- the ROS2 workspace provides an alternate integration path for chassis control and later navigation features

That means there are two important debug/control paths to keep straight:

1. **Direct STM32 mode**
   - PC serial port → STM32 UART
   - implemented mainly through `chassis_comm.py`

2. **Relay-through-A1 mode**
   - PC COM13 → A1_TEST CLI on the board → A1 UART0 → STM32 UART3
   - implemented mainly through `serial_terminal.py` + `relay_comm.py`

### Aurora camera path

Aurora preview is not just OpenCV. Depending on source selection it can use:
- Windows camera capture directly
- the Qt camera bridge (`qt_camera_bridge.py`) for A1-oriented preview flows

The Flask app serves MJPEG endpoints such as `/video_feed` and `/detect_feed`, while the frontend in `templates/companion_ui.html` drives source switching, model switching, capture, relay status, and serial/ROS panels.

## Practical guidance for future edits

- If the task is about firmware outputs, image packaging, or `zImage`, start in `scripts/` and the SDK build path.
- If the task is about A1 inference behavior, A1_TEST behavior, OSD, or UART control from the board side, start in `ssne_ai_demo/`.
- If the task is about camera preview, serial terminal behavior, or the desktop debug UI, start in `tools/aurora/`.
- If the task is about robot nodes, package wiring, or colcon failures, start in `src/ros2_ws/`.
- Prefer repo docs such as `README.md`, `tools/aurora/README.md`, `src/ros2_ws/README.md`, and `docs/` before changing build/integration behavior.
- Do not treat generated artifacts under `output/` as source of truth.
