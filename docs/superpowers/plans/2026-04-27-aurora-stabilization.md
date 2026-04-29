# Aurora 稳定化与调试前端重绘 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `tools/aurora/launch.ps1` 启动后的 A1 相机链路可诊断且尽量稳定直出画面，修复 COM13 终端乱码/错行，并把 Aurora Companion 重绘为偏商用品质的调试前端。

**Architecture:** 保持现有 Flask + PySide6 + 单文件 HTML 架构不变，在 `aurora_companion.py` 与 `qt_camera_bridge.py` 中补充可观测状态和更明确的错误分类，在 `serial_terminal.py` 与前端终端渲染链路中统一换行/文本处理，再在 `companion_ui.html` 上重组信息架构和视觉系统。所有变更都优先做成小步、可验证、可回退的增量修改。

**Tech Stack:** Python 3.9, Flask, PySide6/QtMultimedia, pyserial, OpenCV, HTML/CSS/vanilla JavaScript, unittest, Playwright browser checks

---

## File Map

- Modify: `tools/aurora/launch.ps1`
  - 启动参数、桥接等待提示、必要时的初始化提示文案。
- Modify: `tools/aurora/aurora_companion.py`
  - Qt bridge 启动/状态请求、A1 打开失败分类、状态接口、前端诊断数据。
- Modify: `tools/aurora/qt_camera_bridge.py`
  - 相机打开状态、首帧等待状态、错误原因、状态接口补充。
- Modify: `tools/aurora/serial_terminal.py`
  - 串口行切分、控制字符清洗、部分行输出策略、日志条目结构。
- Modify: `tools/aurora/relay_comm.py`
  - 复用串口状态的展示字段，必要时透传更多诊断信息。
- Modify: `tools/aurora/templates/companion_ui.html`
  - 商用调试台布局、相机诊断面板、终端渲染、深色视觉系统。
- Create: `tools/aurora/tests/test_camera_diagnostics.py`
  - 针对可抽离的相机状态/错误分类辅助函数做单元测试。
- Create: `tools/aurora/tests/test_serial_terminal.py`
  - 针对换行、编码、控制字符和 partial 行输出策略做单元测试。
- Create: `tools/aurora/tests/__init__.py`
  - 使 `unittest discover` 能稳定发现测试。

## Task 1: 为 A1 相机链路建立可测试的诊断分类

**Files:**
- Modify: `tools/aurora/aurora_companion.py:511-688,1766-1928`
- Modify: `tools/aurora/qt_camera_bridge.py:475-656`
- Create: `tools/aurora/tests/test_camera_diagnostics.py`
- Create: `tools/aurora/tests/__init__.py`

- [ ] **Step 1: 写失败测试，定义相机错误分类与状态摘要接口**

```python
# tools/aurora/tests/test_camera_diagnostics.py
import unittest

from aurora_companion import classify_qt_bridge_failure, summarize_qt_bridge_status


class CameraDiagnosticsTests(unittest.TestCase):
    def test_classifies_no_frame_after_switch_as_timeout(self):
        result = classify_qt_bridge_failure(
            status={
                "connected": True,
                "device_name": "Smartsens-FlyingChip-A1-1",
                "frame_count": 0,
                "last_frame_ts": 0.0,
                "message": "Qt 相机桥已连接",
            },
            error_text="Qt 相机桥已切换到 Smartsens-FlyingChip-A1-1，但 5 秒内未收到视频帧",
        )
        self.assertEqual(result["code"], "no_frame_after_switch")
        self.assertEqual(result["severity"], "error")

    def test_classifies_missing_device(self):
        result = classify_qt_bridge_failure(
            status={"connected": False, "device_name": "", "frame_count": 0},
            error_text="Qt 相机桥未找到设备 0",
        )
        self.assertEqual(result["code"], "device_not_found")

    def test_summarizes_waiting_for_first_frame(self):
        summary = summarize_qt_bridge_status({
            "available": True,
            "connected": True,
            "device_name": "Smartsens-FlyingChip-A1-1",
            "frame_count": 0,
            "last_frame_ts": 0.0,
            "message": "Qt 相机桥已连接: Smartsens-FlyingChip-A1-1 / 1280x720 / YUYV",
        })
        self.assertEqual(summary["state"], "waiting_for_first_frame")
        self.assertIn("首帧", summary["detail"])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 运行测试，确认现在失败**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
```

Expected: `ImportError` 或 `cannot import name 'classify_qt_bridge_failure'` / `summarize_qt_bridge_status`

- [ ] **Step 3: 在 `aurora_companion.py` 中加入最小可测辅助函数**

```python
# aurora_companion.py

def classify_qt_bridge_failure(status: Optional[dict], error_text: str) -> Dict[str, str]:
    text = str(error_text or "").lower()
    status = status or {}
    if "未找到设备" in error_text:
        return {"code": "device_not_found", "severity": "error", "hint": "请检查 A1 设备是否已枚举。"}
    if "5 秒内未收到视频帧" in error_text:
        return {"code": "no_frame_after_switch", "severity": "error", "hint": "Qt bridge 已切换成功，但首帧未到达。"}
    if "不可用" in error_text:
        return {"code": "bridge_unavailable", "severity": "error", "hint": "请检查 PySide6 / QtMultimedia 环境。"}
    if status.get("connected") and not int(status.get("frame_count") or 0):
        return {"code": "waiting_for_first_frame", "severity": "warn", "hint": "相机已连接，正在等待首帧。"}
    return {"code": "unknown", "severity": "error", "hint": error_text or "未知错误"}


def summarize_qt_bridge_status(status: Optional[dict]) -> Dict[str, str]:
    payload = status or {}
    if not payload.get("available", False):
        return {"state": "bridge_unavailable", "detail": payload.get("error") or "Qt bridge 不可用"}
    if payload.get("connected") and int(payload.get("frame_count") or 0) <= 0:
        return {"state": "waiting_for_first_frame", "detail": "Qt bridge 已连接，等待首帧到达"}
    if payload.get("connected"):
        return {"state": "streaming", "detail": payload.get("message") or "视频流工作中"}
    return {"state": "idle", "detail": payload.get("message") or "Qt bridge 未连接相机"}
```

- [ ] **Step 4: 在 `QtBridgeCapture._open()` 与 `/status` 响应里接入这些字段**

```python
# aurora_companion.py inside QtBridgeCapture._open()
self._last_status = payload.get("status", {}) or {}
if not _qt_bridge_wait_for_frame(mode="color", timeout=5.0):
    classification = classify_qt_bridge_failure(
        self._last_status,
        f"Qt 相机桥已切换到 {self._last_status.get('device_name') or f'device {self.device_id}'}，但 5 秒内未收到视频帧",
    )
    detail = classification.get("hint") or classification.get("code")
    raise RuntimeError(f"{classification['code']}: {detail}")

# aurora_companion.py inside status()
qt_status = _qt_bridge_status(timeout=0.3) or {}
qt_summary = summarize_qt_bridge_status(qt_status)
return jsonify({
    ...,
    "qt_bridge": qt_status,
    "qt_bridge_summary": qt_summary,
})
```

- [ ] **Step 5: 在 `qt_camera_bridge.py` 中补充首帧等待相关状态字段**

```python
# qt_camera_bridge.py in CameraBridgeState.__init__
self.last_open_attempt_ts = 0.0
self.last_open_error = ""
self.waiting_for_first_frame = False

# qt_camera_bridge.py in _open_camera()
self.last_open_attempt_ts = time.time()
self.last_open_error = ""
self.waiting_for_first_frame = True
...
self.connected = True
self.status_message = f"Qt 相机桥已连接: {self.device_name} / {self.frame_width}x{self.frame_height} / {self.pixel_format}"

# qt_camera_bridge.py in _on_frame()
if self.waiting_for_first_frame:
    self.waiting_for_first_frame = False
    self.status_message = f"Qt 相机桥首帧已到达: {self.device_name}"

# qt_camera_bridge.py in exception path of _open_camera()
self.last_open_error = str(last_error or "")
self.waiting_for_first_frame = False
```

- [ ] **Step 6: 运行测试，确认通过**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
```

Expected: `OK`

- [ ] **Step 7: 提交这一小步**

```bash
git add tools/aurora/aurora_companion.py tools/aurora/qt_camera_bridge.py tools/aurora/tests/test_camera_diagnostics.py tools/aurora/tests/__init__.py
git commit -m "test: add Aurora camera diagnostics coverage"
```

## Task 2: 让 launch 到 Qt bridge 的根因调查结果直接暴露到页面

**Files:**
- Modify: `tools/aurora/launch.ps1:34-187`
- Modify: `tools/aurora/aurora_companion.py:511-576,1766-1901,1901-1928`
- Modify: `tools/aurora/templates/companion_ui.html:1432-1561,2825-2927`

- [ ] **Step 1: 写失败测试，锁定状态摘要里必须暴露的页面诊断字段**

```python
# append to tools/aurora/tests/test_camera_diagnostics.py
    def test_status_summary_exposes_operator_hint(self):
        summary = summarize_qt_bridge_status({
            "available": True,
            "connected": False,
            "message": "Qt 相机桥未连接相机",
        })
        self.assertIn("detail", summary)
        self.assertIn("state", summary)
```

- [ ] **Step 2: 运行测试，确认现在失败或不完整**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
```

Expected: summary 内容不满足新增断言，或状态字段缺少实际页面所需信息

- [ ] **Step 3: 在 `aurora_companion.py` 中返回页面可直接消费的诊断块**

```python
# aurora_companion.py inside status()
qt_status = _qt_bridge_status(timeout=0.3) or {}
qt_summary = summarize_qt_bridge_status(qt_status)
qt_diagnostics = {
    "state": qt_summary["state"],
    "detail": qt_summary["detail"],
    "device_name": qt_status.get("device_name") or "",
    "pixel_format": qt_status.get("pixel_format") or "",
    "frame_count": int(qt_status.get("frame_count") or 0),
    "waiting_for_first_frame": bool(qt_status.get("waiting_for_first_frame")),
    "last_open_error": qt_status.get("last_open_error") or "",
}
return jsonify({
    ...,
    "qt_bridge": qt_status,
    "qt_bridge_summary": qt_summary,
    "qt_diagnostics": qt_diagnostics,
})
```

- [ ] **Step 4: 在页面顶部加入单独诊断区并消费这些字段**

```html
<!-- companion_ui.html near top status area -->
<div class="diag-strip" id="cameraDiagStrip">
  <div class="diag-title">A1 相机诊断</div>
  <div class="diag-state" id="cameraDiagState">等待状态…</div>
  <div class="diag-meta" id="cameraDiagMeta">Qt bridge 未返回诊断数据</div>
</div>
```

```javascript
function renderCameraDiagnostics(diag = {}) {
  const stateEl = document.getElementById('cameraDiagState');
  const metaEl = document.getElementById('cameraDiagMeta');
  if (!stateEl || !metaEl) return;
  stateEl.textContent = diag.detail || '未获取到诊断信息';
  const parts = [];
  if (diag.device_name) parts.push(diag.device_name);
  if (diag.pixel_format) parts.push(diag.pixel_format);
  parts.push(`frame_count=${diag.frame_count || 0}`);
  if (diag.last_open_error) parts.push(diag.last_open_error);
  metaEl.textContent = parts.join(' · ');
}
```

- [ ] **Step 5: 在轮询逻辑里接入诊断渲染，并在启动脚本里统一入口文案**

```javascript
// companion_ui.html in /status poll handler
if (d.qt_diagnostics) {
  renderCameraDiagnostics(d.qt_diagnostics);
}
```

```powershell
# launch.ps1 before starting python
Write-Host "[Aurora] Starting Companion on http://127.0.0.1:$ResolvedPort"
Write-Host "[Aurora] If A1 preview does not appear, use the on-page A1 camera diagnostics first."
```

- [ ] **Step 6: 运行测试并做一次低成本页面检查**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/qt_camera_bridge.py
```

Expected: `OK` and no syntax errors

- [ ] **Step 7: 提交这一小步**

```bash
git add tools/aurora/launch.ps1 tools/aurora/aurora_companion.py tools/aurora/templates/companion_ui.html
git commit -m "feat: expose Aurora camera diagnostics"
```

## Task 3: 用测试先修复 COM13 终端换行、乱码与 partial 行渲染

**Files:**
- Modify: `tools/aurora/serial_terminal.py:82-170,198-247`
- Modify: `tools/aurora/templates/companion_ui.html:2661-2821`
- Create: `tools/aurora/tests/test_serial_terminal.py`

- [ ] **Step 1: 写失败测试，覆盖换行、控制字符和 partial 行策略**

```python
# tools/aurora/tests/test_serial_terminal.py
import unittest

import serial_terminal as st


class SerialTerminalTests(unittest.TestCase):
    def test_normalize_text_removes_carriage_return_and_ansi(self):
        text = st._normalize_text(b'\x1b[32mOK\x1b[0m\r\n')
        self.assertEqual(text, 'OK\n')

    def test_pop_complete_lines_handles_crlf_and_lone_cr(self):
        st._rx_buffer = bytearray(b'first\r\nsecond\rthird\n')
        lines = st._pop_complete_lines()
        self.assertEqual(lines, [b'first', b'second', b'third'])

    def test_flush_partial_buffer_keeps_short_fragment_buffered(self):
        st._rx_buffer = bytearray('半包'.encode('utf-8'))
        before = bytes(st._rx_buffer)
        st._flush_partial_buffer(force=True)
        self.assertEqual(bytes(st._rx_buffer), before)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: 运行测试，确认现在失败**

Run:
```bash
python -m unittest tools.aurora.tests.test_serial_terminal -v
```

Expected: ANSI 不会被清掉，或 partial 行策略与断言不一致

- [ ] **Step 3: 在 `serial_terminal.py` 中最小修复文本规范化与 partial 行逻辑**

```python
# serial_terminal.py
import re

_ANSI_RE = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')


def _clean_terminal_text(text: str) -> str:
    text = _ANSI_RE.sub('', text)
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    return text


def _normalize_text(raw: bytes) -> str:
    if not raw:
        return ""
    for encoding in ("utf-8", "gb18030"):
        try:
            return _clean_terminal_text(raw.decode(encoding, errors="strict"))
        except UnicodeDecodeError:
            continue
    return _clean_terminal_text(raw.decode("utf-8", errors="replace"))
```

```python
# serial_terminal.py in _flush_partial_buffer()
if force:
    safe_raw, tail = _split_decodable_prefix(bytes(_rx_buffer))
    if not safe_raw or len(safe_raw) < 8:
        return
    text = _normalize_text(safe_raw).strip('\n')
    if text:
        _append_rx_entry(safe_raw, text, partial=True)
    _rx_buffer = bytearray(tail)
    return
```

- [ ] **Step 4: 修正前端终端渲染，避免 innerHTML 把日志内容当 HTML 拼装**

```javascript
function escapeLogText(value) {
  return String(value || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function renderSerialTermLines(lines = []) {
  const area = document.getElementById('serialTermRxArea');
  if (!area) return;
  if (!lines.length) {
    area.innerHTML = '<div class="log-empty">暂无串口输出</div>';
    return;
  }
  area.innerHTML = lines.slice(0, 24).map(item => {
    const text = escapeLogText(item.text || item.hex || '');
    const partial = item.partial ? ' · partial' : '';
    return `<div class="terminal-line"><span class="log-ts text-muted">${item.ts || '--:--:--'}${partial}</span><pre class="terminal-text">${text}</pre></div>`;
  }).join('');
}
```

- [ ] **Step 5: 运行测试确认通过**

Run:
```bash
python -m unittest tools.aurora.tests.test_serial_terminal -v
```

Expected: `OK`

- [ ] **Step 6: 做串口模块语法检查**

Run:
```bash
python -m py_compile tools/aurora/serial_terminal.py tools/aurora/relay_comm.py
```

Expected: no output

- [ ] **Step 7: 提交这一小步**

```bash
git add tools/aurora/serial_terminal.py tools/aurora/templates/companion_ui.html tools/aurora/tests/test_serial_terminal.py
git commit -m "fix: normalize Aurora COM13 terminal output"
```

## Task 4: 把 Companion 页面重组为商用品质调试控制台

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html:1-2929`

- [ ] **Step 1: 写失败测试，用最小页面结构断言锁定新布局骨架**

```python
# append to tools/aurora/tests/test_camera_diagnostics.py
from pathlib import Path

    def test_frontend_contains_console_shell_regions(self):
        html = Path('tools/aurora/templates/companion_ui.html').read_text(encoding='utf-8')
        self.assertIn('console-shell', html)
        self.assertIn('console-sidebar', html)
        self.assertIn('console-main', html)
        self.assertIn('console-terminal-panel', html)
```

- [ ] **Step 2: 运行测试，确认现在失败**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
```

Expected: `AssertionError: 'console-shell' not found`

- [ ] **Step 3: 先重组 HTML 结构，不改动已有 DOM id 的功能入口**

```html
<div class="console-shell">
  <aside class="console-sidebar">
    <section class="console-panel console-preview-panel">
      <div class="panel-title-row">
        <h2>视频与诊断</h2>
        <span id="camStatus" class="status-badge">未连接</span>
      </div>
      <div class="diag-strip" id="cameraDiagStrip">
        <div class="diag-title">A1 相机诊断</div>
        <div class="diag-state" id="cameraDiagState">等待状态…</div>
        <div class="diag-meta" id="cameraDiagMeta">Qt bridge 未返回诊断数据</div>
      </div>
      <img id="stream" src="/video_feed" alt="Aurora stream">
    </section>
  </aside>
  <main class="console-main">
    <section class="console-panel console-control-panel">
      <!-- 保留 serialTermPortSelect / relayPortSelect / move controls / telemetry ids -->
    </section>
    <section class="console-panel console-terminal-panel">
      <div id="serialTermRxArea" class="log-area input-panel-tall"></div>
      <div id="txLogArea" class="log-area"></div>
      <div id="rxLogArea" class="log-area"></div>
    </section>
  </main>
</div>
```

- [ ] **Step 4: 再替换视觉系统为深色工业控制台风格**

```css
:root {
  --color-bg: #0b1220;
  --color-surface: #121a2b;
  --color-surface-2: #182235;
  --color-border: #24324a;
  --color-text: #dbe6f7;
  --color-muted: #8ea3c0;
  --color-brand: #4da3ff;
  --color-success: #35c37a;
  --color-warning: #ffb347;
  --color-error: #ff6b6b;
  --shadow-panel: 0 18px 48px rgba(0, 0, 0, 0.35);
}
body {
  background: radial-gradient(circle at top, #13213a 0%, #0b1220 45%, #08101b 100%);
  color: var(--color-text);
}
.console-panel {
  background: linear-gradient(180deg, rgba(24, 34, 53, 0.96), rgba(14, 21, 34, 0.96));
  border: 1px solid var(--color-border);
  box-shadow: var(--shadow-panel);
}
```

- [ ] **Step 5: 运行结构测试和语法校验**

Run:
```bash
python -m unittest tools.aurora.tests.test_camera_diagnostics -v
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py
```

Expected: `OK` and no syntax errors

- [ ] **Step 6: 提交这一小步**

```bash
git add tools/aurora/templates/companion_ui.html tools/aurora/tests/test_camera_diagnostics.py
git commit -m "feat: restyle Aurora as debug console"
```

## Task 5: 实机联调 launch、页面与浏览器验证

**Files:**
- Modify: `tools/aurora/launch.ps1`（仅在联调发现明确问题时）
- Modify: `tools/aurora/aurora_companion.py`（仅在联调发现明确问题时）
- Modify: `tools/aurora/templates/companion_ui.html`（仅在联调发现明确问题时）

- [ ] **Step 1: 先跑本地单元测试，确认基线全绿**

Run:
```bash
python -m unittest discover -s tools/aurora/tests -v
```

Expected: all tests pass

- [ ] **Step 2: 做 Python 语法检查**

Run:
```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py
```

Expected: no output

- [ ] **Step 3: 启动 Aurora Companion 并观察启动日志**

Run:
```bash
powershell.exe -ExecutionPolicy Bypass -File tools/aurora/launch.ps1 -Source a1
```

Expected: 打印 Companion URL、Qt bridge 启动信息；若失败，日志里应明确出现 `device_not_found` / `no_frame_after_switch` / `bridge_unavailable` 等分类信号，而不是只有笼统报错

- [ ] **Step 4: 用浏览器检查页面首屏与相机诊断区**

Use Playwright to verify:
```text
- 打开 http://127.0.0.1:5801
- 检查 console-shell / console-sidebar / console-main 是否存在
- 检查 A1 相机诊断条是否渲染 detail / meta
- 检查 serialTermRxArea、txLogArea、rxLogArea 可见
```

Expected: 页面成功加载，无主要布局错位，诊断区显示当前 bridge 状态

- [ ] **Step 5: 验证串口终端与 COM13 页面显示**

Manual/browser checks:
```text
- 若有 COM13 设备，点击“自动连接”或“连接终端”
- 发送 help / status / A1_TEST debug_status
- 观察 serialTermRxArea 是否无重复空行、无错误 HTML 断行、中文与 JSON 是否可读
```

Expected: 日志连续、无系统性乱码；如果设备不可用，也能显示明确错误提示

- [ ] **Step 6: 若联调暴露最后一处小问题，只做单一最小修复并复跑验证**

```bash
python -m unittest discover -s tools/aurora/tests -v
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py
```

Expected: 仍然全绿

- [ ] **Step 7: 提交联调收尾修改**

```bash
git add tools/aurora/launch.ps1 tools/aurora/aurora_companion.py tools/aurora/qt_camera_bridge.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/templates/companion_ui.html tools/aurora/tests
git commit -m "fix: stabilize Aurora camera and terminal workflows"
```

## Spec Coverage Check

- A1 相机根因调查：Task 1、Task 2、Task 5
- `launch.ps1` 优先直接可用：Task 2、Task 5
- COM13 错行/乱码修复：Task 3、Task 5
- 商用品质调试前端：Task 4、Task 5
- 本地实际页面查看与验证：Task 5

## Placeholder Scan

- 无 `TBD` / `TODO` / “implement later” 占位。
- 每个测试步骤都给出明确命令和预期。
- 每个代码步骤都给出实际代码骨架，而不是泛泛描述。

## Type Consistency Check

- 相机诊断统一使用 `qt_bridge` / `qt_bridge_summary` / `qt_diagnostics`。
- 前端诊断渲染统一使用 `renderCameraDiagnostics()`。
- 串口终端测试与实现统一围绕 `_normalize_text()`、`_pop_complete_lines()`、`_flush_partial_buffer()`。
