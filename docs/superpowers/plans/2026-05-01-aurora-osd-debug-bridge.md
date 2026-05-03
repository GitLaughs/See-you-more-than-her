# Aurora OSD Debug Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a horizontal official-style Aurora OSD preview, restore board-side demo-rps OSD behavior, and expose safe Chinese debug buttons that send `A1_TEST` commands and show board responses.

**Architecture:** Keep Aurora's existing Flask + Qt camera bridge + serial terminal architecture. Add a small serial debug command layer on the host, a small board debug/runtime status layer on the A1 app, and reshape the UI around a `1920x1080` preview plus debug panel. Board OSD remains the source of truth; Aurora preview is only a visual aid.

**Tech Stack:** Python 3 Flask, PySerial, HTML/CSS/vanilla JS, C++17-ish board app, SmartSens SSNE/OSD APIs, Docker build scripts.

---

## File structure

- Modify `tools/aurora/serial_terminal.py`
  - Add safe A1 debug command formatting and route support.
  - Keep existing manual serial terminal behavior.
- Modify `tools/aurora/templates/companion_ui.html`
  - Rework layout into `1920x1080` OSD preview + Chinese debug panel.
  - Keep existing camera/capture/model/advanced terminal controls.
- Modify `tools/aurora/aurora_companion.py`
  - Expose any UI data needed by the template/status response; avoid camera lifecycle rewrites.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
  - Add fields/methods to expose last texture add/flush status.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`
  - Record and print texture draw return values.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
  - Expose OSD status from `VISUALIZER`.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`
  - Add OSD init/draw status and diagnostics.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Restore official demo-rps visual state flow while keeping P/R/S chassis control.
  - Parse debug commands from stdin through existing listener thread.
- Optional modify `tools/aurora/tests/test_aurora_startup.py`
  - Add lightweight tests for serial command formatting if current test harness can import `serial_terminal.py` without hardware.

## Task 1: Add host-side safe A1 debug command helpers

**Files:**
- Modify: `tools/aurora/serial_terminal.py:376-616`
- Test: `python -m py_compile tools/aurora/serial_terminal.py`

- [ ] **Step 1: Add command constants near `_DEFAULT_DESC_HINTS`**

Add this after `_DEFAULT_DESC_HINTS`:

```python
_A1_DEBUG_PREFIX = "A1_TEST"
_A1_DEBUG_WAIT_PREFIX = "A1_DEBUG"
_A1_DEBUG_COMMANDS = {
    "ping": "ping",
    "osd_status": "osd_status",
    "uart_status": "uart_status",
    "chassis_stop": "chassis_test stop",
    "chassis_forward": "chassis_test forward",
    "chassis_backward": "chassis_test backward",
}
```

- [ ] **Step 2: Add command builder helpers before `send_text_line`**

Insert before `def send_text_line(...)`:

```python
def build_a1_debug_line(command_key: str) -> Dict[str, Any]:
    key = str(command_key or "").strip()
    command = _A1_DEBUG_COMMANDS.get(key)
    if command is None:
        return {"success": False, "error": f"不支持的调试命令: {key}"}
    return {
        "success": True,
        "key": key,
        "command": command,
        "line": f"{_A1_DEBUG_PREFIX} {command}",
        "wait_tokens": [
            _A1_DEBUG_WAIT_PREFIX,
            f'"command":"{command.split()[0]}"',
        ],
    }
```

- [ ] **Step 3: Add `/a1_debug` route after `send_test` route**

Insert after `def send_test()` block:

```python
@serial_term_bp.route("/a1_debug", methods=["POST"])
def a1_debug():
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装"})
    data = request.get_json(silent=True) or {}
    built = build_a1_debug_line(str(data.get("command") or ""))
    if not built.get("success"):
        return jsonify(built)

    timeout_sec = max(0.3, float(data.get("timeout_sec") or 2.5))
    ready = ensure_connected()
    if not ready.get("success"):
        return jsonify(ready)

    start_seq = _rx_seq
    result = _send_payload((built["line"] + "\r\n").encode("utf-8"), built["line"], hex_mode=False)
    if not result.get("success"):
        return jsonify(result)

    waited = _wait_for_text(list(built["wait_tokens"]), timeout_sec=timeout_sec, after_seq=start_seq)
    response_received = bool(waited.get("success"))
    return jsonify({
        **result,
        "success": bool(result.get("success")) and response_received,
        "transport_success": bool(result.get("success")),
        "response_received": response_received,
        "command": built["command"],
        "key": built["key"],
        "matched": waited.get("matched"),
        "message": waited.get("matched", {}).get("text", "") if response_received else waited.get("error", ""),
    })
```

- [ ] **Step 4: Keep existing `/send_test` behavior**

Do not delete `send_test()`. Existing page code and tests may still use it.

- [ ] **Step 5: Run syntax check**

Run:

```bash
python -m py_compile tools/aurora/serial_terminal.py
```

Expected: no output and exit code 0.

- [ ] **Step 6: Commit**

```bash
git add tools/aurora/serial_terminal.py
git commit -m "feat: add Aurora A1 debug command bridge"
```

## Task 2: Rework Aurora page into horizontal OSD/debug workstation

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html`
- Test: `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py`

- [ ] **Step 1: Change page grid and preview styles**

In the `<style>` block, replace the `.shell` and `.preview` rules with this shape while keeping existing colors/buttons/log classes:

```css
.shell{display:grid;grid-template-columns:minmax(640px,1.45fr) minmax(380px,.85fr);gap:14px;padding:14px}.stack{display:flex;flex-direction:column;gap:14px}.preview{position:relative;background:#020617;border-radius:10px;overflow:hidden;max-height:72vh;aspect-ratio:16/9;border:1px solid #17324d}.preview img{position:absolute;left:7.8%;top:13.5%;width:28.2%;height:35.2%;object-fit:cover;display:block;filter:grayscale(1);z-index:1}.osd-frame{position:absolute;inset:0;z-index:2;pointer-events:none;background:radial-gradient(circle at 63% 25%,rgba(35,220,255,.18),transparent 10%),linear-gradient(135deg,#020617,#071324 60%,#020617)}.osd-hole{position:absolute;left:7.8%;top:13.5%;width:28.2%;height:35.2%;border:3px solid #25e6ff;box-shadow:0 0 18px rgba(37,230,255,.7);z-index:3;pointer-events:none}.osd-corner{position:absolute;border-color:#20e7ff;border-style:solid;opacity:.8}.osd-corner.a{left:0;top:0;width:33%;height:20%;border-width:3px 0 0 3px}.osd-corner.b{right:0;bottom:0;width:38%;height:22%;border-width:0 3px 3px 0}.fps{position:absolute;right:8px;top:8px;background:rgba(255,255,255,.9);border:1px solid var(--line);border-radius:8px;padding:3px 8px;font:12px/1.4 monospace;color:var(--muted);z-index:5}@media(max-width:1120px){.shell{grid-template-columns:1fr}.preview{max-height:60vh}}
```

- [ ] **Step 2: Replace the preview card title and body**

Replace the current live preview card section with:

```html
<div class="card">
  <div class="head"><span>官方 OSD 横屏预览</span><div class="row" style="margin:0"><button class="btn" onclick="setStream('preview')">预览</button><button class="btn" onclick="setStream('detect')">YOLOv8</button></div></div>
  <div class="body">
    <div class="preview">
      <img id="stream" src="/video_feed" alt="camera">
      <div class="osd-frame"></div>
      <div class="osd-hole"></div>
      <div class="osd-corner a"></div>
      <div class="osd-corner b"></div>
      <div class="fps" id="streamInfo">1920x1080 OSD · 摄像头窗口</div>
    </div>
    <p class="muted">此区域按官方 demo-rps 横屏构图显示；真实 OSD 仍以板端输出为准。</p>
  </div>
</div>
```

- [ ] **Step 3: Add Chinese debug panel above advanced terminal**

In the right column, before the existing `A1 串口终端 / CLI` card, add:

```html
<div class="card">
  <div class="head"><span>A1 调试按钮</span><span id="a1DebugBadge" class="pill">等待连接</span></div>
  <div class="body">
    <div class="grid3">
      <button class="btn blue" onclick="sendA1Debug('ping')">连通测试</button>
      <button class="btn" onclick="sendA1Debug('osd_status')">OSD状态</button>
      <button class="btn" onclick="sendA1Debug('uart_status')">串口状态</button>
      <button class="btn green" onclick="sendA1Debug('chassis_forward')">前进测试</button>
      <button class="btn warn" onclick="sendA1Debug('chassis_backward')">后退测试</button>
      <button class="btn red" onclick="sendA1Debug('chassis_stop')">停止</button>
    </div>
    <pre class="pre" id="a1DebugResult">点击按钮后显示 A1_DEBUG 回包。</pre>
  </div>
</div>
```

- [ ] **Step 4: Add JS helper for new route**

Before `function runA1LinkTest()`, add:

```javascript
function sendA1Debug(command){
  const badge=document.getElementById('a1DebugBadge');
  const result=document.getElementById('a1DebugResult');
  badge.textContent='发送中…';
  result.textContent='正在发送 '+command+' …';
  return apiJson('/api/serial_term/a1_debug',{method:'POST',body:JSON.stringify({command,timeout_sec:2.5})}).then(d=>{
    loadSerialTermStatus();
    if(!d.success){
      badge.textContent='失败';
      result.textContent='✗ '+(d.message||d.error||'未收到 A1_DEBUG 回包');
      return;
    }
    badge.textContent='已响应';
    result.textContent=(d.message||JSON.stringify(d,null,2));
  }).catch(e=>{
    badge.textContent='错误';
    result.textContent='✗ 网络错误: '+e;
  });
}
```

- [ ] **Step 5: Update existing link-test copy to use `ping`**

Change the `电脑-A1-STM32 联通测试` card text from `A1_TEST debug_status` to `A1_TEST ping`, and make its button call `sendA1Debug('ping')` or keep `runA1LinkTest()` but update its body to post `{command:'ping'}` to `/api/serial_term/a1_debug`.

- [ ] **Step 6: Run syntax checks**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py
```

Expected: no output and exit code 0.

- [ ] **Step 7: Commit**

```bash
git add tools/aurora/templates/companion_ui.html tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
git commit -m "feat: add Aurora OSD debug workstation"
```

## Task 3: Record board OSD texture status

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`

- [ ] **Step 1: Add status struct in `include/osd-device.hpp`**

Add near namespace/class declarations before `class OsdDevice`:

```cpp
struct TextureDrawStatus {
    int layer_id = -1;
    int pos_x = 0;
    int pos_y = 0;
    int add_ret = 0;
    int flush_ret = 0;
    bool attempted = false;
    bool succeeded = false;
    std::string bitmap_path;
};
```

Ensure the header includes `<string>` if not already present.

- [ ] **Step 2: Add getter and member to `OsdDevice`**

Inside public section of `OsdDevice`, add:

```cpp
TextureDrawStatus LastTextureStatus() const { return m_last_texture_status; }
```

Inside private section, add:

```cpp
TextureDrawStatus m_last_texture_status;
```

- [ ] **Step 3: Record status in `DrawTexture`**

At start of `OsdDevice::DrawTexture`, after `bm_info` fields are set, assign:

```cpp
m_last_texture_status.layer_id = layer_id;
m_last_texture_status.pos_x = pos_x;
m_last_texture_status.pos_y = pos_y;
m_last_texture_status.add_ret = 0;
m_last_texture_status.flush_ret = 0;
m_last_texture_status.attempted = true;
m_last_texture_status.succeeded = false;
m_last_texture_status.bitmap_path = bitmap_path ? bitmap_path : "";
```

After `osd_add_texture_layer`, add:

```cpp
m_last_texture_status.add_ret = ret;
std::cout << "[OsdDevice] osd_add_texture_layer layer=" << layer_id << " ret=" << ret << std::endl;
```

After `osd_flush_texture_layer`, add:

```cpp
m_last_texture_status.flush_ret = ret;
std::cout << "[OsdDevice] osd_flush_texture_layer layer=" << layer_id << " ret=" << ret << std::endl;
if (ret == 0) {
    m_last_texture_status.succeeded = true;
}
```

- [ ] **Step 4: Add `VisualizerStatus` to `include/utils.hpp`**

Before `class VISUALIZER`, add:

```cpp
struct VisualizerStatus {
    int width = 0;
    int height = 0;
    bool initialized = false;
    bool background_drawn = false;
    sst::device::osd::TextureDrawStatus last_texture;
};
```

In public section of `VISUALIZER`, add:

```cpp
VisualizerStatus Status() const;
```

In private section, add:

```cpp
VisualizerStatus m_status;
```

- [ ] **Step 5: Update `VISUALIZER::Initialize` and `DrawBitmap`**

In `src/utils.cpp`, inside `VISUALIZER::Initialize`, set:

```cpp
m_status.width = in_img_shape[0];
m_status.height = in_img_shape[1];
m_status.initialized = true;
std::cout << "[VISUALIZER] Initialize canvas=" << m_status.width << "x" << m_status.height
          << " lut=" << bitmap_lut_path << std::endl;
```

After `osd_device.DrawTexture(...)` in `VISUALIZER::DrawBitmap`, set:

```cpp
m_status.last_texture = osd_device.LastTextureStatus();
if (bitmap_path == "background.ssbmp" && layer_id == 2 && m_status.last_texture.succeeded) {
    m_status.background_drawn = true;
}
std::cout << "[VISUALIZER] DrawBitmap status layer=" << layer_id
          << " add_ret=" << m_status.last_texture.add_ret
          << " flush_ret=" << m_status.last_texture.flush_ret
          << " success=" << (m_status.last_texture.succeeded ? "true" : "false") << std::endl;
```

Add implementation:

```cpp
VisualizerStatus VISUALIZER::Status() const {
    return m_status;
}
```

- [ ] **Step 6: Build-check board app via Docker app-only**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: build completes or fails only because app-only cache is missing. If cache missing, do not fix in this task; continue to later full build verification.

- [ ] **Step 7: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp
git commit -m "feat: record board OSD draw status"
```

## Task 4: Add board debug command handling and restore demo-rps visual flow

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: Add required includes**

At top of `demo_rps_game.cpp`, add:

```cpp
#include <atomic>
#include <sstream>
```

- [ ] **Step 2: Extend `RuntimeState`**

Replace `RuntimeState` with:

```cpp
struct RuntimeState {
    uint64_t frame_index = 0;
    std::string candidate = "NoTarget";
    std::string locked = "NoTarget";
    int candidate_frames = 0;
    bool chassis_ready = false;
    bool background_drawn = false;
    ChassisState last_chassis_state;
    int16_t last_vx = 0;
    int16_t last_vz = 0;
};
```

Add globals in anonymous namespace:

```cpp
std::mutex g_runtime_mtx;
RuntimeState g_runtime;
VISUALIZER* g_visualizer = nullptr;
ChassisController* g_chassis = nullptr;
```

- [ ] **Step 3: Add JSON debug print helper**

Add in anonymous namespace:

```cpp
void print_debug_response(const std::string& command, const std::string& body, bool success = true) {
    std::cout << "A1_DEBUG {\"command\":\"" << command << "\",\"success\":"
              << (success ? "true" : "false") << body << "}" << std::endl;
}
```

- [ ] **Step 4: Add command handler**

Add in anonymous namespace:

```cpp
void handle_a1_test_command(const std::string& line) {
    if (line.rfind("A1_TEST", 0) != 0) {
        return;
    }

    std::istringstream iss(line);
    std::string prefix;
    std::string command;
    iss >> prefix >> command;

    RuntimeState snapshot;
    {
        std::lock_guard<std::mutex> lock(g_runtime_mtx);
        snapshot = g_runtime;
    }

    if (command == "ping") {
        print_debug_response("ping", ",\"pong\":true,\"frame\":" + std::to_string(snapshot.frame_index));
        return;
    }

    if (command == "osd_status") {
        VisualizerStatus status;
        if (g_visualizer != nullptr) {
            status = g_visualizer->Status();
        }
        std::string body = ",\"canvas\":\"" + std::to_string(status.width) + "x" + std::to_string(status.height) + "\""
                         + ",\"initialized\":" + (status.initialized ? std::string("true") : std::string("false"))
                         + ",\"background\":" + (status.background_drawn ? std::string("true") : std::string("false"))
                         + ",\"layer\":" + std::to_string(status.last_texture.layer_id)
                         + ",\"add_ret\":" + std::to_string(status.last_texture.add_ret)
                         + ",\"flush_ret\":" + std::to_string(status.last_texture.flush_ret);
        print_debug_response("osd_status", body);
        return;
    }

    if (command == "uart_status") {
        std::string body = ",\"chassis_ready\":" + (snapshot.chassis_ready ? std::string("true") : std::string("false"))
                         + ",\"tele_vx\":" + std::to_string(snapshot.last_chassis_state.vx)
                         + ",\"volt\":" + std::to_string(snapshot.last_chassis_state.volt)
                         + ",\"last_vx\":" + std::to_string(snapshot.last_vx)
                         + ",\"last_vz\":" + std::to_string(snapshot.last_vz);
        print_debug_response("uart_status", body);
        return;
    }

    if (command == "chassis_test") {
        std::string action;
        iss >> action;
        int16_t vx = 0;
        if (action == "forward") {
            vx = kForwardVx;
        } else if (action == "backward") {
            vx = kBackwardVx;
        } else {
            action = "stop";
        }
        if (g_chassis != nullptr && snapshot.chassis_ready) {
            g_chassis->SendVelocity(vx, 0, 0);
            if (action != "stop") {
                usleep(250000);
                g_chassis->SendVelocity(0, 0, 0);
            }
        }
        print_debug_response("chassis_test", ",\"action\":\"" + action + "\",\"vx\":" + std::to_string(vx) + ",\"chassis_ready\":" + (snapshot.chassis_ready ? std::string("true") : std::string("false")));
        return;
    }

    print_debug_response(command.empty() ? "unknown" : command, ",\"error\":\"unsupported\"", false);
}
```

- [ ] **Step 5: Update `keyboard_listener` to route A1_TEST commands**

Replace its loop body with line-based input:

```cpp
std::string input;
std::cout << "键盘监听线程已启动，输入 'q' 退出程序，输入 A1_TEST <cmd> 调试..." << std::endl;

while (std::getline(std::cin, input)) {
    if (input == "q" || input == "Q") {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_exit_flag = true;
        std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
        break;
    }
    if (input.rfind("A1_TEST", 0) == 0) {
        handle_a1_test_command(input);
    }
}
```

- [ ] **Step 6: Add official demo-rps OSD bitmap table**

In `main()` after model path, add:

```cpp
struct OsdInfo {
    std::string filename;
    int x;
    int y;
};

static OsdInfo osds[] = {
    {"background.ssbmp", 0, 0},
    {"ready.ssbmp", 960, 270},
    {"1.ssbmp", 1080, 270},
    {"2.ssbmp", 1080, 270},
    {"3.ssbmp", 1080, 270},
    {"p.ssbmp", 960, 300},
    {"r.ssbmp", 960, 300},
    {"s.ssbmp", 960, 300},
};
```

- [ ] **Step 7: Wire globals and background status in `main()`**

After creating `visualizer` and `chassis`, add:

```cpp
g_visualizer = &visualizer;
g_chassis = &chassis;
```

Replace background draw line with:

```cpp
visualizer.DrawBitmap(osds[0].filename, "shared_colorLUT.sscl", osds[0].x, osds[0].y, 2);
{
    std::lock_guard<std::mutex> lock(g_runtime_mtx);
    g_runtime.background_drawn = visualizer.Status().background_drawn;
}
```

- [ ] **Step 8: Restore visible ready/RPS status drawing**

Inside the main loop after `locked_label` is computed, add a low-frequency draw section:

```cpp
if (runtime.frame_index % 15 == 0) {
    if (locked_label == "P") {
        visualizer.DrawBitmap(osds[5].filename, "shared_colorLUT.sscl", osds[5].x, osds[5].y, 3);
    } else if (locked_label == "R") {
        visualizer.DrawBitmap(osds[6].filename, "shared_colorLUT.sscl", osds[6].x, osds[6].y, 3);
    } else if (locked_label == "S") {
        visualizer.DrawBitmap(osds[7].filename, "shared_colorLUT.sscl", osds[7].x, osds[7].y, 3);
    } else {
        visualizer.DrawBitmap(osds[1].filename, "shared_colorLUT.sscl", osds[1].x, osds[1].y, 3);
    }
}
```

- [ ] **Step 9: Mirror runtime state to global status**

After telemetry read, add:

```cpp
{
    std::lock_guard<std::mutex> lock(g_runtime_mtx);
    g_runtime = runtime;
    g_runtime.last_chassis_state = chassis_state;
    g_runtime.last_vx = vx;
    g_runtime.last_vz = vz;
    g_runtime.background_drawn = visualizer.Status().background_drawn;
}
```

- [ ] **Step 10: Clear globals on shutdown**

Before `return 0`, add:

```cpp
g_visualizer = nullptr;
g_chassis = nullptr;
```

- [ ] **Step 11: Build-check board app**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: build completes or reports cache issue. If cache issue appears, run final fallback in Task 6.

- [ ] **Step 12: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
git commit -m "feat: add A1 debug status commands"
```

## Task 5: Verify host UI locally

**Files:**
- No code changes expected unless verification finds a bug.

- [ ] **Step 1: Run Python syntax checks**

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py
```

Expected: no output and exit code 0.

- [ ] **Step 2: Start Aurora companion**

Run from repo root in PowerShell if manually verifying:

```powershell
cd tools/aurora
.\launch.ps1
```

Expected: browser opens Aurora Companion.

- [ ] **Step 3: Browser check**

Verify:

- Main preview is horizontal 16:9.
- Camera stream appears inside the OSD camera window region.
- Buttons exist with Chinese labels: `连通测试`, `OSD状态`, `串口状态`, `前进测试`, `后退测试`, `停止`.
- Advanced serial terminal still visible.

- [ ] **Step 4: Commit any verification fixes**

If edits were needed:

```bash
git add tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/templates/companion_ui.html
git commit -m "fix: polish Aurora OSD debug UI"
```

If no edits were needed, skip commit.

## Task 6: Build board firmware artifact

**Files:**
- No code changes expected unless build fails due to code errors.

- [ ] **Step 1: Run app-only EVB build**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected: final output under `output/evb/<timestamp>/`, or cache-related failure.

- [ ] **Step 2: Run fallback full no-ROS build if app-only cache fails**

Run only if Step 1 reports missing SDK/cache artifacts:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
```

Expected: final output under `output/evb/<timestamp>/`.

- [ ] **Step 3: Record build output path in response**

Do not edit docs unless user requests. In final response, report the build output directory printed by build script.

## Self-review checklist

- Spec coverage:
  - Aurora horizontal preview: Task 2.
  - Chinese debug buttons: Task 2.
  - Safe debug bridge: Task 1.
  - Board OSD diagnostics: Task 3.
  - Board debug commands: Task 4.
  - Build verification: Tasks 5 and 6.
- Placeholder scan: no `TBD`, no `TODO`, no unspecified tests.
- Type consistency:
  - Host command keys in Task 1 match UI keys in Task 2.
  - Board debug command names match host command strings.
  - `VisualizerStatus` and `TextureDrawStatus` names match across headers and implementation.
