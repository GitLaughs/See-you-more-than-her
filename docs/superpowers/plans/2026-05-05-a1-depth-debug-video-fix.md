# A1 Depth Stream And Debug Commands Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-add board-side fake depth streaming, make Aurora debug buttons match board commands, fix the missing model startup path, and reduce Aurora preview tearing/jitter from stale cached frames.

**Architecture:** Keep the board feature in `demo_rps_game.cpp`: one text protocol emitter for `A1_DEPTH_*`, one small fake depth generator, and one `A1_TEST` handler extension for Aurora commands. Keep Aurora parsing/UI mostly unchanged, but align command maps and stop serving stale Qt-bridge JPEG cache across frame reads. Fix model startup by making `run.sh` and board code use the actual model filename already present in `app_assets/models/`.

**Tech Stack:** C++17-ish board demo built by existing CMake, POSIX shell `run.sh`, Python Flask Aurora tools, PySide/Qt camera bridge, existing `py_compile` validation.

---

## File structure

- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Add fake depth frame generation and base64 emit helpers.
  - Add `A1_TEST test_echo` and `A1_TEST depth_snapshot` handlers.
  - Include `command`, `success`, `message`, `chassis_ok`, `action`, `vx` fields in expected Aurora responses.
  - Keep existing `ping`, `rps_snapshot`, `chassis_test`, `move`, `stop` behavior.
  - Use model path that matches bundled file.

- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/scripts/run.sh`
  - Check actual bundled model file: `1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model` if present, else `test.m1model` fallback.
  - Print exact model path found instead of generic missing message.

- Modify: `tools/aurora/serial_terminal.py`
  - Add `depth_snapshot` to `_A1_DEBUG_COMMANDS` and descriptions.
  - Make `send_test` default command `test_echo` wait for board response emitted by new handler.

- Modify: `tools/aurora/qt_camera_bridge.py`
  - Stop returning stale `latest_color_jpeg/latest_gray_jpeg` blindly from `frame_bytes()` when no new frame arrived.
  - Return frame sequence with bytes or invalidate cache per request so HTTP `/frame.jpg` does not reuse old cached payload during preview.

- Modify: `tools/aurora/aurora_companion.py`
  - If needed after Qt bridge change, ensure stream workers wait for next frame sequence and never re-yield identical cached payload forever.
  - Keep `Cache-Control` headers already present.

## Task 1: Board model path and startup check

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/scripts/run.sh:12-15`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp:228-231`

- [ ] **Step 1: Write failing shell check command**

Run from repo root:

```bash
test -f "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model" && test -f "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/models/test.m1model"
```

Expected: PASS if both files exist in repo. If first file is missing, stop and list `app_assets/models/`; do not change code to non-existent filename.

- [ ] **Step 2: Update `run.sh` model selection**

Replace lines 12-15 with:

```sh
MODEL_PATH="./app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model"
if [ ! -f "${MODEL_PATH}" ]; then
    MODEL_PATH="./app_assets/models/test.m1model"
fi
if [ ! -f "${MODEL_PATH}" ]; then
    echo "[APP] missing A1 5-class model: expected ./app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model or ./app_assets/models/test.m1model"
    exit 1
fi
echo "[APP] model=${MODEL_PATH}"
```

- [ ] **Step 3: Update C++ model path fallback**

In `demo_rps_game.cpp`, replace:

```cpp
std::string model_path = "/app_demo/app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model";
```

with:

```cpp
const char* model_path = "/app_demo/app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model";
if (access(model_path, F_OK) != 0) {
    model_path = "/app_demo/app_assets/models/test.m1model";
}
std::cout << "[APP] classifier_model=" << model_path << std::endl;
```

- [ ] **Step 4: Verify shell syntax**

Run:

```bash
bash -n "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/scripts/run.sh"
```

Expected: no output, exit 0.

## Task 2: Board A1_TEST command compatibility

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp:139-197`
- Modify: `tools/aurora/serial_terminal.py:30-40`

- [ ] **Step 1: Add command names to Aurora command map**

Update `_A1_DEBUG_COMMANDS` to:

```python
_A1_DEBUG_COMMANDS = {
    "ping": "ping",
    "test_echo": "test_echo",
    "chassis_stop": "chassis_test stop",
    "chassis_forward": "chassis_test forward",
    "rps_snapshot": "rps_snapshot",
    "depth_snapshot": "depth_snapshot",
}
```

Update `_A1_DEBUG_DESCRIPTIONS` to include:

```python
_A1_DEBUG_DESCRIPTIONS = {
    "ping": "PC -> COM13 -> A1_TEST -> A1_DEBUG link check",
    "test_echo": "serial terminal smoke test",
    "chassis_forward": "forward gesture -> forward",
    "chassis_stop": "stop/person/obstacle -> stop",
    "rps_snapshot": "latest board classification snapshot -> overlay on PC preview",
    "depth_snapshot": "emit one fake A1_DEPTH frame",
}
```

- [ ] **Step 2: Add `test_echo` handler in board code**

Inside `handle_a1_test_command()` after `ping`, add:

```cpp
if (command == "test_echo") {
    std::string message;
    std::getline(iss, message);
    if (!message.empty() && message[0] == ' ') message.erase(0, 1);
    if (message.empty()) message = "pc_frontend_test";
    print_debug_response("test_echo", "\"message\":\"测试回传成功: " + json_escape(message) + "\",\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false"));
    return;
}
```

- [ ] **Step 3: Ensure existing Aurora buttons map to board commands**

Confirm these UI paths now work:

- `runA1LinkTest()` sends `/api/serial_term/a1_debug` with `command:'ping'`; board returns `A1_DEBUG {"command":"ping",...}`.
- `sendA1Debug('ping')` uses same.
- `sendA1Debug('chassis_forward')` maps to `A1_TEST chassis_test forward`; board returns `command":"chassis_test"`.
- `sendA1Debug('chassis_stop')` maps to `A1_TEST chassis_test stop`; board returns `command":"chassis_test"`.
- Serial terminal `test_echo` button calls `/send_test` with `command:'test_echo'`; board returns `command":"test_echo"` and message containing `测试回传成功`.

- [ ] **Step 4: Verify Python syntax**

Run:

```bash
python -m py_compile tools/aurora/serial_terminal.py
```

Expected: no output, exit 0.

## Task 3: Board fake depth stream emitter

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Add includes and constants**

Add includes near top:

```cpp
#include <vector>
```

Add constants after `kForwardVelocity`:

```cpp
constexpr int kDepthWidth = 80;
constexpr int kDepthHeight = 60;
constexpr int kDepthChunkChars = 960;
constexpr int kDepthAutoIntervalMs = 1000;
```

- [ ] **Step 2: Add base64 encoder**

Add in anonymous namespace before `print_debug_response()`:

```cpp
std::string base64_encode(const uint8_t* data, size_t len) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < len) ? data[i + 1] : 0;
        const uint32_t b2 = (i + 2 < len) ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? table[triple & 0x3F] : '=');
    }
    return out;
}
```

- [ ] **Step 3: Add fake depth generator**

Add:

```cpp
std::vector<uint8_t> build_fake_depth_frame(uint64_t frame_index) {
    std::vector<uint8_t> depth(kDepthWidth * kDepthHeight);
    const int cx = kDepthWidth / 2 + static_cast<int>((frame_index % 21) - 10);
    const int cy = kDepthHeight / 2;
    for (int y = 0; y < kDepthHeight; ++y) {
        for (int x = 0; x < kDepthWidth; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int dist = dx * dx + dy * dy;
            const int wave = static_cast<int>((x * 3 + y * 5 + frame_index * 7) & 0x3F);
            int value = 40 + ((x * 180) / kDepthWidth) + wave;
            if (dist < 180) value = 230 - dist / 2;
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            depth[y * kDepthWidth + x] = static_cast<uint8_t>(value);
        }
    }
    return depth;
}
```

- [ ] **Step 4: Add depth emitter**

Add:

```cpp
void emit_depth_frame(uint64_t depth_frame_index) {
    const std::vector<uint8_t> depth = build_fake_depth_frame(depth_frame_index);
    const std::string encoded = base64_encode(depth.data(), depth.size());
    const int chunks = static_cast<int>((encoded.size() + kDepthChunkChars - 1) / kDepthChunkChars);
    std::cout << "A1_DEPTH_BEGIN frame=" << depth_frame_index
              << " w=" << kDepthWidth
              << " h=" << kDepthHeight
              << " fmt=u8 encoding=base64 chunks=" << chunks
              << " bytes=" << depth.size() << std::endl;
    for (int i = 0; i < chunks; ++i) {
        const size_t start = static_cast<size_t>(i) * kDepthChunkChars;
        std::cout << "A1_DEPTH_CHUNK frame=" << depth_frame_index
                  << " index=" << i
                  << " data=" << encoded.substr(start, kDepthChunkChars) << std::endl;
    }
    std::cout << "A1_DEPTH_OBJECT frame=" << depth_frame_index
              << " cls=fake score=1.00 bucket=mid depth=1.20 box=0.35,0.35,0.30,0.30" << std::endl;
    std::cout << "A1_DEPTH_END frame=" << depth_frame_index << std::endl;
}
```

- [ ] **Step 5: Add `depth_snapshot` handler**

Add global variable near other globals:

```cpp
uint64_t g_depth_frame_index = 0;
```

Inside `handle_a1_test_command()` after `test_echo`, add:

```cpp
if (command == "depth_snapshot") {
    emit_depth_frame(++g_depth_frame_index);
    print_debug_response("depth_snapshot", "\"message\":\"depth frame emitted\",\"frame\":" + std::to_string(g_depth_frame_index));
    return;
}
```

- [ ] **Step 6: Add automatic depth emission**

In `main()`, after `last_summary_log`, add:

```cpp
auto last_depth_emit = std::chrono::steady_clock::now() - std::chrono::milliseconds(kDepthAutoIntervalMs);
```

Inside main loop after `send_velocity_if_changed(...)`, add:

```cpp
if (now - last_depth_emit >= std::chrono::milliseconds(kDepthAutoIntervalMs)) {
    emit_depth_frame(++g_depth_frame_index);
    last_depth_emit = now;
}
```

If `now` currently declared later, move `const auto now = std::chrono::steady_clock::now();` before both depth and summary sections so one timestamp is reused.

- [ ] **Step 7: Verify C++ syntax via incremental build or compile command**

Preferred if container available:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: app build succeeds and output package updates.

If container unavailable, run at least:

```bash
python -m py_compile tools/aurora/serial_terminal.py
bash -n "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/scripts/run.sh"
```

Expected: both pass; note C++ compile not verified locally.

## Task 4: Aurora Qt frame cache jitter fix

**Files:**
- Modify: `tools/aurora/qt_camera_bridge.py:199-206,600-625,711-718`
- Modify: `tools/aurora/aurora_companion.py:2064-2088` only if needed after bridge change.

- [ ] **Step 1: Track JPEG sequence per mode**

In `CameraBridgeState.__init__`, after `latest_gray_jpeg`, add:

```python
self.latest_color_jpeg_seq = 0
self.latest_gray_jpeg_seq = 0
```

- [ ] **Step 2: Update sequence when encoding new frame**

In `_on_video_frame_changed`, after `self.latest_color_jpeg = color_jpeg` and `self.latest_gray_jpeg = gray_jpeg`, add:

```python
if color_jpeg:
    self.latest_color_jpeg_seq = self.frame_count + 1
if gray_jpeg:
    self.latest_gray_jpeg_seq = self.frame_count + 1
```

- [ ] **Step 3: Add frame_bytes sequence-aware behavior**

Replace `frame_bytes()` with:

```python
def frame_bytes(self, mode: str = "color") -> Optional[bytes]:
    with self.lock:
        cached = self.latest_gray_jpeg if mode == "gray" else self.latest_color_jpeg
        cached_seq = self.latest_gray_jpeg_seq if mode == "gray" else self.latest_color_jpeg_seq
        frame_seq = self.frame_count
        image = self.latest_image.copy() if self.latest_image is not None else None
    if cached and cached_seq == frame_seq:
        return cached
    if image is None or image.isNull():
        return None
    now = time.time()
    try:
        encoded = self._jpeg_bytes(image, grayscale=(mode == "gray"))
        with self.lock:
            if mode == "gray":
                self.latest_gray_jpeg = encoded
                self.latest_gray_jpeg_seq = self.frame_count
            else:
                self.latest_color_jpeg = encoded
                self.latest_color_jpeg_seq = self.frame_count
        return encoded
    except Exception as exc:
        if now - self.last_encode_error_ts >= 5.0:
            self.last_encode_error_ts = now
            print(f"[WARN] Qt frame encode failed: {exc}")
        return None
```

- [ ] **Step 4: Add no-cache timestamp header**

In `_write_bytes()`, after existing cache headers, add:

```python
self.send_header("Last-Modified", "0")
```

- [ ] **Step 5: Verify Python syntax**

Run:

```bash
python -m py_compile tools/aurora/qt_camera_bridge.py tools/aurora/aurora_companion.py
```

Expected: no output, exit 0.

## Task 5: End-to-end checks

**Files:**
- No new files.

- [ ] **Step 1: Run Windows tool compile check**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py
```

Expected: no output, exit 0.

- [ ] **Step 2: Run shell check**

Run:

```bash
bash -n "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/scripts/run.sh"
```

Expected: no output, exit 0.

- [ ] **Step 3: Build app in Docker**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: app-only build succeeds. If it fails because no prior full SDK build cache exists, run full build only after user approval:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

- [ ] **Step 4: Verify target model path after build**

Run:

```bash
docker exec A1_Builder bash -lc "test -f /app/data/A1_SDK_SC132GS/smartsens_sdk/output/target/app_demo/app_assets/models/1cfd4504-c065-4698-9554-9e114f5bfd47_best.m1model || test -f /app/data/A1_SDK_SC132GS/smartsens_sdk/output/target/app_demo/app_assets/models/test.m1model"
```

Expected: exit 0.

- [ ] **Step 5: Manual board/Aurora verification**

After flashing/deploying build:

1. Board log prints `[APP] model=...` and no `missing A1 5-class model`.
2. Board stdout shows `A1_DEPTH_BEGIN`, `A1_DEPTH_CHUNK`, `A1_DEPTH_END` once per second.
3. Aurora “A1 深度图链路” leaves `无帧` and shows changing heatmap.
4. Aurora “电脑-A1-STM32 联通测试” returns `A1_TEST 已响应`.
5. Aurora “A1 调试按钮 / 前进” returns success and STM32 receives forward velocity.
6. Aurora “A1 调试按钮 / 停止” returns success and STM32 receives stop velocity.
7. Aurora serial terminal `test_echo` button returns `测试回传成功`.
8. Preview stream no longer reuses old cached frame when source changes or camera reconnects.

## Self-review

Spec coverage:
- Fake auto depth stream: Task 3.
- Manual depth snapshot: Task 3 Step 5 and Task 2 Aurora map.
- Aurora command compatibility: Task 2.
- Missing model startup error: Task 1 and Task 5.
- Preview cache tearing/jitter: Task 4 and Task 5.

Placeholder scan: no TBD/TODO/fill-later language.

Type consistency:
- Board command names match Aurora map: `ping`, `test_echo`, `chassis_test`, `rps_snapshot`, `depth_snapshot`.
- Depth protocol fields match existing Aurora parser: `frame`, `w`, `h`, `fmt=u8`, `encoding=base64`, `chunks`, `bytes`, `index`, `data`.
