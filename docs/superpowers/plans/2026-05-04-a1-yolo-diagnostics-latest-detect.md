# A1 YOLO Diagnostics and Latest-Only Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add board-side YOLO diagnostic fields, show raw A1_DEBUG snapshot responses in Aurora, and make PC detection streaming latest-only to prevent latency buildup.

**Architecture:** The A1 board records lightweight pre-threshold diagnostics during YOLO decode and exposes them in the existing `A1_TEST yolo_snapshot` response. Aurora preserves the raw matched `A1_DEBUG` line and displays it under the annotated snapshot. PC `detect_feed` moves synchronous inference out of the HTTP generator into a background latest-only worker so slow inference drops frames instead of queueing stale frames.

**Tech Stack:** C++ board demo (`ssne_ai_demo`), Flask backend (`tools/aurora/aurora_companion.py`), vanilla HTML/JS (`tools/aurora/templates/companion_ui.html`), Python `unittest` and `py_compile`, Docker A1 build.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
  - Add diagnostic fields to `DetectionResult`.

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
  - Populate raw candidate count and top candidate before threshold filtering.

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Include diagnostic fields in `yolo_snapshot` JSON.

- Modify `tools/aurora/aurora_companion.py`
  - Return raw A1_DEBUG line from `/api/a1/yolo_snapshot`.
  - Add latest-only detection worker and cache.
  - Add detection timing metrics to `/api/detect/latest`.

- Modify `tools/aurora/templates/companion_ui.html`
  - Render snapshot diagnostics and raw A1_DEBUG line below returned image.

- Modify `tools/aurora/tests/test_a1_yolo_snapshot.py`
  - Assert raw line and diagnostic fields survive backend route.
  - Assert latest-only frame cache behavior.

---

### Task 1: Add board-side YOLO diagnostics

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Extend `DetectionResult`**

In `include/common.hpp`, replace:

```cpp
    void Clear() { boxes.clear(); scores.clear(); class_ids.clear(); }
```

with:

```cpp
    int raw_candidates = 0;
    float top_score = 0.0f;
    int top_class_id = -1;
    void Clear() {
        boxes.clear();
        scores.clear();
        class_ids.clear();
        raw_candidates = 0;
        top_score = 0.0f;
        top_class_id = -1;
    }
```

- [ ] **Step 2: Track top raw candidate during decode**

In `src/yolov8_detector.cpp`, inside `YOLOV8::DecodeHeadOutputs`, immediately after best class loop and before threshold check, replace:

```cpp
            if (best_score < conf_threshold) continue;
```

with:

```cpp
            ++raw_candidate_count_current;
            if (best_score > top_score_current) {
                top_score_current = best_score;
                top_class_current = best_class;
            }
            if (best_score < conf_threshold) continue;
```

- [ ] **Step 3: Add detector diagnostic accumulators**

In `src/yolov8_detector.cpp`, add file-scope variables inside the anonymous namespace after constants:

```cpp
int raw_candidate_count_current = 0;
float top_score_current = 0.0f;
int top_class_current = -1;
```

- [ ] **Step 4: Reset and copy diagnostics in `Predict`**

In `YOLOV8::Predict`, before first `DecodeHeadOutputs(...)`, add:

```cpp
    raw_candidate_count_current = 0;
    top_score_current = 0.0f;
    top_class_current = -1;
```

After `Postprocess(&boxes, &scores, &class_ids, result);`, add:

```cpp
    result->raw_candidates = raw_candidate_count_current;
    result->top_score = top_score_current;
    result->top_class_id = top_class_current;
```

- [ ] **Step 5: Add diagnostics to board JSON**

In `demo_rps_game.cpp`, inside `build_yolo_snapshot_json()`, after:

```cpp
         << ",\"camera_h\":" << kCameraHeight
```

add:

```cpp
         << ",\"threshold\":0.400"
         << ",\"raw_candidates\":" << det.raw_candidates
         << ",\"top_score\":" << det.top_score
         << ",\"top_class_id\":" << det.top_class_id
         << ",\"top_class\":\"" << json_escape(kClassNames.count(det.top_class_id) ? kClassNames.at(det.top_class_id) : "unknown") << "\""
```

- [ ] **Step 6: Inspect C++ diff**

Run:

```bash
git diff -- data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_detector.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
```

Expected: only diagnostic fields and JSON additions changed.

---

### Task 2: Preserve raw A1_DEBUG response in backend and tests

**Files:**
- Modify: `tools/aurora/aurora_companion.py`
- Modify: `tools/aurora/tests/test_a1_yolo_snapshot.py`

- [ ] **Step 1: Add failing backend test assertions**

In `test_snapshot_route_draws_board_boxes_on_camera_frame`, add these payload fields:

```python
            "threshold": 0.4,
            "raw_candidates": 12,
            "top_score": 0.23,
            "top_class_id": 0,
            "top_class": "person",
```

After `self.assertTrue(data["image_b64"])`, add:

```python
        self.assertIn("A1_DEBUG", data["raw_line"])
        self.assertEqual(data["diagnostics"]["raw_candidates"], 12)
        self.assertEqual(data["diagnostics"]["top_class"], "person")
```

- [ ] **Step 2: Run test to confirm failure**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: FAIL because `raw_line` and `diagnostics` are not returned yet.

- [ ] **Step 3: Return raw line from extractor**

In `aurora_companion.py`, change `_extract_yolo_snapshot_payload` return type from:

```python
def _extract_yolo_snapshot_payload(serial_result: Dict[str, Any]) -> Tuple[Optional[Dict[str, Any]], str, list]:
```

to:

```python
def _extract_yolo_snapshot_payload(serial_result: Dict[str, Any]) -> Tuple[Optional[Dict[str, Any]], str, list, str]:
```

Change successful return:

```python
            return payload, "", recent_rx
```

to:

```python
            return payload, "", recent_rx, line
```

Change final return:

```python
    return None, message, recent_rx
```

to:

```python
    return None, message, recent_rx, ""
```

- [ ] **Step 4: Add diagnostics helper**

After `_extract_yolo_snapshot_payload`, add:

```python
def _snapshot_diagnostics(snapshot: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "threshold": snapshot.get("threshold"),
        "raw_candidates": int(snapshot.get("raw_candidates") or 0),
        "top_score": float(snapshot.get("top_score") or 0.0),
        "top_class_id": int(snapshot.get("top_class_id") if snapshot.get("top_class_id") is not None else -1),
        "top_class": str(snapshot.get("top_class") or "unknown"),
    }
```

- [ ] **Step 5: Return diagnostics and raw line in route**

In `/api/a1/yolo_snapshot`, change:

```python
    snapshot, error, recent_rx = _extract_yolo_snapshot_payload(serial_result)
```

to:

```python
    snapshot, error, recent_rx, raw_line = _extract_yolo_snapshot_payload(serial_result)
```

In final `jsonify`, add:

```python
        "raw_line": raw_line,
        "diagnostics": _snapshot_diagnostics(snapshot),
```

- [ ] **Step 6: Run backend tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: PASS.

---

### Task 3: Add latest-only detect worker

**Files:**
- Modify: `tools/aurora/aurora_companion.py`
- Modify: `tools/aurora/tests/test_a1_yolo_snapshot.py`

- [ ] **Step 1: Add latest cache regression test**

In `tools/aurora/tests/test_a1_yolo_snapshot.py`, add:

```python
    def test_latest_frame_cache_replaces_stale_payload(self):
        cache = aurora_companion.LatestFrameCache()
        seq1 = cache.publish(b"old")
        seq2 = cache.publish(b"new")

        self.assertGreater(seq2, seq1)
        self.assertEqual(cache.latest(), (seq2, b"new"))
```

- [ ] **Step 2: Add detect cache globals**

In `aurora_companion.py`, after `_last_detect_snapshot`, add:

```python
_detect_frame_cache = LatestFrameCache()
_detect_worker_lock = threading.Lock()
_detect_worker_thread: Optional[threading.Thread] = None
_detect_worker_key: Optional[Tuple[int, str, str]] = None
_detect_target_fps = 10.0
```

- [ ] **Step 3: Add detect frame publisher helper**

After `_update_detection_runtime`, add:

```python
def _publish_detect_frame(display: np.ndarray, snapshot: Dict[str, Any], inference_ms: float, encode_ms: float) -> None:
    _detect_frame_cache.publish(cv2.imencode(".jpg", display, [cv2.IMWRITE_JPEG_QUALITY, 75])[1].tobytes())
    with _detect_state_lock:
        _last_detect_snapshot["inference_ms"] = round(inference_ms, 1)
        _last_detect_snapshot["encode_ms"] = round(encode_ms, 1)
        _last_detect_snapshot["detect_fps"] = _detect_target_fps
        _last_detect_snapshot["source_frame_age_ms"] = 0.0
```

- [ ] **Step 4: Add detect frame renderer helper**

After `_publish_detect_frame`, add:

```python
def _render_detect_frame(frame_gray: np.ndarray, detections) -> np.ndarray:
    display = cv2.cvtColor(frame_gray, cv2.COLOR_GRAY2BGR)
    for (x1, y1, x2, y2, score, cls_id) in detections:
        color = _CLASS_COLORS[cls_id % len(_CLASS_COLORS)]
        name = _CLASS_NAMES.get(cls_id, f"cls{cls_id}")
        cv2.rectangle(display, (int(x1), int(y1)), (int(x2), int(y2)), color, 2)
        label = f"{name} {score:.2f}"
        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
        ty = max(int(y1) - 4, th + 4)
        cv2.rectangle(display, (int(x1), ty - th - 4), (int(x1) + tw + 4, ty), color, -1)
        cv2.putText(display, label, (int(x1) + 2, ty - 2), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1)
    cv2.putText(display, f"Det: {len(detections)}  conf>={_DETECT_CONF:.2f}", (8, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 200, 80), 2)
    return display
```

- [ ] **Step 5: Add detect worker**

After `_render_detect_frame`, add:

```python
def _detect_worker(stream_key: Tuple[int, str, str]) -> None:
    interval = 1.0 / max(1.0, _detect_target_fps)
    while True:
        with camera_lock:
            cap = camera
            current_key = (device_id_global, camera_source_global, _DETECT_MODEL_PATH.name)
        if current_key != stream_key or cap is None:
            return
        loop_start = time.time()
        frame = _read_gray(cap)
        if frame is not None:
            infer_start = time.time()
            detections = detect_on_frame(frame)
            inference_ms = (time.time() - infer_start) * 1000.0
            snapshot = _update_detection_runtime(detections, frame.shape[:2])
            display = _render_detect_frame(frame, detections)
            encode_start = time.time()
            ok, buf = cv2.imencode(".jpg", display, [cv2.IMWRITE_JPEG_QUALITY, 75])
            encode_ms = (time.time() - encode_start) * 1000.0
            if ok:
                _detect_frame_cache.publish(buf.tobytes())
                with _detect_state_lock:
                    _last_detect_snapshot.update({
                        "inference_ms": round(inference_ms, 1),
                        "encode_ms": round(encode_ms, 1),
                        "detect_fps": round(1.0 / max(0.001, time.time() - loop_start), 1),
                        "source_frame_age_ms": 0.0,
                    })
        elapsed = time.time() - loop_start
        if elapsed < interval:
            time.sleep(interval - elapsed)
```

- [ ] **Step 6: Add detect worker starter**

After `_detect_worker`, add:

```python
def _ensure_detect_worker() -> None:
    global _detect_worker_thread, _detect_worker_key
    key = (device_id_global, camera_source_global, _DETECT_MODEL_PATH.name)
    with _detect_worker_lock:
        if _detect_worker_thread is not None and _detect_worker_thread.is_alive() and _detect_worker_key == key:
            return
        _detect_worker_key = key
        _detect_worker_thread = threading.Thread(target=_detect_worker, args=(key,), daemon=True, name="aurora-detect-worker")
        _detect_worker_thread.start()
```

- [ ] **Step 7: Replace `_generate_detect_frames` with latest-only stream**

Replace body of `_generate_detect_frames()` with:

```python
def _generate_detect_frames():
    """本地 YOLOv8 检测视频流；后台推理只保留最新帧。"""
    last_sequence = 0
    while True:
        _ensure_detect_worker()
        item = _detect_frame_cache.wait_for_next(last_sequence=last_sequence, timeout=1.0)
        if item is None:
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH, 3), dtype=np.uint8)
            cv2.putText(blk, "Waiting for detect frame", (CAMERA_WIDTH // 2 - 160, CAMERA_HEIGHT // 2), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (60, 60, 60), 2)
            _, buf = cv2.imencode(".jpg", blk, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + buf.tobytes() + b"\r\n")
            continue
        last_sequence, payload = item
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + payload + b"\r\n")
```

- [ ] **Step 8: Run tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot -v
```

Expected: PASS.

---

### Task 4: Render diagnostics and raw line in UI

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html`

- [ ] **Step 1: Add raw line elements under snapshot image**

Inside `#a1SnapshotPanel`, after `<div class="pre" id="a1SnapshotObjects" style="margin-top:8px"></div>`, add:

```html
  <div class="panel-title" style="margin-top:8px">A1_DEBUG 原始回包</div>
  <div class="pre mono" id="a1SnapshotRaw" style="margin-top:8px;max-height:160px;overflow:auto"></div>
```

- [ ] **Step 2: Add diagnostics renderer**

After `renderA1SnapshotObjects`, add:

```javascript
function renderA1SnapshotDiagnostics(d={}){
  const diag=d.diagnostics||{};
  const topScore=Number(diag.top_score||0).toFixed(3);
  return `threshold=${diag.threshold??'—'} · raw_candidates=${diag.raw_candidates??0} · top=${diag.top_class||'unknown'} ${topScore}`;
}
```

- [ ] **Step 3: Populate diagnostics and raw line**

In `takeA1YoloSnapshot()`, after:

```javascript
      const objects=document.getElementById('a1SnapshotObjects');
```

add:

```javascript
      const raw=document.getElementById('a1SnapshotRaw');
```

Change meta text to:

```javascript
      meta.textContent=`A1 frame=${d.frame??'—'} · 目标 ${d.count??0} · ${renderA1SnapshotDiagnostics(d)} · ${d.warning||'预览帧与板端检测帧为近似同步，不保证像素级同帧'}`;
```

After `objects.textContent=...`, add:

```javascript
      raw.textContent=d.raw_line||'未收到 A1_DEBUG 原始回包';
```

- [ ] **Step 4: Inspect HTML diff**

Run:

```bash
git diff -- tools/aurora/templates/companion_ui.html
```

Expected: snapshot panel has raw line block and JS renders diagnostics.

---

### Task 5: Verification

**Files:**
- Verify Python files and board build.

- [ ] **Step 1: Python compile**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/aurora/tests/test_a1_yolo_snapshot.py tools/aurora/tests/test_qt_bridge_lifecycle.py
```

Expected: no output, exit code 0.

- [ ] **Step 2: Unit tests**

Run:

```bash
python -m unittest tools.aurora.tests.test_a1_yolo_snapshot tools.aurora.tests.test_qt_bridge_lifecycle -v
```

Expected: PASS all tests.

- [ ] **Step 3: A1 app-only build**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: exit code 0 and EVB output path.

- [ ] **Step 4: Manual runtime check**

Run:

```bash
python tools/aurora/aurora_companion.py --host 127.0.0.1 --port 6209 --source windows
```

Then consume `/video_feed` for 3 seconds and check FPS:

```bash
python - <<'PY'
import json, time, urllib.request
start=time.time()
count=0
with urllib.request.urlopen('http://127.0.0.1:6209/video_feed', timeout=8) as r:
    while time.time()-start < 3:
        count += r.read(16384).count(b'--frame')
with urllib.request.urlopen('http://127.0.0.1:6209/status', timeout=2) as r:
    data=json.loads(r.read().decode('utf-8'))
print('frames_seen', count)
print('companion_fps', data.get('fps'))
print('qt_bridge_fps', data.get('qt_bridge', {}).get('fps'))
PY
```

Expected: Companion FPS in camera-rate range, not thousands.

---

## Self-Review

Spec coverage:
- A1 diagnostic fields: Task 1.
- Raw A1_DEBUG under snapshot image: Task 2 and Task 4.
- Latest-only detection stream: Task 3.
- Performance metrics in detect status: Task 3 adds timing fields.
- Verification: Task 5.

Placeholder scan:
- No TBD/TODO/implement-later placeholders.
- Each code-changing step includes exact code.

Type consistency:
- Board, backend, and UI use `threshold`, `raw_candidates`, `top_score`, `top_class_id`, `top_class`, `raw_line`, `diagnostics` consistently.
