# Video ROI YOLO Dataset Tool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local video-to-YOLO dataset tool that extracts 640x480 frames from `video.mp4` or a chosen video path, writes fixed-ROI YOLO labels to `tools/yolo/raw/`, and documents the full local YOLOv8 train/export/head6 conversion workflow.

**Architecture:** Add `tools/video/` for capture/label UI and backend processing, independent from Aurora. Keep YOLO training assets under `tools/yolo/`: raw images/labels, split script, local dataset YAML, and README covering capture -> split -> `third_party/ultralytics` train -> ONNX export -> head6 crop -> A1 conversion.

**Tech Stack:** Python standard library HTTP server, OpenCV, PowerShell launcher, YOLO txt labels, Ultralytics YOLOv8 from `third_party/ultralytics`.

---

## File Structure

- Create: `tools/video/video_label_tool.py` — local HTTP UI + API + reusable frame extraction functions.
- Create: `tools/video/launch.ps1` — Windows launcher for the local labeling UI.
- Create: `tools/video/README.md` — focused docs for using the video ROI tool.
- Create: `tools/yolo/split_dataset.py` — split `tools/yolo/raw/images|labels` into train/val/test.
- Create: `tools/yolo/dataset.yaml` — local dataset config pointing at `tools/yolo`.
- Modify: `tools/yolo/README.md` — replace MobileNet-only README with combined local YOLO workflow plus pointer to MobileNet sigmoid scripts.
- Keep existing: `tools/yolo/train_mobilenet_sigmoid_classifier.py`, `export_mobilenet_sigmoid_onnx.py`, `infer_mobilenet_sigmoid_classifier.py`, `save_calibration_tensors.py`.
- Create on demand at runtime: `tools/yolo/raw/images/`, `tools/yolo/raw/labels/`.

## Task 1: Create Video ROI Tool Backend and UI

**Files:**
- Create: `tools/video/video_label_tool.py`

- [ ] **Step 1: Write implementation**

Create `tools/video/video_label_tool.py` with a single-file local web app. Required behavior:

- Default video path: `video.mp4` relative to repo root.
- Default output image dir: `tools/yolo/raw/images`.
- Default output label dir: `tools/yolo/raw/labels`.
- Output image size fixed to `640x480`.
- UI fields: video path, class name, class id, x1, y1, x2, y2, frame step, output prefix.
- API validates ROI is inside source frame before processing.
- Every saved frame gets one YOLO label line using scaled ROI.

Implementation skeleton:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import mimetypes
import time
from dataclasses import asdict, dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import cv2

OUTPUT_WIDTH = 640
OUTPUT_HEIGHT = 480


@dataclass(frozen=True)
class ExtractConfig:
    video_path: Path
    output_images_dir: Path
    output_labels_dir: Path
    class_name: str
    class_id: int
    x1: int
    y1: int
    x2: int
    y2: int
    frame_step: int
    output_prefix: str


@dataclass(frozen=True)
class ExtractResult:
    video_path: str
    source_width: int
    source_height: int
    output_width: int
    output_height: int
    class_name: str
    class_id: int
    frame_step: int
    saved_count: int
    output_images_dir: str
    output_labels_dir: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def resolve_repo_path(value: str) -> Path:
    path = Path(value).expanduser()
    if path.is_absolute():
        return path
    return repo_root() / path


def clamp_prefix(value: str) -> str:
    cleaned = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in value.strip())
    return cleaned or f"video_{int(time.time())}"


def yolo_line_for_roi(
    roi: tuple[int, int, int, int], source_size: tuple[int, int], class_id: int
) -> str:
    x1, y1, x2, y2 = roi
    source_width, source_height = source_size
    scale_x = OUTPUT_WIDTH / source_width
    scale_y = OUTPUT_HEIGHT / source_height
    sx1 = x1 * scale_x
    sx2 = x2 * scale_x
    sy1 = y1 * scale_y
    sy2 = y2 * scale_y
    x_center = ((sx1 + sx2) / 2.0) / OUTPUT_WIDTH
    y_center = ((sy1 + sy2) / 2.0) / OUTPUT_HEIGHT
    width = (sx2 - sx1) / OUTPUT_WIDTH
    height = (sy2 - sy1) / OUTPUT_HEIGHT
    return f"{class_id} {x_center:.6f} {y_center:.6f} {width:.6f} {height:.6f}\n"


def validate_config(config: ExtractConfig, source_width: int, source_height: int) -> None:
    if config.class_id < 0:
        raise ValueError("class_id must be >= 0")
    if config.frame_step <= 0:
        raise ValueError("frame_step must be >= 1")
    if not config.class_name.strip():
        raise ValueError("class_name is required")
    if config.x1 < 0 or config.y1 < 0 or config.x2 <= config.x1 or config.y2 <= config.y1:
        raise ValueError("ROI must satisfy 0 <= x1 < x2 and 0 <= y1 < y2")
    if config.x2 > source_width or config.y2 > source_height:
        raise ValueError(
            f"ROI ({config.x1}, {config.y1}, {config.x2}, {config.y2}) exceeds source size {source_width}x{source_height}"
        )


def extract_frames(config: ExtractConfig) -> ExtractResult:
    if not config.video_path.exists():
        raise FileNotFoundError(f"video not found: {config.video_path}")

    capture = cv2.VideoCapture(str(config.video_path))
    if not capture.isOpened():
        raise RuntimeError(f"failed to open video: {config.video_path}")

    source_width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    source_height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    validate_config(config, source_width, source_height)

    config.output_images_dir.mkdir(parents=True, exist_ok=True)
    config.output_labels_dir.mkdir(parents=True, exist_ok=True)
    label_line = yolo_line_for_roi(
        (config.x1, config.y1, config.x2, config.y2),
        (source_width, source_height),
        config.class_id,
    )

    saved_count = 0
    frame_index = 0
    prefix = clamp_prefix(config.output_prefix)
    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            if frame_index % config.frame_step == 0:
                resized = cv2.resize(frame, (OUTPUT_WIDTH, OUTPUT_HEIGHT), interpolation=cv2.INTER_LINEAR)
                stem = f"{prefix}_frame_{frame_index:06d}"
                image_path = config.output_images_dir / f"{stem}.jpg"
                label_path = config.output_labels_dir / f"{stem}.txt"
                if not cv2.imwrite(str(image_path), resized):
                    raise RuntimeError(f"failed to save image: {image_path}")
                label_path.write_text(label_line, encoding="utf-8")
                saved_count += 1
            frame_index += 1
    finally:
        capture.release()

    return ExtractResult(
        video_path=str(config.video_path),
        source_width=source_width,
        source_height=source_height,
        output_width=OUTPUT_WIDTH,
        output_height=OUTPUT_HEIGHT,
        class_name=config.class_name,
        class_id=config.class_id,
        frame_step=config.frame_step,
        saved_count=saved_count,
        output_images_dir=str(config.output_images_dir),
        output_labels_dir=str(config.output_labels_dir),
    )


HTML = """<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>Video ROI YOLO Dataset Tool</title>
<style>
body { font-family: system-ui, sans-serif; max-width: 920px; margin: 32px auto; line-height: 1.5; }
label { display: block; margin-top: 12px; font-weight: 600; }
input { width: 100%; box-sizing: border-box; padding: 8px; margin-top: 4px; }
.grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
button { margin-top: 20px; padding: 10px 16px; }
pre { background: #111; color: #eee; padding: 12px; overflow: auto; }
</style>
</head>
<body>
<h1>Video ROI YOLO Dataset Tool</h1>
<p>输出固定 640×480，图片写入 tools/yolo/raw/images，标签写入 tools/yolo/raw/labels。</p>
<form id="form">
<label>视频路径<input name="video_path" value="video.mp4"></label>
<div class="grid">
<label>类别名<input name="class_name" value="person"></label>
<label>类别 ID<input name="class_id" type="number" value="0"></label>
<label>抽帧间隔<input name="frame_step" type="number" value="5"></label>
<label>输出前缀<input name="output_prefix" value="video"></label>
</div>
<div class="grid">
<label>x1<input name="x1" type="number" value="0"></label>
<label>y1<input name="y1" type="number" value="0"></label>
<label>x2<input name="x2" type="number" value="640"></label>
<label>y2<input name="y2" type="number" value="480"></label>
</div>
<button type="submit">生成训练图片和 YOLO 标签</button>
</form>
<h2>结果</h2>
<pre id="result">等待操作</pre>
<script>
document.getElementById('form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const result = document.getElementById('result');
  result.textContent = '处理中...';
  const data = Object.fromEntries(new FormData(event.target).entries());
  const response = await fetch('/api/extract', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  });
  const payload = await response.json();
  result.textContent = JSON.stringify(payload, null, 2);
});
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def send_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload, indent=2, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path != "/":
            self.send_error(404)
            return
        body = HTML.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        if path != "/api/extract":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            config = ExtractConfig(
                video_path=resolve_repo_path(payload["video_path"]),
                output_images_dir=repo_root() / "tools/yolo/raw/images",
                output_labels_dir=repo_root() / "tools/yolo/raw/labels",
                class_name=str(payload["class_name"]),
                class_id=int(payload["class_id"]),
                x1=int(payload["x1"]),
                y1=int(payload["y1"]),
                x2=int(payload["x2"]),
                y2=int(payload["y2"]),
                frame_step=int(payload["frame_step"]),
                output_prefix=payload.get("output_prefix", "video"),
            )
            result = extract_frames(config)
            self.send_json(200, {"ok": True, "result": asdict(result)})
        except Exception as exc:
            self.send_json(400, {"ok": False, "error": str(exc)})


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Local video ROI YOLO dataset tool")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6210)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    mimetypes.add_type("text/html", ".html")
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Video ROI YOLO Dataset Tool: http://{args.host}:{args.port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Verify compile**

Run:

```bash
python -m py_compile tools/video/video_label_tool.py
```

Expected: no output.

## Task 2: Add Video Tool Launcher and README

**Files:**
- Create: `tools/video/launch.ps1`
- Create: `tools/video/README.md`

- [ ] **Step 1: Write launcher**

Create `tools/video/launch.ps1`:

```powershell
param(
    [string]$HostName = "127.0.0.1",
    [int]$Port = 6210
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
Set-Location $RepoRoot
python tools/video/video_label_tool.py --host $HostName --port $Port
```

- [ ] **Step 2: Write README**

Create `tools/video/README.md`:

```markdown
# Video ROI YOLO Dataset Tool

本工具从视频中抽帧，统一输出 640x480 图片，并根据前端输入的固定 ROI 坐标自动生成 YOLO 标签。

## 启动

```powershell
cd <repo-root>
.\tools\video\launch.ps1
```

打开：`http://127.0.0.1:6210`

## 默认输入输出

- 默认视频：`video.mp4`
- 输出图片：`tools/yolo/raw/images/`
- 输出标签：`tools/yolo/raw/labels/`
- 输出尺寸：`640x480`

## 坐标规则

前端输入的 `x1 y1 x2 y2` 是原始视频帧坐标。脚本保存图片时会 resize 到 640x480，并自动把 ROI 等比例换算成 YOLO 归一化标签。

## 标签格式

每张图生成一个同名 `.txt`：

```text
class_id x_center y_center width height
```
```

- [ ] **Step 3: Verify launcher exists**

Run:

```bash
python -m py_compile tools/video/video_label_tool.py
```

Expected: no output.

## Task 3: Add Local YOLO Dataset Split Script

**Files:**
- Create: `tools/yolo/split_dataset.py`

- [ ] **Step 1: Write split script**

Create `tools/yolo/split_dataset.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import random
import shutil
from pathlib import Path

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}


def collect_pairs(raw_images: Path, raw_labels: Path) -> list[tuple[Path, Path]]:
    pairs = []
    for image_path in sorted(raw_images.rglob("*")):
        if not image_path.is_file() or image_path.suffix.lower() not in IMAGE_EXTENSIONS:
            continue
        label_path = raw_labels / f"{image_path.stem}.txt"
        if not label_path.exists():
            raise RuntimeError(f"missing label for image: {image_path}")
        pairs.append((image_path, label_path))
    if not pairs:
        raise RuntimeError(f"no images found under: {raw_images}")
    return pairs


def reset_split_dirs(dataset_root: Path) -> None:
    for kind in ["images", "labels"]:
        for split in ["train", "val", "test"]:
            path = dataset_root / kind / split
            if path.exists():
                shutil.rmtree(path)
            path.mkdir(parents=True, exist_ok=True)


def split_pairs(pairs: list[tuple[Path, Path]], train: float, val: float):
    if train <= 0 or val < 0 or train + val >= 1:
        raise RuntimeError("expected train > 0, val >= 0, and train + val < 1")
    train_count = int(len(pairs) * train)
    val_count = int(len(pairs) * val)
    train_items = pairs[:train_count]
    val_items = pairs[train_count : train_count + val_count]
    test_items = pairs[train_count + val_count :]
    if not train_items or not val_items or not test_items:
        raise RuntimeError(
            f"split too small: train={len(train_items)} val={len(val_items)} test={len(test_items)}"
        )
    return {"train": train_items, "val": val_items, "test": test_items}


def copy_split(dataset_root: Path, splits: dict[str, list[tuple[Path, Path]]]) -> None:
    for split, items in splits.items():
        for image_path, label_path in items:
            shutil.copy2(image_path, dataset_root / "images" / split / image_path.name)
            shutil.copy2(label_path, dataset_root / "labels" / split / label_path.name)
        print(f"{split}: {len(items)}")


def parse_args():
    parser = argparse.ArgumentParser(description="Split tools/yolo raw dataset into train/val/test")
    parser.add_argument("--dataset-root", default="tools/yolo")
    parser.add_argument("--train", type=float, default=0.8)
    parser.add_argument("--val", type=float, default=0.1)
    parser.add_argument("--seed", type=int, default=42)
    return parser.parse_args()


def main():
    args = parse_args()
    dataset_root = Path(args.dataset_root)
    pairs = collect_pairs(dataset_root / "raw" / "images", dataset_root / "raw" / "labels")
    rng = random.Random(args.seed)
    rng.shuffle(pairs)
    reset_split_dirs(dataset_root)
    splits = split_pairs(pairs, args.train, args.val)
    copy_split(dataset_root, splits)


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Verify compile**

Run:

```bash
python -m py_compile tools/yolo/split_dataset.py
```

Expected: no output.

## Task 4: Add Local YOLO Dataset YAML

**Files:**
- Create: `tools/yolo/dataset.yaml`

- [ ] **Step 1: Write YAML**

Create `tools/yolo/dataset.yaml`:

```yaml
# Local YOLOv8 dataset config generated from tools/video ROI labeling flow.
# Images are 640x480.
path: tools/yolo
train: images/train
val: images/val
test: images/test

names:
  0: person
  1: gesture1
  2: gesture2
  3: obstacle_box
```

## Task 5: Update YOLO README for Full Local Workflow

**Files:**
- Modify: `tools/yolo/README.md`

- [ ] **Step 1: Replace README**

Replace `tools/yolo/README.md` with docs that include:

```markdown
# YOLOv8 本地训练与 A1 转换流程

## 目标

从根目录 `video.mp4` 或自选视频制作 640x480 YOLO 训练集，使用 `third_party/ultralytics` 训练 YOLOv8，再导出 ONNX、剪裁 head6、转换为 A1 `.m1model`。

## 目录

- 视频标注工具：`tools/video/`
- 原始图片：`tools/yolo/raw/images/`
- 原始标签：`tools/yolo/raw/labels/`
- 划分后图片：`tools/yolo/images/train|val|test/`
- 划分后标签：`tools/yolo/labels/train|val|test/`
- 数据配置：`tools/yolo/dataset.yaml`
- YOLOv8 代码：`third_party/ultralytics/`

## 1. 拍视频

把视频放到仓库根目录：

```text
video.mp4
```

也可以在前端输入绝对路径或相对仓库根目录的路径。

## 2. 从视频生成图片和标签

```powershell
cd <repo-root>
.\tools\video\launch.ps1
```

打开 `http://127.0.0.1:6210`，输入：

- 视频路径：默认 `video.mp4`
- 类别名：例如 `person`
- 类别 ID：例如 `0`
- ROI 坐标：原始视频坐标 `x1 y1 x2 y2`
- 抽帧间隔：例如 `5`

输出固定为 640x480：

```text
tools/yolo/raw/images/*.jpg
tools/yolo/raw/labels/*.txt
```

## 3. 检查类别配置

编辑 `tools/yolo/dataset.yaml`，确保 `names` 顺序与前端输入 class id 一致。

## 4. 划分训练集

```bash
python tools/yolo/split_dataset.py --dataset-root tools/yolo --train 0.8 --val 0.1 --seed 42
```

## 5. 安装/使用 Ultralytics

```powershell
py -3.9 -m venv .venv39
.\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -e third_party/ultralytics
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

## 6. 训练 YOLOv8

```powershell
.\.venv39\Scripts\yolo.exe detect train model=yolov8n.pt data=tools/yolo/dataset.yaml epochs=100 batch=16 imgsz=640 device=0 project=tools/yolo/runs name=a1_yolo_640x480
```

## 7. 验证与推理

```powershell
.\.venv39\Scripts\yolo.exe detect val model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt data=tools/yolo/dataset.yaml device=0
.\.venv39\Scripts\yolo.exe detect predict model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt source=tools/yolo/images/test device=0
```

## 8. 导出 ONNX

```powershell
.\.venv39\Scripts\yolo.exe export model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt format=onnx opset=13 simplify=True imgsz=640
```

输出通常在：

```text
tools/yolo/runs/a1_yolo_640x480/weights/best.onnx
```

## 9. 剪裁 ONNX head6

按 `docs/15_AI模型转换与部署.md` 的 head6 输出节点剪裁方法处理 `best.onnx`。剪裁后模型把 YOLO decode/DFL/NMS 放到 CPU 后处理，便于 A1 NPU 运行。

## 10. 转换 m1model 和上板

按 `docs/15_AI模型转换与部署.md`：

1. 用 SmartSens 模型转换工具/思思 AI 助手把 head6 ONNX 转成 `.m1model`。
2. 放到板端 app assets models 目录。
3. 确认板端前处理和训练一致：输入链路统一 640x480。
4. 重新构建镜像并上板验证。

## MobileNet Sigmoid 模板

本目录还保留 RPS 风格 MobileNet sigmoid 分类脚本：

- `train_mobilenet_sigmoid_classifier.py`
- `export_mobilenet_sigmoid_onnx.py`
- `infer_mobilenet_sigmoid_classifier.py`
- `save_calibration_tensors.py`

它们用于固定 ROI 分类，不用于 YOLO bbox 检测训练。
```

## Task 6: Final Verification

**Files:**
- Verify: `tools/video/video_label_tool.py`
- Verify: `tools/yolo/split_dataset.py`

- [ ] **Step 1: Compile Python files**

Run:

```bash
python -m py_compile tools/video/video_label_tool.py tools/yolo/split_dataset.py
```

Expected: no output.

- [ ] **Step 2: Check working tree**

Run:

```bash
git status --short
```

Expected: new `tools/video/`, modified `tools/yolo/`, spec/plan docs, and pre-existing untracked `demo-rps/` if still present.

## Self-Review

Spec coverage:
- Frontend for video path, label name, class id, ROI, frame step: Task 1.
- Fixed 640x480 outputs: Task 1 and Task 5.
- Output to `tools/yolo/raw/images` and `tools/yolo/raw/labels`: Task 1 and Task 5.
- Localized YOLO paths: Task 3, Task 4, Task 5.
- Ultralytics train/export/head6/m1model docs: Task 5.

Placeholder scan: no TBD/TODO/later placeholders.

Type consistency: `ExtractConfig`, `ExtractResult`, `extract_frames`, `yolo_line_for_roi`, `split_dataset.py` paths align with README and YAML.
