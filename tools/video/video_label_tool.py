#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import mimetypes
import time
from dataclasses import asdict, dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

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
            f"ROI ({config.x1}, {config.y1}, {config.x2}, {config.y2}) "
            f"exceeds source size {source_width}x{source_height}"
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
