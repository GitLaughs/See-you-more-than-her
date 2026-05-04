# A1 Heuristic Depth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Replace synthetic A1 depth frames with YOLO detection-box heuristic depth maps and object summaries for Aurora visualization.

**Architecture:** Board-side code converts existing YOLO boxes into an 80×60 uint8 depth map and emits it through the existing `A1_DEPTH_*` protocol. Aurora serial parser keeps existing base64 frame parsing and additionally tracks optional `A1_DEPTH_OBJECT` lines so `/api/depth/latest` can return target depth metadata for frontend display. Frontend keeps canvas rendering and adds compact target-depth chips.

**Tech Stack:** C++ board demo (`ssne_ai_demo`), Python Flask/Aurora serial terminal, vanilla HTML/CSS/JS frontend.

---

## Files

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Replace synthetic gradient generation with detection-box heuristic depth map.
  - Emit `A1_DEPTH_OBJECT` lines after each depth frame.
- Modify `tools/aurora/serial_terminal.py`
  - Parse `A1_DEPTH_OBJECT` lines and attach objects to latest depth frame.
- Modify `tools/aurora/templates/companion_ui.html`
  - Render depth object chips below depth canvas.
- Verify with Python syntax checks and extracted frontend JS syntax check.
- Skip board build unless user requests; previous instruction said not to compile.

## Task 1: Board-side heuristic depth map

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [x] **Step 1: Add heuristic depth helpers near existing depth emitter**

Add helpers in the anonymous namespace:

```cpp
const char* depth_bucket_name(float depth_score) {
    if (depth_score >= 0.55f) return "near";
    if (depth_score >= 0.30f) return "mid";
    return "far";
}

uint8_t depth_value_for_score(float depth_score) {
    if (depth_score >= 0.55f) return 240;
    if (depth_score >= 0.30f) return 160;
    return 80;
}

float compute_box_depth_score(const std::array<float, 4>& box) {
    const float x1 = std::max(0.0f, std::min(static_cast<float>(kImageWidth), box[0]));
    const float y1 = std::max(0.0f, std::min(static_cast<float>(kImageHeight), box[1]));
    const float x2 = std::max(0.0f, std::min(static_cast<float>(kImageWidth), box[2]));
    const float y2 = std::max(0.0f, std::min(static_cast<float>(kImageHeight), box[3]));
    const float width = std::max(0.0f, x2 - x1);
    const float height = std::max(0.0f, y2 - y1);
    const float area_ratio = (width * height) / static_cast<float>(kImageWidth * kImageHeight);
    const float bottom_ratio = y2 / static_cast<float>(kImageHeight);
    return 0.65f * std::sqrt(area_ratio) + 0.35f * bottom_ratio;
}
```

Required include additions:

```cpp
#include <algorithm>
#include <cmath>
```

- [x] **Step 2: Replace synthetic depth function signature**

Replace:

```cpp
void emit_synthetic_depth_frame(uint64_t frame_index)
```

with:

```cpp
void emit_heuristic_depth_frame(uint64_t frame_index, const DetectionResult& det_result)
```

- [x] **Step 3: Generate depth map from YOLO boxes**

Inside `emit_heuristic_depth_frame`, initialize background and rasterize boxes:

```cpp
constexpr int kDepthWidth = 80;
constexpr int kDepthHeight = 60;
constexpr size_t kDepthBytes = kDepthWidth * kDepthHeight;
constexpr size_t kMaxChunkChars = 1600;

std::vector<uint8_t> depth(kDepthBytes, 20);
for (size_t i = 0; i < det_result.boxes.size(); ++i) {
    const auto& box = det_result.boxes[i];
    const float depth_score = compute_box_depth_score(box);
    const uint8_t value = depth_value_for_score(depth_score);
    const int x1 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[0] * kDepthWidth / kImageWidth)));
    const int y1 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[1] * kDepthHeight / kImageHeight)));
    const int x2 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[2] * kDepthWidth / kImageWidth)));
    const int y2 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[3] * kDepthHeight / kImageHeight)));
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            uint8_t& pixel = depth[y * kDepthWidth + x];
            if (value > pixel) pixel = value;
        }
    }
}
```

- [x] **Step 4: Keep existing A1_DEPTH frame output**

After map generation, keep existing base64 chunk output unchanged:

```cpp
const std::string encoded = base64_encode(depth.data(), depth.size());
const size_t chunks = (encoded.size() + kMaxChunkChars - 1) / kMaxChunkChars;
printf("A1_DEPTH_BEGIN frame=%llu w=%d h=%d fmt=u8 encoding=base64 chunks=%zu bytes=%zu\n",
       static_cast<unsigned long long>(frame_index), kDepthWidth, kDepthHeight, chunks, kDepthBytes);
for (size_t i = 0; i < chunks; ++i) {
    const size_t offset = i * kMaxChunkChars;
    printf("A1_DEPTH_CHUNK frame=%llu index=%zu data=%s\n",
           static_cast<unsigned long long>(frame_index), i, encoded.substr(offset, kMaxChunkChars).c_str());
}
printf("A1_DEPTH_END frame=%llu\n", static_cast<unsigned long long>(frame_index));
```

- [x] **Step 5: Emit object summary lines**

After `A1_DEPTH_END`, emit one line per detection:

```cpp
for (size_t i = 0; i < det_result.boxes.size(); ++i) {
    const int cls = i < det_result.class_ids.size() ? det_result.class_ids[i] : -1;
    const float score = i < det_result.scores.size() ? det_result.scores[i] : 0.0f;
    const float depth_score = compute_box_depth_score(det_result.boxes[i]);
    const char* bucket = depth_bucket_name(depth_score);
    auto it = kClassNames.find(cls);
    const char* name = (it != kClassNames.end()) ? it->second.c_str() : "unknown";
    printf("A1_DEPTH_OBJECT frame=%llu cls=%s score=%.3f bucket=%s depth=%.3f box=%.1f,%.1f,%.1f,%.1f\n",
           static_cast<unsigned long long>(frame_index), name, score, bucket, depth_score,
           det_result.boxes[i][0], det_result.boxes[i][1], det_result.boxes[i][2], det_result.boxes[i][3]);
}
fflush(stdout);
```

- [x] **Step 6: Update call site**

Replace:

```cpp
emit_synthetic_depth_frame(frame_index);
```

with:

```cpp
emit_heuristic_depth_frame(frame_index, det_result);
```

## Task 2: Aurora backend object parsing

**Files:**
- Modify: `tools/aurora/serial_terminal.py`

- [x] **Step 1: Add depth object state**

Near `_latest_depth_frame`, add:

```python
_depth_objects: Dict[int, list] = {}
_DEPTH_MAX_OBJECTS = 32
```

- [x] **Step 2: Parse object lines**

At start of `_handle_depth_line`, support `A1_DEPTH_OBJECT`:

```python
if text.startswith("A1_DEPTH_OBJECT"):
    with _depth_lock:
        if frame < 0:
            return
        items = _depth_objects.setdefault(frame, [])
        if len(items) >= _DEPTH_MAX_OBJECTS:
            return
        box_text = fields.get("box", "")
        try:
            box = [float(v) for v in box_text.split(",")]
        except ValueError:
            box = []
        if len(box) != 4:
            box = []
        items.append({
            "class": fields.get("cls", "unknown"),
            "score": float(fields.get("score", 0.0)),
            "bucket": fields.get("bucket", "far"),
            "depth": float(fields.get("depth", 0.0)),
            "box": box,
        })
    return
```

- [x] **Step 3: Attach objects to latest frame**

When publishing `_latest_depth_frame`, include:

```python
"objects": list(_depth_objects.pop(frame, [])),
```

Also prune stale `_depth_objects` frames:

```python
for old_frame in list(_depth_objects.keys()):
    if old_frame < frame - 3:
        _depth_objects.pop(old_frame, None)
```

- [x] **Step 4: Ensure object lines after END still attach**

Because board emits object lines after `A1_DEPTH_END`, update object parsing: if `_latest_depth_frame` exists with same frame, append into `_latest_depth_frame["objects"]` directly:

```python
if _latest_depth_frame and _latest_depth_frame.get("frame") == frame:
    objects = _latest_depth_frame.setdefault("objects", [])
    if len(objects) < _DEPTH_MAX_OBJECTS:
        objects.append(item)
    return
```

## Task 3: Aurora frontend object display

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html`

- [x] **Step 1: Add object target container**

Inside depth card body after `depthMeta`, add:

```html
<div class="rx-chips" id="depthObjects"></div>
```

- [x] **Step 2: Add object chip rendering helper**

Add JS helper:

```js
function renderDepthObjects(objects=[]){
  const el=document.getElementById('depthObjects');
  if(!el)return;
  if(!Array.isArray(objects)||!objects.length){el.innerHTML='<span class="rx-chip">无目标深度</span>';return}
  el.innerHTML=objects.slice(0,8).map(o=>{
    const bucket=o.bucket||'far';
    const mark=bucket==='near'?'●':(bucket==='mid'?'◆':'·');
    const cls=o.class||'unknown';
    const depth=Number(o.depth||0).toFixed(2);
    const score=Number(o.score||0).toFixed(2);
    return `<span class="rx-chip">${mark} ${escapeHtml(cls)} ${escapeHtml(bucket)} d=${depth} s=${score}</span>`;
  }).join('')
}
```

- [x] **Step 3: Update depth loader**

In `loadDepthLatest()`, when no frame:

```js
renderDepthObjects([]);
```

After successful canvas render:

```js
renderDepthObjects(d.objects||[]);
```

## Task 4: Verification

**Files:**
- Verify all modified files.

- [x] **Step 1: Python syntax check**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py
```

Expected: exit 0, no output.

- [x] **Step 2: Extracted frontend JS syntax check**

Run:

```bash
python - <<'PY'
from pathlib import Path
text=Path('tools/aurora/templates/companion_ui.html').read_text(encoding='utf-8')
script=text.split('<script>',1)[1].split('</script>',1)[0]
Path('.claude/tmp_companion_ui.js').parent.mkdir(exist_ok=True)
Path('.claude/tmp_companion_ui.js').write_text(script,encoding='utf-8')
PY
node --check .claude/tmp_companion_ui.js
```

Expected: exit 0, no output.

- [x] **Step 3: Parser smoke test**

Run a Python one-off against `serial_terminal.py` internals:

```bash
python - <<'PY'
import base64
import sys
sys.path.insert(0, 'tools/aurora')
import serial_terminal as st
raw = bytes([20] * (80 * 60))
data = base64.b64encode(raw).decode('ascii')
chunks = [data[i:i+1600] for i in range(0, len(data), 1600)]
st._handle_depth_line(f'A1_DEPTH_BEGIN frame=7 w=80 h=60 fmt=u8 encoding=base64 chunks={len(chunks)} bytes=4800')
for i, chunk in enumerate(chunks):
    st._handle_depth_line(f'A1_DEPTH_CHUNK frame=7 index={i} data={chunk}')
st._handle_depth_line('A1_DEPTH_END frame=7')
st._handle_depth_line('A1_DEPTH_OBJECT frame=7 cls=obstacle score=0.900 bucket=near depth=0.700 box=10.0,20.0,30.0,40.0')
frame = st.get_latest_depth_frame()
assert frame['success'] is True
assert frame['frame'] == 7
assert frame['width'] == 80
assert len(frame['objects']) == 1
assert frame['objects'][0]['class'] == 'obstacle'
assert frame['objects'][0]['bucket'] == 'near'
print('depth parser smoke ok')
PY
```

Expected: `depth parser smoke ok`.

- [x] **Step 4: Board build note**

Do not run board build unless user asks. Record: `Board build skipped per user request`.

---

## Self-Review

- Spec coverage: Board heuristic map, object summary, Aurora parser, frontend object chips, verification all covered.
- Placeholder scan: No TBD/TODO placeholders.
- Type consistency: `objects` array fields use `class`, `score`, `bucket`, `depth`, `box` consistently across backend and frontend.
