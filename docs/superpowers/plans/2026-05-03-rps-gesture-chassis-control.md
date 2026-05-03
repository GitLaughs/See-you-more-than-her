# RPS Gesture Chassis Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the RPS demo into continuous gesture-to-chassis control: rock moves forward, scissors moves backward, paper/no gesture stops, with matching tool test responses.

**Architecture:** Keep the RPS classifier and demo-rps entry point. Restore board-side `ChassisController`, add a small gesture-to-command mapping in `demo_rps_game.cpp`, remove guessing-game/countdown state, and update OSD to show current action only. Tools keep existing A1_TEST routes but label tests as the same R/P/S actions.

**Tech Stack:** C++11 board app, SmartSens SSNE/GPIO/UART APIs, Buildroot/CMake, Python Flask tools.

---

### Task 1: Restore board-side chassis controller

**Files:**
- Create: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp`
- Create: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp`
- Source reference: `memories/repo/chassis_controller_backup.md`

- [ ] Extract the two code blocks from `memories/repo/chassis_controller_backup.md` exactly into the target nested SDK paths.
- [ ] Keep public API: `Init()`, `Release()`, `SendVelocity(int16_t vx, int16_t vy = 0, int16_t vz = 0)`, `ReadTelemetry(ChassisState&)`, `is_connected()`.
- [ ] Verify `CMakeLists.txt` `file(GLOB src/*.cpp)` will include `src/chassis_controller.cpp` automatically.

### Task 2: Convert RPS demo from guessing game to control loop

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] Add includes: `#include <chrono>` if needed and `#include "include/chassis_controller.hpp"`.
- [ ] Remove game-only state: `GamePhase`, `g_game_mtx`, `g_phase`, `g_trigger_battle`, countdown/random/accum_score logic, and `'a'` start handling. Keep keyboard `q` exit.
- [ ] Add mapping:
  - `R` -> action `forward`, `vx=200`
  - `S` -> action `backward`, `vx=-200`
  - `P`, `NoTarget`, `Error`, other -> action `stop`, `vx=0`
- [ ] Initialize `ChassisController chassis; const bool chassis_ready = chassis.Init();` after visualizer init.
- [ ] Each frame: `classifier.Predict(...)`, map label, send velocity only when action changes, but always force stop when label is not valid gesture.
- [ ] On exit: send `SendVelocity(0,0,0)`, release chassis, release classifier/visualizer/processor.

### Task 3: Update OSD action display

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] Keep background bitmap on layer 2.
- [ ] Use layer 3 for current gesture/action:
  - `R`/forward -> `r.ssbmp`
  - `S`/backward -> `s.ssbmp`
  - `P`/stop -> `p.ssbmp`
  - no gesture -> `ready.ssbmp` or existing neutral asset
- [ ] Draw only when displayed action changes to reduce OSD churn.
- [ ] Remove countdown assets `1.ssbmp`, `2.ssbmp`, `3.ssbmp` from active call logic.

### Task 4: Update tools test responses

**Files:**
- Modify: `tools/A1/a1_relay.py`
- Modify: `tools/aurora/templates/companion_ui.html`
- Check: `tools/PC/pc_chassis.py` if it has user-facing test labels.

- [ ] Keep transport routes unchanged: `A1_TEST move vx vy vz`, `A1_TEST stop`.
- [ ] Update response labels/notes so:
  - `chassis_forward` says `R/rock -> forward`
  - `chassis_backward` says `S/scissors -> backward`
  - `chassis_stop` says `P/paper or NoTarget -> stop`
- [ ] Update UI button text or toast text to match mapping.

### Task 5: Verify

**Commands:**
- [ ] `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py`
- [ ] `docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"`
- [ ] If incremental build passes: `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"`

**Expected:** Python compile succeeds, `ssne_ai_demo` builds with `rps_classifier.cpp` and `chassis_controller.cpp`, full EVB packaging completes.
