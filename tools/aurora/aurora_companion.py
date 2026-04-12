#!/usr/bin/env python3
"""
Aurora Companion — A1 开发板摄像头可视化采集伴侣

增强版拍照工具，在 aurora_capture 基础上提供：
  - 精心设计的现代暗色玻璃态 UI
  - 摄像头断联自动检测 + 一键刷新恢复
  - 实时 FPS 及连接状态显示
  - 最近拍摄缩略图画廊 (最多 8 张)
  - 传感器采集 1280×720 灰度图，训练输出 640×360 (中心裁剪)
  - A1↔STM32 底盘通信调试 + 联通性测试
  - 键盘快捷键：1/2/R

用法:
    python aurora_companion.py [--device 0] [--output ../../data/yolov8_dataset/raw/images] [--port 5801]
"""

import argparse
import base64
import contextlib
import os
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
# SC132GS 传感器采集为 1280×720 灰度帧
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

CAPTURE_FORMATS = {
    "1280x720": (1280, 720),   # 原始灰度图（传感器采集分辨率）
    "640x360":  (640,  360),   # YOLOv8 训练集尺寸（16:9 中心裁剪）
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
MAX_DEVICE_SCAN = 5
PREFERRED_DEVICE_FILE = Path(__file__).with_name(".a1_camera_device")


# ─── 摄像头操作 ───────────────────────────────────────────────────────────────

@contextlib.contextmanager
def _suppress_c_stderr():
    """临时屏蔽 C/DLL 层 stderr（摄像头驱动初始化噪声），仅 Windows 生效。"""
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


def _open_raw_camera(device_id: int) -> cv2.VideoCapture:
    with _suppress_c_stderr():
        cap = cv2.VideoCapture(device_id, cv2.CAP_DSHOW if sys.platform == "win32" else cv2.CAP_V4L2)
        if not cap.isOpened():
            cap = cv2.VideoCapture(device_id)
    return cap


def load_preferred_device() -> Optional[int]:
    try:
        if not PREFERRED_DEVICE_FILE.exists():
            return None
        text = PREFERRED_DEVICE_FILE.read_text(encoding="utf-8").strip()
        if text == "":
            return None
        return int(text)
    except Exception:
        return None


def save_preferred_device(device_id: int) -> None:
    try:
        PREFERRED_DEVICE_FILE.write_text(str(device_id), encoding="utf-8")
    except Exception:
        pass


def probe_camera_device(device_id: int) -> dict:
    cap = _open_raw_camera(device_id)
    if not cap.isOpened():
        return {
            "id": device_id,
            "opened": False,
            "score": -1,
            "actual_width": 0,
            "actual_height": 0,
            "is_grayscale": False,
            "has_content": False,
            "supports_gray_fourcc": False,
        }

    with _suppress_c_stderr():
        gray_fourccs = [
            cv2.VideoWriter_fourcc(*"Y800"),
            cv2.VideoWriter_fourcc(*"GREY"),
            cv2.VideoWriter_fourcc(*"Y8  "),
        ]
        supports_gray_fourcc = False
        for fourcc in gray_fourccs:
            cap.set(cv2.CAP_PROP_FOURCC, fourcc)
            actual = int(cap.get(cv2.CAP_PROP_FOURCC))
            if actual == fourcc:
                supports_gray_fourcc = True
                break

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
        ret, frame = cap.read()
        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        cap.release()

    if not ret or frame is None:
        return {
            "id": device_id,
            "opened": False,
            "score": 0,
            "actual_width": actual_w,
            "actual_height": actual_h,
            "is_grayscale": False,
            "has_content": False,
            "supports_gray_fourcc": supports_gray_fourcc,
        }

    frame_h, frame_w = frame.shape[:2]
    if len(frame.shape) == 2:
        is_grayscale = True
    elif frame.shape[2] >= 3:
        b, g, r = frame[:, :, 0], frame[:, :, 1], frame[:, :, 2]
        is_grayscale = np.array_equal(b, g) and np.array_equal(g, r)
    else:
        is_grayscale = False

    check = frame if len(frame.shape) == 2 else frame[:, :, 0]
    has_content = float(np.std(check.astype(np.float32))) > 3.0

    score = 0
    if supports_gray_fourcc:
        score += 8
    if frame_w == CAMERA_WIDTH and frame_h == CAMERA_HEIGHT:
        score += 6
    elif frame_w == 640 and frame_h == 360:
        score += 2
    if frame_w == 360 and frame_h == 1280:
        score += 5
    if frame_w == 720 and frame_h == 1280:
        score += 4
    if is_grayscale:
        score += 8
    else:
        score -= 4
    if has_content:
        score += 2
    else:
        score -= 6
    if frame_w == CAMERA_WIDTH and frame_h == CAMERA_HEIGHT and (not is_grayscale) and (not supports_gray_fourcc):
        score -= 3

    return {
        "id": device_id,
        "opened": True,
        "score": score,
        "actual_width": frame_w,
        "actual_height": frame_h,
        "is_grayscale": is_grayscale,
        "has_content": has_content,
        "supports_gray_fourcc": supports_gray_fourcc,
    }


def list_camera_devices(max_scan: int = MAX_DEVICE_SCAN) -> list:
    devices = []
    for i in range(max_scan):
        info = probe_camera_device(i)
        if info["opened"]:
            devices.append(info)
    return devices


def choose_camera_device(requested_device: int) -> Tuple[int, list]:
    """返回 (device_id, candidates)。requested_device=-1 时自动优先 A1。"""
    if requested_device >= 0:
        return requested_device, list_camera_devices()

    preferred = load_preferred_device()
    if preferred is not None:
        preferred_info = probe_camera_device(preferred)
        if preferred_info["opened"] and (preferred_info.get("has_content") or preferred_info.get("supports_gray_fourcc")):
            return preferred, [preferred_info]

    candidates = list_camera_devices()
    if not candidates:
        return 0, []

    candidates_sorted = sorted(candidates, key=lambda x: (x["score"], x["id"]), reverse=True)
    return candidates_sorted[0]["id"], candidates_sorted


def _try_open(device_id: int) -> Optional[cv2.VideoCapture]:
    cap = _open_raw_camera(device_id)
    if not cap.isOpened():
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
    if frame.shape == (CAMERA_HEIGHT, CAMERA_WIDTH):
        return frame
    # 尺寸不符时直接缩放（EVB 固件已更新，不再有竖屏旋转需求）
    return cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT))


def crop_center(img: np.ndarray, target_w: int, target_h: int) -> np.ndarray:
    """从图像中心裁剪到目标尺寸"""
    h, w = img.shape[:2]
    if target_w >= w and target_h >= h:
        return img
    x_start = max(0, (w - target_w) // 2)
    y_start = max(0, (h - target_h) // 2)
    return img[y_start:y_start + target_h, x_start:x_start + target_w]


def _save_capture(frame: np.ndarray, fmt: str) -> dict:
    global capture_count
    tw, th = CAPTURE_FORMATS[fmt]

    if fmt == "640x360":
        # 传感器 1280×720 → 中心裁剪 640×360
        out = crop_center(frame, tw, th)
    else:
        out = frame.copy()

    # 确保尺寸正确
    if out.shape[1] != tw or out.shape[0] != th:
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


@app.route("/camera_devices")
def camera_devices():
    devices = list_camera_devices()
    items = []
    for d in devices:
        kind = "Gray" if d.get("is_grayscale") else "Color"
        y8 = "Y8" if d.get("supports_gray_fourcc") else "NoY8"
        label = f"device {d['id']} - {d['actual_width']}x{d['actual_height']} {kind} {y8} score={d['score']}"
        items.append({
            "id": d["id"],
            "label": label,
            "score": d["score"],
            "actual_width": d["actual_width"],
            "actual_height": d["actual_height"],
            "is_grayscale": d.get("is_grayscale", False),
            "supports_gray_fourcc": d.get("supports_gray_fourcc", False),
        })
    return jsonify({"current_device": device_id_global, "devices": items})


@app.route("/switch_camera", methods=["POST"])
def switch_camera():
    global camera, device_id_global, _fail_streak, camera_connected

    data = request.get_json(silent=True) or {}
    if "device" not in data:
        return jsonify({"success": False, "error": "缺少 device 参数"})

    try:
        new_device = int(data.get("device"))
    except (TypeError, ValueError):
        return jsonify({"success": False, "error": "device 必须是整数"})

    with camera_lock:
        old_camera = camera
        new_camera = _try_open(new_device)
        if new_camera is None:
            return jsonify({"success": False, "error": "目标摄像头无法打开"})

        camera = new_camera
        device_id_global = new_device
        if old_camera:
            old_camera.release()

    _fail_streak = 0
    camera_connected = True
    save_preferred_device(new_device)
    print(f"[INFO] 已切换到摄像头设备 {new_device}")
    return jsonify({"success": True, "device": new_device})


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
    parser.add_argument("--device", type=int, default=-1,   help="摄像头设备 ID (默认: -1 自动优先 A1)")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw/images",
                        help="拍照保存目录")
    parser.add_argument("--port",   type=int, default=5801, help="Web 服务端口 (默认: 5801)")
    parser.add_argument("--host",   type=str, default="0.0.0.0", help="监听地址")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    selected_device, candidates = choose_camera_device(args.device)
    device_id_global = selected_device

    if args.device < 0:
        if candidates:
            print("[INFO] 自动探测摄像头结果（按优先级）:")
            for c in candidates:
                kind = "Gray" if c.get("is_grayscale") else "Color"
                y8 = "Y8" if c.get("supports_gray_fourcc") else "NoY8"
                print(f"  - device {c['id']}: {c['actual_width']}x{c['actual_height']} {kind} {y8} score={c['score']}")
            print(f"[INFO] 自动选择设备: {selected_device}")
        else:
            print("[WARN] 未探测到可用摄像头，回退到设备 0")

    print(f"[INFO] 正在连接 A1 摄像头 (设备 {selected_device})...")
    camera = _try_open(selected_device)
    if camera is None:
        print("[WARN] 摄像头未连接，工具仍可启动，请连接后点击「刷新摄像头」")
    else:
        save_preferred_device(selected_device)

    print(f"[INFO] Aurora Companion 已启动: http://localhost:{args.port}")
    print(f"[INFO] 快捷键: 1=1280×720  2=640×360  R=刷新摄像头")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
