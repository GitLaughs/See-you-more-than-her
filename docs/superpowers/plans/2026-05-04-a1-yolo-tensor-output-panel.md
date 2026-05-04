# A1 YOLO Tensor Output Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Print YOLOv8 model output tensor shapes and first values on frame 100, then show the captured block in Aurora for copy/paste analysis.

**Architecture:** Board code emits a compact, marker-delimited tensor dump once when `frame_index == 100`. Aurora already polls serial-terminal logs, so the UI adds a dedicated panel that scans recent log lines for the marked dump and renders a copyable text area. No new backend endpoint is needed.

**Tech Stack:** C++ board demo (`ssne_ai_demo`), SmartSens SSNE tensors, Flask/Jinja Aurora companion UI, vanilla JavaScript, pytest/py_compile verification.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
  - Add `YOLOV8::DebugPrintOutputs(uint64_t frame_index)` declaration.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
  - Add `kOutputShapes` metadata and `YOLOV8::DebugPrintOutputs` implementation.
  - Call debug print after `ssne_getoutput(model_id, 6, outputs)` only for frame 100.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Pass `frame_index` into `detector.Predict` if signature changes, or call separate debug method after prediction if feasible.
- Modify `tools/aurora/templates/companion_ui.html`
  - Add "YOLO tensor output" card with copyable textarea.
  - Parse serial-terminal lines for marker-delimited tensor dump.
- Modify/add tests under `tools/aurora/tests/`
  - Prefer template-content test for UI markers because existing tests do not run browser JS.

## Task 1: Add frame-100 tensor dump in board detector

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Inspect exact `YOLOV8::Predict` declaration**

Use Grep/Read, not shell grep:

```text
Search `void Predict` in common.hpp and yolov8_detector.cpp.
```

Expected current shape:

```cpp
void Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold);
```

- [ ] **Step 2: Change declaration to accept frame index**

In `common.hpp`, replace:

```cpp
void Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold);
```

with:

```cpp
void Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold, uint64_t frame_index);
```

If `uint64_t` is not available in this header, add:

```cpp
#include <cstdint>
```

near existing includes.

- [ ] **Step 3: Add tensor dump helper near anonymous namespace constants**

In `yolov8_detector.cpp`, after `top_class_current`, add:

```cpp
constexpr int kOutputTensorCount = 6;
constexpr int kOutputSampleCount = 5;
constexpr int kDebugOutputFrame = 100;

struct OutputShape {
    int n;
    int h;
    int w;
    int c;
};

constexpr std::array<OutputShape, kOutputTensorCount> kOutputShapes = {{
    {1, 80, 80, 4},
    {1, 40, 40, 4},
    {1, 20, 20, 4},
    {1, 80, 80, 64},
    {1, 40, 40, 64},
    {1, 20, 20, 64},
}};

void PrintOutputTensorDebug(uint64_t frame_index, ssne_tensor_t* outputs) {
    printf("[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=%llu\n", static_cast<unsigned long long>(frame_index));
    printf("Output tensor count: %d\n", kOutputTensorCount);
    for (int i = 0; i < kOutputTensorCount; ++i) {
        const auto& shape = kOutputShapes[i];
        const int tensor_size = shape.n * shape.h * shape.w * shape.c;
        const float* data = static_cast<const float*>(get_data(outputs[i]));
        printf("Output[%d] shape: [%d, %d, %d, %d]\n", i, shape.n, shape.h, shape.w, shape.c);
        printf("First 5 values: ");
        for (int j = 0; j < kOutputSampleCount && j < tensor_size; ++j) {
            printf("%f ", data ? data[j] : 0.0f);
        }
        printf("\n");
    }
    printf("[YOLOV8_TENSOR_OUTPUT_END] frame=%llu\n", static_cast<unsigned long long>(frame_index));
}
```

- [ ] **Step 4: Update `Predict` definition signature**

Replace:

```cpp
void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold) {
```

with:

```cpp
void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold, uint64_t frame_index) {
```

- [ ] **Step 5: Print only after frame-100 outputs exist**

Immediately after:

```cpp
ssne_getoutput(model_id, 6, outputs);
```

add:

```cpp
if (frame_index == kDebugOutputFrame) {
    PrintOutputTensorDebug(frame_index, outputs);
}
```

- [ ] **Step 6: Update call site**

In `demo_rps_game.cpp`, replace:

```cpp
detector.Predict(&img_sensor, &det_result, 0.4f);
```

with:

```cpp
detector.Predict(&img_sensor, &det_result, 0.4f, frame_index);
```

- [ ] **Step 7: Run syntax-oriented verification**

Run a targeted compile if A1 container is available:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: command exits `0` and rebuilds `ssne_ai_demo` into EVB app output.

If container is unavailable, run no fake compile. Record blocker and continue only with host-side checks.

## Task 2: Add Aurora copy panel for tensor output

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html`

- [ ] **Step 1: Find serial-terminal card in template**

Use Read/Grep to locate:

```text
renderSerialTermLines(lines)
loadSerialTermStatus()
serial terminal log container/card markup
```

Expected: UI already polls `/api/serial_term/status` and renders latest serial lines.

- [ ] **Step 2: Add tensor output card markup near serial terminal card**

Insert this block near serial terminal / diagnostics section:

```html
<section class="card tensor-output-card">
  <div class="card-header">
    <div>
      <h2>YOLO Tensor Output</h2>
      <p>Frame 100 tensor shapes and first 5 float values from board serial logs.</p>
    </div>
    <button class="secondary" type="button" onclick="copyTensorOutput()">Copy</button>
  </div>
  <textarea id="tensorOutputText" class="tensor-output-text" readonly placeholder="Waiting for [YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100..."></textarea>
</section>
```

If existing template uses different class names for cards/buttons, keep existing style names and preserve `id="tensorOutputText"` and `copyTensorOutput()`.

- [ ] **Step 3: Add textarea CSS if no reusable style exists**

In template style block, add:

```css
.tensor-output-text {
  width: 100%;
  min-height: 220px;
  resize: vertical;
  font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
  font-size: 12px;
  line-height: 1.45;
  white-space: pre;
}
```

If existing log textarea style exists, reuse it and only add missing height/monospace details.

- [ ] **Step 4: Add parser and renderer JavaScript**

Near serial terminal JS helpers, add:

```javascript
let latestTensorOutputText = '';

function normalizeSerialLine(line) {
  if (typeof line === 'string') return line;
  if (line && typeof line.text === 'string') return line.text;
  if (line && typeof line.message === 'string') return line.message;
  return String(line ?? '');
}

function extractTensorOutput(lines) {
  const normalized = (lines || []).map(normalizeSerialLine);
  const beginIndex = normalized.findIndex((line) => line.includes('[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100'));
  if (beginIndex < 0) return '';

  const captured = [];
  for (let i = beginIndex; i < normalized.length; i += 1) {
    captured.push(normalized[i]);
    if (normalized[i].includes('[YOLOV8_TENSOR_OUTPUT_END] frame=100')) break;
  }

  const hasEnd = captured.some((line) => line.includes('[YOLOV8_TENSOR_OUTPUT_END] frame=100'));
  return hasEnd ? captured.join('\n') : '';
}

function updateTensorOutput(lines) {
  const text = extractTensorOutput(lines);
  if (!text) return;
  latestTensorOutputText = text;
  const textarea = document.getElementById('tensorOutputText');
  if (textarea) textarea.value = text;
}

async function copyTensorOutput() {
  const textarea = document.getElementById('tensorOutputText');
  const text = textarea ? textarea.value : latestTensorOutputText;
  if (!text) return;
  await navigator.clipboard.writeText(text);
}
```

- [ ] **Step 5: Call renderer from existing serial status path**

In `loadSerialTermStatus()` or whichever function receives `d.logs` / `d.latest_lines`, after current serial line render call, add:

```javascript
updateTensorOutput(d.logs || d.latest_lines || d.lines || []);
```

If endpoint shape differs, use exact field already passed into `renderSerialTermLines(lines)`:

```javascript
updateTensorOutput(lines);
```

- [ ] **Step 6: Verify template still renders**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
```

Expected: no output and exit `0`.

## Task 3: Add host-side regression checks

**Files:**
- Create or modify: `tools/aurora/tests/test_a1_yolo_tensor_output_panel.py`

- [ ] **Step 1: Add template content test**

Create `tools/aurora/tests/test_a1_yolo_tensor_output_panel.py` with:

```python
from pathlib import Path


TEMPLATE = Path(__file__).resolve().parents[1] / "templates" / "companion_ui.html"


def test_tensor_output_panel_is_present():
    html = TEMPLATE.read_text(encoding="utf-8")

    assert "YOLO Tensor Output" in html
    assert "tensorOutputText" in html
    assert "copyTensorOutput" in html


def test_tensor_output_parser_looks_for_frame_100_markers():
    html = TEMPLATE.read_text(encoding="utf-8")

    assert "[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100" in html
    assert "[YOLOV8_TENSOR_OUTPUT_END] frame=100" in html
    assert "extractTensorOutput" in html
    assert "updateTensorOutput" in html
```

- [ ] **Step 2: Run new test**

Run:

```bash
python -m pytest tools/aurora/tests/test_a1_yolo_tensor_output_panel.py -q
```

Expected:

```text
2 passed
```

- [ ] **Step 3: Run existing Aurora tests touched by UI/backend import path**

Run:

```bash
python -m pytest tools/aurora/tests/test_qt_bridge_lifecycle.py tools/aurora/tests/test_a1_yolo_snapshot.py tools/aurora/tests/test_a1_yolo_tensor_output_panel.py -q
```

Expected: all selected tests pass.

## Task 4: Manual UI verification

**Files:**
- Runtime only; no code file expected unless bug found.

- [ ] **Step 1: Start Aurora companion**

Run preferred Windows flow from repo docs if environment supports it:

```powershell
cd tools/aurora
.\launch.ps1 -SkipAurora
```

If running from bash is required, use project-supported command that starts `tools/aurora/aurora_companion.py` with default port `6201`.

Expected: Companion reachable on configured local port.

- [ ] **Step 2: Open UI and confirm panel**

Use browser to open Companion page. Expected visible section:

```text
YOLO Tensor Output
Frame 100 tensor shapes and first 5 float values from board serial logs.
```

Textarea placeholder should read:

```text
Waiting for [YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100...
```

- [ ] **Step 3: Confirm parser with injected serial-like lines if board not connected**

In browser console, run:

```javascript
updateTensorOutput([
  '[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=100',
  'Output tensor count: 6',
  'Output[0] shape: [1, 80, 80, 4]',
  'First 5 values: 0.100000 0.200000 0.300000 0.400000 0.500000 ',
  '[YOLOV8_TENSOR_OUTPUT_END] frame=100'
]);
document.getElementById('tensorOutputText').value;
```

Expected returned value contains all five lines joined by `\n`.

- [ ] **Step 4: Confirm copy action**

Click `Copy`. Expected: clipboard contains exact textarea text. If browser blocks clipboard outside user gesture, click button manually rather than calling function from console.

## Task 5: Final verification and handoff

**Files:**
- No planned file changes.

- [ ] **Step 1: Check working tree**

Run:

```bash
git status --short
```

Expected changed files only:

```text
 M data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp
 M data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp
 M data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
 M tools/aurora/templates/companion_ui.html
?? tools/aurora/tests/test_a1_yolo_tensor_output_panel.py
```

Pre-existing unrelated changes may also appear; do not stage or modify them unless they are part of this task.

- [ ] **Step 2: Run final host checks**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
python -m pytest tools/aurora/tests/test_a1_yolo_tensor_output_panel.py -q
```

Expected: py_compile exits `0`; pytest reports `2 passed`.

- [ ] **Step 3: Run board build check when available**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: exit `0`. If unavailable, final response must say board build not verified.

- [ ] **Step 4: Summarize result**

Report:

```text
已加 frame 100 张量 dump：board stdout markers `[YOLOV8_TENSOR_OUTPUT_BEGIN/END] frame=100`。
Aurora 新增 YOLO Tensor Output 面板，可显示并复制该块。
验证：列出实际运行命令和结果；未运行项明确说明。
```

## Self-Review

- Spec coverage: frame 100 dump covered by Task 1; front-end copy panel covered by Task 2; tests and verification covered by Tasks 3-5.
- Placeholder scan: no TBD/TODO/fill-later wording remains; all code snippets and commands are explicit.
- Type consistency: `uint64_t frame_index` signature is consistent across declaration, definition, and call site. JS uses stable ids `tensorOutputText`, `extractTensorOutput`, `updateTensorOutput`, `copyTensorOutput` across tests and markup.
