#!/usr/bin/env python3
"""
Aurora Companion — Windows/A1 摄像头可视化采集伴侣

增强版拍照工具，在 aurora_capture 基础上提供：
  - 精心设计的现代暗色玻璃态 UI
  - 摄像头断联自动检测 + 一键刷新恢复
  - 实时 FPS 及连接状态显示
  - 最近拍摄缩略图画廊 (最多 8 张)
    - Windows 纯预览 / A1 采集源，训练输出 640×360 (中心裁剪)
  - A1↔STM32 底盘通信调试 + 联通性测试
  - 键盘快捷键：1/2/R

用法:
    python aurora_companion.py [--device 0] [--output ../../data/yolov8_dataset/raw] [--port 5801]
"""

import argparse
import base64
import contextlib
import ctypes
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import tkinter as tk
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, Optional, Tuple
from urllib import error as urllib_error
from urllib import parse as urllib_parse
from urllib import request as urllib_request

import cv2
import numpy as np
from PIL import ImageGrab, ImageTk
from flask import Flask, Response, jsonify, render_template, request

# 可选：onnxruntime（用于 YOLOv8 检测流）
try:
    import onnxruntime as ort
    _ORT_AVAILABLE = True
except ImportError:
    _ORT_AVAILABLE = False

_THIRD_PARTY_ROOT = Path(__file__).resolve().parents[2] / "third_party"
if _THIRD_PARTY_ROOT.exists() and str(_THIRD_PARTY_ROOT) not in sys.path:
    sys.path.insert(0, str(_THIRD_PARTY_ROOT))

try:
    from ultralytics import YOLO as UltralyticsYOLO

    _ULTRALYTICS_AVAILABLE = True
except Exception:
    UltralyticsYOLO = None
    _ULTRALYTICS_AVAILABLE = False

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
# SC132GS 传感器采集为 1280×720 灰度帧
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

CAPTURE_FORMATS = {
    "1280x720": (1280, 720),   # 原始灰度图（传感器采集分辨率）
    "640x360":  (640,  360),   # YOLOv8 训练集尺寸（16:9 中心裁剪）
}
DEFAULT_CAPTURE_FORMAT = "1280x720"

app = Flask(__name__, template_folder="templates")
_ros_detection_hook = None
try:
    from chassis_comm import chassis_bp
    app.register_blueprint(chassis_bp)
    print("[INFO] 底盘通信模块已加载")
except ImportError:
    print("[WARN] chassis_comm 未找到，底盘功能不可用")
try:
    from relay_comm import relay_bp
    app.register_blueprint(relay_bp)
    print("[INFO] COM13 经由 A1 控制模块已加载")
except ImportError:
    print("[WARN] relay_comm 未找到，COM13 经由 A1 控制不可用")
try:
    from serial_terminal import serial_term_bp
    app.register_blueprint(serial_term_bp)
    print("[INFO] A1 终端模块已加载")
except ImportError:
    print("[WARN] serial_terminal 未找到，A1 终端模块不可用")
try:
    from ros_bridge import handle_yolo_detections, ros_bp

    _ros_detection_hook = handle_yolo_detections
    app.register_blueprint(ros_bp)
    print("[INFO] ROS 桥接模块已加载")
except ImportError:
    print("[WARN] ros_bridge 未找到，ROS 联动功能不可用")

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: Optional[Any] = None
camera_lock = threading.Lock()
device_id_global = 0
camera_source_global = "auto"
camera_bootstrap_active = False
camera_devices_snapshot: list = []
camera_scan_active = False
camera_scan_error = ""
camera_scan_started_at = 0.0
output_dir: str = ""
capture_count = 0
recent_captures: deque = deque(maxlen=8)
desktop_capture_lock = threading.Lock()
desktop_capture_target: Dict[str, Any] = {
    "mode": "auto_aurora",
    "label": "Aurora 自动窗口",
    "hwnd": None,
    "bbox": None,
    "crop_norm": None,
    "process_name": "Aurora.exe",
    "title": "Aurora",
    "class_name": "",
    "use_client_area": True,
}

_frame_count = 0
_fps_ts = time.time()
current_fps = 0.0
camera_connected = False
_fail_streak = 0
_consecutive_failures = 0
_last_reconnect_time = 0.0

MAX_DEVICE_SCAN = 5
PREFERRED_DEVICE_FILE = Path(__file__).with_name(".a1_camera_device")
PREFERRED_SOURCE_FILE = Path(__file__).with_name(".a1_camera_source")
AURORA_EXE_PATH = Path(__file__).resolve().parents[2] / "Aurora-2.0.0-ciciec.16" / "Aurora.exe"
PREFERRED_A1_CAMERA_NAME = "Smartsens-FlyingChip-A1-1"
QT_BRIDGE_SCRIPT = Path(__file__).with_name("qt_camera_bridge.py")
QT_BRIDGE_PORT = 5911
QT_BRIDGE_HOST = "127.0.0.1"
QT_BRIDGE_URL = f"http://{QT_BRIDGE_HOST}:{QT_BRIDGE_PORT}"
AURORA_WINDOW_DEVICE_ID = -100
CAMERA_SOURCE_WINDOWS = "windows"
CAMERA_SOURCE_A1 = "a1"
CAMERA_SOURCE_AURORA_WINDOW = "aurora_window"
CAMERA_SOURCE_AUTO = "auto"
SOURCE_LABELS = {
    CAMERA_SOURCE_WINDOWS: "Windows 摄像头",
    CAMERA_SOURCE_A1: "A1 开发板",
    CAMERA_SOURCE_AURORA_WINDOW: "Aurora 窗口",
    CAMERA_SOURCE_AUTO: "自动识别",
}
_qt_bridge_process: Optional[subprocess.Popen] = None
_qt_bridge_lock = threading.Lock()

# ─── YOLOv8 检测器（PC 端 ONNX 推理）────────────────────────────────────────
MODEL_ROOT = Path(__file__).parent.parent.parent / "models"
_DETECT_MODEL_PATH = MODEL_ROOT / "best_a1_formal.onnx"
_DETECT_MODEL_MODE = "standard"
_SUPPORTED_DETECT_MODEL_SUFFIXES = (".onnx", ".pt")
_DETECT_MODEL_PREF_FILE = Path(__file__).with_name(".a1_detect_model")
_DETECT_CONF = 0.4
_DETECT_NMS = 0.45
_DETECT_NUM_CLASSES = 4
_DETECT_REG_BINS = 16
_DETECT_TOP_K = 30
_ort_session = None
_pt_model = None
_ort_session_lock = threading.Lock()
_CLASS_NAMES = {0: "person", 1: "forward", 2: "stop", 3: "obstacle_box"}
_CLASS_COLORS = [(0, 200, 80), (80, 140, 255), (255, 160, 50), (255, 80, 80)]
_detect_state_lock = threading.Lock()
_last_detect_snapshot: Dict[str, Any] = {
    "timestamp": 0.0,
    "count": 0,
    "class_counts": {},
    "items": [],
    "ros": None,
    "frame_width": CAMERA_WIDTH,
    "frame_height": CAMERA_HEIGHT,
}


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
        if sys.platform == "win32":
            backends = [cv2.CAP_MSMF, cv2.CAP_DSHOW, cv2.CAP_ANY]
        else:
            backends = [cv2.CAP_V4L2, cv2.CAP_ANY]
        cap = cv2.VideoCapture()
        for backend in backends:
            trial = cv2.VideoCapture(device_id, backend)
            if trial.isOpened():
                cap = trial
                break
            trial.release()
    return cap


if sys.platform == "win32":
    from ctypes import wintypes

    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    GA_ROOT = 2
    GWL_STYLE = -16
    WS_CHILD = 0x40000000
    SM_XVIRTUALSCREEN = 76
    SM_YVIRTUALSCREEN = 77
    SM_CXVIRTUALSCREEN = 78
    SM_CYVIRTUALSCREEN = 79
    VK_ESCAPE = 0x1B
    VK_LBUTTON = 0x01
    SRCCOPY = 0x00CC0020
    BI_RGB = 0
    DIB_RGB_COLORS = 0
    PW_RENDERFULLCONTENT = 0x00000002
    _user32 = ctypes.windll.user32
    _kernel32 = ctypes.windll.kernel32
    _gdi32 = ctypes.windll.gdi32
    _query_full_process_image_name = _kernel32.QueryFullProcessImageNameW
    _query_full_process_image_name.argtypes = [
        wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)
    ]
    _query_full_process_image_name.restype = wintypes.BOOL

    class _RECT(ctypes.Structure):
        _fields_ = [
            ("left", wintypes.LONG),
            ("top", wintypes.LONG),
            ("right", wintypes.LONG),
            ("bottom", wintypes.LONG),
        ]

    class _POINT(ctypes.Structure):
        _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


    class _BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", wintypes.DWORD),
            ("biWidth", wintypes.LONG),
            ("biHeight", wintypes.LONG),
            ("biPlanes", wintypes.WORD),
            ("biBitCount", wintypes.WORD),
            ("biCompression", wintypes.DWORD),
            ("biSizeImage", wintypes.DWORD),
            ("biXPelsPerMeter", wintypes.LONG),
            ("biYPelsPerMeter", wintypes.LONG),
            ("biClrUsed", wintypes.DWORD),
            ("biClrImportant", wintypes.DWORD),
        ]


    class _RGBQUAD(ctypes.Structure):
        _fields_ = [
            ("rgbBlue", ctypes.c_ubyte),
            ("rgbGreen", ctypes.c_ubyte),
            ("rgbRed", ctypes.c_ubyte),
            ("rgbReserved", ctypes.c_ubyte),
        ]


    class _BITMAPINFO(ctypes.Structure):
        _fields_ = [
            ("bmiHeader", _BITMAPINFOHEADER),
            ("bmiColors", _RGBQUAD * 1),
        ]


def _window_process_name(hwnd: int) -> str:
    if sys.platform != "win32":
        return ""
    pid = wintypes.DWORD()
    _user32.GetWindowThreadProcessId(wintypes.HWND(hwnd), ctypes.byref(pid))
    if pid.value == 0:
        return ""
    handle = _kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid.value)
    if not handle:
        return ""
    try:
        size = wintypes.DWORD(1024)
        buf = ctypes.create_unicode_buffer(size.value)
        if _query_full_process_image_name(handle, 0, buf, ctypes.byref(size)):
            return Path(buf.value).name
        return ""
    finally:
        _kernel32.CloseHandle(handle)


def _window_text(hwnd: int) -> str:
    if sys.platform != "win32":
        return ""
    length = _user32.GetWindowTextLengthW(wintypes.HWND(hwnd))
    buf = ctypes.create_unicode_buffer(max(1, length + 1))
    _user32.GetWindowTextW(wintypes.HWND(hwnd), buf, len(buf))
    return buf.value


def _client_bbox(hwnd: int) -> Optional[Tuple[int, int, int, int]]:
    if sys.platform != "win32":
        return None
    rect = _RECT()
    if not _user32.GetClientRect(wintypes.HWND(hwnd), ctypes.byref(rect)):
        return None
    left_top = _POINT(rect.left, rect.top)
    right_bottom = _POINT(rect.right, rect.bottom)
    if not _user32.ClientToScreen(wintypes.HWND(hwnd), ctypes.byref(left_top)):
        return None
    if not _user32.ClientToScreen(wintypes.HWND(hwnd), ctypes.byref(right_bottom)):
        return None
    width = right_bottom.x - left_top.x
    height = right_bottom.y - left_top.y
    if width < 64 or height < 64:
        return None
    return (left_top.x, left_top.y, right_bottom.x, right_bottom.y)


def _window_rect(hwnd: int) -> Optional[Tuple[int, int, int, int]]:
    if sys.platform != "win32":
        return None
    rect = _RECT()
    if not _user32.GetWindowRect(wintypes.HWND(hwnd), ctypes.byref(rect)):
        return None
    width = rect.right - rect.left
    height = rect.bottom - rect.top
    if width < 64 or height < 64:
        return None
    return (rect.left, rect.top, rect.right, rect.bottom)


def _window_class_name(hwnd: int) -> str:
    if sys.platform != "win32":
        return ""
    buf = ctypes.create_unicode_buffer(256)
    _user32.GetClassNameW(wintypes.HWND(hwnd), buf, len(buf))
    return buf.value


def _window_root(hwnd: int) -> int:
    if sys.platform != "win32":
        return hwnd
    root = _user32.GetAncestor(wintypes.HWND(hwnd), GA_ROOT)
    return int(root) if root else int(hwnd)


def _window_style(hwnd: int) -> int:
    if sys.platform != "win32":
        return 0
    return int(_user32.GetWindowLongW(wintypes.HWND(hwnd), GWL_STYLE))


def _virtual_screen_bbox() -> Tuple[int, int, int, int]:
    if sys.platform != "win32":
        return (0, 0, CAMERA_WIDTH, CAMERA_HEIGHT)
    left = int(_user32.GetSystemMetrics(SM_XVIRTUALSCREEN))
    top = int(_user32.GetSystemMetrics(SM_YVIRTUALSCREEN))
    width = int(_user32.GetSystemMetrics(SM_CXVIRTUALSCREEN))
    height = int(_user32.GetSystemMetrics(SM_CYVIRTUALSCREEN))
    return (left, top, left + width, top + height)


def _point_on_desktop() -> Tuple[int, int]:
    if sys.platform != "win32":
        return (0, 0)
    pt = _POINT()
    _user32.GetCursorPos(ctypes.byref(pt))
    return int(pt.x), int(pt.y)


def _window_from_point(x: int, y: int) -> Optional[int]:
    if sys.platform != "win32":
        return None
    pt = _POINT(x, y)
    hwnd = _user32.WindowFromPoint(pt)
    return int(hwnd) if hwnd else None


def _describe_target_for_hwnd(hwnd: int, force_window_rect: bool = False) -> Optional[Dict[str, Any]]:
    if sys.platform != "win32" or not hwnd or not _user32.IsWindow(wintypes.HWND(hwnd)):
        return None
    root = _window_root(hwnd)
    style = _window_style(hwnd)
    is_child = root != hwnd or bool(style & WS_CHILD)
    use_client_area = not is_child and not force_window_rect
    bbox = _client_bbox(hwnd) if use_client_area else _window_rect(hwnd)
    if bbox is None:
        bbox = _window_rect(root)
        if bbox is None:
            return None
        use_client_area = False
        hwnd = root
    title = _window_text(hwnd).strip() or _window_text(root).strip() or "(无标题窗口)"
    process_name = _window_process_name(hwnd) or _window_process_name(root)
    class_name = _window_class_name(hwnd)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    return {
        "mode": "manual_window",
        "label": f"{title} [{process_name or 'unknown'}]",
        "hwnd": int(hwnd),
        "root_hwnd": int(root),
        "bbox": [int(b) for b in bbox],
        "process_name": process_name,
        "title": title,
        "class_name": class_name,
        "use_client_area": bool(use_client_area),
        "width": width,
        "height": height,
    }


def _serialize_capture_target() -> Dict[str, Any]:
    with desktop_capture_lock:
        target = dict(desktop_capture_target)
    bbox = target.get("bbox")
    base_bbox = _resolve_base_capture_bbox(dict(target))
    resolved_bbox = _apply_target_crop(base_bbox, dict(target))
    if base_bbox is not None:
        target["base_bbox"] = [int(v) for v in base_bbox]
    else:
        target["base_bbox"] = None
    if resolved_bbox is not None:
        target["resolved_bbox"] = [int(v) for v in resolved_bbox]
    else:
        target["resolved_bbox"] = None
    if bbox is None and resolved_bbox is not None:
        bbox = resolved_bbox
    if bbox:
        target["bbox"] = [int(v) for v in bbox]
        target["width"] = int(bbox[2] - bbox[0])
        target["height"] = int(bbox[3] - bbox[1])
    else:
        target["width"] = 0
        target["height"] = 0
    target["opened"] = bool(_resolve_capture_bbox(dict(target)))
    return target


def _set_capture_target(target: Dict[str, Any]) -> None:
    global desktop_capture_target, camera_devices_snapshot
    normalized = dict(target)
    bbox = normalized.get("bbox")
    if bbox is not None:
        normalized["bbox"] = [int(v) for v in bbox]
    if normalized.get("hwnd") is not None:
        normalized["hwnd"] = int(normalized["hwnd"])
    if normalized.get("root_hwnd") is not None:
        normalized["root_hwnd"] = int(normalized["root_hwnd"])
    crop_norm = normalized.get("crop_norm")
    if crop_norm is not None and len(crop_norm) == 4:
        normalized["crop_norm"] = [float(v) for v in crop_norm]
    else:
        normalized["crop_norm"] = None
    with desktop_capture_lock:
        desktop_capture_target = normalized
    camera_devices_snapshot = []


def _resolve_base_capture_bbox(target: Optional[Dict[str, Any]] = None) -> Optional[Tuple[int, int, int, int]]:
    if sys.platform != "win32":
        return None
    if target is None:
        with desktop_capture_lock:
            target = dict(desktop_capture_target)
    mode = target.get("mode", "auto_aurora")
    if mode == "manual_region":
        bbox = target.get("bbox")
        if bbox and len(bbox) == 4:
            left, top, right, bottom = [int(v) for v in bbox]
            if right - left >= 32 and bottom - top >= 32:
                return (left, top, right, bottom)
        return None
    if mode == "manual_window":
        hwnd = int(target.get("hwnd") or 0)
        if not hwnd or not _user32.IsWindow(wintypes.HWND(hwnd)):
            return None
        if bool(target.get("use_client_area", False)):
            bbox = _client_bbox(hwnd)
            if bbox is not None:
                return bbox
        return _window_rect(hwnd)
    hwnd = _find_aurora_window_handle()
    if hwnd is None:
        return None
    return _client_bbox(hwnd)


def _apply_target_crop(base_bbox: Optional[Tuple[int, int, int, int]],
                       target: Optional[Dict[str, Any]] = None) -> Optional[Tuple[int, int, int, int]]:
    if base_bbox is None:
        return None
    if target is None:
        with desktop_capture_lock:
            target = dict(desktop_capture_target)
    crop_norm = target.get("crop_norm")
    if not crop_norm or len(crop_norm) != 4:
        return base_bbox
    try:
        x0, y0, x1, y1 = [float(v) for v in crop_norm]
    except Exception:
        return base_bbox
    x0 = min(max(x0, 0.0), 1.0)
    y0 = min(max(y0, 0.0), 1.0)
    x1 = min(max(x1, 0.0), 1.0)
    y1 = min(max(y1, 0.0), 1.0)
    if x1 - x0 < 0.02 or y1 - y0 < 0.02:
        return base_bbox
    left, top, right, bottom = base_bbox
    width = max(1, right - left)
    height = max(1, bottom - top)
    cropped = (
        int(round(left + width * x0)),
        int(round(top + height * y0)),
        int(round(left + width * x1)),
        int(round(top + height * y1)),
    )
    if cropped[2] - cropped[0] < 32 or cropped[3] - cropped[1] < 32:
        return base_bbox
    return cropped


def _resolve_capture_bbox(target: Optional[Dict[str, Any]] = None) -> Optional[Tuple[int, int, int, int]]:
    if target is None:
        with desktop_capture_lock:
            target = dict(desktop_capture_target)
    base_bbox = _resolve_base_capture_bbox(dict(target))
    return _apply_target_crop(base_bbox, dict(target))


def _capture_window_bitmap(hwnd: int) -> Optional[np.ndarray]:
    if sys.platform != "win32" or not hwnd or not _user32.IsWindow(wintypes.HWND(hwnd)):
        return None
    rect = _window_rect(hwnd)
    if rect is None:
        return None
    width = int(rect[2] - rect[0])
    height = int(rect[3] - rect[1])
    if width < 2 or height < 2:
        return None

    hwnd_dc = _user32.GetWindowDC(wintypes.HWND(hwnd))
    if not hwnd_dc:
        return None

    mem_dc = _gdi32.CreateCompatibleDC(hwnd_dc)
    bitmap = _gdi32.CreateCompatibleBitmap(hwnd_dc, width, height)
    old_obj = _gdi32.SelectObject(mem_dc, bitmap)

    frame = None
    try:
        printed = int(_user32.PrintWindow(wintypes.HWND(hwnd), mem_dc, PW_RENDERFULLCONTENT))
        if not printed:
            _gdi32.BitBlt(mem_dc, 0, 0, width, height, hwnd_dc, 0, 0, SRCCOPY)

        bmi = _BITMAPINFO()
        bmi.bmiHeader.biSize = ctypes.sizeof(_BITMAPINFOHEADER)
        bmi.bmiHeader.biWidth = width
        bmi.bmiHeader.biHeight = -height
        bmi.bmiHeader.biPlanes = 1
        bmi.bmiHeader.biBitCount = 32
        bmi.bmiHeader.biCompression = BI_RGB

        buf = ctypes.create_string_buffer(width * height * 4)
        rows = _gdi32.GetDIBits(mem_dc, bitmap, 0, height, buf, ctypes.byref(bmi), DIB_RGB_COLORS)
        if rows == height:
            arr = np.frombuffer(buf, dtype=np.uint8).reshape((height, width, 4))
            frame = arr[:, :, :3].copy()
    finally:
        if old_obj:
            _gdi32.SelectObject(mem_dc, old_obj)
        if bitmap:
            _gdi32.DeleteObject(bitmap)
        if mem_dc:
            _gdi32.DeleteDC(mem_dc)
        _user32.ReleaseDC(wintypes.HWND(hwnd), hwnd_dc)
    return frame


def _crop_frame_to_bbox(frame: np.ndarray,
                        origin_rect: Tuple[int, int, int, int],
                        target_bbox: Tuple[int, int, int, int]) -> Optional[np.ndarray]:
    if frame is None or origin_rect is None or target_bbox is None:
        return None
    left = max(0, int(target_bbox[0] - origin_rect[0]))
    top = max(0, int(target_bbox[1] - origin_rect[1]))
    right = min(frame.shape[1], int(target_bbox[2] - origin_rect[0]))
    bottom = min(frame.shape[0], int(target_bbox[3] - origin_rect[1]))
    if right - left < 2 or bottom - top < 2:
        return None
    return frame[top:bottom, left:right].copy()


def _capture_target_frame(target: Optional[Dict[str, Any]] = None) -> Optional[np.ndarray]:
    if sys.platform != "win32":
        return None
    if target is None:
        with desktop_capture_lock:
            target = dict(desktop_capture_target)
    mode = target.get("mode", "auto_aurora")
    bbox = _resolve_capture_bbox(dict(target))
    if bbox is None:
        return None

    if mode == "manual_region":
        try:
            image = ImageGrab.grab(bbox=bbox, all_screens=True)
        except Exception:
            return None
        frame_rgb = np.array(image)
        if frame_rgb.size == 0:
            return None
        return cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2BGR) if len(frame_rgb.shape) == 3 else frame_rgb

    hwnd = None
    root_hwnd = None
    if mode == "manual_window":
        hwnd = int(target.get("hwnd") or 0)
        root_hwnd = int(target.get("root_hwnd") or _window_root(hwnd or 0) or 0)
    else:
        hwnd = _find_aurora_window_handle() or 0
        root_hwnd = hwnd
    if not hwnd or not root_hwnd:
        return None

    root_rect = _window_rect(root_hwnd)
    frame = _capture_window_bitmap(root_hwnd)
    if frame is None or root_rect is None:
        return None
    cropped = _crop_frame_to_bbox(frame, root_rect, bbox)
    return cropped if cropped is not None else frame


def _float01(value: Any, default: float = 0.0) -> float:
    try:
        out = float(value)
    except Exception:
        out = default
    return min(max(out, 0.0), 1.0)


def _float_signed(value: Any, default: float = 0.0, limit: float = 0.5) -> float:
    try:
        out = float(value)
    except Exception:
        out = default
    return min(max(out, -limit), limit)


def _update_capture_crop_norm(crop_norm: Optional[list]) -> Dict[str, Any]:
    with desktop_capture_lock:
        target = dict(desktop_capture_target)
    target["crop_norm"] = crop_norm
    _set_capture_target(target)
    return _serialize_capture_target()


def _pick_window_from_desktop(timeout_sec: float = 20.0) -> Dict[str, Any]:
    if sys.platform != "win32":
        raise RuntimeError("仅支持 Windows 桌面点选")
    popup = tk.Tk()
    popup.title("Aurora Companion - 点选窗口")
    popup.attributes("-topmost", True)
    popup.resizable(False, False)
    popup.geometry("360x120+80+80")
    tk.Label(
        popup,
        text="请在桌面上单击目标窗口或视频区域。\n支持直接点 Aurora 的 device 视频区。\nEsc 取消。",
        justify="left",
        padx=16,
        pady=14,
    ).pack(fill="both", expand=True)
    popup.update_idletasks()
    popup_hwnd = int(popup.winfo_id())
    deadline = time.time() + timeout_sec
    last_down = False
    try:
        while time.time() < deadline:
            popup.update()
            if _user32.GetAsyncKeyState(VK_ESCAPE) & 0x8000:
                raise RuntimeError("已取消点选窗口")
            is_down = bool(_user32.GetAsyncKeyState(VK_LBUTTON) & 0x8000)
            if is_down and not last_down:
                time.sleep(0.05)
                x, y = _point_on_desktop()
                hwnd = _window_from_point(x, y)
                if hwnd and _window_root(hwnd) != _window_root(popup_hwnd):
                    info = _describe_target_for_hwnd(hwnd)
                    if info is None:
                        raise RuntimeError("未识别到可抓取窗口")
                    return info
            last_down = is_down
            time.sleep(0.02)
        raise RuntimeError("点选窗口超时")
    finally:
        popup.destroy()


def _pick_screen_region(timeout_sec: float = 30.0) -> Dict[str, Any]:
    if sys.platform != "win32":
        raise RuntimeError("仅支持 Windows 桌面框选")
    left, top, right, bottom = _virtual_screen_bbox()
    width = right - left
    height = bottom - top
    overlay = tk.Tk()
    overlay.title("Aurora Companion - 框选区域")
    overlay.attributes("-topmost", True)
    overlay.attributes("-alpha", 0.20)
    overlay.overrideredirect(True)
    overlay.geometry(f"{width}x{height}+{left}+{top}")
    overlay.configure(bg="black")

    canvas = tk.Canvas(overlay, width=width, height=height, bg="black", highlightthickness=0)
    canvas.pack(fill="both", expand=True)
    try:
        screenshot = ImageGrab.grab(bbox=(left, top, right, bottom), all_screens=True)
        photo = ImageTk.PhotoImage(screenshot)
        canvas.create_image(0, 0, anchor="nw", image=photo)
        canvas.image = photo
    except Exception:
        pass
    canvas.create_text(
        24,
        24,
        anchor="nw",
        fill="white",
        text="拖拽框选要抓取的视频区域，松开确认；Esc 取消。",
        font=("Segoe UI", 14, "bold"),
    )

    state: Dict[str, Any] = {"start": None, "rect": None, "done": False, "bbox": None}

    def on_press(event):
        state["start"] = (event.x, event.y)
        if state["rect"]:
            canvas.delete(state["rect"])
        state["rect"] = canvas.create_rectangle(event.x, event.y, event.x, event.y, outline="#ffffff", width=2)

    def on_drag(event):
        if not state["start"] or not state["rect"]:
            return
        x0, y0 = state["start"]
        canvas.coords(state["rect"], x0, y0, event.x, event.y)

    def on_release(event):
        if not state["start"]:
            return
        x0, y0 = state["start"]
        x1, y1 = event.x, event.y
        l = min(x0, x1) + left
        t = min(y0, y1) + top
        r = max(x0, x1) + left
        b = max(y0, y1) + top
        if r - l < 32 or b - t < 32:
            return
        state["bbox"] = [int(l), int(t), int(r), int(b)]
        state["done"] = True

    def on_escape(_event):
        state["done"] = True
        state["bbox"] = None

    canvas.bind("<ButtonPress-1>", on_press)
    canvas.bind("<B1-Motion>", on_drag)
    canvas.bind("<ButtonRelease-1>", on_release)
    overlay.bind("<Escape>", on_escape)
    overlay.focus_force()

    deadline = time.time() + timeout_sec
    try:
        while time.time() < deadline and not state["done"]:
            overlay.update()
            time.sleep(0.01)
    finally:
        overlay.destroy()
    if not state["bbox"]:
        raise RuntimeError("已取消框选区域")
    bbox = state["bbox"]
    return {
        "mode": "manual_region",
        "label": f"手动框选区域 {bbox[2] - bbox[0]}x{bbox[3] - bbox[1]}",
        "hwnd": None,
        "bbox": bbox,
        "process_name": "",
        "title": "手动框选区域",
        "class_name": "",
        "use_client_area": False,
        "width": bbox[2] - bbox[0],
        "height": bbox[3] - bbox[1],
    }


def _list_desktop_windows(limit: int = 60) -> list:
    if sys.platform != "win32":
        return []
    items = []
    seen = set()

    def push_info(info: Optional[Dict[str, Any]], child: bool = False) -> None:
        if info is None:
            return
        key = (int(info.get("hwnd") or 0), tuple(info.get("bbox") or []))
        if key in seen:
            return
        seen.add(key)
        title = (info.get("title") or "").strip()
        process_name = (info.get("process_name") or "").strip()
        if not title and not process_name:
            return
        info["id"] = info["hwnd"]
        info["score"] = info["width"] * info["height"] + (400000 if child else 0)
        if child:
            info["label"] = f"{title} [Aurora 子窗口]"
            info["title"] = title + " [子窗口]"
        items.append(info)

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def enum_proc(hwnd, _lparam):
        if not _user32.IsWindowVisible(hwnd):
            return True
        info = _describe_target_for_hwnd(int(hwnd))
        push_info(info, child=False)
        process_name = (info.get("process_name") or "").lower() if info else ""
        title = (info.get("title") or "").lower() if info else ""
        if process_name == "aurora.exe" or "aurora" in title:
            @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
            def enum_child_proc(child_hwnd, _child_lparam):
                child_info = _describe_target_for_hwnd(int(child_hwnd), force_window_rect=True)
                push_info(child_info, child=True)
                return True

            _user32.EnumChildWindows(wintypes.HWND(hwnd), enum_child_proc, 0)
        return len(items) < limit * 2

    _user32.EnumWindows(enum_proc, 0)
    items.sort(key=lambda item: (("aurora" in (item.get("title", "") + item.get("process_name", "")).lower()),
                                 item.get("score", 0)), reverse=True)
    return items[:limit]


def _find_aurora_window_handle() -> Optional[int]:
    if sys.platform != "win32":
        return None

    matches = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def enum_proc(hwnd, _lparam):
        if not _user32.IsWindowVisible(hwnd):
            return True
        bbox = _client_bbox(hwnd)
        if bbox is None:
            return True
        title = _window_text(hwnd).strip()
        process_name = _window_process_name(hwnd).lower()
        title_lower = title.lower()
        if process_name != "aurora.exe" and "aurora" not in title_lower:
            return True
        width = bbox[2] - bbox[0]
        height = bbox[3] - bbox[1]
        score = width * height
        if "aurora" in title_lower:
            score += 1000000
        matches.append((score, int(hwnd)))
        return True

    _user32.EnumWindows(enum_proc, 0)
    if not matches:
        return None
    matches.sort(reverse=True)
    return matches[0][1]


class AuroraWindowCapture:
    """从桌面已显示内容抓帧，可自动找 Aurora，也可手动点选窗口/框选区域。"""

    def __init__(self):
        self.hwnd: Optional[int] = None
        self._last_refresh = 0.0
        self._target_mode = "auto_aurora"
        self._bbox: Optional[Tuple[int, int, int, int]] = None
        self._target_signature: Optional[str] = None

    def _refresh_handle(self, force: bool = False) -> Optional[int]:
        now = time.time()
        target = _serialize_capture_target()
        signature = json.dumps({
            "mode": target.get("mode"),
            "hwnd": target.get("hwnd"),
            "root_hwnd": target.get("root_hwnd"),
            "bbox": target.get("bbox"),
            "resolved_bbox": target.get("resolved_bbox"),
            "crop_norm": target.get("crop_norm"),
        }, ensure_ascii=False, sort_keys=True)
        target_changed = signature != self._target_signature
        if not force and not target_changed and self.hwnd and self._target_mode == "manual_window" and sys.platform == "win32" and _user32.IsWindow(wintypes.HWND(self.hwnd)):
            return self.hwnd
        if not force and not target_changed and now - self._last_refresh < 0.25:
            return self.hwnd
        self._last_refresh = now
        self._target_signature = signature
        self._target_mode = target.get("mode", "auto_aurora")
        self._bbox = _resolve_capture_bbox(target)
        if self._target_mode == "manual_window":
            self.hwnd = int(target.get("hwnd") or 0) or None
        else:
            self.hwnd = _find_aurora_window_handle() if self._target_mode == "auto_aurora" else None
        return self.hwnd

    def isOpened(self) -> bool:
        self._refresh_handle()
        return self._bbox is not None

    def release(self) -> None:
        self.hwnd = None
        self._bbox = None

    def read(self) -> Tuple[bool, Optional[np.ndarray]]:
        self._refresh_handle()
        target = _serialize_capture_target()
        frame = _capture_target_frame(target)
        if frame is None or frame.size == 0:
            return False, None
        return True, frame


def _source_label(source: str) -> str:
    return SOURCE_LABELS.get(source, source)


def _infer_device_source(info: dict) -> str:
    if info.get("source") == CAMERA_SOURCE_AURORA_WINDOW:
        return CAMERA_SOURCE_AURORA_WINDOW
    if info.get("supports_gray_fourcc"):
        return CAMERA_SOURCE_A1
    # 仅当帧有实际内容时才凭灰度特征判断为 A1；全黑帧 (0==0==0) 不能作为依据
    if info.get("is_grayscale") and info.get("has_content"):
        return CAMERA_SOURCE_A1
    return CAMERA_SOURCE_WINDOWS


def _normalize_frame_for_display(frame: np.ndarray) -> np.ndarray:
    """统一画面朝向和尺寸，但保留原始通道数。"""
    if frame is None:
        return frame
    if len(frame.shape) >= 2:
        current_source = camera_source_global
        height, width = frame.shape[:2]
        if current_source == CAMERA_SOURCE_A1:
            # Aurora/Qt 将 A1 的 UYVY 灰度流暴露为 360x1280；横向按打包宽度还原为 720x1280，
            # 并保持与 Aurora 一致的竖向画面，旋转交给网页端预览控制。
            if height >= 1000 and width <= 400:
                frame = cv2.resize(frame, (width * 2, height), interpolation=cv2.INTER_NEAREST)
            return frame
        if height > width:
            frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        if frame.shape[:2] != (CAMERA_HEIGHT, CAMERA_WIDTH):
            frame = cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT))
    return frame


def _is_effectively_grayscale(frame: np.ndarray) -> bool:
    """判断 3 通道图像是否本质上仍是灰度。"""
    if frame is None or len(frame.shape) != 3 or frame.shape[2] < 3:
        return len(frame.shape) == 2 if frame is not None else False
    sample = frame[::8, ::8, :3].astype(np.int16, copy=False)
    bg = np.abs(sample[:, :, 0] - sample[:, :, 1])
    gr = np.abs(sample[:, :, 1] - sample[:, :, 2])
    return int(bg.max(initial=0)) <= 2 and int(gr.max(initial=0)) <= 2


def _extract_best_mono_channel(frame: np.ndarray) -> np.ndarray:
    """从异常伪彩帧中选取信息量最高的单通道作为亮度图。"""
    if frame is None:
        return frame
    if len(frame.shape) == 2:
        return frame
    if frame.shape[2] == 1:
        return frame[:, :, 0]
    if frame.shape[2] == 2:
        return frame[:, :, 0]

    stats = []
    for idx in range(min(frame.shape[2], 3)):
        channel = frame[::8, ::8, idx].astype(np.float32, copy=False)
        stats.append((float(np.std(channel)), idx))
    best_idx = max(stats)[1] if stats else 0
    return frame[:, :, best_idx]


def _frame_to_gray(frame: np.ndarray) -> np.ndarray:
    if frame is None:
        return frame
    if len(frame.shape) == 3:
        if frame.shape[2] == 1:
            return frame[:, :, 0]
        return cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    return frame


def _configure_camera_for_source(cap: cv2.VideoCapture, source: str) -> bool:
    """按源类型配置摄像头；OpenCV 只作为兜底，不强行改写驱动像素格式。"""
    if source == CAMERA_SOURCE_AURORA_WINDOW:
        return False
    cap.set(cv2.CAP_PROP_CONVERT_RGB, 1)
    if source == CAMERA_SOURCE_A1:
        print("[INFO] A1 OpenCV 兜底以驱动默认格式读取，随后按 Aurora 风格转灰度")
    return False


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


def save_preferred_source(source: str) -> None:
    try:
        PREFERRED_SOURCE_FILE.write_text(str(source), encoding="utf-8")
    except Exception:
        pass


def _qt_bridge_request(path: str, method: str = "GET", payload: Optional[dict] = None, timeout: float = 2.5) -> dict:
    url = QT_BRIDGE_URL + path
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json; charset=utf-8"
    req = urllib_request.Request(url, data=data, headers=headers, method=method)
    with urllib_request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read()
    return json.loads(raw.decode("utf-8")) if raw else {}


def _qt_bridge_status(timeout: float = 1.0) -> Optional[dict]:
    try:
        return _qt_bridge_request("/status", timeout=timeout)
    except Exception:
        return None


def _python_has_module(python_exe: str, module_name: str) -> bool:
    try:
        result = subprocess.run(
            [python_exe, "-c", f"import importlib.util; raise SystemExit(0 if importlib.util.find_spec({module_name!r}) else 1)"],
            cwd=str(Path(__file__).resolve().parent),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
            check=False,
        )
        return result.returncode == 0
    except Exception:
        return False


def _select_qt_bridge_python() -> str:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "venv_39" / "Scripts" / "python.exe",
        Path(sys.executable),
        repo_root / ".venv39" / "Scripts" / "python.exe",
    ]
    if sys.platform == "win32":
        candidates.append(Path("python"))
    seen = set()
    for candidate in candidates:
        key = str(candidate).lower()
        if key in seen:
            continue
        seen.add(key)
        if str(candidate) != "python" and not candidate.exists():
            continue
        python_exe = str(candidate)
        if _python_has_module(python_exe, "PySide6"):
            return python_exe
    return sys.executable


def _stop_stale_qt_bridge_on_port() -> None:
    if sys.platform != "win32":
        return
    script = rf"""
$connections = Get-NetTCPConnection -LocalPort {QT_BRIDGE_PORT} -State Listen -ErrorAction SilentlyContinue
foreach ($conn in $connections) {{
    $procId = [int]$conn.OwningProcess
    if ($procId -le 0) {{ continue }}
    $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$procId" -ErrorAction SilentlyContinue
    if (-not $proc) {{ continue }}
    $cmd = [string]$proc.CommandLine
    if ($cmd -match "qt_camera_bridge\.py") {{
        Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
    }}
}}
"""
    try:
        subprocess.run(
            ["powershell", "-NoProfile", "-Command", script],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=6,
            check=False,
        )
    except Exception:
        pass


def ensure_qt_bridge_running(timeout: float = 8.0) -> dict:
    global _qt_bridge_process

    status = _qt_bridge_status(timeout=0.6)
    if status is not None and status.get("available", False):
        return status
    if status is not None:
        _stop_stale_qt_bridge_on_port()

    with _qt_bridge_lock:
        status = _qt_bridge_status(timeout=0.6)
        if status is not None and status.get("available", False):
            return status
        if status is not None:
            _stop_stale_qt_bridge_on_port()

        if not QT_BRIDGE_SCRIPT.exists():
            raise RuntimeError(f"Qt 相机桥脚本不存在: {QT_BRIDGE_SCRIPT}")

        if _qt_bridge_process is None or _qt_bridge_process.poll() is not None:
            bridge_python = _select_qt_bridge_python()
            kwargs: Dict[str, Any] = {
                "cwd": str(Path(__file__).resolve().parent),
                "stdout": subprocess.DEVNULL,
                "stderr": subprocess.DEVNULL,
            }
            if sys.platform == "win32":
                kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            _qt_bridge_process = subprocess.Popen(
                [bridge_python, str(QT_BRIDGE_SCRIPT), "--host", QT_BRIDGE_HOST, "--port", str(QT_BRIDGE_PORT)],
                **kwargs,
            )

        deadline = time.time() + timeout
        last_status = None
        while time.time() < deadline:
            last_status = _qt_bridge_status(timeout=0.8)
            if last_status is not None:
                return last_status
            if _qt_bridge_process is not None and _qt_bridge_process.poll() is not None:
                break
            time.sleep(0.25)

        if last_status is not None:
            return last_status
        raise RuntimeError("Qt 相机桥启动失败")


def _qt_bridge_devices() -> list:
    try:
        status = ensure_qt_bridge_running()
        if not status.get("available", False):
            return []
        payload = _qt_bridge_request("/devices", timeout=4.0)
        if not payload.get("success", False):
            return []
        devices = payload.get("devices", []) or []
        devices.sort(key=lambda item: (
            PREFERRED_A1_CAMERA_NAME.lower() in str(item.get("device_name") or "").lower(),
            item.get("source") == CAMERA_SOURCE_A1,
            int(item.get("score") or 0),
        ), reverse=True)
        return devices
    except Exception:
        return []


def _qt_bridge_probe_device(device_id: int) -> Optional[dict]:
    for item in _qt_bridge_devices():
        if int(item.get("id", -999)) == int(device_id):
            return item
    return None


def _qt_bridge_fetch_frame(mode: str = "color", timeout: float = 2.5) -> Optional[np.ndarray]:
    raw = _qt_bridge_fetch_frame_bytes(mode=mode, timeout=timeout)
    if not raw:
        return None
    flags = cv2.IMREAD_GRAYSCALE if mode == "gray" else cv2.IMREAD_COLOR
    arr = np.frombuffer(raw, dtype=np.uint8)
    return cv2.imdecode(arr, flags)


def _qt_bridge_fetch_frame_bytes(mode: str = "color", timeout: float = 2.5) -> Optional[bytes]:
    query = urllib_parse.urlencode({"mode": mode})
    req = urllib_request.Request(f"{QT_BRIDGE_URL}/frame.jpg?{query}", method="GET")
    raw = b""
    for _ in range(2):
        try:
            with urllib_request.urlopen(req, timeout=timeout) as resp:
                if resp.status != 200:
                    raw = b""
                else:
                    raw = resp.read()
            if raw:
                break
        except urllib_error.URLError:
            raw = b""
        except Exception:
            raw = b""
        time.sleep(0.03)
    return raw or None


class QtBridgeCapture:
    """使用独立 QtMultimedia 桥进程获取相机画面。"""

    def __init__(self, device_id: int, source: str):
        self.device_id = int(device_id)
        self.source = source if source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1} else CAMERA_SOURCE_AUTO
        self._last_status: Dict[str, Any] = {}
        self._open()

    def _open(self) -> None:
        status = ensure_qt_bridge_running()
        if not status.get("available", False):
            error_text = status.get("error") or "PySide6 / QtMultimedia 不可用"
            raise RuntimeError(f"Qt 相机桥不可用: {error_text}")
        payload = _qt_bridge_request(
            "/switch",
            method="POST",
            payload={"device": self.device_id, "source": self.source},
            timeout=6.0,
        )
        if not payload.get("success", False):
            raise RuntimeError(payload.get("error") or "Qt 相机桥切换摄像头失败")
        self._last_status = payload.get("status", {}) or {}

    def isOpened(self) -> bool:
        try:
            status = _qt_bridge_status(timeout=0.8)
            if status is None:
                return False
            self._last_status = status
            return bool(status.get("connected"))
        except Exception:
            return False

    def release(self) -> None:
        return

    def read(self) -> Tuple[bool, Optional[np.ndarray]]:
        return self.read_color()

    def read_color(self) -> Tuple[bool, Optional[np.ndarray]]:
        frame = _qt_bridge_fetch_frame(mode="color")
        return (frame is not None), frame

    def read_color_jpeg(self) -> Tuple[bool, Optional[bytes]]:
        payload = _qt_bridge_fetch_frame_bytes(mode="color")
        return (payload is not None), payload

    def read_gray(self) -> Tuple[bool, Optional[np.ndarray]]:
        frame = _qt_bridge_fetch_frame(mode="gray")
        return (frame is not None), frame

    def read_gray_jpeg(self) -> Tuple[bool, Optional[bytes]]:
        payload = _qt_bridge_fetch_frame_bytes(mode="gray")
        return (payload is not None), payload


def probe_aurora_window_source() -> dict:
    cap = AuroraWindowCapture()
    target = _serialize_capture_target()
    opened = cap.isOpened()
    ret, frame = cap.read() if opened else (False, None)
    source_label = "桌面抓取"
    if target.get("mode") == "auto_aurora":
        source_label = "Aurora 自动窗口"
    elif target.get("mode") == "manual_window":
        source_label = "手动点选窗口"
    elif target.get("mode") == "manual_region":
        source_label = "手动框选区域"
    if not ret or frame is None:
        return {
            "id": AURORA_WINDOW_DEVICE_ID,
            "opened": opened,
            "score": 18 if opened else -1,
            "actual_width": CAMERA_WIDTH,
            "actual_height": CAMERA_HEIGHT,
            "is_grayscale": False,
            "has_content": opened,
            "supports_gray_fourcc": False,
            "source": CAMERA_SOURCE_AURORA_WINDOW,
            "source_label": source_label,
            "target": target,
        }

    frame_h, frame_w = frame.shape[:2]
    has_content = float(np.std(frame.astype(np.float32))) > 3.0
    return {
        "id": AURORA_WINDOW_DEVICE_ID,
        "opened": True,
        "score": 40 if has_content else 24,
        "actual_width": frame_w,
        "actual_height": frame_h,
        "is_grayscale": _is_effectively_grayscale(frame),
        "has_content": has_content,
        "supports_gray_fourcc": False,
        "source": CAMERA_SOURCE_AURORA_WINDOW,
        "source_label": source_label,
        "target": target,
    }


def probe_camera_device(device_id: int) -> dict:
    if device_id == AURORA_WINDOW_DEVICE_ID:
        return probe_aurora_window_source()
    bridge_info = _qt_bridge_probe_device(device_id)
    if bridge_info is not None:
        return bridge_info
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
            "source": CAMERA_SOURCE_WINDOWS,
            "source_label": _source_label(CAMERA_SOURCE_WINDOWS),
        }

    with _suppress_c_stderr():
        supports_gray_fourcc = False
        cap.set(cv2.CAP_PROP_CONVERT_RGB, 1)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
        # 丢弃前几帧：DirectShow 管道初始化期间第 1 帧通常为全黑
        for _ in range(3):
            cap.read()
        ret, frame = cap.read()
        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        cap.release()

    if not ret or frame is None:
        source = CAMERA_SOURCE_A1 if supports_gray_fourcc else CAMERA_SOURCE_WINDOWS
        return {
            "id": device_id,
            "opened": False,
            "score": 0,
            "actual_width": actual_w,
            "actual_height": actual_h,
            "is_grayscale": False,
            "has_content": False,
            "supports_gray_fourcc": supports_gray_fourcc,
            "source": source,
            "source_label": _source_label(source),
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
        # 竖向 portrait 帧：SC132GS 不应以此分辨率输出，通常是分辨率协商失败的产物
        score -= 4
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

    source = _infer_device_source({
        "supports_gray_fourcc": supports_gray_fourcc,
        "is_grayscale": is_grayscale,
        "has_content": has_content,
    })

    return {
        "id": device_id,
        "opened": True,
        "score": score,
        "actual_width": frame_w,
        "actual_height": frame_h,
        "is_grayscale": is_grayscale,
        "has_content": has_content,
        "supports_gray_fourcc": supports_gray_fourcc,
        "source": source,
        "source_label": _source_label(source),
    }


def list_camera_devices(max_scan: int = MAX_DEVICE_SCAN) -> list:
    """串行探测摄像头设备（优先 QtMultimedia，OpenCV 仅兜底）。"""
    devices = []
    bridge_devices = _qt_bridge_devices()
    if bridge_devices:
        devices.extend(bridge_devices)
        return devices
    for i in range(max_scan):
        info = probe_camera_device(i)
        if info["opened"]:
            devices.append(info)
    return devices


def _scan_camera_devices_background() -> None:
    global camera_devices_snapshot, camera_scan_active, camera_scan_error
    try:
        devices = list_camera_devices()
        camera_devices_snapshot = list(devices)
        camera_scan_error = ""
    except Exception as exc:
        camera_scan_error = str(exc)
    finally:
        camera_scan_active = False


def _start_aurora_desktop() -> Dict[str, Any]:
    if sys.platform != "win32":
        return {"started": False, "running": False, "error": "Aurora.exe 启动仅支持 Windows"}
    if not AURORA_EXE_PATH.exists():
        return {"started": False, "running": False, "error": f"未找到 Aurora.exe: {AURORA_EXE_PATH}"}
    hwnd = _find_aurora_window_handle()
    if hwnd is not None:
        try:
            _user32.SetForegroundWindow(wintypes.HWND(hwnd))
        except Exception:
            pass
        return {"started": False, "running": True, "path": str(AURORA_EXE_PATH), "hwnd": int(hwnd)}
    kwargs: Dict[str, Any] = {"cwd": str(AURORA_EXE_PATH.parent)}
    if sys.platform == "win32":
        kwargs["creationflags"] = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    subprocess.Popen([str(AURORA_EXE_PATH)], **kwargs)
    deadline = time.time() + 8.0
    while time.time() < deadline:
        hwnd = _find_aurora_window_handle()
        if hwnd is not None:
            try:
                _user32.SetForegroundWindow(wintypes.HWND(hwnd))
            except Exception:
                pass
            return {"started": True, "running": True, "path": str(AURORA_EXE_PATH), "hwnd": int(hwnd)}
        time.sleep(0.25)
    return {"started": True, "running": False, "path": str(AURORA_EXE_PATH), "error": "Aurora.exe 已启动，但暂未发现窗口"}


def choose_camera_device(requested_device: int) -> Tuple[int, list]:
    """返回 (device_id, candidates)。requested_device=-1 时自动优先 A1。"""
    if requested_device >= 0:
        return requested_device, list_camera_devices()

    candidates = list_camera_devices()

    preferred = load_preferred_device()
    if preferred is not None:
        preferred_info = next((c for c in candidates if int(c.get("id", -999)) == int(preferred)), None)
        if preferred_info is None:
            preferred_info = probe_camera_device(preferred)
        if preferred_info["opened"] and (preferred_info.get("has_content") or preferred_info.get("supports_gray_fourcc")):
            if not any(int(c.get("id", -999)) == int(preferred) for c in candidates):
                candidates.append(preferred_info)
            return preferred, candidates

    if not candidates:
        return 0, []

    candidates_sorted = sorted(candidates, key=lambda x: (x["score"], x["id"]), reverse=True)
    return candidates_sorted[0]["id"], candidates_sorted


def open_camera(device_id: int, source: str = CAMERA_SOURCE_AUTO) -> Optional[Any]:
    """打开输入源，可为物理摄像头或 Aurora 窗口抓取。"""
    if source == CAMERA_SOURCE_AUTO:
        source = camera_source_global if camera_source_global != CAMERA_SOURCE_AUTO else CAMERA_SOURCE_WINDOWS

    if source == CAMERA_SOURCE_AURORA_WINDOW or device_id == AURORA_WINDOW_DEVICE_ID:
        cap = AuroraWindowCapture()
        if cap.isOpened():
            print("[INFO] Aurora 窗口抓取源已连接")
        else:
            print("[WARN] Aurora 窗口尚未就绪，将等待窗口出现")
        return cap

    try:
        cap = QtBridgeCapture(device_id, source)
        if cap.isOpened():
            status = _qt_bridge_status(timeout=0.8) or {}
            msg = status.get("message") or f"Qt 相机桥已连接设备 {device_id}"
            print(f"[INFO] {msg}")
            return cap
    except Exception as exc:
        print(f"[WARN] Qt 相机桥打开失败，回退 OpenCV: {exc}")

    cap = _open_raw_camera(device_id)
    if not cap.isOpened():
        raise RuntimeError(f"无法打开摄像头设备 {device_id}")

    format_set = _configure_camera_for_source(cap, source)
    if source == CAMERA_SOURCE_WINDOWS:
        print("[INFO] Windows 摄像头以纯图像模式读取")
    elif source == CAMERA_SOURCE_A1 and not format_set:
        print("[INFO] A1 摄像头以兼容模式读取")

    if source == CAMERA_SOURCE_A1:
        probe = _qt_bridge_probe_device(device_id) or {}
        target_w = int(probe.get("actual_width") or 360)
        target_h = int(probe.get("actual_height") or 1280)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, target_w)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, target_h)
    else:
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, CAMERA_FPS)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps_actual = cap.get(cv2.CAP_PROP_FPS)
    print(f"[INFO] 摄像头已打开: {w}×{h} @ {fps_actual:.1f}fps (设备 {device_id})")
    if w != CAMERA_WIDTH or h != CAMERA_HEIGHT:
        print(f"[WARN] 实际分辨率 {w}×{h} 与预期 {CAMERA_WIDTH}×{CAMERA_HEIGHT} 不匹配")
    return cap


def bootstrap_camera(requested_device: int) -> None:
    """在后台完成摄像头自动探测与打开，避免阻塞 Web 服务启动。"""
    global camera, device_id_global, camera_source_global, camera_connected, _fail_streak, _consecutive_failures
    global camera_bootstrap_active, camera_devices_snapshot

    camera_bootstrap_active = True
    try:
        selected_device, candidates = choose_camera_device(requested_device)
        if candidates:
            selected_info = next((c for c in candidates if c["id"] == selected_device), None)
            if selected_info is None:
                selected_info = probe_camera_device(selected_device)
        else:
            selected_info = probe_camera_device(selected_device)

        camera_devices_snapshot = list(candidates) if candidates else [selected_info]

        camera_source_global = selected_info.get("source", CAMERA_SOURCE_WINDOWS)
        device_id_global = selected_device

        if requested_device < 0:
            if candidates:
                print("[INFO] 自动探测摄像头结果（按优先级）:")
                for c in candidates:
                    kind = "Gray" if c.get("is_grayscale") else "Color"
                    y8 = "Y8" if c.get("supports_gray_fourcc") else "NoY8"
                    print(f"  - device {c['id']}: {c['actual_width']}x{c['actual_height']} {kind} {y8} score={c['score']} source={c.get('source', CAMERA_SOURCE_WINDOWS)}")
                print(f"[INFO] 自动选择设备: {selected_device} source={camera_source_global}")
            else:
                print("[WARN] 未探测到可用摄像头，回退到设备 0")

        print(f"[INFO] 正在连接摄像头 (设备 {selected_device}, source={camera_source_global})...")
        save_preferred_device(selected_device)
        try:
            opened_camera = open_camera(selected_device, camera_source_global)
        except Exception as e:
            print(f"[WARN] 摄像头打开失败: {e}，工具仍可启动，请连接后点击「刷新摄像头」")
            opened_camera = None

        # 到这里 snapshot 已设置，/camera_devices 可正常应答，再做初始帧预热
        if (opened_camera is not None and opened_camera.isOpened()
                and selected_device != AURORA_WINDOW_DEVICE_ID
                and not isinstance(opened_camera, QtBridgeCapture)):
            print("[INFO] 刷新 DirectShow 初始化队列帧...")
            with _suppress_c_stderr():
                for _ in range(5):
                    opened_camera.read()

        with camera_lock:
            if camera:
                camera.release()
            camera = opened_camera

        camera_connected = opened_camera is not None and opened_camera.isOpened()
        if camera_connected:
            _fail_streak = 0
            _consecutive_failures = 0
    except Exception as e:
        print(f"[WARN] 摄像头后台初始化失败: {e}")
        with camera_lock:
            if camera:
                camera.release()
            camera = None
        camera_connected = False
    finally:
        camera_bootstrap_active = False


def _read_display_frame(cap: Any) -> Optional[np.ndarray]:
    with camera_lock:
        if isinstance(cap, QtBridgeCapture):
            ret, frame = cap.read_color()
        else:
            ret, frame = cap.read()
    if not ret or frame is None:
        return None

    corrected = _normalize_frame_for_display(frame)
    if corrected is None:
        return None

    current_source = camera_source_global
    if len(corrected.shape) == 2:
        corrected = cv2.cvtColor(corrected, cv2.COLOR_GRAY2BGR)
    elif corrected.shape[2] == 1:
        corrected = cv2.cvtColor(corrected[:, :, 0], cv2.COLOR_GRAY2BGR)
    elif current_source == CAMERA_SOURCE_A1:
        # A1 源理论上应为单通道灰度；少数驱动会把 Y 通道伪装成 2/3 通道彩色帧。
        mono = None
        if corrected.shape[2] == 2:
            mono = _extract_best_mono_channel(corrected)
        elif corrected.shape[2] >= 3 and not _is_effectively_grayscale(corrected):
            mono = _extract_best_mono_channel(corrected)
        if mono is not None:
            corrected = cv2.cvtColor(mono, cv2.COLOR_GRAY2BGR)
    return corrected


def _read_gray(cap: Any) -> Optional[np.ndarray]:
    if isinstance(cap, QtBridgeCapture):
        with camera_lock:
            ret, frame = cap.read_gray()
        if not ret or frame is None:
            return None
        corrected = _normalize_frame_for_display(frame)
        return _frame_to_gray(corrected)
    frame = _read_display_frame(cap)
    if frame is None:
        return None
    return _frame_to_gray(frame)


def _detect_model_backend_from_path(model_path: Path) -> str:
    return "pt" if model_path.suffix.lower() == ".pt" else "onnx"


def _detect_model_mode_from_path(model_path: Path) -> str:
    if model_path.suffix.lower() == ".pt":
        return "standard"
    return "head6" if "head6" in model_path.stem.lower() else "standard"


def _read_detect_model_preference() -> Optional[str]:
    try:
        if not _DETECT_MODEL_PREF_FILE.exists():
            return None
        text = _DETECT_MODEL_PREF_FILE.read_text(encoding="utf-8").strip()
        return text or None
    except Exception:
        return None


def _write_detect_model_preference(model_name: str) -> None:
    try:
        _DETECT_MODEL_PREF_FILE.write_text(model_name, encoding="utf-8")
    except Exception:
        pass


def _resolve_detect_model_path(model_name: str) -> Optional[Path]:
    if not model_name:
        return None
    candidate = Path(model_name).expanduser()
    if candidate.exists():
        return candidate.resolve()
    candidate = MODEL_ROOT / candidate.name
    return candidate.resolve() if candidate.exists() else None


def _list_detect_models() -> list:
    current_path = _DETECT_MODEL_PATH.resolve() if _DETECT_MODEL_PATH.exists() else _DETECT_MODEL_PATH
    items = []
    seen_paths = set()
    if MODEL_ROOT.exists():
        for path in sorted(MODEL_ROOT.iterdir(), key=lambda item: item.name.lower()):
            if not path.is_file() or path.suffix.lower() not in _SUPPORTED_DETECT_MODEL_SUFFIXES:
                continue
            resolved = path.resolve()
            seen_paths.add(resolved)
            items.append({
                "name": path.name,
                "label": path.name,
                "path": str(path),
                "mode": _detect_model_mode_from_path(path),
                "backend": _detect_model_backend_from_path(path),
                "selected": resolved == current_path,
            })
    if current_path.exists() and current_path not in seen_paths and current_path.suffix.lower() in _SUPPORTED_DETECT_MODEL_SUFFIXES:
        items.append({
            "name": current_path.name,
            "label": current_path.name,
            "path": str(current_path),
            "mode": _detect_model_mode_from_path(current_path),
            "backend": _detect_model_backend_from_path(current_path),
            "selected": True,
        })
    if not items and _DETECT_MODEL_PATH.exists():
        items.append({
            "name": _DETECT_MODEL_PATH.name,
            "label": _DETECT_MODEL_PATH.name,
            "path": str(_DETECT_MODEL_PATH),
            "mode": _detect_model_mode_from_path(_DETECT_MODEL_PATH),
            "backend": _detect_model_backend_from_path(_DETECT_MODEL_PATH),
            "selected": True,
        })
    return items


def set_detect_model_path(model_path: Path, persist: bool = True) -> dict:
    global _DETECT_MODEL_PATH, _DETECT_MODEL_MODE, _ort_session, _pt_model

    resolved = Path(model_path)
    if not resolved.is_absolute():
        if resolved.exists():
            resolved = resolved.resolve()
        else:
            resolved = MODEL_ROOT / resolved.name
    resolved = resolved.resolve()
    if not resolved.exists():
        raise FileNotFoundError(f"模型不存在: {resolved}")
    if resolved.suffix.lower() not in _SUPPORTED_DETECT_MODEL_SUFFIXES:
        raise ValueError("仅支持 .onnx / .pt 模型")

    _DETECT_MODEL_PATH = resolved
    _DETECT_MODEL_MODE = _detect_model_mode_from_path(resolved)
    with _ort_session_lock:
        _ort_session = None
        _pt_model = None
    if persist:
        _write_detect_model_preference(str(resolved))
    return {
        "model_name": resolved.name,
        "model_path": str(resolved),
        "model_mode": _DETECT_MODEL_MODE,
        "model_backend": _detect_model_backend_from_path(resolved),
    }


def _initial_detect_model_path() -> Path:
    preferred_name = _read_detect_model_preference()
    if preferred_name:
        resolved = _resolve_detect_model_path(preferred_name)
        if resolved is not None:
            return resolved
    return _DETECT_MODEL_PATH


set_detect_model_path(_initial_detect_model_path(), persist=False)


def crop_center(img: np.ndarray, target_w: int, target_h: int) -> np.ndarray:
    """从图像中心裁剪到目标尺寸"""
    h, w = img.shape[:2]
    if target_w >= w and target_h >= h:
        return img
    x_start = max(0, (w - target_w) // 2)
    y_start = max(0, (h - target_h) // 2)
    return img[y_start:y_start + target_h, x_start:x_start + target_w]


# ─── YOLOv8 推理（移植自 aurora_capture.py）──────────────────────────────────

def _load_ort_session():
    """懒加载 ONNX 推理会话（线程安全）。"""
    global _ort_session, _DETECT_MODEL_MODE
    if not _ORT_AVAILABLE:
        return None
    if _DETECT_MODEL_PATH.suffix.lower() != ".onnx":
        return None
    if not _DETECT_MODEL_PATH.exists():
        print(f"[WARN] YOLOv8 ONNX 模型不存在: {_DETECT_MODEL_PATH}")
        return None
    with _ort_session_lock:
        if _ort_session is None:
            try:
                opts = ort.SessionOptions()
                opts.inter_op_num_threads = 2
                opts.intra_op_num_threads = 2
                _ort_session = ort.InferenceSession(
                    str(_DETECT_MODEL_PATH),
                    sess_options=opts,
                    providers=["CPUExecutionProvider"],
                )
                outputs = _ort_session.get_outputs()
                _DETECT_MODEL_MODE = "head6" if len(outputs) > 1 else "standard"
                print(f"[INFO] YOLOv8 ONNX 模型已加载: {_DETECT_MODEL_PATH.name} ({_DETECT_MODEL_MODE})")
            except Exception as e:
                print(f"[ERROR] 加载 ONNX 失败: {e}")
                return None
    return _ort_session


def _load_pt_model():
    """懒加载 Ultralytics PT 模型（线程安全）。"""
    global _pt_model
    if not _ULTRALYTICS_AVAILABLE:
        return None
    if _DETECT_MODEL_PATH.suffix.lower() != ".pt":
        return None
    if not _DETECT_MODEL_PATH.exists():
        print(f"[WARN] YOLOv8 PT 模型不存在: {_DETECT_MODEL_PATH}")
        return None
    with _ort_session_lock:
        if _pt_model is None:
            try:
                _pt_model = UltralyticsYOLO(str(_DETECT_MODEL_PATH))
                print(f"[INFO] YOLOv8 PT 模型已加载: {_DETECT_MODEL_PATH.name}")
            except Exception as e:
                print(f"[ERROR] 加载 PT 失败: {e}")
                return None
    return _pt_model


def _letterbox(img_gray: np.ndarray, target: int = 640):
    """灰度图 letterbox 到 target×target，返回 (padded_gray, scale, pad_x, pad_y)。"""
    h, w = img_gray.shape
    scale = target / max(h, w)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))
    resized = cv2.resize(img_gray, (new_w, new_h))
    canvas = np.zeros((target, target), dtype=np.uint8)
    pad_y = (target - new_h) // 2
    pad_x = (target - new_w) // 2
    canvas[pad_y:pad_y + new_h, pad_x:pad_x + new_w] = resized
    return canvas, scale, pad_x, pad_y


def _sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def _nms_boxes(boxes, scores, cls_ids, nms_thr=_DETECT_NMS, top_k=_DETECT_TOP_K):
    if not len(boxes):
        return []
    order = np.argsort(scores)[::-1][:200]
    suppressed = np.zeros(len(order), dtype=bool)
    keep = []
    for i, idx in enumerate(order):
        if suppressed[i]:
            continue
        keep.append(idx)
        if len(keep) >= top_k:
            break
        b1 = boxes[idx]
        for j in range(i + 1, len(order)):
            if suppressed[j]:
                continue
            jdx = order[j]
            if cls_ids[idx] != cls_ids[jdx]:
                continue
            b2 = boxes[jdx]
            ix1, iy1 = max(b1[0], b2[0]), max(b1[1], b2[1])
            ix2, iy2 = min(b1[2], b2[2]), min(b1[3], b2[3])
            inter = max(0, ix2 - ix1) * max(0, iy2 - iy1)
            a1 = max(0, b1[2] - b1[0]) * max(0, b1[3] - b1[1])
            a2 = max(0, b2[2] - b2[0]) * max(0, b2[3] - b2[1])
            union = a1 + a2 - inter
            if union > 0 and inter / union > nms_thr:
                suppressed[j] = True
    return keep


def _decode_yolov8_head6(cls_outs, reg_outs,
                         conf_thr=_DETECT_CONF, nms_thr=_DETECT_NMS):
    """head6 模型后处理：DFL decode + NMS。返回 [(box, score, cls_id)]。"""
    strides = [8, 16, 32]
    bins_arr = np.arange(_DETECT_REG_BINS, dtype=np.float32)
    all_boxes, all_scores, all_cls = [], [], []

    for i, (cls_out, reg_out) in enumerate(zip(cls_outs, reg_outs)):
        stride = strides[i]
        H, W = cls_out.shape[2], cls_out.shape[3]
        cls = np.transpose(cls_out[0], (1, 2, 0))
        cls = 1.0 / (1.0 + np.exp(-cls.astype(np.float32)))
        best_scores = cls.max(axis=2)
        best_cls = cls.argmax(axis=2)
        ys, xs = np.where(best_scores >= conf_thr)
        if len(ys) == 0:
            continue
        reg = np.transpose(reg_out[0], (1, 2, 0))
        reg_sel = reg[ys, xs].reshape(-1, 4, _DETECT_REG_BINS).astype(np.float32)
        reg_sel -= reg_sel.max(axis=2, keepdims=True)
        reg_sel = np.exp(reg_sel)
        reg_sel /= reg_sel.sum(axis=2, keepdims=True)
        dist = (reg_sel * bins_arr).sum(axis=2)
        ax = xs.astype(np.float32) + 0.5
        ay = ys.astype(np.float32) + 0.5
        x1 = (ax - dist[:, 0]) * stride
        y1 = (ay - dist[:, 1]) * stride
        x2 = (ax + dist[:, 2]) * stride
        y2 = (ay + dist[:, 3]) * stride
        all_boxes.append(np.stack([x1, y1, x2, y2], axis=1))
        all_scores.append(best_scores[ys, xs])
        all_cls.append(best_cls[ys, xs])

    if not all_boxes:
        return []

    boxes = np.concatenate(all_boxes, axis=0).astype(np.float32)
    scores = np.concatenate(all_scores).astype(np.float32)
    cls_ids = np.concatenate(all_cls).astype(np.int32)

    keep = _nms_boxes(boxes, scores, cls_ids, nms_thr=nms_thr)

    return [(boxes[idx], float(scores[idx]), int(cls_ids[idx])) for idx in keep]


def _decode_yolov8_standard(output: np.ndarray,
                            conf_thr=_DETECT_CONF, nms_thr=_DETECT_NMS):
    """标准 YOLOv8 输出后处理：xywh + class scores。"""
    pred = np.asarray(output)
    if pred.ndim != 3:
        return []

    if pred.shape[1] <= pred.shape[2]:
        pred = np.transpose(pred[0], (1, 0))
    else:
        pred = pred[0]

    if pred.shape[1] < 5:
        return []

    boxes = pred[:, :4].astype(np.float32)
    scores = pred[:, 4:].astype(np.float32)
    if scores.size == 0:
        return []
    if float(scores.min()) < 0.0 or float(scores.max()) > 1.0 + 1e-3:
        scores = _sigmoid(scores)

    best_scores = scores.max(axis=1)
    cls_ids = scores.argmax(axis=1).astype(np.int32)
    mask = best_scores >= conf_thr
    if not np.any(mask):
        return []

    boxes = boxes[mask]
    best_scores = best_scores[mask]
    cls_ids = cls_ids[mask]

    cx = boxes[:, 0]
    cy = boxes[:, 1]
    bw = boxes[:, 2]
    bh = boxes[:, 3]
    xyxy = np.stack([
        cx - bw / 2.0,
        cy - bh / 2.0,
        cx + bw / 2.0,
        cy + bh / 2.0,
    ], axis=1)

    keep = _nms_boxes(xyxy, best_scores, cls_ids, nms_thr=nms_thr)
    return [(xyxy[idx], float(best_scores[idx]), int(cls_ids[idx])) for idx in keep]


def _decode_yolov8_outputs(outputs):
    if len(outputs) == 1:
        return _decode_yolov8_standard(outputs[0])
    if len(outputs) >= 6:
        return _decode_yolov8_head6(outputs[:3], outputs[3:])
    return []


def detect_on_frame(frame_gray: np.ndarray):
    """对灰度帧运行 YOLOv8 推理，返回 [(x1,y1,x2,y2,score,cls_id)]（1280×720 坐标系）。"""
    h, w = frame_gray.shape
    if _DETECT_MODEL_PATH.suffix.lower() == ".pt":
        model = _load_pt_model()
        if model is None:
            return []
        frame_bgr = cv2.cvtColor(frame_gray, cv2.COLOR_GRAY2BGR)
        try:
            results = model.predict(
                source=frame_bgr,
                conf=_DETECT_CONF,
                iou=_DETECT_NMS,
                imgsz=640,
                verbose=False,
                device="cpu",
                max_det=_DETECT_TOP_K,
            )
        except Exception as e:
            print(f"[ERROR] PT 推理失败: {e}")
            return []

        if not results:
            return []
        result = results[0]
        boxes = getattr(result, "boxes", None)
        if boxes is None or len(boxes) == 0:
            return []

        xyxy = boxes.xyxy.cpu().numpy()
        scores = boxes.conf.cpu().numpy()
        cls_ids = boxes.cls.cpu().numpy().astype(np.int32)
        results_list = []
        for box, score, cls_id in zip(xyxy, scores, cls_ids):
            results_list.append((float(box[0]), float(box[1]), float(box[2]), float(box[3]), float(score), int(cls_id)))
        return results_list

    sess = _load_ort_session()
    if sess is None:
        return []

    lb, scale, pad_x, pad_y = _letterbox(frame_gray, 640)
    rgb3 = cv2.cvtColor(lb, cv2.COLOR_GRAY2RGB).astype(np.float32) / 255.0
    inp = np.transpose(rgb3, (2, 0, 1))[np.newaxis]

    output_names = [o.name for o in sess.get_outputs()]
    outputs = sess.run(output_names, {sess.get_inputs()[0].name: inp})
    detections = _decode_yolov8_outputs(outputs)

    results = []
    for box, score, cls_id in detections:
        x1 = max(0.0, (box[0] - pad_x) / scale)
        y1 = max(0.0, (box[1] - pad_y) / scale)
        x2 = min(float(w), (box[2] - pad_x) / scale)
        y2 = min(float(h), (box[3] - pad_y) / scale)
        results.append((x1, y1, x2, y2, score, cls_id))
    return results


def _summarize_detections(detections, frame_shape: Tuple[int, int]) -> Dict[str, Any]:
    class_counts: Dict[str, int] = {}
    items = []
    for x1, y1, x2, y2, score, cls_id in detections:
        name = _CLASS_NAMES.get(int(cls_id), f"cls{int(cls_id)}")
        class_counts[name] = class_counts.get(name, 0) + 1
        items.append({
            "class_id": int(cls_id),
            "class_name": name,
            "score": round(float(score), 4),
            "box": [round(float(x1), 1), round(float(y1), 1), round(float(x2), 1), round(float(y2), 1)],
        })
    frame_h, frame_w = frame_shape
    return {
        "timestamp": time.time(),
        "count": len(items),
        "class_counts": class_counts,
        "items": items[:12],
        "frame_width": int(frame_w),
        "frame_height": int(frame_h),
    }


def _update_detection_runtime(detections, frame_shape: Tuple[int, int]) -> Dict[str, Any]:
    summary = _summarize_detections(detections, frame_shape)
    ros_result = None
    if _ros_detection_hook is not None:
        try:
            ros_result = _ros_detection_hook(detections, frame_shape)
        except Exception as exc:
            ros_result = {
                "enabled": True,
                "action": "error",
                "reason": str(exc),
                "dispatched": False,
            }
    summary["ros"] = ros_result
    with _detect_state_lock:
        _last_detect_snapshot.update(summary)
        return dict(_last_detect_snapshot)


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
    os.makedirs(output_dir, exist_ok=True)
    cv2.imwrite(path, out)

    thumb = cv2.resize(out, (160, 90 if fmt == "640x360" else 80))
    _, buf = cv2.imencode(".jpg", thumb, [cv2.IMWRITE_JPEG_QUALITY, 72])
    thumb_b64 = base64.b64encode(buf).decode()

    info = {
        "filename": name,
        "path": path,
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
    """MJPEG 预览流，只输出摄像头原始画面，不叠加训练框。"""
    global camera, current_fps, _frame_count, _fps_ts, camera_connected
    global _fail_streak, _consecutive_failures, _last_reconnect_time
    RECONNECT_INTERVAL = 3.0
    FAIL_THRESHOLD = 10

    while True:
        with camera_lock:
            cap = camera
        if isinstance(cap, QtBridgeCapture):
            ret, payload = cap.read_color_jpeg()
            if ret and payload:
                camera_connected = True
                _fail_streak = 0
                _consecutive_failures = 0
                _frame_count += 1
                now = time.time()
                if now - _fps_ts >= 1.0:
                    current_fps = _frame_count / (now - _fps_ts)
                    _frame_count = 0
                    _fps_ts = now
                yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                       + payload + b"\r\n")
                continue
            frame = None
        else:
            frame = _read_display_frame(cap) if cap else None

        if frame is None:
            camera_connected = False
            _fail_streak += 1
            _consecutive_failures += 1
            now = time.time()
            if (not camera_bootstrap_active
                    and _consecutive_failures >= FAIL_THRESHOLD
                    and now - _last_reconnect_time > RECONNECT_INTERVAL):
                _last_reconnect_time = now
                _consecutive_failures = 0
                _fail_streak = 0
                print("[INFO] 视频流中断，自动尝试重连摄像头...")
                try:
                    with camera_lock:
                        if camera:
                            camera.release()
                        camera = open_camera(device_id_global, camera_source_global)
                    camera_connected = True
                    print("[INFO] 摄像头自动重连成功")
                except Exception as e:
                    print(f"[WARN] 自动重连失败: {e}")
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH, 3), dtype=np.uint8)
            cv2.putText(blk, "No Signal - Reconnecting...",
                        (CAMERA_WIDTH // 2 - 190, CAMERA_HEIGHT // 2 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (80, 80, 80), 2)
            cv2.putText(blk, "Click [Refresh Camera] to reconnect",
                        (CAMERA_WIDTH // 2 - 220, CAMERA_HEIGHT // 2 + 36),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (60, 60, 60), 1)
            _, buf = cv2.imencode(".jpg", blk, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                   + buf.tobytes() + b"\r\n")
            time.sleep(0.3)
            continue

        camera_connected = True
        _fail_streak = 0
        _consecutive_failures = 0
        _frame_count += 1
        now = time.time()
        if now - _fps_ts >= 1.0:
            current_fps = _frame_count / (now - _fps_ts)
            _frame_count = 0
            _fps_ts = now

        _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 97])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
               + buf.tobytes() + b"\r\n")


def _generate_detect_frames():
    """本地 YOLOv8+OSD 检测视频流（MJPEG）。"""
    global current_fps, _frame_count, _fps_ts
    while True:
        with camera_lock:
            cap = camera
        frame = _read_gray(cap) if cap else None

        if frame is None:
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH, 3), dtype=np.uint8)
            cv2.putText(blk, "No Signal", (CAMERA_WIDTH // 2 - 80, CAMERA_HEIGHT // 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.0, (60, 60, 60), 2)
            _, buf = cv2.imencode(".jpg", blk, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + buf.tobytes() + b"\r\n")
            time.sleep(0.5)
            continue

        # YOLOv8 需要灰度输入（_read_gray 已返回灰度帧）
        frame_gray = frame
        detections = detect_on_frame(frame_gray)
        _update_detection_runtime(detections, frame_gray.shape[:2])
        display = cv2.cvtColor(frame_gray, cv2.COLOR_GRAY2BGR)

        for (x1, y1, x2, y2, score, cls_id) in detections:
            color = _CLASS_COLORS[cls_id % len(_CLASS_COLORS)]
            name = _CLASS_NAMES.get(cls_id, f"cls{cls_id}")
            cv2.rectangle(display, (int(x1), int(y1)), (int(x2), int(y2)), color, 2)
            label = f"{name} {score:.2f}"
            (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
            ty = max(int(y1) - 4, th + 4)
            cv2.rectangle(display, (int(x1), ty - th - 4), (int(x1) + tw + 4, ty), color, -1)
            cv2.putText(display, label, (int(x1) + 2, ty - 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1)

        n = len(detections)
        cv2.putText(display, f"Det: {n}  conf>={_DETECT_CONF:.2f}", (8, 28),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 200, 80), 2)

        _frame_count += 1
        now = time.time()
        if now - _fps_ts >= 1.0:
            current_fps = _frame_count / (now - _fps_ts)
            _frame_count = 0
            _fps_ts = now

        _, buf = cv2.imencode(".jpg", display, [cv2.IMWRITE_JPEG_QUALITY, 75])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + buf.tobytes() + b"\r\n")


# ─── Flask 路由 ───────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("companion_ui.html", output_dir=output_dir)


@app.route("/video_feed")
def video_feed():
    return Response(
        generate_frames(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
            "Pragma": "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


@app.route("/detect_feed")
def detect_feed():
    return Response(
        _generate_detect_frames(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
            "Pragma": "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


@app.route("/detect_status")
def detect_status():
    model_exists = _DETECT_MODEL_PATH.exists()
    backend = _detect_model_backend_from_path(_DETECT_MODEL_PATH)
    model_loaded = _pt_model is not None if backend == "pt" else _ort_session is not None
    return jsonify({
        "ort_available": _ORT_AVAILABLE,
        "ultralytics_available": _ULTRALYTICS_AVAILABLE,
        "model_exists": model_exists,
        "model_path": str(_DETECT_MODEL_PATH),
        "model_loaded": model_loaded,
        "num_classes": _DETECT_NUM_CLASSES,
        "conf_threshold": _DETECT_CONF,
        "model_name": _DETECT_MODEL_PATH.name,
        "model_mode": _DETECT_MODEL_MODE,
        "model_backend": backend,
        "models": _list_detect_models(),
    })


@app.route("/detect_models")
def detect_models():
    return jsonify({
        "current_model": _DETECT_MODEL_PATH.name,
        "current_path": str(_DETECT_MODEL_PATH),
        "current_mode": _DETECT_MODEL_MODE,
        "current_backend": _detect_model_backend_from_path(_DETECT_MODEL_PATH),
        "models": _list_detect_models(),
    })


@app.route("/api/detect/latest")
def detect_latest():
    with _detect_state_lock:
        snapshot = dict(_last_detect_snapshot)
    snapshot["model_name"] = _DETECT_MODEL_PATH.name
    snapshot["model_mode"] = _DETECT_MODEL_MODE
    snapshot["model_backend"] = _detect_model_backend_from_path(_DETECT_MODEL_PATH)
    return jsonify(snapshot)


@app.route("/api/detect/snapshot", methods=["GET", "POST"])
def detect_snapshot():
    with camera_lock:
        cap = camera
    frame_gray = _read_gray(cap) if cap else None
    if frame_gray is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})

    detections = detect_on_frame(frame_gray)
    snapshot = _update_detection_runtime(detections, frame_gray.shape[:2])
    return jsonify({
        "success": True,
        "model_name": _DETECT_MODEL_PATH.name,
        "model_mode": _DETECT_MODEL_MODE,
        "model_backend": _detect_model_backend_from_path(_DETECT_MODEL_PATH),
        **snapshot,
    })


@app.route("/switch_detect_model", methods=["POST"])
def switch_detect_model():
    data = request.get_json(silent=True) or {}
    model_name = str(data.get("model_name") or data.get("model") or data.get("path") or "").strip()
    if not model_name:
        return jsonify({"success": False, "error": "缺少模型名称"})

    resolved = _resolve_detect_model_path(model_name)
    if resolved is None:
        return jsonify({"success": False, "error": f"未找到模型: {model_name}"})

    info = set_detect_model_path(resolved, persist=True)
    return jsonify({
        "success": True,
        **info,
        "models": _list_detect_models(),
    })


@app.route("/capture", methods=["POST"])
def do_capture():
    data = request.get_json(silent=True) or {}
    fmt = str(data.get("format") or DEFAULT_CAPTURE_FORMAT).strip()
    if fmt not in CAPTURE_FORMATS:
        return jsonify({"success": False, "error": f"不支持的格式: {fmt}"})
    with camera_lock:
        cap = camera
    frame = _read_display_frame(cap) if cap else None
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})
    info = _save_capture(frame, fmt)
    return jsonify({"success": True, "output_dir": output_dir, **info})


@app.route("/refresh_camera", methods=["POST"])
def refresh_camera():
    global camera, _fail_streak, _consecutive_failures, camera_connected
    try:
        with camera_lock:
            if camera:
                camera.release()
            camera = open_camera(device_id_global, camera_source_global)
            ok = camera is not None and camera.isOpened()
    except Exception as e:
        ok = False
        print(f"[WARN] 摄像头刷新异常: {e}")
    _fail_streak = 0
    _consecutive_failures = 0
    camera_connected = ok
    if ok:
        print("[INFO] 摄像头手动刷新成功")
        return jsonify({"success": True, "message": "摄像头已重新连接"})
    print("[WARN] 摄像头刷新失败")
    return jsonify({"success": False, "error": "无法连接到摄像头设备"})


@app.route("/camera_devices")
def camera_devices():
    global camera_devices_snapshot, camera_scan_active, camera_scan_error, camera_scan_started_at

    force_refresh = str(request.args.get("refresh") or "").lower() in {"1", "true", "yes"}
    if camera_scan_active and time.time() - camera_scan_started_at > 20.0:
        camera_scan_active = False
        camera_scan_error = "摄像头扫描超过 20 秒，已保留当前列表；可稍后再次扫描"
    bootstrapping = False
    if force_refresh:
        if not camera_scan_active:
            camera_scan_active = True
            camera_scan_started_at = time.time()
            camera_scan_error = ""
            threading.Thread(target=_scan_camera_devices_background, daemon=True, name="camera-device-scan").start()
        devices = list(camera_devices_snapshot)
        bootstrapping = True
    elif camera_devices_snapshot:
        devices = list(camera_devices_snapshot)
    elif camera_bootstrap_active or camera_scan_active:
        devices = []
        bootstrapping = True
    else:
        devices = list_camera_devices()
        camera_devices_snapshot = list(devices)
    items = []
    for d in devices:
        kind = "Gray" if d.get("is_grayscale") else "Color"
        y8 = "Y8" if d.get("supports_gray_fourcc") else "NoY8"
        if d["id"] == AURORA_WINDOW_DEVICE_ID:
            target = d.get("target") or {}
            target_label = target.get("label") or "Aurora Window"
            label = f"{target_label} - {d['actual_width']}x{d['actual_height']} score={d['score']}"
        else:
            device_name = str(d.get("device_name") or f"device {d['id']}").strip()
            pixel_format = str(d.get("pixel_format") or "").strip()
            format_suffix = f" {pixel_format}" if pixel_format else ""
            label = f"{device_name} - {d['actual_width']}x{d['actual_height']} {kind} {y8}{format_suffix} score={d['score']}"
        items.append({
            "id": d["id"],
            "label": label,
            "score": d["score"],
            "actual_width": d["actual_width"],
            "actual_height": d["actual_height"],
            "is_grayscale": d.get("is_grayscale", False),
            "has_content": d.get("has_content", False),
            "supports_gray_fourcc": d.get("supports_gray_fourcc", False),
            "source": d.get("source", CAMERA_SOURCE_WINDOWS),
            "source_label": d.get("source_label", _source_label(CAMERA_SOURCE_WINDOWS)),
            "device_name": d.get("device_name"),
            "pixel_format": d.get("pixel_format"),
            "raw8_hint": d.get("raw8_hint", False),
            "target": d.get("target"),
        })
    return jsonify({
        "current_device": device_id_global,
        "current_source": camera_source_global,
        "devices": items,
        "bootstrapping": bootstrapping or camera_scan_active,
        "scan_error": camera_scan_error,
        "qt_bridge": _qt_bridge_status(timeout=0.5),
    })


@app.route("/desktop_capture/status")
def desktop_capture_status():
    return jsonify(_serialize_capture_target())


@app.route("/desktop_windows")
def desktop_windows():
    force_start = str(request.args.get("start_aurora") or "").lower() in {"1", "true", "yes"}
    launch_info = _start_aurora_desktop() if force_start else None
    return jsonify({
        "items": _list_desktop_windows(),
        "current_target": _serialize_capture_target(),
        "aurora": launch_info,
    })


@app.route("/desktop_capture/start_aurora", methods=["POST"])
def desktop_capture_start_aurora():
    result = _start_aurora_desktop()
    return jsonify({
        "success": bool(result.get("running")),
        **result,
        "items": _list_desktop_windows(),
        "target": _serialize_capture_target(),
    })


@app.route("/desktop_capture/select_window", methods=["POST"])
def desktop_capture_select_window():
    data = request.get_json(silent=True) or {}
    info = None
    raw_target = data.get("target")
    if isinstance(raw_target, dict):
        candidate = dict(raw_target)
        try:
            candidate["hwnd"] = int(candidate.get("hwnd"))
        except (TypeError, ValueError):
            candidate = None
        if candidate:
            info = candidate
    if info is None:
        try:
            hwnd = int(data.get("hwnd"))
        except (TypeError, ValueError):
            return jsonify({"success": False, "error": "hwnd 必须是整数"})
        info = _describe_target_for_hwnd(hwnd)
    if info is None:
        return jsonify({"success": False, "error": "所选窗口无效或不可抓取"})
    info["crop_norm"] = None
    _set_capture_target(info)
    return jsonify({"success": True, "target": _serialize_capture_target()})


@app.route("/desktop_capture/pick_window", methods=["POST"])
def desktop_capture_pick_window():
    try:
        info = _pick_window_from_desktop()
        _set_capture_target(info)
        return jsonify({"success": True, "target": _serialize_capture_target()})
    except Exception as exc:
        return jsonify({"success": False, "error": str(exc)})


@app.route("/desktop_capture/pick_region", methods=["POST"])
def desktop_capture_pick_region():
    try:
        info = _pick_screen_region()
        _set_capture_target(info)
        return jsonify({"success": True, "target": _serialize_capture_target()})
    except Exception as exc:
        return jsonify({"success": False, "error": str(exc)})


@app.route("/desktop_capture/set_crop", methods=["POST"])
def desktop_capture_set_crop():
    data = request.get_json(silent=True) or {}
    crop_norm = data.get("crop_norm")
    if not crop_norm:
        target = _update_capture_crop_norm(None)
        return jsonify({"success": True, "target": target})
    if not isinstance(crop_norm, list) or len(crop_norm) != 4:
        return jsonify({"success": False, "error": "crop_norm 必须是 4 个归一化坐标"})
    x0 = _float01(crop_norm[0])
    y0 = _float01(crop_norm[1])
    x1 = _float01(crop_norm[2], 1.0)
    y1 = _float01(crop_norm[3], 1.0)
    if x1 - x0 < 0.02 or y1 - y0 < 0.02:
        return jsonify({"success": False, "error": "裁剪区域过小"})
    target = _update_capture_crop_norm([x0, y0, x1, y1])
    return jsonify({"success": True, "target": target})


@app.route("/desktop_capture/nudge", methods=["POST"])
def desktop_capture_nudge():
    data = request.get_json(silent=True) or {}
    dx = _float_signed(data.get("dx", 0.0), 0.0, 0.5)
    dy = _float_signed(data.get("dy", 0.0), 0.0, 0.5)
    dw = _float_signed(data.get("dw", 0.0), 0.0, 0.5)
    dh = _float_signed(data.get("dh", 0.0), 0.0, 0.5)
    with desktop_capture_lock:
        target = dict(desktop_capture_target)
    crop = target.get("crop_norm") or [0.0, 0.0, 1.0, 1.0]
    x0, y0, x1, y1 = [float(v) for v in crop]
    x0 = min(max(x0 + dx, 0.0), 0.98)
    y0 = min(max(y0 + dy, 0.0), 0.98)
    x1 = min(max(x1 + dx + dw, x0 + 0.02), 1.0)
    y1 = min(max(y1 + dy + dh, y0 + 0.02), 1.0)
    if x1 - x0 < 0.02 or y1 - y0 < 0.02:
        return jsonify({"success": False, "error": "微调后裁剪区域过小"})
    target = _update_capture_crop_norm([x0, y0, x1, y1])
    return jsonify({"success": True, "target": target})


@app.route("/desktop_capture/reset_crop", methods=["POST"])
def desktop_capture_reset_crop():
    target = _update_capture_crop_norm(None)
    return jsonify({"success": True, "target": target})


@app.route("/desktop_capture/reset", methods=["POST"])
def desktop_capture_reset():
    _set_capture_target({
        "mode": "auto_aurora",
        "label": "Aurora 自动窗口",
        "hwnd": None,
        "bbox": None,
        "crop_norm": None,
        "process_name": "Aurora.exe",
        "title": "Aurora",
        "class_name": "",
        "use_client_area": True,
    })
    return jsonify({"success": True, "target": _serialize_capture_target()})


@app.route("/desktop_capture/force_refresh", methods=["POST"])
def desktop_capture_force_refresh():
    with camera_lock:
        if isinstance(camera, AuroraWindowCapture):
            camera._target_signature = None
            camera._last_refresh = 0.0
            camera._refresh_handle(force=True)
    return jsonify({"success": True, "target": _serialize_capture_target()})


@app.route("/switch_camera", methods=["POST"])
def switch_camera():
    global camera, device_id_global, camera_source_global, _fail_streak, _consecutive_failures, camera_connected

    data = request.get_json(silent=True) or {}
    if "device" not in data:
        return jsonify({"success": False, "error": "缺少 device 参数"})

    try:
        new_device = int(data.get("device"))
    except (TypeError, ValueError):
        return jsonify({"success": False, "error": "device 必须是整数"})

    requested_source = str(data.get("source") or CAMERA_SOURCE_AUTO).strip().lower()
    if requested_source not in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AURORA_WINDOW, CAMERA_SOURCE_AUTO}:
        requested_source = CAMERA_SOURCE_AUTO

    try:
        if requested_source == CAMERA_SOURCE_AUTO:
            for info in list_camera_devices():
                if info["id"] == new_device:
                    requested_source = info.get("source", CAMERA_SOURCE_WINDOWS)
                    break
        if requested_source == CAMERA_SOURCE_AUTO:
            requested_probe = probe_camera_device(new_device)
            requested_source = requested_probe.get("source", CAMERA_SOURCE_WINDOWS)
        with camera_lock:
            if camera:
                camera.release()
            camera = open_camera(new_device, requested_source)
            ok = camera is not None and camera.isOpened()
        if not ok:
            raise RuntimeError("目标摄像头无法打开")
        device_id_global = new_device
        camera_source_global = requested_source
        _fail_streak = 0
        _consecutive_failures = 0
        camera_connected = True
        save_preferred_device(new_device)
        save_preferred_source(requested_source)
        print(f"[INFO] 已切换到摄像头设备 {new_device} ({requested_source})")
        return jsonify({"success": True, "device": new_device, "source": requested_source})
    except Exception as e:
        camera_connected = False
        return jsonify({"success": False, "error": str(e)})


@app.route("/recent_captures")
def recent_captures_api():
    return jsonify(list(recent_captures))


@app.route("/status")
def status():
    return jsonify({
        "connected": bool(camera_connected),
        "capture_count": capture_count,
        "fps": round(current_fps, 1),
        "output_dir": output_dir,
        "camera_resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
        "device": device_id_global,
        "source": camera_source_global,
        "source_label": _source_label(camera_source_global),
        "capture_target": _serialize_capture_target(),
        "qt_bridge": _qt_bridge_status(timeout=0.5),
        "model_name": _DETECT_MODEL_PATH.name,
        "model_path": str(_DETECT_MODEL_PATH),
        "model_mode": _DETECT_MODEL_MODE,
    })


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    global camera, output_dir, device_id_global, camera_source_global

    parser = argparse.ArgumentParser(description="Aurora Companion — Windows/A1 摄像头可视化采集伴侣")
    parser.add_argument("--device", type=int, default=-1,   help="摄像头设备 ID (默认: -1 自动优先 A1)")
    parser.add_argument("--source", type=str, default=CAMERA_SOURCE_AUTO,
                        help="输入源: windows / a1 / aurora_window / auto")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw",
                        help="拍照保存目录")
    parser.add_argument("--port",   type=int, default=5801, help="Web 服务端口 (默认: 5801)")
    parser.add_argument("--host",   type=str, default="127.0.0.1", help="监听地址")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    initial_source = str(args.source or CAMERA_SOURCE_AUTO).strip().lower()
    if initial_source not in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AURORA_WINDOW, CAMERA_SOURCE_AUTO}:
        initial_source = CAMERA_SOURCE_AUTO
    camera_source_global = initial_source

    if initial_source != CAMERA_SOURCE_AURORA_WINDOW:
        try:
            bridge_status = ensure_qt_bridge_running(timeout=2.0)
            if bridge_status.get("available", False):
                print(f"[INFO] Qt 相机桥已就绪: {QT_BRIDGE_URL}")
            else:
                print(f"[WARN] Qt 相机桥不可用，将自动回退 OpenCV: {bridge_status.get('error')}")
        except Exception as exc:
            print(f"[WARN] Qt 相机桥启动失败，将自动回退 OpenCV: {exc}")

    print(f"[INFO] Aurora Companion 正在启动 Web 服务: http://127.0.0.1:{args.port}")
    print(f"[INFO] 快捷键: 1=1280×720  2=640×360  R=刷新摄像头")
    camera_bootstrap = threading.Thread(target=bootstrap_camera, args=(args.device,), daemon=True)
    camera_bootstrap.start()
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
