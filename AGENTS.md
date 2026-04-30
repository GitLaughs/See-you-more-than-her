# Repository Guidelines

## Project Structure & Module Organization
This repository is a multi-layer A1 vision robot stack, not a single app. Make changes in the layer you actually mean to affect:

- `scripts/`: repo-level build, bootstrap, and packaging wrappers.
- `tools/aurora/`: Windows Aurora companion tool, Flask/PySide6 UI, and Python tests.
- `src/ros2_ws/`: ROS 2 Jazzy workspace and packages under `src/ros2_ws/src/`.
- `docs/`: onboarding, build, architecture, and integration docs.
- `data/A1_SDK_SC132GS/.../ssne_ai_demo/`: board-side AI demo and runtime path.

Treat `third_party/`, `WHEELTEC_C50X_2025.12.26/`, most of `data/.../smartsens_sdk/`, and `output/` as vendor-heavy or generated content.

## Build, Test, and Development Commands
- `bash scripts/bootstrap.sh`: prepare the base environment.
- `docker build -f docker/Dockerfile -t a1-sdk-builder:latest .`: build the SDK container.
- `docker compose -f docker/docker-compose.yml up -d`: start the builder container.
- `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"`: build the full flashable EVB image.
- `bash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot`: package-level ROS 2 build.
- `python -m pytest tools/aurora/tests -q`: run Aurora tests.
- `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py tools/aurora/chassis_comm.py tools/aurora/ros_bridge.py`: fast Python syntax check.

## Coding Style & Naming Conventions
Use 4-space indentation in Python. Keep shell scripts POSIX/Bash-friendly and prefer descriptive long flags such as `--clean` and `--verbose`. Preserve existing ROS package names, launch file naming, and vendor directory layout. Match local naming patterns like `test_*.py`, `*.launch.py`, and commit scopes tied to one subsystem.

## Testing Guidelines
Aurora changes should include `pytest` coverage in `tools/aurora/tests/` when behavior changes. ROS 2 changes should get at least a package-level `build_ros2_ws.sh` run for affected packages. Board-side or image-packaging changes should be verified with `build_incremental.sh` or `build_complete_evb.sh`. Doc-only edits still need link, path, script-name, and port checks.

## Commit & Pull Request Guidelines
Recent history uses Conventional Commit prefixes: `feat:`, `docs:`, `chore:`. Keep each commit and PR focused on one layer or one problem. PRs should state the impacted path, hardware/runtime assumptions, and exact verification commands run. Include screenshots for Aurora UI changes, and avoid mixing unrelated vendor or generated-file diffs.

## Security & Configuration Tips
Do not commit `output/`, local virtualenvs, or machine-specific artifacts. If you change serial defaults, ports, or host URLs such as `COM13` or `http://127.0.0.1:5801`, update the matching docs and launch scripts in the same PR.
