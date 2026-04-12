#!/usr/bin/env python3
"""
Aurora Capture Tool — A1 开发板摄像头拍照工具

通过 USB Type-C 连接 A1 开发板，接收 SC132GS 摄像头的灰度视频流。
摄像头原始输出为 640×360 灰度图 (Y8 格式，16:9)。

功能:
  - 实时预览摄像头画面
  - 拍照保存 640×360 原始灰度图 (摄像头原生分辨率，用于 YOLOv8 训练集)
  - Web 前端界面选择拍照格式
  - 摄像头断联自动检测与一键刷新恢复

注意:
  SC132GS 传感器输出 16:9 的 640×360 灰度图 (与 YOLOv8 模型输入匹配)。
  若 EVB 固件未更新，摄像头可能以 YUYV 竖屏格式输出 360×1280，
  此时工具会自动旋转 + 缩放到 640×360。

用法:
  python aurora_capture.py [--device 0] [--output ../data/yolov8_dataset/raw/images]
"""

import argparse
import os
import sys
import threading
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template_string, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
# SC132GS 通过 USB Type-C 传输，输出 16:9 格式 640×360 灰度 (Y8)
CAMERA_WIDTH = 640
CAMERA_HEIGHT = 360
CAMERA_FPS = 30

# 拍照输出格式
CAPTURE_FORMATS = {
    "640x360":  (640, 360),    # 原始灰度图 (摄像头原生输出, YOLOv8 训练集格式)
    "1280x720": (1280, 720),   # 放大版 (2x 上采样，仅供参考)
}

app = Flask(__name__)

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: Optional[cv2.VideoCapture] = None
camera_lock = threading.Lock()
device_id_global = 0
output_dir = None
capture_count = 0
_consecutive_failures = 0  # 连续读帧失败计数，用于触发自动重连
_last_reconnect_time = 0.0


def open_camera(device_id: int) -> Optional[cv2.VideoCapture]:
    """打开 A1 开发板摄像头 (SC132GS via USB Type-C)

    关键: 必须显式设置 FOURCC 为 GREY/Y800 以正确解析灰度格式,
    否则 OpenCV 会按默认的 YUV/RGB 解析。摄像头目标分辨率为 640×360。
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


def read_grayscale_frame(cap: cv2.VideoCapture) -> Optional[np.ndarray]:
    """读取一帧灰度图像

    确保输出为单通道 640×360 灰度图，无论摄像头驱动如何解析。
    EVB 固件更新后，摄像头直接输出 640×360 Y8；
    固件未更新时，YUYV 竖屏 720×1280 经 DirectShow 汇报为 360(W)×1280(H)，
    此时自动旋转 90° 并缩放到 640×360。
    """
    ret, frame = cap.read()
    if not ret:
        return None

    # 如果读出来是彩色的，转灰度
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # 已是目标尺寸，直接返回
    if frame.shape == (CAMERA_HEIGHT, CAMERA_WIDTH):
        return frame

    # 360(W)×1280(H) — SDK 未更新时的竖屏 YUYV 兼容帧格式
    # 传感器竖屏 720(W)×1280(H)，YUYV 宏像素使 DirectShow 汇报宽度减半 → 360×1280
    # 旋转 90° 顺时针：(H=1280, W=360) → (H=360, W=1280)，再缩放到 640×360
    if frame.shape == (1280, 360):
        frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        return cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT))

    # 通用兜底：直接缩放
    return cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT))


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

    if fmt == "640x360":
        # 摄像头原生 640×360，直接使用（crop_center 在尺寸相同时原样返回）
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
            background: #0d1117;
            color: #c9d1d9;
            min-height: 100vh;
        }
        .header {
            background: linear-gradient(135deg, #161b22 0%, #21262d 100%);
            padding: 14px 28px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 1px solid #30363d;
            box-shadow: 0 2px 12px rgba(0,0,0,0.4);
        }
        .header-left { display: flex; align-items: center; gap: 12px; }
        .header h1 {
            font-size: 1.3em;
            background: linear-gradient(90deg, #58a6ff, #a371f7);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            font-weight: 700;
        }
        .header-badge {
            font-size: 0.7em;
            background: #21262d;
            border: 1px solid #30363d;
            color: #8b949e;
            padding: 2px 8px;
            border-radius: 12px;
        }
        .status-pill {
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 0.85em;
            padding: 5px 14px;
            border-radius: 20px;
            border: 1px solid #30363d;
            background: #161b22;
        }
        .status-dot {
            width: 8px; height: 8px;
            border-radius: 50%;
            background: #3fb950;
            animation: pulse 2s infinite;
        }
        .status-dot.offline { background: #f85149; animation: none; }
        @keyframes pulse {
            0%,100% { opacity: 1; }
            50% { opacity: 0.4; }
        }
        .main {
            display: grid;
            grid-template-columns: 1fr 300px;
            gap: 16px;
            padding: 16px;
            max-width: 1440px;
            margin: 0 auto;
        }
        .card {
            background: #161b22;
            border: 1px solid #30363d;
            border-radius: 10px;
            overflow: hidden;
        }
        .card-header {
            padding: 12px 16px;
            border-bottom: 1px solid #21262d;
            font-size: 0.85em;
            font-weight: 600;
            color: #8b949e;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }
        .preview .card-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .preview-wrap {
            position: relative;
            background: #0d1117;
        }
        .preview-wrap img {
            width: 100%;
            display: block;
        }
        .preview-overlay {
            position: absolute;
            top: 8px; left: 8px;
            background: rgba(0,0,0,0.6);
            color: #3fb950;
            font-size: 0.75em;
            padding: 3px 8px;
            border-radius: 4px;
            font-family: monospace;
        }
        .controls {
            display: flex;
            flex-direction: column;
            gap: 0;
        }
        .controls .card { flex: none; }
        .controls-inner { padding: 16px; }
        .counter-row {
            text-align: center;
            margin-bottom: 14px;
        }
        .counter-num {
            font-size: 3em;
            font-weight: 700;
            background: linear-gradient(135deg, #58a6ff, #a371f7);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            line-height: 1;
        }
        .counter-label { font-size: 0.75em; color: #8b949e; margin-top: 2px; }
        .btn {
            width: 100%;
            padding: 11px 16px;
            margin: 5px 0;
            border: 1px solid transparent;
            border-radius: 8px;
            font-size: 0.95em;
            cursor: pointer;
            transition: all 0.15s;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .btn-icon { font-size: 1.2em; }
        .btn small { font-weight: 400; color: rgba(255,255,255,0.6); font-size: 0.78em; display: block; margin-top: 1px; }
        .btn-primary {
            background: linear-gradient(135deg, #1f6feb, #388bfd);
            color: white;
            border-color: #1f6feb;
        }
        .btn-primary:hover { background: linear-gradient(135deg, #388bfd, #58a6ff); transform: translateY(-1px); box-shadow: 0 4px 12px rgba(31,111,235,0.4); }
        .btn-secondary {
            background: linear-gradient(135deg, #238636, #2ea043);
            color: white;
            border-color: #238636;
        }
        .btn-secondary:hover { background: linear-gradient(135deg, #2ea043, #3fb950); transform: translateY(-1px); box-shadow: 0 4px 12px rgba(35,134,54,0.4); }
        .btn-refresh {
            background: #21262d;
            color: #8b949e;
            border-color: #30363d;
        }
        .btn-refresh:hover { background: #30363d; color: #c9d1d9; border-color: #58a6ff; }
        .btn-refresh.spinning .btn-icon { display: inline-block; animation: spin 1s linear infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }
        .divider {
            height: 1px;
            background: #21262d;
            margin: 12px 0;
        }
        .info-table { font-size: 0.82em; width: 100%; }
        .info-table tr td { padding: 3px 0; }
        .info-table td:first-child { color: #8b949e; width: 90px; }
        .info-table td:last-child { color: #c9d1d9; word-break: break-all; }
        .shortcut-row {
            display: flex;
            gap: 6px;
            flex-wrap: wrap;
            margin-top: 10px;
        }
        .kbd {
            background: #21262d;
            border: 1px solid #30363d;
            border-radius: 4px;
            padding: 2px 7px;
            font-size: 0.78em;
            font-family: monospace;
            color: #8b949e;
        }
        .toast {
            position: fixed;
            bottom: 24px;
            right: 24px;
            background: #238636;
            color: white;
            padding: 10px 20px;
            border-radius: 8px;
            font-weight: 600;
            font-size: 0.9em;
            opacity: 0;
            transform: translateY(8px);
            transition: all 0.25s;
            z-index: 1000;
            max-width: 340px;
            box-shadow: 0 4px 16px rgba(0,0,0,0.4);
        }
        .toast.show { opacity: 1; transform: translateY(0); }
        .toast.error { background: #da3633; }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-left">
            <h1>⚡ Aurora Capture</h1>
            <span class="header-badge">SC132GS · 1280×720</span>
        </div>
        <div class="status-pill">
            <div class="status-dot" id="statusDot"></div>
            <span id="statusText">连接中...</span>
        </div>
    </div>
    <div class="main">
        <div class="card preview">
            <div class="card-header">
                <span>实时预览</span>
                <span id="resLabel" style="color:#3fb950;font-size:0.9em">1280 × 720</span>
            </div>
            <div class="preview-wrap">
                <img id="stream" src="/video_feed" alt="摄像头预览"
                     onload="setOnline(true)"
                     onerror="setOnline(false)">
                <div class="preview-overlay" id="fpsLabel">一 fps</div>
            </div>
        </div>
        <div class="controls">
            <div class="card" style="margin-bottom:12px">
                <div class="card-header">拍照控制</div>
                <div class="controls-inner">
                    <div class="counter-row">
                        <div class="counter-num" id="counter">0</div>
                        <div class="counter-label">已拍照数量</div>
                    </div>
                    <button class="btn btn-primary" onclick="capture('1280x720')">
                        <span class="btn-icon">📷</span>
                        <span>拍照 1280×720<small>原始灰度图</small></span>
                    </button>
                    <button class="btn btn-secondary" onclick="capture('640x360')">
                        <span class="btn-icon">🏹</span>
                        <span>拍照 640×360<small>YOLOv8 训练集 (16:9)</small></span>
                    </button>
                    <div class="divider"></div>
                    <button class="btn btn-refresh" id="refreshBtn" onclick="refreshCamera()">
                        <span class="btn-icon">🔄</span>
                        <span>刷新摄像头<small>断联后点此恢复</small></span>
                    </button>
                </div>
            </div>
            <div class="card">
                <div class="card-header">设备信息</div>
                <div class="controls-inner">
                    <table class="info-table">
                        <tr><td>摄像头</td><td>SC132GS (USB-C)</td></tr>
                        <tr><td>原始分辨率</td><td>1280 × 720 灰度</td></tr>
                        <tr><td>训练裁剪</td><td>640 × 360 (16:9 中心)</td></tr>
                        <tr><td>输出格式</td><td>PNG 灰度</td></tr>
                        <tr><td>输出目录</td><td>{{ output_dir }}</td></tr>
                    </table>
                    <div class="shortcut-row">
                        <span class="kbd">1</span> <span style="font-size:.78em;color:#8b949e">拍够 1280×720</span>
                        &nbsp;&nbsp;
                        <span class="kbd">2</span> <span style="font-size:.78em;color:#8b949e">拍切 640×360</span>
                        &nbsp;&nbsp;
                        <span class="kbd">R</span> <span style="font-size:.78em;color:#8b949e">刷新</span>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <div class="toast" id="toast"></div>

    <script>
        let captureCount = 0;
        let fpsTimer = null;

        function setOnline(online) {
            const dot = document.getElementById('statusDot');
            const txt = document.getElementById('statusText');
            dot.className = 'status-dot' + (online ? '' : ' offline');
            txt.textContent = online ? '实时预览中' : '摄像头未连接';
        }

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
                    showToast('✓ 已保存: ' + data.filename);
                } else {
                    showToast('✗ 拍照失败: ' + data.error, true);
                }
            })
            .catch(err => showToast('✗ 请求失败: ' + err, true));
        }

        function refreshCamera() {
            const btn = document.getElementById('refreshBtn');
            btn.classList.add('spinning');
            btn.disabled = true;
            fetch('/refresh_camera', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                showToast(data.success ? '✓ 摄像头已刷新' : '✗ ' + data.error, !data.success);
                if (data.success) {
                    const img = document.getElementById('stream');
                    img.src = '/video_feed?' + Date.now();
                    setOnline(true);
                }
            })
            .catch(err => showToast('✗ 刷新失败: ' + err, true))
            .finally(() => {
                btn.classList.remove('spinning');
                btn.disabled = false;
            });
        }

        function showToast(msg, isError = false) {
            const t = document.getElementById('toast');
            t.textContent = msg;
            t.className = 'toast show' + (isError ? ' error' : '');
            setTimeout(() => t.classList.remove('show'), 3000);
        }

        // 键盘快捷键
        document.addEventListener('keydown', (e) => {
            if (e.key === '1') capture('1280x720');
            if (e.key === '2') capture('640x360');
            if (e.key === 'r' || e.key === 'R') refreshCamera();
        });

        // 定期轮询摄像头状态
        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                setOnline(d.connected);
            }).catch(() => {});
        }, 3000);
    </script>
</body>
</html>
"""


def generate_frames():
    """生成 MJPEG 视频流，内置自动重连逻辑"""
    global camera, _consecutive_failures, _last_reconnect_time
    RECONNECT_INTERVAL = 3.0   # 两次重连尝试之间的最小间隔秒
    FAIL_THRESHOLD = 10        # 连续读帧失败达到此阈值才触发重连

    while True:
        with camera_lock:
            cap = camera
        frame = read_grayscale_frame(cap) if cap else None

        if frame is None:
            _consecutive_failures += 1
            now = time.time()
            if (_consecutive_failures >= FAIL_THRESHOLD
                    and now - _last_reconnect_time > RECONNECT_INTERVAL):
                _last_reconnect_time = now
                _consecutive_failures = 0
                print("[INFO] 视频流中断，自动尝试重连摄像头...")
                with camera_lock:
                    if camera:
                        camera.release()
                    camera = open_camera(device_id_global)
                if camera:
                    print("[INFO] 摄像头自动重连成功")
            # 发送暗帧占位
            black = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH), dtype=np.uint8)
            cv2.putText(black, "No Signal - Reconnecting...",
                        (CAMERA_WIDTH // 2 - 160, CAMERA_HEIGHT // 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (80, 80, 80), 2)
            disp = cv2.cvtColor(black, cv2.COLOR_GRAY2BGR)
            _, buffer = cv2.imencode('.jpg', disp, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'
                   + buffer.tobytes() + b'\r\n')
            time.sleep(0.3)
            continue

        _consecutive_failures = 0
        # 转为 3 通道用于 JPEG 编码 (浏览器需要)
        display = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)

        # 绘制裁剪区域参考框 (640×360 中心区域, 16:9)
        h, w = frame.shape
        cx, cy = w // 2, h // 2
        x1 = cx - 320
        y1 = cy - 180
        x2 = cx + 320
        y2 = cy + 180
        cv2.rectangle(display, (x1, y1), (x2, y2), (50, 205, 90), 1)
        cv2.putText(display, "640x360 (train)", (x1 + 5, y1 + 18),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (50, 205, 90), 1)

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
def do_capture():
    data = request.get_json()
    fmt = data.get('format', '1280x720')

    if fmt not in CAPTURE_FORMATS:
        return jsonify({"success": False, "error": f"不支持的格式: {fmt}"})

    with camera_lock:
        cap = camera
    frame = read_grayscale_frame(cap) if cap else None
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})

    filename = save_capture(frame, fmt)
    return jsonify({"success": True, "filename": filename, "format": fmt})


@app.route('/refresh_camera', methods=['POST'])
def refresh_camera():
    """(手动)刷新摄像头，断开旧连接并重新打开"""
    global camera, _consecutive_failures
    with camera_lock:
        if camera:
            camera.release()
        camera = open_camera(device_id_global)
        ok = camera is not None and camera.isOpened()
    _consecutive_failures = 0
    if ok:
        print("[INFO] 摄像头手动刷新成功")
        return jsonify({"success": True, "message": "摄像头已重新连接"})
    else:
        print("[WARN] 摄像头刷新失败")
        return jsonify({"success": False, "error": "无法连接到摄像头设备"})


@app.route('/status')
def status():
    with camera_lock:
        connected = camera is not None and camera.isOpened()
    return jsonify({
        "connected": connected,
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
    global device_id_global
    device_id_global = args.device
    print(f"[INFO] 正在连接 A1 摄像头 (设备 {args.device})...")
    camera = open_camera(args.device)

    print(f"[INFO] Web 界面已启动: http://localhost:{args.port}")
    print(f"[INFO] 快捷键: 1=1280x720  2=640x360  R=刷新摄像头")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
