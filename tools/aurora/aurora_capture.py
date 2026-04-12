#!/usr/bin/env python3
"""
Aurora Capture Tool — A1 开发板摄像头拍照工具

通过 USB Type-C 连接 A1 开发板，接收 SC132GS 摄像头的灰度视频流。
传感器采集分辨率为 1280×720 (16:9)，训练输出使用 640×360。

功能:
  - 实时预览摄像头画面
    - 拍照保存 1280×720 原始灰度图 (传感器采集)
    - 拍照保存 640×360 训练灰度图 (中心裁剪)
  - Web 前端界面选择拍照格式
  - 摄像头断联自动检测与一键刷新恢复

注意:
    SC132GS 传感器采集 16:9 的 1280×720 灰度图。
  若 EVB 固件未更新，摄像头可能以 YUYV 竖屏格式输出 360×1280，
  此时工具会自动旋转 + 缩放到 640×360。

用法:
    python aurora_capture.py [--device -1] [--output ../data/yolov8_dataset/raw/images]
"""

import argparse
import contextlib
import os
import sys
import threading
import time
import webbrowser
from datetime import datetime
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template_string, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
# SC132GS 通过 USB Type-C 传输，传感器采集为 16:9 的 1280×720
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

# 拍照输出格式
CAPTURE_FORMATS = {
    "1280x720": (1280, 720),   # 原始灰度图 (传感器采集分辨率)
    "640x360":  (640, 360),    # YOLOv8 训练集尺寸（中心裁剪）
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
_orientation_warned = False
MAX_DEVICE_SCAN = 4
PREFERRED_DEVICE_FILE = Path(__file__).with_name(".a1_camera_device")


@contextlib.contextmanager
def _suppress_c_stderr():
    """临时屏蔽 C/DLL 层 stderr（如摄像头驱动初始化日志），仅 Windows 生效。"""
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
        yield  # 降级：不抑制


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
    """探测摄像头并给出是否更像 A1 的评分。"""
    cap = _open_raw_camera(device_id)
    if not cap.isOpened():
        return {
            "id": device_id,
            "opened": False,
            "score": -1,
            "actual_width": 0,
            "actual_height": 0,
            "is_grayscale": False,
            "label": f"设备 {device_id} (不可用)",
        }

    with _suppress_c_stderr():
        gray_fourccs = [
            cv2.VideoWriter_fourcc(*'Y800'),
            cv2.VideoWriter_fourcc(*'GREY'),
            cv2.VideoWriter_fourcc(*'Y8  '),
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
            "supports_gray_fourcc": False,
            "label": f"设备 {device_id} ({actual_w}x{actual_h}, 无帧)",
        }

    frame_h, frame_w = frame.shape[:2]

    # 检测是否为灰度内容：三通道完全相等 → 灰度源（A1 SC132GS 特征）
    if len(frame.shape) == 2:
        is_grayscale = True
    elif frame.shape[2] >= 3:
        b, g, r = frame[:, :, 0], frame[:, :, 1], frame[:, :, 2]
        is_grayscale = np.array_equal(b, g) and np.array_equal(g, r)
    else:
        is_grayscale = False

    # 检测画面是否有内容（标准差 > 阈值 → 非空帧/非虚拟摄像头）
    check = frame if len(frame.shape) == 2 else frame[:, :, 0]
    has_content = float(np.std(check.astype(np.float32))) > 3.0

    score = 0
    if supports_gray_fourcc:
        score += 8   # Aurora 以灰度源为主，优先支持灰度 FOURCC 的设备
    if frame_w == CAMERA_WIDTH and frame_h == CAMERA_HEIGHT:
        score += 6   # 传感器采集分辨率匹配
    elif frame_w == 640 and frame_h == 360:
        score += 2   # 次优分辨率
    if frame_w == 360 and frame_h == 1280:
        score += 5   # Aurora 日志中常见的竖屏灰度源尺寸
    if frame_w == 720 and frame_h == 1280:
        score += 4   # Aurora pipeline 运行中常见的灰度缓冲尺寸
    if is_grayscale:
        score += 8   # 灰度内容优先，尽量贴近 Aurora 的 gray source 行为
    else:
        score -= 4   # 彩色设备（常见内置/虚拟摄像头）降权
    if has_content:
        score += 2   # 非空帧 → 非虚拟摄像头
    else:
        score -= 6   # 空帧设备显著降权
    if frame_w == CAMERA_WIDTH and frame_h == CAMERA_HEIGHT and (not is_grayscale) and (not supports_gray_fourcc):
        score -= 3   # 1280x720 彩色且非 Y8，多为普通/虚拟摄像头

    display_type = "灰度" if is_grayscale else "彩色"
    return {
        "id": device_id,
        "opened": True,
        "score": score,
        "actual_width": frame_w,
        "actual_height": frame_h,
        "is_grayscale": is_grayscale,
        "has_content": has_content,
        "supports_gray_fourcc": supports_gray_fourcc,
        "label": f"设备 {device_id} ({frame_w}x{frame_h}, {display_type}, {'Y8' if supports_gray_fourcc else '非Y8'})",
    }


def list_camera_devices(max_scan: int = MAX_DEVICE_SCAN) -> list:
    devices = []
    for i in range(max_scan):
        info = probe_camera_device(i)
        if info["opened"]:
            devices.append(info)
    return devices


def choose_camera_device(requested_device: int) -> tuple:
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
    best = candidates_sorted[0]["id"]
    return best, candidates_sorted


def open_camera(device_id: int) -> Optional[cv2.VideoCapture]:
    """打开 A1 开发板摄像头 (SC132GS via USB Type-C)

    关键: 必须显式设置 FOURCC 为 GREY/Y800 以正确解析灰度格式,
    否则 OpenCV 会按默认的 YUV/RGB 解析。摄像头目标分辨率为 1280×720。
    """
    cap = _open_raw_camera(device_id)

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

    摄像头标准输出为 1280×720。
    若遇到旧 EVB/SDK 的竖屏缓冲（如 360×1280 / 720×1280），先自动旋转再兜底缩放。
    """
    global _orientation_warned

    ret, frame = cap.read()
    if not ret:
        return None

    # 如果读出来是彩色的，转灰度
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    src_h, src_w = frame.shape[:2]
    corrected = frame

    if src_h > src_w:
        corrected = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        if not _orientation_warned:
            dst_h, dst_w = corrected.shape[:2]
            print(f"[WARN] 检测到竖屏帧 {src_w}x{src_h}，已自动旋转为 {dst_w}x{dst_h}。建议升级 EVB/SDK 以输出原生 1280x720。")
            _orientation_warned = True

    # 已是目标尺寸，直接返回
    if corrected.shape == (CAMERA_HEIGHT, CAMERA_WIDTH):
        return corrected

    # 通用兜底：直接缩放
    return cv2.resize(corrected, (CAMERA_WIDTH, CAMERA_HEIGHT))


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
        # 传感器 1280×720 -> 中心裁剪 640×360（保持 16:9）
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
                    <div class="divider"></div>
                    <div style="display:flex;gap:8px;align-items:center;margin-top:8px">
                        <select id="cameraSelect" style="flex:1;background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:6px;padding:8px"></select>
                        <button class="btn btn-secondary" style="width:auto;margin:0" onclick="switchCamera()">
                            <span class="btn-icon">🎥</span>
                            <span>切换</span>
                        </button>
                    </div>
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

        function loadCameraDevices() {
            fetch('/camera_devices')
            .then(r => r.json())
            .then(data => {
                const sel = document.getElementById('cameraSelect');
                const current = data.current_device;
                sel.innerHTML = '';
                (data.devices || []).forEach(d => {
                    const op = document.createElement('option');
                    op.value = d.id;
                    op.textContent = d.label;
                    if (d.id === current) op.selected = true;
                    sel.appendChild(op);
                });
            })
            .catch(() => {});
        }

        function switchCamera() {
            const sel = document.getElementById('cameraSelect');
            const device = Number(sel.value);
            if (Number.isNaN(device)) {
                showToast('✗ 请选择有效摄像头', true);
                return;
            }
            fetch('/switch_camera', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({device})
            })
            .then(r => r.json())
            .then(data => {
                showToast(data.success ? ('✓ 已切换到设备 ' + device) : ('✗ ' + data.error), !data.success);
                if (data.success) {
                    const img = document.getElementById('stream');
                    img.src = '/video_feed?' + Date.now();
                    loadCameraDevices();
                }
            })
            .catch(err => showToast('✗ 切换失败: ' + err, true));
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

        loadCameraDevices();
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
        "device_id": device_id_global,
    })


@app.route('/camera_devices')
def camera_devices():
    devices = list_camera_devices()
    return jsonify({
        "devices": [{"id": d["id"], "label": d["label"]} for d in devices],
        "current_device": device_id_global,
    })


@app.route('/switch_camera', methods=['POST'])
def switch_camera():
    global camera, device_id_global, _consecutive_failures
    data = request.get_json(silent=True) or {}
    if "device" not in data:
        return jsonify({"success": False, "error": "缺少 device 参数"}), 400

    try:
        new_device = int(data["device"])
    except (TypeError, ValueError):
        return jsonify({"success": False, "error": "device 必须是整数"}), 400

    try:
        with camera_lock:
            if camera:
                camera.release()
            camera = open_camera(new_device)
            ok = camera is not None and camera.isOpened()
        if not ok:
            raise RuntimeError("目标摄像头无法打开")
        device_id_global = new_device
        save_preferred_device(new_device)
        _consecutive_failures = 0
        print(f"[INFO] 已切换到摄像头设备 {new_device}")
        return jsonify({"success": True, "device": new_device})
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500


def main():
    global camera, output_dir

    parser = argparse.ArgumentParser(description="Aurora Capture - A1 摄像头拍照工具")
    parser.add_argument("--device", type=int, default=-1,
                        help="摄像头设备 ID (默认: -1, 自动优先 A1)")
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
    selected_device, candidates = choose_camera_device(args.device)
    device_id_global = selected_device
    if args.device < 0:
        print("[INFO] 自动选择摄像头候选:")
        for item in candidates:
            print(f"[INFO]   - {item['label']} (score={item['score']})")

    print(f"[INFO] 正在连接 A1 摄像头 (设备 {selected_device})...")
    camera = open_camera(selected_device)
    save_preferred_device(selected_device)

    url = f"http://localhost:{args.port}"

    # 1.5 秒后自动打开浏览器（等 Flask 完成绑定）
    def _open_browser():
        time.sleep(1.5)
        webbrowser.open_new_tab(url)
    threading.Thread(target=_open_browser, daemon=True).start()

    # 突出显示 Web 地址，避免被 Flask/驱动日志淹没
    border = "=" * 54
    print(f"\n{border}")
    print(f"  Aurora Capture 已启动")
    print(f"  Web 界面: {url}")
    print(f"  快捷键:  1=1280x720  2=640x360  R=刷新  切换摄像头见下拉")
    print(f"{border}\n")

    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
