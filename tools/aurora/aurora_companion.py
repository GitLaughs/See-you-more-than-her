#!/usr/bin/env python3
"""
Aurora Companion — A1 开发板摄像头可视化采集伴侣

增强版拍照工具，在 aurora_capture 基础上提供：
  - 精心设计的现代暗色玻璃态 UI
  - 摄像头断联自动检测 + 一键刷新恢复
  - 实时 FPS 及连接状态显示
  - 最近拍摄缩略图画廊 (最多 8 张)
  - 双分辨率拍摄：1280×720 (原始) 和 640×360 (训练集, 16:9)
  - 键盘快捷键：1/2/R

用法:
  python aurora_companion.py [--device 0] [--output ../../data/yolov8_dataset/raw/images] [--port 5001]
"""

import argparse
import base64
import os
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

CAPTURE_FORMATS = {
    "1280x720": (1280, 720),   # 原始灰度图（全分辨率）
    "640x360":  (640,  360),   # YOLOv8 训练集（16:9 中心裁剪）
}

app = Flask(__name__, template_folder="templates")
try:
    from chassis_comm import chassis_bp
    app.register_blueprint(chassis_bp)
    print("[INFO] 底盘通信模块已加载")
except ImportError:
    print("[WARN] chassis_comm 未找到，底盘功能不可用")

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: Optional[cv2.VideoCapture] = None
camera_lock = threading.Lock()
device_id_global = 0
output_dir: str = ""
capture_count = 0
recent_captures: deque = deque(maxlen=8)

_frame_count = 0
_fps_ts = time.time()
current_fps = 0.0
camera_connected = False
_fail_streak = 0
_last_reconnect = 0.0


# ─── 摄像头操作 ───────────────────────────────────────────────────────────────

def _try_open(device_id: int) -> Optional[cv2.VideoCapture]:
    apis = [cv2.CAP_DSHOW] if sys.platform == "win32" else [cv2.CAP_V4L2]
    for api in apis + [cv2.CAP_ANY]:
        cap = cv2.VideoCapture(device_id, api)
        if cap.isOpened():
            break
    else:
        return None

    for s in ("Y800", "GREY", "Y8  "):
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*s))

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  CAMERA_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS,          CAMERA_FPS)
    cap.set(cv2.CAP_PROP_CONVERT_RGB,  0)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"[INFO] 摄像头已连接: {w}×{h} (设备 {device_id})")
    return cap


def _read_gray(cap: cv2.VideoCapture) -> Optional[np.ndarray]:
    ret, frame = cap.read()
    if not ret:
        return None
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    if frame.shape != (CAMERA_HEIGHT, CAMERA_WIDTH):
        n = CAMERA_WIDTH * CAMERA_HEIGHT
        flat = frame.flatten()
        frame = (flat[:n].reshape(CAMERA_HEIGHT, CAMERA_WIDTH)
                 if flat.size >= n
                 else cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT)))
    return frame


def _save_capture(frame: np.ndarray, fmt: str) -> dict:
    global capture_count
    tw, th = CAPTURE_FORMATS[fmt]
    h, w = frame.shape[:2]
    if fmt == "640x360":
        x = max(0, (w - tw) // 2)
        y = max(0, (h - th) // 2)
        out = frame[y:y + th, x:x + tw]
    else:
        out = frame.copy()
    if out.shape != (th, tw):
        out = cv2.resize(out, (tw, th))

    capture_count += 1
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    name = f"capture_{ts}_{capture_count:04d}_{fmt}.png"
    path = os.path.join(output_dir, name)
    cv2.imwrite(path, out)

    thumb = cv2.resize(out, (160, 90 if fmt == "640x360" else 80))
    _, buf = cv2.imencode(".jpg", thumb, [cv2.IMWRITE_JPEG_QUALITY, 72])
    thumb_b64 = base64.b64encode(buf).decode()

    info = {
        "filename": name,
        "format": fmt,
        "size": f"{tw}×{th}",
        "thumb": thumb_b64,
        "time": datetime.now().strftime("%H:%M:%S"),
        "index": capture_count,
    }
    recent_captures.appendleft(info)
    print(f"[CAPTURE] {path}  ({tw}×{th})")
    return info


# ─── 视频流 ───────────────────────────────────────────────────────────────────

def generate_frames():
    global camera, current_fps, _frame_count, _fps_ts
    global camera_connected, _fail_streak, _last_reconnect

    FAIL_THRESH      = 10
    RECONNECT_GAP    = 3.0

    while True:
        with camera_lock:
            cap = camera

        frame = _read_gray(cap) if cap else None

        if frame is None:
            camera_connected = False
            _fail_streak += 1
            now = time.time()
            if (_fail_streak >= FAIL_THRESH
                    and now - _last_reconnect > RECONNECT_GAP):
                _last_reconnect = now
                _fail_streak = 0
                print("[INFO] 自动重连摄像头...")
                with camera_lock:
                    if camera:
                        camera.release()
                    camera = _try_open(device_id_global)
                if camera:
                    print("[INFO] 摄像头自动重连成功")
                    camera_connected = True

            # 占位黑帧
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH), dtype=np.uint8)
            cv2.putText(blk, "No Signal  —  Reconnecting...",
                        (CAMERA_WIDTH // 2 - 200, CAMERA_HEIGHT // 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.9, (60, 60, 60), 2)
            disp = cv2.cvtColor(blk, cv2.COLOR_GRAY2BGR)
            _, buf = cv2.imencode(".jpg", disp, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                   + buf.tobytes() + b"\r\n")
            time.sleep(0.35)
            continue

        camera_connected = True
        _fail_streak = 0

        # FPS 计算
        _frame_count += 1
        now = time.time()
        if now - _fps_ts >= 1.0:
            current_fps = _frame_count / (now - _fps_ts)
            _frame_count = 0
            _fps_ts = now

        # 绘制叠加层
        disp = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
        h, w = disp.shape[:2]
        cx, cy = w // 2, h // 2

        # 640×360 训练裁剪框
        x1, y1, x2, y2 = cx - 320, cy - 180, cx + 320, cy + 180
        cv2.rectangle(disp, (x1, y1), (x2, y2), (45, 210, 100), 1)
        cv2.putText(disp, "640x360 train", (x1 + 6, y1 + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.52, (45, 210, 100), 1)

        # FPS 水印
        cv2.putText(disp, f"FPS {current_fps:.1f}", (w - 100, 24),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1)

        _, buf = cv2.imencode(".jpg", disp, [cv2.IMWRITE_JPEG_QUALITY, 82])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
               + buf.tobytes() + b"\r\n")


# ─── Flask 路由 ───────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("companion_ui.html", output_dir=output_dir)


@app.route("/video_feed")
def video_feed():
    return Response(generate_frames(),
                    mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/capture", methods=["POST"])
def do_capture():
    data = request.get_json(silent=True) or {}
    fmt = data.get("format", "1280x720")
    if fmt not in CAPTURE_FORMATS:
        return jsonify({"success": False, "error": f"不支持的格式: {fmt}"})
    with camera_lock:
        cap = camera
    frame = _read_gray(cap) if cap else None
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})
    info = _save_capture(frame, fmt)
    return jsonify({"success": True, **info})


@app.route("/refresh_camera", methods=["POST"])
def refresh_camera():
    global camera, _fail_streak
    with camera_lock:
        if camera:
            camera.release()
        camera = _try_open(device_id_global)
        ok = camera is not None and camera.isOpened()
    _fail_streak = 0
    if ok:
        print("[INFO] 摄像头手动刷新成功")
        return jsonify({"success": True, "message": "摄像头已重新连接"})
    print("[WARN] 摄像头刷新失败")
    return jsonify({"success": False, "error": "无法连接到摄像头设备"})


@app.route("/recent_captures")
def recent_captures_api():
    return jsonify(list(recent_captures))


@app.route("/status")
def status():
    with camera_lock:
        connected = camera is not None and camera.isOpened()
    return jsonify({
        "connected": connected,
        "capture_count": capture_count,
        "fps": round(current_fps, 1),
        "output_dir": output_dir,
        "camera_resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
    })


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    global camera, output_dir, device_id_global

    parser = argparse.ArgumentParser(description="Aurora Companion — A1 摄像头可视化采集伴侣")
    parser.add_argument("--device", type=int, default=0,    help="摄像头设备 ID (默认: 0)")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw/images",
                        help="拍照保存目录")
    parser.add_argument("--port",   type=int, default=5001, help="Web 服务端口 (默认: 5001)")
    parser.add_argument("--host",   type=str, default="0.0.0.0", help="监听地址")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    device_id_global = args.device
    print(f"[INFO] 正在连接 A1 摄像头 (设备 {args.device})...")
    camera = _try_open(args.device)
    if camera is None:
        print("[WARN] 摄像头未连接，工具仍可启动，请连接后点击「刷新摄像头」")

    print(f"[INFO] Aurora Companion 已启动: http://localhost:{args.port}")
    print(f"[INFO] 快捷键: 1=1280×720  2=640×360  R=刷新摄像头")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
