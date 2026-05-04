# A1 OSD 诊断、灰度框、随机深度与 Aurora 热力图 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让板端能区分“没识别到”与“画框链路坏了”，同时把 OSD 检测框统一成灰度，把板端伪深度改成随机深度，并把 Aurora 深度预览改成彩色热力图，补上 YOLO 结果摘要。

**Architecture:** 板端保留现有 YOLO 推理与 OSD 管线，只在检测输出、可视化输入和 OSD flush 周围加最小诊断日志，并把框颜色固定为灰度。深度链路不改协议和尺寸，只改板端伪深度生成逻辑与 Aurora 前端渲染；YOLO 摘要复用 `serial_terminal.py` 现有 A1_DEPTH 文本通道，`companion_ui.html` 只做展示层改动。

**Tech Stack:** C++17、SmartSens A1 SDK、现有 OSD API、Python 3、Flask、HTML/CSS/JS、OpenCV、numpy

---

### Task 1: 板端 OSD 诊断点 + 灰度框

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: 写最小失败检查点**

在 `utils.cpp` 里把检测框颜色常量抽成单独函数，先保留旧值，但把 `Draw(const std::vector<std::array<float, 4>>& boxes)` 的颜色赋值改成显式调用，方便后面统一换成灰度。

```cpp
namespace {
constexpr int kGrayBoxColor = 1;

int detection_box_color() {
    return kGrayBoxColor;
}
}
```

- [ ] **Step 2: 先跑现状验证**

Run:
```bash
python -m py_compile tools/aurora/serial_terminal.py tools/aurora/aurora_companion.py
```
Expected: 通过，说明前端/串口链路没被板端改动影响。

- [ ] **Step 3: 改最小实现**

在 `utils.cpp` 里把 `q.color = 2;` 和 `DrawFixedSquare()` 里的颜色改成灰度值统一来源，例如：

```cpp
q.color = detection_box_color();
...
osd_device.Draw(square_box,
                0,
                layer_id,
                fdevice::TYPE_SOLID,
                fdevice::TYPE_ALPHA100,
                detection_box_color());
```

在 `demo_rps_game.cpp` 里补三层日志：

```cpp
printf("[YOLOV8] frame=%llu det_count=%zu\n", frame_index, det_result.boxes.size());
if (!det_result.boxes.empty()) {
    printf("[YOLOV8] frame=%llu first_cls=%s score=%.3f box=[%.1f,%.1f,%.1f,%.1f]\n", ...);
}
printf("[YOLOV8] frame=%llu osd_draw_count=%zu\n", frame_index, det_result.boxes.size());
```

如果本轮真正启用 OSD 绘制，再在 flush 前后补：

```cpp
printf("[YOLOV8] frame=%llu osd_flush_begin\n", frame_index);
...
printf("[YOLOV8] frame=%llu osd_flush_end\n", frame_index);
```

- [ ] **Step 4: 跑板端构建验证**

Run:
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"
```
Expected: 通过；日志里能看到新诊断输出编译进应用。

- [ ] **Step 5: 提交前检查**

确认日志只加最小证据，不把大段调试刷屏。`git diff` 里不应出现无关重构。

---

### Task 2: 板端随机深度图输出

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`

- [ ] **Step 1: 写现状保护检查**

保留 `A1_DEPTH_BEGIN / CHUNK / END` 协议不变，只替换深度数组生成逻辑。先在代码里保持尺寸常量不动：

```cpp
constexpr int kDepthWidth = 80;
constexpr int kDepthHeight = 60;
constexpr size_t kDepthBytes = kDepthWidth * kDepthHeight;
```

- [ ] **Step 2: 先跑现状验证**

Run:
```bash
python -m py_compile tools/aurora/serial_terminal.py
```
Expected: 通过，保证深度解析端没问题。

- [ ] **Step 3: 改最小实现**

把 `std::vector<uint8_t> depth(kDepthBytes, 20);` 改成逐像素随机深度，但保持范围稳定、肉眼可见变化。推荐用帧号做种子，避免每次完全一样：

```cpp
std::vector<uint8_t> depth(kDepthBytes, 0);
uint32_t seed = static_cast<uint32_t>(frame_index * 2654435761u);
for (size_t i = 0; i < depth.size(); ++i) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    depth[i] = static_cast<uint8_t>(20 + (seed % 200));
}
```

如果要保留检测框区域的深浅提示，也可以在随机底图上叠加现有 box 深度值，但不改协议。

- [ ] **Step 4: 跑板端构建验证**

Run:
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"
```
Expected: 通过；`A1_DEPTH_*` 文本仍按原格式输出。

- [ ] **Step 5: 复核输出格式**

确认 `A1_DEPTH_BEGIN` 的 `w/h/fmt/encoding/chunks/bytes` 没变，Aurora 端无需改解析。

---

### Task 3: Aurora 深度预览改彩色热力图

**Files:**
- Modify: `tools/aurora/templates/companion_ui.html`
- Modify: `tools/aurora/aurora_companion.py`（仅当现有深度接口需要补字段时）

- [ ] **Step 1: 写前端失败检查点**

先保留 `depthCanvas`、`depthMeta`、`depthObjects` 这些 DOM 节点不变，只换渲染函数。现有代码里这段是灰度直写：

```javascript
for(let i=0,j=0;i<raw.length;i++,j+=4){const v=raw[i];img.data[j]=v;img.data[j+1]=v;img.data[j+2]=v;img.data[j+3]=255}
```

- [ ] **Step 2: 先跑现状验证**

Run:
```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
```
Expected: 通过，保证后台接口没坏。

- [ ] **Step 3: 写最小热力图实现**

把单通道深度映射成彩色渐变，不引入额外库。可先放在 `companion_ui.html` 的 `<script>` 里：

```javascript
function depthToHeatColor(v){
  const t=v/255;
  if(t<0.25) return [0, Math.round(t*4*255), 255];
  if(t<0.5) return [0, 255, Math.round((1-(t-0.25)*4)*255)];
  if(t<0.75) return [Math.round((t-0.5)*4*255), 255, 0];
  return [255, Math.round((1-(t-0.75)*4)*255), 0];
}
```

然后改 `loadDepthLatest()` 里的像素写入：

```javascript
const [r,g,b]=depthToHeatColor(v);
img.data[j]=r; img.data[j+1]=g; img.data[j+2]=b; img.data[j+3]=255;
```

- [ ] **Step 4: 跑前端/后端语法检查**

Run:
```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
```
Expected: PASS。

- [ ] **Step 5: 确认 UI 文字不改乱**

`depthMeta`、`depthBadge`、`depthObjects` 保留，避免联调时找不到深度面板。

---

### Task 4: Aurora A1 识别状态输出 YOLO 摘要

**Files:**
- Modify: `tools/aurora/serial_terminal.py`
- Modify: `tools/aurora/templates/companion_ui.html`
- Modify: `tools/aurora/aurora_companion.py`（如需把摘要接到现有 API 汇总）

- [ ] **Step 1: 写数据结构检查点**

先看现有 `A1_DEPTH_OBJECT` 解析结构，复用同一条文本通道承载 YOLO 摘要，不新增独立协议。现有对象结构是：

```python
return {
    "class": fields.get("cls", "unknown"),
    "score": _parse_depth_float(fields, "score"),
    "bucket": fields.get("bucket", "far"),
    "depth": _parse_depth_float(fields, "depth"),
    "box": box,
}
```

- [ ] **Step 2: 先跑现状验证**

Run:
```bash
python -m py_compile tools/aurora/serial_terminal.py tools/aurora/aurora_companion.py
```
Expected: 通过，说明解析器当前可运行。

- [ ] **Step 3: 扩展串口解析为 YOLO 摘要**

在 `serial_terminal.py` 里给 `_parse_depth_object()` 或新 helper 加 YOLO 字段，比如 `label/count/confidence`，并在 `get_latest_depth_frame()` 返回值里保留 `objects`，再补一个 `yolo` 摘要字段：

```python
payload["yolo"] = {
    "count": len(payload.get("objects") or []),
    "items": payload.get("objects") or [],
}
```

如果板端当前仍只发 `A1_DEPTH_OBJECT`，那前端先按对象列表展示；如果后面板端补了更细字段，再顺手映射。

- [ ] **Step 4: 改前端“A1 识别状态”面板**

把当前手势/动作三块改成 YOLO 摘要三块：目标数、首个/前几个类别名、置信度。保留 `a1GestureBadge` 这类 DOM 名称也行，但文本改成识别态。

示例：

```javascript
function renderYoloStatus(status={}){
  const items=Array.isArray(status.items)?status.items:[];
  document.getElementById('a1GestureBadge').textContent=items.length?`检测到 ${items.length} 个目标`:'0 个目标';
  document.getElementById('a1GestureSummary').innerHTML=[
    `<div class="status-tile"><div class="status-label">目标数</div><div class="status-value">${items.length}</div></div>`,
    `<div class="status-tile"><div class="status-label">类别</div><div class="status-value">${escapeHtml((items[0]&&items[0].class)||'—')}</div></div>`,
    `<div class="status-tile"><div class="status-label">置信度</div><div class="status-value">${escapeHtml((items[0]&&Number(items[0].score||0).toFixed(2))||'—')}</div></div>`,
  ].join('');
}
```

- [ ] **Step 5: 跑前端语法检查**

Run:
```bash
python -m py_compile tools/aurora/serial_terminal.py tools/aurora/aurora_companion.py
```
Expected: PASS。

- [ ] **Step 6: 验证无目标状态**

确认 `items` 为空时，面板显示 `0 个目标`、`—`，不沿用旧的手势/动作文案。

---

### Task 5: 联调验证与收尾

**Files:**
- Modify: 仅修正前述文件中联调发现的问题
- Test: 视结果补最小检查命令

- [ ] **Step 1: 写联调检查脚本/命令**

板端：
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```
Aurora：
```powershell
cd tools/aurora
.\launch.ps1 -SkipAurora
```

- [ ] **Step 2: 验证板端日志**

确认能看到三类信息：
1. `det_count=0` 时确实无框。
2. `det_count>0` 时有绘制输入。
3. 若 OSD 不显示，能从 flush 前后日志定位到具体断点。

- [ ] **Step 3: 验证前端表现**

确认：
1. 深度图变彩色热力图。
2. `A1 识别状态` 只显示 YOLO 摘要。
3. `depthObjects` 继续随帧更新。

- [ ] **Step 4: 最终整理**

检查 `git diff`，确认没有多余格式化、无关重命名、重复 helper 或死代码。

- [ ] **Step 5: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp \
        data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp \
        data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp \
        tools/aurora/serial_terminal.py \
        tools/aurora/aurora_companion.py \
        tools/aurora/templates/companion_ui.html

git commit -m "feat: debug osd and depth preview"
```
