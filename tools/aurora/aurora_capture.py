#!/usr/bin/env python3
"""
Aurora Capture Tool — A1 开发板摄像头拍照工具

通过 USB Type-C 连接 A1 开发板，接收 SC132GS 摄像头的灰度视频流。
摄像头原始输出为 1280×640 灰度图 (Y8 格式)。

功能:
  - 实时预览摄像头画面
  - 拍照保存 1280×640 原始灰度图 (用于通用用途)
  - 拍照保存 640×480 裁剪灰度图 (用于 YOLOv8 训练集)
  - Web 前端界面选择拍照格式

注意:
  SDK 中 pipeline_image.cpp 会将模型输入裁剪到 720×540，
  此工具的 640×480 裁剪模式用于制作 YOLO 训练数据集。

用法:
  python aurora_capture.py [--device 0] [--output ../data/yolov8_dataset/raw/images]
"""

import argparse
import os
import sys
import time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template_string, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
# SC132GS 通过 USB Type-C 传输，原始格式为 1280×640 灰度 (Y8)
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 640
CAMERA_FPS = 30

# 拍照输出格式
CAPTURE_FORMATS = {
    "1280x640": (1280, 640),   # 原始灰度图
    "640x480":  (640, 480),    # YOLOv8 训练集格式 (从中心裁剪)
}

app = Flask(__name__)

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera = None
output_dir = None
capture_count = 0


def open_camera(device_id: int) -> cv2.VideoCapture:
    """打开 A1 开发板摄像头 (SC132GS via USB Type-C)

    关键: 必须显式设置 FOURCC 为 GREY/Y800 以正确解析灰度格式,
    否则 OpenCV 会按默认的 YUV/RGB 解析导致错误的比例和灰度。
    """
    cap = cv2.VideoCapture(device_id, cv2.CAP_DSHOW if sys.platform == "win32" else cv2.CAP_V4L2)
    if not cap.isOpened():
        # 回退: 不指定 API
        cap = cv2.VideoCapture(device_id)

    if not cap.isOpened():
        raise RuntimeError(f"无法打开摄像头设备 {device_id}")

    # 尝试设置 Y800/GREY 灰度格式 (避免被解析为错误的比例和灰度)
    grey_fourccs = [
        cv2.VideoWriter_fourcc(*'Y800'),
        cv2.VideoWriter_fourcc(*'GREY'),
        cv2.VideoWriter_fourcc(*'Y8  '),
        cv2.VideoWriter_fourcc(*'Y16 '),
    ]
    format_set = False
    for fourcc in grey_fourccs:
        cap.set(cv2.CAP_PROP_FOURCC, fourcc)
        actual = int(cap.get(cv2.CAP_PROP_FOURCC))
        if actual == fourcc:
            format_set = True
            fourcc_str = "".join([chr((fourcc >> (8 * i)) & 0xFF) for i in range(4)])
            print(f"[INFO] 摄像头格式设置为: {fourcc_str}")
            break

    if not format_set:
        print("[WARN] 无法设置灰度 FOURCC, 将在读取后转换为灰度")

    # 设置分辨率
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, CAMERA_FPS)

    # 禁用自动格式转换 (保持原始灰度)
    cap.set(cv2.CAP_PROP_CONVERT_RGB, 0)

    actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    actual_fps = cap.get(cv2.CAP_PROP_FPS)
    print(f"[INFO] 摄像头已打开: {actual_w}x{actual_h} @ {actual_fps:.1f}fps")

    if actual_w != CAMERA_WIDTH or actual_h != CAMERA_HEIGHT:
        print(f"[WARN] 实际分辨率 {actual_w}x{actual_h} 与预期 {CAMERA_WIDTH}x{CAMERA_HEIGHT} 不匹配")
        print(f"[WARN] 将尝试重新解析帧数据为 {CAMERA_WIDTH}x{CAMERA_HEIGHT} 灰度格式")

    return cap


def read_grayscale_frame(cap: cv2.VideoCapture) -> np.ndarray | None:
    """读取一帧灰度图像

    确保输出为单通道 1280×640 灰度图, 无论摄像头驱动如何解析。
    """
    ret, frame = cap.read()
    if not ret:
        return None

    # 如果读出来是彩色的, 转灰度
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # 如果尺寸不对, 尝试从原始字节重新解析
    if frame.shape[0] != CAMERA_HEIGHT or frame.shape[1] != CAMERA_WIDTH:
        total_pixels = CAMERA_WIDTH * CAMERA_HEIGHT
        raw = frame.flatten()
        if raw.size >= total_pixels:
            frame = raw[:total_pixels].reshape(CAMERA_HEIGHT, CAMERA_WIDTH)
        else:
            # 尺寸不够, resize
            frame = cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT))

    return frame


def crop_center(img: np.ndarray, target_w: int, target_h: int) -> np.ndarray:
    """从图像中心裁剪到目标尺寸"""
    h, w = img.shape[:2]
    if target_w >= w and target_h >= h:
        return img

    x_start = max(0, (w - target_w) // 2)
    y_start = max(0, (h - target_h) // 2)
    return img[y_start:y_start + target_h, x_start:x_start + target_w]


def save_capture(frame: np.ndarray, fmt: str) -> str:
    """保存拍照图片"""
    global capture_count

    target_w, target_h = CAPTURE_FORMATS[fmt]

    if fmt == "640x480":
        # 从 1280x640 中心裁剪为 640x480
        result = crop_center(frame, target_w, target_h)
    else:
        result = frame.copy()

    # 确保尺寸正确
    if result.shape[1] != target_w or result.shape[0] != target_h:
        result = cv2.resize(result, (target_w, target_h))

    capture_count += 1
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"capture_{timestamp}_{capture_count:04d}_{fmt}.png"
    filepath = os.path.join(output_dir, filename)
    cv2.imwrite(filepath, result)
    print(f"[CAPTURE] 保存: {filepath} ({target_w}x{target_h} gray)")
    return filename


# ─── Web 前端 ─────────────────────────────────────────────────────────────────
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Aurora Capture - A1 摄像头拍照工具</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
        }
        .header {
            background: #16213e;
            padding: 15px 30px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 2px solid #0f3460;
        }
        .header h1 {
            font-size: 1.4em;
            color: #e94560;
        }
        .header .status {
            color: #53d769;
            font-size: 0.9em;
        }
        .main {
            display: flex;
            gap: 20px;
            padding: 20px;
            max-width: 1400px;
            margin: 0 auto;
        }
        .preview {
            flex: 1;
            background: #16213e;
            border-radius: 8px;
            padding: 15px;
        }
        .preview img {
            width: 100%;
            border-radius: 4px;
            background: #000;
        }
        .controls {
            width: 300px;
            background: #16213e;
            border-radius: 8px;
            padding: 20px;
        }
        .controls h2 {
            font-size: 1.1em;
            margin-bottom: 15px;
            color: #e94560;
        }
        .btn {
            width: 100%;
            padding: 12px;
            margin: 8px 0;
            border: none;
            border-radius: 6px;
            font-size: 1em;
            cursor: pointer;
            transition: all 0.2s;
            font-weight: 600;
        }
        .btn-full {
            background: #e94560;
            color: white;
        }
        .btn-full:hover { background: #c73550; }
        .btn-crop {
            background: #0f3460;
            color: white;
        }
        .btn-crop:hover { background: #1a4a7a; }
        .info {
            margin-top: 20px;
            padding: 12px;
            background: #1a1a2e;
            border-radius: 6px;
            font-size: 0.85em;
            line-height: 1.6;
        }
        .info .label { color: #888; }
        .toast {
            position: fixed;
            bottom: 30px;
            right: 30px;
            background: #53d769;
            color: #1a1a2e;
            padding: 12px 24px;
            border-radius: 6px;
            font-weight: 600;
            opacity: 0;
            transition: opacity 0.3s;
            z-index: 1000;
        }
        .toast.show { opacity: 1; }
        .counter {
            text-align: center;
            font-size: 2em;
            color: #e94560;
            margin: 15px 0;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Aurora Capture</h1>
        <span class="status" id="status">● 连接中...</span>
    </div>
    <div class="main">
        <div class="preview">
            <img id="stream" src="/video_feed" alt="摄像头预览"
                 onerror="document.getElementById('status').textContent='● 摄像头未连接'"
                 onload="document.getElementById('status').textContent='● 实时预览中'">
        </div>
        <div class="controls">
            <h2>拍照控制</h2>
            <div class="counter" id="counter">0</div>
            <button class="btn btn-full" onclick="capture('1280x640')">
                📷 拍照 1280×640<br>
                <small>原始灰度图</small>
            </button>
            <button class="btn btn-crop" onclick="capture('640x480')">
                📷 拍照 640×480<br>
                <small>YOLOv8 训练集</small>
            </button>
            <div class="info">
                <div><span class="label">摄像头:</span> SC132GS (USB-C)</div>
                <div><span class="label">原始分辨率:</span> 1280×640 灰度</div>
                <div><span class="label">640×480模式:</span> 中心裁剪</div>
                <div><span class="label">输出目录:</span> {{ output_dir }}</div>
                <div><span class="label">格式:</span> PNG 灰度</div>
            </div>
        </div>
    </div>
    <div class="toast" id="toast"></div>

    <script>
        let captureCount = 0;

        function capture(fmt) {
            fetch('/capture', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({format: fmt})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    captureCount++;
                    document.getElementById('counter').textContent = captureCount;
                    showToast('已保存: ' + data.filename);
                } else {
                    showToast('拍照失败: ' + data.error);
                }
            })
            .catch(err => showToast('请求失败: ' + err));
        }

        function showToast(msg) {
            const t = document.getElementById('toast');
            t.textContent = msg;
            t.classList.add('show');
            setTimeout(() => t.classList.remove('show'), 2000);
        }

        // 键盘快捷键
        document.addEventListener('keydown', (e) => {
            if (e.key === '1') capture('1280x640');
            if (e.key === '2') capture('640x480');
        });
    </script>
</body>
</html>
"""


def generate_frames():
    """生成 MJPEG 视频流"""
    while True:
        frame = read_grayscale_frame(camera)
        if frame is None:
            time.sleep(0.01)
            continue

        # 转为 3 通道用于 JPEG 编码 (浏览器需要)
        display = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)

        # 绘制裁剪区域参考框 (640x480 中心区域)
        h, w = frame.shape
        cx, cy = w // 2, h // 2
        x1 = cx - 320
        y1 = cy - 240
        x2 = cx + 320
        y2 = cy + 240
        cv2.rectangle(display, (x1, y1), (x2, y2), (0, 255, 0), 1)
        cv2.putText(display, "640x480", (x1 + 5, y1 + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

        _, buffer = cv2.imencode('.jpg', display, [cv2.IMWRITE_JPEG_QUALITY, 80])
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')


@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE, output_dir=output_dir)


@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/capture', methods=['POST'])
def capture():
    data = request.get_json()
    fmt = data.get('format', '1280x640')

    if fmt not in CAPTURE_FORMATS:
        return jsonify({"success": False, "error": f"不支持的格式: {fmt}"})

    frame = read_grayscale_frame(camera)
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})

    filename = save_capture(frame, fmt)
    return jsonify({"success": True, "filename": filename, "format": fmt})


@app.route('/status')
def status():
    return jsonify({
        "connected": camera is not None and camera.isOpened(),
        "capture_count": capture_count,
        "output_dir": output_dir,
        "camera_resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
    })


def main():
    global camera, output_dir

    parser = argparse.ArgumentParser(description="Aurora Capture - A1 摄像头拍照工具")
    parser.add_argument("--device", type=int, default=0,
                        help="摄像头设备 ID (默认: 0)")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw/images",
                        help="拍照保存目录")
    parser.add_argument("--port", type=int, default=5000,
                        help="Web 服务端口 (默认: 5000)")
    parser.add_argument("--host", type=str, default="0.0.0.0",
                        help="Web 服务地址 (默认: 0.0.0.0)")
    args = parser.parse_args()

    # 解析输出目录
    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    # 打开摄像头
    print(f"[INFO] 正在连接 A1 摄像头 (设备 {args.device})...")
    camera = open_camera(args.device)

    print(f"[INFO] Web 界面已启动: http://localhost:{args.port}")
    print(f"[INFO] 快捷键: 按 1 拍摄 1280x640, 按 2 拍摄 640x480")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
