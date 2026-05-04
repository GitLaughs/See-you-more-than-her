# A1 YOLO Snapshot Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove obsolete 1920×1280 OSD/background behavior, make board YOLO detection non-OSD, and make Aurora's A1 snapshot button trigger tensor dump plus image save.

**Architecture:** Board `ssne_ai_demo` keeps continuous YOLO detection and latest snapshot state, but stops initializing or drawing OSD. `A1_TEST yolo_snapshot` becomes the only board-side trigger for tensor dump; it returns the latest detection JSON and emits marker-delimited tensor output at click time. Aurora keeps the A1 snapshot button, sends that command, saves the current preview image with overlay, and displays the returned tensor dump.

**Tech Stack:** C++ SmartSens board demo (`ssne_ai_demo`), SSNE tensors, Aurora Flask companion, vanilla JS template, Python tests, Docker A1 build wrapper.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
  - Add optional tensor dump trigger state to `YOLOV8::Predict` signature or add `YOLOV8::RequestTensorDump()` / `ConsumeTensorDump()` style method.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
  - Remove frame-100 dump and print tensor output only when requested by A1 command.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Remove OSD/1920 constants and `VISUALIZER` usage.
  - Keep `A1_TEST ping`, `A1_TEST yolo_snapshot`, and front-end chassis commands.
  - Disable unsupported/debug commands not used by front-end buttons.
- Modify `tools/aurora/serial_terminal.py`
  - Remove obsolete command mappings for `osd_status`, `uart_status`, `debug_status`, `debug_frame`, `debug_last` if no longer in UI.
  - Keep `ping`, `chassis_forward`, `chassis_stop`, `yolo_snapshot`.
- Modify `tools/aurora/aurora_companion.py`
  - Ensure `/api/a1/yolo_snapshot` saves current image on button click and extracts tensor dump from recent serial output.
- Modify `tools/aurora/templates/companion_ui.html`
  - Remove OSD/debug buttons.
  - Update tensor output placeholder to click-triggered wording.
- Modify tests under `tools/aurora/tests/`
  - Update snapshot tests to expect tensor dump extraction and current image save.

## Task 1: Remove board OSD/background path

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Remove OSD constants and scaling helper**

Delete these definitions:

```cpp
constexpr int kOsdWidth = 1920;
constexpr int kOsdHeight = 1280;
```

Delete this helper:

```cpp
std::array<float, 4> scale_box_to_osd(const std::array<float, 4>& box) {
    constexpr float kXScale = static_cast<float>(kOsdWidth) / static_cast<float>(kCameraWidth);
    constexpr float kYScale = static_cast<float>(kOsdHeight) / static_cast<float>(kCameraHeight);
    return {box[0] * kXScale, box[1] * kYScale, box[2] * kXScale, box[3] * kYScale};
}
```

- [ ] **Step 2: Remove OSD initialization and release**

Delete:

```cpp
std::array<int, 2> osd_shape = {kOsdWidth, kOsdHeight};
```

Delete the `VISUALIZER visualizer` block:

```cpp
VISUALIZER visualizer;
const bool osd_ready = visualizer.Initialize(osd_shape, "background_colorLUT.sscl");
if (!osd_ready) {
    std::cout << "[YOLOV8_OSD] init failed, detection continues without OSD" << std::endl;
} else {
    std::cout << "[YOLOV8_OSD] init ok, background bitmap disabled" << std::endl;
}
```

Delete release block:

```cpp
if (osd_ready) {
    visualizer.Release();
}
```

- [ ] **Step 3: Remove OSD draw loop**

Delete the whole block inside main loop:

```cpp
if (osd_ready) {
    std::vector<std::array<float, 4>> osd_boxes;
    osd_boxes.reserve(det_result.boxes.size());
    for (const auto& box : det_result.boxes) {
        if (is_valid_box(box)) {
            osd_boxes.emplace_back(scale_box_to_osd(box));
        }
    }
    if (boxes_changed(osd_boxes, last_osd_boxes)) {
        visualizer.Draw(osd_boxes);
        last_osd_boxes = osd_boxes;
    }
}
```

Also delete:

```cpp
std::vector<std::array<float, 4>> last_osd_boxes;
bool boxes_changed(...)
```

- [ ] **Step 4: Verify no board OSD references remain in main app**

Run content search:

```text
Grep path `.../ssne_ai_demo/demo_rps_game.cpp` for `kOsd|VISUALIZER|scale_box_to_osd|osd_ready|Draw\(`.
```

Expected: no matches in `demo_rps_game.cpp` except includes or unrelated comments. `utils.cpp`/`osd-device.cpp` may remain compiled but unused.

## Task 2: Make tensor dump click-triggered

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Add request flag to `DetectionResult`**

In `common.hpp`, add to `DetectionResult`:

```cpp
bool tensor_dump_printed = false;
```

And reset it in `Clear()`:

```cpp
tensor_dump_printed = false;
```

- [ ] **Step 2: Change `Predict` signature**

Replace declaration:

```cpp
void Predict(ssne_tensor_t* img_in, DetectionResult* result,
             float conf_threshold = 0.4f, uint64_t frame_index = 0);
```

with:

```cpp
void Predict(ssne_tensor_t* img_in, DetectionResult* result,
             float conf_threshold = 0.4f, uint64_t frame_index = 0,
             bool print_tensor_dump = false);
```

- [ ] **Step 3: Remove frame-100 constant and condition**

In `yolov8_detector.cpp`, delete:

```cpp
constexpr int kDebugOutputFrame = 100;
```

Replace:

```cpp
if (frame_index == kDebugOutputFrame) {
    PrintOutputTensorDebug(frame_index, outputs);
}
```

with:

```cpp
if (print_tensor_dump) {
    PrintOutputTensorDebug(frame_index, outputs);
    result->tensor_dump_printed = true;
}
```

- [ ] **Step 4: Update `Predict` definition signature**

Replace:

```cpp
void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold, uint64_t frame_index) {
```

with:

```cpp
void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold, uint64_t frame_index, bool print_tensor_dump) {
```

- [ ] **Step 5: Add global tensor dump request flag**

In `demo_rps_game.cpp` globals, add:

```cpp
volatile sig_atomic_t g_yolo_tensor_dump_requested = 0;
```

In `handle_a1_test_command`, inside `command == "yolo_snapshot"`, set flag before returning response:

```cpp
g_yolo_tensor_dump_requested = 1;
```

- [ ] **Step 6: Pass flag into next prediction**

Before `detector.Predict(...)` in main loop, add:

```cpp
const bool print_tensor_dump = g_yolo_tensor_dump_requested != 0;
g_yolo_tensor_dump_requested = 0;
```

Replace predict call with:

```cpp
detector.Predict(&img_sensor, &det_result, 0.4f, frame_index, print_tensor_dump);
```

- [ ] **Step 7: Include tensor flag in snapshot JSON**

In `build_yolo_snapshot_json()`, add field:

```cpp
<< ",\"tensor_dump_printed\":" << (det.tensor_dump_printed ? "true" : "false")
```

Place it near `error_code`.

## Task 3: Disable unused board A1_TEST commands

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Keep only front-end command handlers**

In `handle_a1_test_command`, keep:

```cpp
if (command == "ping") { ... }
if (command == "yolo_snapshot") { ... }
if (command == "chassis_test") { ... }
if (command == "move") { ... }
```

Delete handlers for:

```cpp
debug_status
uart_status
osd_status
```

Unknown commands should still return unsupported command via existing final `print_debug_response(...)`.

- [ ] **Step 2: Update startup text**

Replace:

```cpp
std::cout << "Input 'q' to exit or A1_TEST commands..." << std::endl;
```

with:

```cpp
std::cout << "Input 'q' to exit or A1_TEST ping/yolo_snapshot/chassis_test/move commands..." << std::endl;
```

## Task 4: Simplify Aurora command surface

**Files:**
- Modify: `tools/aurora/serial_terminal.py`
- Modify: `tools/aurora/templates/companion_ui.html`

- [ ] **Step 1: Reduce `_A1_DEBUG_COMMANDS` mapping**

In `serial_terminal.py`, make `_A1_DEBUG_COMMANDS` exactly:

```python
_A1_DEBUG_COMMANDS = {
    "ping": "ping",
    "chassis_stop": "chassis_test stop",
    "chassis_forward": "chassis_test forward",
    "yolo_snapshot": "yolo_snapshot",
}
```

Make `_A1_DEBUG_DESCRIPTIONS` exactly:

```python
_A1_DEBUG_DESCRIPTIONS = {
    "chassis_forward": "forward gesture -> forward",
    "chassis_stop": "stop/person/obstacle -> stop",
    "yolo_snapshot": "latest board YOLO boxes + tensor dump -> overlay on PC preview",
}
```

- [ ] **Step 2: Remove unused UI buttons**

In `companion_ui.html`, remove buttons:

```html
<button class="btn" onclick="sendA1Debug('osd_status')">OSD状态</button>
<button class="btn" onclick="sendA1Debug('uart_status')">串口状态</button>
<button class="btn" onclick="sendSerialTestCommand('debug_status')">debug_status</button>
<button class="btn" onclick="sendSerialTestCommand('debug_frame')">debug_frame</button>
<button class="btn" onclick="sendSerialTestCommand('debug_last')">debug_last</button>
```

Keep:

```html
<button class="btn blue" onclick="sendA1Debug('ping')">连通测试</button>
<button class="btn green" onclick="sendA1Debug('chassis_forward')">前进</button>
<button class="btn red" onclick="sendA1Debug('chassis_stop')">停止</button>
<button class="btn" onclick="sendSerialQuick('help')">help</button>
<button class="btn" onclick="sendSerialQuick('status')">status</button>
<button class="btn blue" onclick="sendSerialTestCommand('test_echo')">test_echo</button>
```

- [ ] **Step 3: Update tensor textarea placeholder**

Replace:

```html
Waiting for [YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100...
```

with:

```html
Click A1 拍照检测 to request [YOLOV8_TENSOR_OUTPUT_BEGIN] from board...
```

## Task 5: Fix Aurora A1 snapshot flow and tensor capture

**Files:**
- Modify: `tools/aurora/aurora_companion.py`
- Modify: `tools/aurora/templates/companion_ui.html`
- Test: `tools/aurora/tests/test_a1_yolo_snapshot.py`

- [ ] **Step 1: Ensure `/api/a1/yolo_snapshot` saves current preview image**

In `a1_yolo_snapshot()`, keep flow:

```python
frame = _snapshot_current_preview_frame()
serial_result = serial_terminal.send_text_line(
    "A1_TEST yolo_snapshot",
    wait_tokens=["A1_DEBUG", '"command":"yolo_snapshot"'],
    timeout_sec=4.0,
)
snapshot, error, recent_rx, raw_line = _extract_yolo_snapshot_payload(serial_result)
display = _draw_a1_yolo_snapshot_overlay(frame, snapshot)
image_info = _save_a1_yolo_snapshot_image(display, snapshot)
```

If current code captures frame after serial command, move `_snapshot_current_preview_frame()` before `send_text_line(...)` so saved image corresponds to click time.

- [ ] **Step 2: Extract tensor dump from serial recent lines**

Add helper in `aurora_companion.py`:

```python
def _extract_tensor_dump_lines(serial_result: Dict[str, Any]) -> str:
    lines = []
    for entry in serial_result.get("recent_rx") or []:
        if isinstance(entry, dict):
            lines.append(str(entry.get("text") or entry.get("message") or ""))
        else:
            lines.append(str(entry))
    raw_line = str(serial_result.get("raw_line") or "")
    if raw_line:
        lines.append(raw_line)

    begin = -1
    for idx, line in enumerate(lines):
        if "[YOLOV8_TENSOR_OUTPUT_BEGIN]" in line:
            begin = idx
            break
    if begin < 0:
        return ""

    captured = []
    for line in lines[begin:]:
        captured.append(line)
        if "[YOLOV8_TENSOR_OUTPUT_END]" in line:
            break
    if not any("[YOLOV8_TENSOR_OUTPUT_END]" in line for line in captured):
        return ""
    return "\n".join(captured)
```

- [ ] **Step 3: Return tensor dump to UI**

In successful response from `a1_yolo_snapshot()`, include:

```python
"tensor_dump": _extract_tensor_dump_lines(serial_result),
```

- [ ] **Step 4: Update UI to fill tensor textarea after click**

In `takeA1YoloSnapshot()` success handler, after raw text assignment, add:

```javascript
if(d.tensor_dump){
  const tensor=document.getElementById('tensorOutputText');
  if(tensor)tensor.value=d.tensor_dump;
}
```

- [ ] **Step 5: Update tests**

In `tools/aurora/tests/test_a1_yolo_snapshot.py`, add or update test fixture so `serial_result` includes recent tensor lines:

```python
"recent_rx": [
    {"text": "[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=42"},
    {"text": "Output tensor count: 6"},
    {"text": "Output[0] shape: [1, 80, 80, 4]"},
    {"text": "First 5 values: 0.1 0.2 0.3 0.4 0.5 "},
    {"text": "[YOLOV8_TENSOR_OUTPUT_END] frame=42"},
]
```

Assert response JSON:

```python
assert data["tensor_dump"].startswith("[YOLOV8_TENSOR_OUTPUT_BEGIN]")
assert "Output tensor count: 6" in data["tensor_dump"]
```

## Task 6: Verification

**Files:**
- No planned code changes unless verification exposes compile/runtime bug.

- [ ] **Step 1: Host Python compile**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
```

Expected: exit `0`, no output.

- [ ] **Step 2: Host tests if pytest exists**

Run:

```bash
python -m pytest tools/aurora/tests/test_a1_yolo_snapshot.py tools/aurora/tests/test_a1_yolo_tensor_output_panel.py -q
```

Expected: pass. If environment has no pytest, run fallback direct Python assertions for template markers and report pytest unavailable.

- [ ] **Step 3: Board build**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: exit `0`; final line contains `zImage.smartsens-m1-evb` path.

- [ ] **Step 4: Runtime manual check**

After flashing board and launching Aurora:

1. Open Aurora Companion.
2. Connect COM13 terminal.
3. Click `A1 拍照检测`.
4. Expected:
   - board emits `[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=<current>` only after click.
   - Aurora saves current preview image as `a1_yolo_snapshot_*.jpg`.
   - UI shows overlay result and tensor dump textarea.
   - no OSD boxes drawn on board preview.

## Self-Review

- Spec coverage: removes 1920/1280 OSD path; disables unused board/UI debug commands; click-triggered tensor dump; snapshot image save; YOLO detection continues without OSD.
- Placeholder scan: no TBD/TODO placeholders; all command mappings and code snippets explicit.
- Type consistency: board `Predict(..., bool print_tensor_dump)` matches header/definition/call; Python response field `tensor_dump` matches UI update and tests.
