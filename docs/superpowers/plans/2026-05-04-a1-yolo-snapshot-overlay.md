# A1 YOLO Snapshot Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an Aurora “A1 拍照检测” diagnostic button that overlays board-reported YOLO boxes on the current PC preview frame.

**Architecture:** The A1 board keeps a thread-safe latest detection snapshot from the main loop and exposes it through `A1_TEST yolo_snapshot`. Aurora captures its current preview frame, sends the serial command through existing COM13 helpers, parses the `A1_DEBUG` JSON payload, draws boxes on the captured frame, saves a JPEG, and renders the result in the existing UI. This path is approximate-frame sync by design and must show that warning.

**Tech Stack:** C++ board demo (`ssne_ai_demo`), Flask backend (`tools/aurora/aurora_companion.py`), existing serial terminal helpers (`tools/aurora/serial_terminal.py`), vanilla HTML/JS UI (`tools/aurora/templates/companion_ui.html`), Python `unittest` and `py_compile`.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Add mutex-protected latest YOLO snapshot storage.
  - Add `A1_TEST yolo_snapshot` command handler.
  - Update latest snapshot after each main-loop detection.

- Modify `tools/aurora/serial_terminal.py`
  - Register `yolo_snapshot` as an A1 debug quick command so backend/UI can use shared command-building semantics.

- Modify `tools/aurora/aurora_companion.py`
  - Add helper to parse `A1_DEBUG` JSON from serial result.
  - Add helper to draw A1 snapshot boxes on a captured preview frame.
  - Add `POST /api/a1/yolo_snapshot` route.
  - Save annotated JPEG and append thumbnail to `recent_captures`.

- Modify `tools/aurora/templates/companion_ui.html`
  - Add `A1 拍照检测` button.
  - Add result panel showing annotated image, frame index, count, object rows, and approximate-sync warning.

- Create `tools/aurora/tests/test_a1_yolo_snapshot.py`
  - Backend route tests with mocked camera frame and serial response.
  - Error path tests for no camera frame and malformed serial response.

---

### Task 1: Add board-side latest snapshot command

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp:1-408`

- [ ] **Step 1: Add required includes**

Add these includes near existing includes:

```cpp
#include <iomanip>
#include <mutex>
```

- [ ] **Step 2: Add latest snapshot state**

Inside the anonymous namespace, after `std::string g_last_cli_action = "stop";`, add:

```cpp
struct LatestYoloSnapshot {
    bool valid = false;
    uint64_t frame_index = 0;
    DetectionResult detections;
};

std::mutex g_latest_yolo_mutex;
LatestYoloSnapshot g_latest_yolo_snapshot;
```

- [ ] **Step 3: Add JSON string escaping helper**

After `base64_encode(...)`, add:

```cpp
std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}
```

- [ ] **Step 4: Add snapshot update helper**

After `print_detection_summary(...)`, add:

```cpp
void update_latest_yolo_snapshot(uint64_t frame_index, const DetectionResult& det_result) {
    std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
    g_latest_yolo_snapshot.valid = true;
    g_latest_yolo_snapshot.frame_index = frame_index;
    g_latest_yolo_snapshot.detections = det_result;
}
```

- [ ] **Step 5: Add snapshot JSON builder**

After `update_latest_yolo_snapshot(...)`, add:

```cpp
std::string build_yolo_snapshot_json() {
    LatestYoloSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
        snapshot = g_latest_yolo_snapshot;
    }

    if (!snapshot.valid) {
        return "\"message\":\"no detection snapshot yet\"";
    }

    const DetectionResult& det = snapshot.detections;
    std::ostringstream body;
    body << std::fixed << std::setprecision(3);
    body << "\"frame\":" << snapshot.frame_index
         << ",\"count\":" << det.boxes.size()
         << ",\"camera_w\":" << kCameraWidth
         << ",\"camera_h\":" << kCameraHeight
         << ",\"objects\":[";

    for (size_t i = 0; i < det.boxes.size(); ++i) {
        const int cls = i < det.class_ids.size() ? det.class_ids[i] : -1;
        auto it = kClassNames.find(cls);
        const std::string name = (it != kClassNames.end()) ? it->second : "unknown";
        const float score = i < det.scores.size() ? det.scores[i] : 0.0f;
        const auto& box = det.boxes[i];
        if (i > 0) body << ",";
        body << "{\"class_id\":" << cls
             << ",\"class\":\"" << json_escape(name) << "\""
             << ",\"score\":" << score
             << ",\"box\":[" << box[0] << "," << box[1] << "," << box[2] << "," << box[3] << "]}";
    }

    body << "],\"message\":\"latest detection snapshot\"";
    return body.str();
}
```

- [ ] **Step 6: Add command handler branch**

In `handle_a1_test_command(...)`, after the `osd_status` branch and before `chassis_test`, add:

```cpp
    if (command == "yolo_snapshot") {
        LatestYoloSnapshot snapshot;
        {
            std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
            snapshot = g_latest_yolo_snapshot;
        }
        if (!snapshot.valid) {
            print_debug_response("yolo_snapshot", "\"message\":\"no detection snapshot yet\"", false);
            return;
        }
        print_debug_response("yolo_snapshot", build_yolo_snapshot_json(), true);
        return;
    }
```

- [ ] **Step 7: Update snapshot in main loop**

In `main()`, immediately after `print_detection_summary(frame_index, det_result);`, add:

```cpp
        update_latest_yolo_snapshot(frame_index, det_result);
```

- [ ] **Step 8: Compile-check board file if local compiler available**

Run syntax-level project build later through full SDK flow. For this task, inspect modified C++ for missing symbols:

```bash
git diff -- data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
```

Expected: only includes, snapshot helpers, command branch, and main-loop update changed.

---

### Task 2: Add backend route tests

**Files:**
- Create: `tools/aurora/tests/test_a1_yolo_snapshot.py`

- [ ] **Step 1: Create failing route tests**

Create `tools/aurora/tests/test_a1_yolo_snapshot.py`:

```python
import importlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import numpy as np


_REAL_PATH_EXISTS = Path.exists


def _patched_path_exists(self):
    if self.name == "best_a1_formal.onnx":
        return True
    return _REAL_PATH_EXISTS(self)


with mock.patch("pathlib.Path.exists", new=_patched_path_exists):
    aurora_companion = importlib.import_module("tools.aurora.aurora_companion")


class A1YoloSnapshotTests(unittest.TestCase):
    def test_snapshot_route_draws_board_boxes_on_camera_frame(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "yolo_snapshot",
            "success": True,
            "frame": 42,
            "count": 1,
            "camera_w": 720,
            "camera_h": 1280,
            "objects": [
                {"class_id": 2, "class": "forward", "score": 0.88, "box": [100.0, 200.0, 240.0, 420.0]}
            ],
            "message": "latest detection snapshot",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [],
        }

        with tempfile.TemporaryDirectory() as tmpdir, \
             mock.patch.object(aurora_companion, "output_dir", tmpdir), \
             mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        self.assertEqual(data["frame"], 42)
        self.assertEqual(data["count"], 1)
        self.assertEqual(data["objects"][0]["class"], "forward")
        self.assertTrue(data["image_b64"])
        self.assertIn("近似同步", data["warning"])

    def test_snapshot_route_returns_error_when_camera_frame_missing(self):
        client = aurora_companion.app.test_client()
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=None):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertIn("无法获取摄像头画面", data["error"])

    def test_snapshot_route_returns_error_for_malformed_a1_debug_payload(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG not-json"},
            "recent_rx": ["A1_DEBUG not-json"],
        }
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertIn("A1_DEBUG", data["error"])
        self.assertEqual(data["recent_rx"], ["A1_DEBUG not-json"])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: FAIL/ERROR because `/api/a1/yolo_snapshot` and helpers do not exist yet.

---

### Task 3: Implement backend snapshot route

**Files:**
- Modify: `tools/aurora/aurora_companion.py:70-80`
- Modify: `tools/aurora/aurora_companion.py:1815-1855`
- Modify: `tools/aurora/aurora_companion.py:2069-2122`

- [ ] **Step 1: Import serial terminal module object**

Near existing serial terminal import block, change:

```python
try:
    from serial_terminal import get_latest_depth_frame, serial_term_bp
    app.register_blueprint(serial_term_bp)
except ImportError:
```

to:

```python
try:
    import serial_terminal
    from serial_terminal import get_latest_depth_frame, serial_term_bp
    app.register_blueprint(serial_term_bp)
except ImportError:
```

In the `except ImportError:` branch, add this before `def get_latest_depth_frame():`:

```python
    serial_terminal = None
```

- [ ] **Step 2: Add A1_DEBUG JSON parser helper**

Before `_save_capture(...)`, add:

```python
def _parse_a1_debug_json_line(text: str) -> Optional[Dict[str, Any]]:
    raw = str(text or "").strip()
    prefix = "A1_DEBUG "
    if not raw.startswith(prefix):
        return None
    payload = raw[len(prefix):].strip()
    if not payload.startswith("{"):
        return None
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return None
    return parsed if isinstance(parsed, dict) else None
```

- [ ] **Step 3: Add snapshot response extractor**

After `_parse_a1_debug_json_line(...)`, add:

```python
def _extract_yolo_snapshot_payload(serial_result: Dict[str, Any]) -> Tuple[Optional[Dict[str, Any]], str, list]:
    recent_rx = serial_result.get("recent_rx") or []
    matched = serial_result.get("matched") or {}
    candidates = []
    if matched.get("text"):
        candidates.append(str(matched.get("text")))
    candidates.extend(str(line) for line in recent_rx)

    for line in candidates:
        payload = _parse_a1_debug_json_line(line)
        if payload and payload.get("command") == "yolo_snapshot":
            return payload, "", recent_rx

    message = str(serial_result.get("message") or serial_result.get("error") or "未收到有效 A1_DEBUG yolo_snapshot 回包")
    return None, message, recent_rx
```

- [ ] **Step 4: Add drawing helper**

After `_extract_yolo_snapshot_payload(...)`, add:

```python
def _draw_a1_snapshot_overlay(frame: np.ndarray, snapshot: Dict[str, Any]) -> np.ndarray:
    display = frame.copy()
    if len(display.shape) == 2:
        display = cv2.cvtColor(display, cv2.COLOR_GRAY2BGR)
    elif display.shape[2] == 1:
        display = cv2.cvtColor(display[:, :, 0], cv2.COLOR_GRAY2BGR)

    frame_h, frame_w = display.shape[:2]
    camera_w = float(snapshot.get("camera_w") or frame_w)
    camera_h = float(snapshot.get("camera_h") or frame_h)
    x_scale = frame_w / max(1.0, camera_w)
    y_scale = frame_h / max(1.0, camera_h)

    for item in snapshot.get("objects") or []:
        box = item.get("box") or []
        if len(box) != 4:
            continue
        cls_id = int(item.get("class_id") or 0)
        color = _CLASS_COLORS[cls_id % len(_CLASS_COLORS)]
        x1 = int(max(0, min(frame_w - 1, float(box[0]) * x_scale)))
        y1 = int(max(0, min(frame_h - 1, float(box[1]) * y_scale)))
        x2 = int(max(0, min(frame_w - 1, float(box[2]) * x_scale)))
        y2 = int(max(0, min(frame_h - 1, float(box[3]) * y_scale)))
        cv2.rectangle(display, (x1, y1), (x2, y2), color, 2)
        label_name = str(item.get("class") or item.get("class_name") or _CLASS_NAMES.get(cls_id, f"cls{cls_id}"))
        score = float(item.get("score") or 0.0)
        label = f"A1 {label_name} {score:.2f}"
        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
        ty = max(y1 - 4, th + 4)
        cv2.rectangle(display, (x1, ty - th - 4), (min(frame_w - 1, x1 + tw + 4), ty), color, -1)
        cv2.putText(display, label, (x1 + 2, ty - 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1)

    cv2.putText(display, f"A1 snapshot frame={snapshot.get('frame', '—')} count={snapshot.get('count', 0)}", (8, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 200, 80), 2)
    return display
```

- [ ] **Step 5: Add save helper for annotated result**

After `_draw_a1_snapshot_overlay(...)`, add:

```python
def _save_a1_yolo_snapshot_image(display: np.ndarray, snapshot: Dict[str, Any]) -> Dict[str, Any]:
    global capture_count
    capture_count += 1
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    name = f"a1_yolo_snapshot_{ts}_{capture_count:04d}.jpg"
    path = os.path.join(output_dir, name)
    os.makedirs(output_dir, exist_ok=True)
    cv2.imwrite(path, display, [cv2.IMWRITE_JPEG_QUALITY, 90])

    thumb = cv2.resize(display, (160, 120))
    _, thumb_buf = cv2.imencode(".jpg", thumb, [cv2.IMWRITE_JPEG_QUALITY, 72])
    thumb_b64 = base64.b64encode(thumb_buf).decode()
    _, image_buf = cv2.imencode(".jpg", display, [cv2.IMWRITE_JPEG_QUALITY, 86])
    image_b64 = base64.b64encode(image_buf).decode()

    info = {
        "filename": name,
        "path": path,
        "format": "a1_yolo_snapshot",
        "size": f"{display.shape[1]}×{display.shape[0]}",
        "thumb": thumb_b64,
        "image_b64": image_b64,
        "time": datetime.now().strftime("%H:%M:%S"),
        "index": capture_count,
        "frame": snapshot.get("frame"),
        "count": int(snapshot.get("count") or len(snapshot.get("objects") or [])),
    }
    recent_captures.appendleft(info)
    print(f"[A1_SNAPSHOT] {path} frame={info['frame']} count={info['count']}")
    return info
```

- [ ] **Step 6: Add Flask route**

Before `/switch_detect_model`, add:

```python
@app.route("/api/a1/yolo_snapshot", methods=["POST"])
def a1_yolo_snapshot():
    if serial_terminal is None:
        return jsonify({"success": False, "error": "serial_terminal unavailable"})

    with camera_lock:
        cap = camera
    frame = _read_display_frame(cap) if cap else None
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})

    serial_result = serial_terminal.send_text_line(
        "A1_TEST yolo_snapshot",
        wait_tokens=["A1_DEBUG", '"command":"yolo_snapshot"'],
        timeout_sec=2.5,
    )
    if not serial_result.get("success"):
        return jsonify({
            "success": False,
            "error": str(serial_result.get("message") or serial_result.get("error") or "A1 yolo_snapshot 回包超时"),
            "serial": serial_result,
            "recent_rx": serial_result.get("recent_rx", []),
        })

    snapshot, error, recent_rx = _extract_yolo_snapshot_payload(serial_result)
    if not snapshot:
        return jsonify({"success": False, "error": f"未解析到有效 A1_DEBUG yolo_snapshot 回包: {error}", "recent_rx": recent_rx})
    if not snapshot.get("success"):
        return jsonify({"success": False, "error": str(snapshot.get("message") or "A1 尚无检测快照"), "snapshot": snapshot})

    display = _draw_a1_snapshot_overlay(frame, snapshot)
    image_info = _save_a1_yolo_snapshot_image(display, snapshot)
    objects = snapshot.get("objects") or []
    return jsonify({
        "success": True,
        "warning": "预览帧与板端检测帧为近似同步，不保证像素级同帧",
        "objects": objects,
        "frame": snapshot.get("frame"),
        "count": int(snapshot.get("count") or len(objects)),
        "camera_w": snapshot.get("camera_w"),
        "camera_h": snapshot.get("camera_h"),
        **image_info,
    })
```

- [ ] **Step 7: Run backend tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: PASS all 3 tests.

---

### Task 4: Register serial quick command

**Files:**
- Modify: `tools/aurora/serial_terminal.py:29-39`

- [ ] **Step 1: Add command mapping**

In `_A1_DEBUG_COMMANDS`, add:

```python
    "yolo_snapshot": "yolo_snapshot",
```

Final block should include:

```python
_A1_DEBUG_COMMANDS = {
    "ping": "ping",
    "osd_status": "osd_status",
    "uart_status": "uart_status",
    "chassis_stop": "chassis_test stop",
    "chassis_forward": "chassis_test forward",
    "yolo_snapshot": "yolo_snapshot",
}
```

- [ ] **Step 2: Add description**

In `_A1_DEBUG_DESCRIPTIONS`, add:

```python
    "yolo_snapshot": "latest board YOLO boxes -> overlay on PC preview",
```

- [ ] **Step 3: Run py_compile for serial helper**

Run:

```bash
python -m py_compile tools/aurora/serial_terminal.py
```

Expected: no output and exit code 0.

---

### Task 5: Add Aurora UI button and result panel

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html:76-86`
- Modify: `tools/aurora/templates/companion_ui.html:181-184`

- [ ] **Step 1: Add result panel HTML**

In the `摄像头与拍照` card, replace this block:

```html
<div class="grid2"><button class="btn green" onclick="takeCapture('720x1280')">保存原图</button><button class="btn blue" onclick="takeCapture('640x480')">保存训练图</button></div>
<p class="muted" id="captureHint">保存目录：{{ output_dir }}</p>
<div class="gallery" id="captureGallery"></div>
```

with:

```html
<div class="grid3"><button class="btn green" onclick="takeCapture('720x1280')">保存原图</button><button class="btn blue" onclick="takeCapture('640x480')">保存训练图</button><button class="btn warn" id="a1SnapshotBtn" onclick="takeA1YoloSnapshot()">A1 拍照检测</button></div>
<p class="muted" id="captureHint">保存目录：{{ output_dir }}</p>
<div class="info-panel" id="a1SnapshotPanel" style="display:none">
  <div class="panel-title">A1 拍照检测结果</div>
  <div id="a1SnapshotMeta" class="muted">等待检测</div>
  <img id="a1SnapshotImage" alt="A1 snapshot" style="width:100%;max-height:420px;object-fit:contain;border:1px solid var(--line);border-radius:10px;margin-top:8px;background:#000">
  <div class="pre" id="a1SnapshotObjects" style="margin-top:8px"></div>
</div>
<div class="gallery" id="captureGallery"></div>
```

- [ ] **Step 2: Add frontend function**

After `takeCapture(format)`, add:

```javascript
function renderA1SnapshotObjects(items=[]){
  if(!items.length)return '无目标';
  return items.map((x,i)=>{
    const box=Array.isArray(x.box)?x.box.map(v=>Number(v).toFixed(1)).join(', '):'—';
    const score=Number(x.score||0).toFixed(3);
    return `${i+1}. ${x.class||x.class_name||('cls'+(x.class_id??'?' ))} score=${score} box=[${box}]`;
  }).join('\n');
}
function takeA1YoloSnapshot(){
  const btn=document.getElementById('a1SnapshotBtn');
  if(btn)btn.disabled=true;
  return apiJson('/api/a1/yolo_snapshot',{method:'POST'})
    .then(d=>{
      if(!d.success)throw new Error(d.error||'A1 拍照检测失败');
      const panel=document.getElementById('a1SnapshotPanel');
      const meta=document.getElementById('a1SnapshotMeta');
      const img=document.getElementById('a1SnapshotImage');
      const objects=document.getElementById('a1SnapshotObjects');
      panel.style.display='block';
      meta.textContent=`A1 frame=${d.frame??'—'} · 目标 ${d.count??0} · ${d.warning||'预览帧与板端检测帧为近似同步，不保证像素级同帧'}`;
      img.src='data:image/jpeg;base64,'+(d.image_b64||'');
      objects.textContent=renderA1SnapshotObjects(d.objects||[]);
      toast('A1 拍照检测完成');
      loadRecentCaptures();
    })
    .catch(e=>toast('A1 拍照检测失败: '+e.message))
    .finally(()=>{if(btn)btn.disabled=false;});
}
```

- [ ] **Step 3: Manual inspect HTML syntax**

Run:

```bash
git diff -- tools/aurora/templates/companion_ui.html
```

Expected: one button/panel added and one JS function pair added; no unrelated UI changes.

---

### Task 6: Run verification

**Files:**
- Verify: `tools/aurora/aurora_companion.py`
- Verify: `tools/aurora/serial_terminal.py`
- Verify: `tools/aurora/tests/test_a1_yolo_snapshot.py`

- [ ] **Step 1: Run Python compile checks**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py
```

Expected: no output and exit code 0.

- [ ] **Step 2: Run focused unit tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: PASS all tests.

- [ ] **Step 3: Run existing Aurora unit tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_qt_bridge_lifecycle -v
```

Expected: PASS all existing tests.

- [ ] **Step 4: Manual browser check if environment allows**

Run Aurora:

```powershell
cd tools/aurora
.\launch.ps1 -SkipAurora
```

Open reported local URL. Expected:
- `A1 拍照检测` button visible under `摄像头与拍照`.
- With COM13 disconnected, click shows toast error, no page crash.
- With board connected and updated firmware, click displays annotated image and object list.

---

### Task 7: Build board app after code changes

**Files:**
- Verify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Run app-only build if SDK cache exists**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: build succeeds and emits updated EVB output path. If it fails because SDK baseline cache is missing, run full build instead.

- [ ] **Step 2: Run full build only if app-only reports missing baseline**

Run only when Step 1 says baseline/cache missing:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

Expected: build succeeds and emits `zImage.smartsens-m1-evb` under `output/evb/<timestamp>/`.

- [ ] **Step 3: Inspect build diff/log outcome**

Run:

```bash
git status --short
```

Expected: source changes and generated output policy understood; do not commit `output/` artifacts.

---

## Self-Review

Spec coverage:
- Aurora button/result panel: Task 5.
- Backend endpoint, serial command, parse, draw, save: Task 3.
- A1 `A1_TEST yolo_snapshot`: Task 1.
- Serial command registration: Task 4.
- Error cases: Task 2 and Task 3.
- Approximate-sync warning: Task 3 and Task 5.
- Verification: Task 6 and Task 7.

Placeholder scan:
- No TBD/TODO/implement-later placeholders.
- Each code-changing step includes exact code or exact replacement block.

Type consistency:
- Board response uses `objects`, `class_id`, `class`, `score`, `box`, `camera_w`, `camera_h`.
- Backend and frontend consume same property names.
- Route path is consistently `/api/a1/yolo_snapshot`.
