#!/usr/bin/env python3
"""
A1 Viewer — A1 开发板实时结果显示工具

用途：
  接收 A1 开发板经 USB-UVC 上传的视频流，板端已在 NPU 完成 YOLOv8 推理
  并通过硬件 OSD 将检测框烧录在帧中——本工具不再做任何推理，仅负责
  流转发与显示。

  前端基于 MJPEG + WebSocket，为平板/触摸设备优化，
  预留了 3D 点云、雷达、遥测等扩展面板的接口。

用法:
    python a1_viewer.py [--device 0] [--port 5802] [--host 0.0.0.0]
"""

import argparse
import contextlib
import os
import sys
import threading
import time
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
CAMERA_WIDTH  = 1280
CAMERA_HEIGHT = 720
PREFERRED_DEVICE_FILE = Path(__file__).with_name(".a1_camera_device")

app = Flask(__name__, template_folder="templates")

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: Optional[cv2.VideoCapture] = None
camera_lock = threading.Lock()
device_id_global: int = 0
camera_connected: bool = False

_latest_frame: Optional[np.ndarray] = None
_frame_lock = threading.Lock()
_capture_running: bool = False
_capture_thread: Optional[threading.Thread] = None

_fps_count  = 0
_fps_ts     = time.time()
current_fps = 0.0


# ─── 工具函数 ─────────────────────────────────────────────────────────────────

@contextlib.contextmanager
def _suppress_c_stderr():
    if sys.platform != "win32":
        yield
        return
    try:
        devnull_fd = os.open(os.devnull, os.O_WRONLY)
        saved = os.dup(2)
        os.dup2(devnull_fd, 2)
        try:
            yield
        finally:
            os.dup2(saved, 2)
            os.close(saved)
            os.close(devnull_fd)
    except Exception:
        yield


def _open_camera(device_id: int) -> Optional[cv2.VideoCapture]:
    with _suppress_c_stderr():
        cap = cv2.VideoCapture(
            device_id,
            cv2.CAP_DSHOW if sys.platform == "win32" else cv2.CAP_V4L2,
        )
        if not cap.isOpened():
            cap = cv2.VideoCapture(device_id)
    if not cap.isOpened():
        return None
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  CAMERA_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"[INFO] A1 摄像头已连接: {w}×{h} (设备 {device_id})")
    return cap


# ─── 专用抓帧线程 ─────────────────────────────────────────────────────────────
# DirectShow 要求 cap.read() 必须在创建 VideoCapture 的同一 OS 线程中调用。
# 本线程是唯一调用 cap.read() 的地方。Flask MJPEG generator 只读 _latest_frame。

def _capture_thread_func(device_id: int) -> None:
    global _latest_frame, camera, camera_connected, current_fps, _fps_count, _fps_ts
    cap = _open_camera(device_id)
    with camera_lock:
        camera = cap
    if cap is None:
        camera_connected = False
        print(f"[ERROR] 无法打开摄像头设备 {device_id}")
        return
    camera_connected = True
    while _capture_running:
        ret, frame = cap.read()
        if ret and frame is not None:
            fh, fw = frame.shape[:2]
            if fh > fw:
                frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
            with _frame_lock:
                _latest_frame = frame
            camera_connected = True
            # FPS 计算
            _fps_count += 1
            now = time.time()
            if now - _fps_ts >= 1.0:
                current_fps = _fps_count / (now - _fps_ts)
                _fps_count  = 0
                _fps_ts     = now
        else:
            camera_connected = False
            time.sleep(0.05)
    cap.release()
    with camera_lock:
        camera = None


def _start_capture_thread(device_id: int) -> None:
    global _capture_running, _capture_thread, _latest_frame
    _stop_capture_thread()
    _latest_frame    = None
    _capture_running = True
    _capture_thread  = threading.Thread(
        target=_capture_thread_func,
        args=(device_id,),
        daemon=True,
        name=f"A1CaptureThread-{device_id}",
    )
    _capture_thread.start()


def _stop_capture_thread() -> None:
    global _capture_running, _capture_thread
    _capture_running = False
    if _capture_thread and _capture_thread.is_alive():
        _capture_thread.join(timeout=2.0)
    _capture_thread = None


# ─── MJPEG 生成器 ─────────────────────────────────────────────────────────────

def _generate_stream():
    """原样转发 A1 帧（OSD 已由板端硬件烧录），不做任何推理。"""
    while True:
        with _frame_lock:
            raw = _latest_frame.copy() if _latest_frame is not None else None

        if raw is None:
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH, 3), dtype=np.uint8)
            cv2.putText(blk, "Waiting for A1 board...",
                        (CAMERA_WIDTH // 2 - 190, CAMERA_HEIGHT // 2 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.1, (60, 140, 200), 2)
            cv2.putText(blk, "Connect A1 via USB and press Reconnect",
                        (CAMERA_WIDTH // 2 - 250, CAMERA_HEIGHT // 2 + 40),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.65, (60, 60, 80), 1)
            _, buf = cv2.imencode(".jpg", blk, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                   + buf.tobytes() + b"\r\n")
            time.sleep(0.2)
            continue

        # BGR → 确保三通道（A1 输出已是彩色）
        frame = raw if len(raw.shape) == 3 else cv2.cvtColor(raw, cv2.COLOR_GRAY2BGR)
        _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 85])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
               + buf.tobytes() + b"\r\n")


# ─── Flask 路由 ───────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("a1_viewer.html")


@app.route("/stream")
def stream():
    return Response(
        _generate_stream(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
    )


@app.route("/status")
def status():
    return jsonify({
        "connected":  camera_connected,
        "fps":        round(current_fps, 1),
        "device":     device_id_global,
        "resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
    })


@app.route("/reconnect", methods=["POST"])
def reconnect():
    """停止并重新启动抓帧线程（在线程内重新打开摄像头）。"""
    _stop_capture_thread()
    time.sleep(0.3)
    _start_capture_thread(device_id_global)
    time.sleep(1.5)
    return jsonify({"success": camera_connected, "connected": camera_connected})


# ─── 数据推送扩展占位 ─────────────────────────────────────────────────────────
# 未来可在此添加 WebSocket / SSE 端点，用于：
#   - 3D 点云数据（LiDAR 或深度传感器）
#   - 底盘遥测（速度、里程、IMU）
#   - 检测结果 JSON（若需要前端二次处理）

@app.route("/api/pointcloud")
def api_pointcloud():
    """点云数据接口（占位）。实现时替换本函数返回实际数据。"""
    return jsonify({
        "status": "placeholder",
        "message": "Point cloud API not yet connected. Implement this endpoint to push LiDAR/depth data.",
        "format":  "expected: {points: [[x,y,z,intensity], ...], timestamp: float}",
    })


@app.route("/api/telemetry")
def api_telemetry():
    """底盘遥测接口（占位）。"""
    return jsonify({
        "status": "placeholder",
        "message": "Telemetry API not yet connected.",
        "format":  "expected: {vx, vy, omega, battery_pct, timestamp}",
    })


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    global device_id_global

    parser = argparse.ArgumentParser(
        description="A1 Viewer — 接收 A1 开发板 YOLOv8+OSD 视频流并在浏览器显示"
    )
    parser.add_argument("--device", type=int, default=-1,
                        help="摄像头设备 ID (默认: -1 自动选择)")
    parser.add_argument("--port",   type=int, default=5802,
                        help="Web 服务端口 (默认: 5802)")
    parser.add_argument("--host",   type=str, default="0.0.0.0",
                        help="监听地址")
    args = parser.parse_args()

    # 自动选设备
    if args.device >= 0:
        device_id_global = args.device
    else:
        try:
            device_id_global = int(
                PREFERRED_DEVICE_FILE.read_text(encoding="utf-8").strip()
            )
        except Exception:
            device_id_global = 0

    print(f"[INFO] A1 Viewer 启动，设备 {device_id_global}，端口 {args.port}")
    _start_capture_thread(device_id_global)
    time.sleep(1.0)
    if not camera_connected:
        print("[WARN] 摄像头未就绪，请连接 A1 后点击页面中的 Reconnect")
    print(f"[INFO] 打开浏览器访问: http://localhost:{args.port}")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
