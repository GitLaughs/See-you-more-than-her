#!/usr/bin/env python3
"""
Aurora Companion — 原始摄像头/A1 摄像头可视化采集伴侣

增强版拍照工具，在 aurora_capture 基础上提供：
  - 精心设计的现代白色玻璃态 UI
  - 摄像头断联自动检测 + 一键刷新恢复
  - 实时 FPS 及连接状态显示
  - 最近拍摄缩略图画廊 (最多 8 张)
    - 原始摄像头 / A1 采集源，训练输出 640×480 (中心裁剪)
  - A1↔STM32 底盘通信调试 + 联通性测试
  - 键盘快捷键：1/2/R

用法:
    python aurora_companion.py [--device 0] [--output ../../data/yolov8_dataset/raw] [--port 6201]
"""

import argparse
import base64
import contextlib
import json
import os
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, Optional, Tuple
from http.server import BaseHTTPRequestHandler
from urllib import error as urllib_error
from urllib import parse as urllib_parse
from urllib import request as urllib_request

import cv2
import numpy as np
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
# A1 / SC132GS 通过 USB 输出 720×1280 Y8 灰度帧
CAMERA_WIDTH = 720
CAMERA_HEIGHT = 1280
CAMERA_FPS = 30

CAPTURE_FORMATS = {
    "720x1280": (720, 1280),   # 原始灰度图（传感器采集分辨率）
    "640x480":  (640,  480),   # A1 训练集尺寸（中心裁剪）
}
DEFAULT_CAPTURE_FORMAT = "720x1280"

app = Flask(__name__, template_folder="templates")
try:
    from serial_terminal import get_latest_depth_frame, serial_term_bp
    app.register_blueprint(serial_term_bp)
    print("[INFO] A1 终端模块已加载")
except ImportError:
    def get_latest_depth_frame():
        return {"success": False, "error": "serial_terminal unavailable"}
    print("[WARN] serial_terminal 未找到，A1 终端模块不可用")

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: Optional[Any] = None
camera_lock = threading.Lock()
device_id_global = 0
_qt_stream_threads: Dict[Tuple[int, str], threading.Thread] = {}
_qt_stream_threads_lock = threading.Lock()
_qt_stream_latest_frames: Dict[Tuple[int, str], "LatestFrameCache"] = {}
camera_source_global = "auto"
camera_bootstrap_active = False
camera_devices_snapshot: list = []
camera_scan_active = False
camera_scan_error = ""
camera_scan_started_at = 0.0
output_dir: str = ""
capture_count = 0
recent_captures: deque = deque(maxlen=8)

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
PREFERRED_A1_CAMERA_HINTS = (
    "smartsens-flyingchip-a1",
    "flyingchip",
    "smartsens",
    "sc132",
)
QT_BRIDGE_SCRIPT = Path(__file__).with_name("qt_camera_bridge.py")
QT_BRIDGE_PORT = 5911
QT_BRIDGE_HOST = "127.0.0.1"
QT_BRIDGE_URL = f"http://{QT_BRIDGE_HOST}:{QT_BRIDGE_PORT}"
COMPANION_PORT = 5801
QT_BRIDGE_PROTOCOL_VERSION = 2
QT_BRIDGE_OWNER_STATE_FILE = Path(__file__).with_name(".qt_bridge_owner.json")
AURORA_PYTHON_ENV_VAR = "AURORA_PYTHON"
CAMERA_SOURCE_WINDOWS = "windows"
CAMERA_SOURCE_A1 = "a1"
CAMERA_SOURCE_AUTO = "auto"
SOURCE_LABELS = {
    CAMERA_SOURCE_WINDOWS: "原始摄像头",
    CAMERA_SOURCE_A1: "A1 开发板",
    CAMERA_SOURCE_AUTO: "自动识别",
}
_qt_bridge_process: Optional[subprocess.Popen] = None
_qt_bridge_lock = threading.Lock()


class LatestFrameCache:
    def __init__(self):
        self._condition = threading.Condition()
        self._sequence = 0
        self._payload: Optional[bytes] = None

    def publish(self, payload: Optional[bytes]) -> int:
        if not payload:
            return self._sequence
        with self._condition:
            self._sequence += 1
            self._payload = payload
            self._condition.notify_all()
            return self._sequence

    def wait_for_next(self, last_sequence: int = 0, timeout: float = 1.0) -> Optional[Tuple[int, bytes]]:
        deadline = time.time() + max(0.0, timeout)
        with self._condition:
            while self._sequence <= last_sequence or not self._payload:
                remaining = deadline - time.time()
                if remaining <= 0:
                    return None
                self._condition.wait(remaining)
            return self._sequence, self._payload

# ─── YOLOv8 检测器（PC 端 ONNX 推理）────────────────────────────────────────
MODEL_ROOT = Path(__file__).parent.parent.parent / "models"
_DETECT_MODEL_PATH = MODEL_ROOT / "best_a1_640x480.onnx"
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
_CLASS_NAMES = {0: "person", 1: "gesture1", 2: "gesture2", 3: "obstacle_box"}
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

def _mark_camera_connected() -> None:
    global camera_connected, _fail_streak, _consecutive_failures
    camera_connected = True
    _fail_streak = 0
    _consecutive_failures = 0


def _mark_camera_disconnected() -> None:
    global camera_connected
    camera_connected = False


def _reset_camera_failure_state() -> None:
    global _fail_streak, _consecutive_failures
    _fail_streak = 0
    _consecutive_failures = 0


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


def _source_label(source: str) -> str:
    return SOURCE_LABELS.get(source, source)


def _device_name_looks_like_a1(device_name: str) -> bool:
    text = str(device_name or "").strip().lower()
    if not text:
        return False
    if PREFERRED_A1_CAMERA_NAME.lower() in text:
        return True
    return any(token in text for token in PREFERRED_A1_CAMERA_HINTS)


def _infer_device_source(info: dict) -> str:
    if info.get("supports_gray_fourcc"):
        return CAMERA_SOURCE_A1
    # 仅当帧有实际内容时才凭灰度特征判断为 A1；全黑帧 (0==0==0) 不能作为依据
    if info.get("is_grayscale") and info.get("has_content"):
        return CAMERA_SOURCE_A1
    return CAMERA_SOURCE_WINDOWS


def _normalize_frame_for_display(frame: np.ndarray) -> np.ndarray:
    """统一 A1 原始帧尺寸，但保留原始通道数。"""
    if frame is None:
        return frame
    if len(frame.shape) >= 2:
        current_source = camera_source_global
        height, width = frame.shape[:2]
        if current_source == CAMERA_SOURCE_A1:
            # 某些后端会把 Y8/UYVY 误判为 2 字节/像素格式，暴露为 360×1280。
            # 对于360×1280，先尝试提取有效的Y通道
            if height >= 1000 and width <= 400:
                if len(frame.shape) == 3:
                    # 如果是彩色格式，可能需要特殊处理
                    if frame.shape[2] == 3 or frame.shape[2] == 2:
                        # 尝试提取Y通道或直接展开
                        try:
                            if frame.shape[2] == 2:
                                # 可能是UYVY或YUYV被解释为2通道
                                # 尝试提取第一个通道作为Y
                                y_channel = frame[:, :, 0]
                                frame = cv2.resize(y_channel, (width * 2, height), interpolation=cv2.INTER_NEAREST)
                            else:
                                # 3通道，提取最佳通道
                                frame = _extract_best_mono_channel(frame)
                                frame = cv2.resize(frame, (width * 2, height), interpolation=cv2.INTER_NEAREST)
                        except Exception:
                            # 出错则回退到简单resize
                            frame = cv2.resize(frame, (width * 2, height), interpolation=cv2.INTER_NEAREST)
                    else:
                        # 其他通道情况
                        frame = cv2.resize(frame, (width * 2, height), interpolation=cv2.INTER_NEAREST)
                else:
                    # 单通道，直接resize
                    frame = cv2.resize(frame, (width * 2, height), interpolation=cv2.INTER_NEAREST)
            
            # 确保最终尺寸正确
            if frame is not None and len(frame.shape) >= 2:
                fh, fw = frame.shape[:2]
                if (fh, fw) != (CAMERA_HEIGHT, CAMERA_WIDTH):
                    frame = cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT), interpolation=cv2.INTER_NEAREST)
            return frame
    return frame


def _display_dims_for_device(info: dict) -> Tuple[int, int]:
    width = int(info.get("actual_width") or 0)
    height = int(info.get("actual_height") or 0)
    source = info.get("source", CAMERA_SOURCE_WINDOWS)
    pixel = str(info.get("pixel_format") or "").lower()
    if source == CAMERA_SOURCE_A1 and height >= 1000 and width <= 400 and any(token in pixel for token in ("uyvy", "yuyv", "yuv")):
        return width * 2, height
    return width, height


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
    cap.set(cv2.CAP_PROP_CONVERT_RGB, 1)
    if source == CAMERA_SOURCE_A1:
        print("[INFO] A1 OpenCV 兜底以驱动默认格式读取，随后按 Aurora 风格转灰度")
        # 尝试设置为更合适的分辨率
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 720)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1280)
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


def load_preferred_source() -> Optional[str]:
    try:
        if not PREFERRED_SOURCE_FILE.exists():
            return None
        text = PREFERRED_SOURCE_FILE.read_text(encoding="utf-8").strip().lower()
        if text in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AUTO}:
            return text
    except Exception:
        return None
    return None


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


def _set_qt_bridge_endpoint(port: int) -> None:
    global QT_BRIDGE_PORT, QT_BRIDGE_URL
    QT_BRIDGE_PORT = int(port)
    QT_BRIDGE_URL = f"http://{QT_BRIDGE_HOST}:{QT_BRIDGE_PORT}"


def _qt_bridge_port_available(host: str, port: int) -> bool:
    from http.server import ThreadingHTTPServer as _ThreadingHTTPServer

    server = None
    try:
        server = _ThreadingHTTPServer((host, int(port)), BaseHTTPRequestHandler)
        return True
    except OSError:
        return False
    finally:
        if server is not None:
            with contextlib.suppress(Exception):
                server.server_close()


def _resolve_qt_bridge_port(host: str, preferred_port: int, blocked_ports: Optional[set] = None) -> int:
    blocked = {int(port) for port in (blocked_ports or set())}
    preferred = int(preferred_port)
    if preferred not in blocked and _qt_bridge_port_available(host, preferred):
        return preferred
    for candidate in range(preferred + 1, 65536):
        if candidate in blocked:
            continue
        if _qt_bridge_port_available(host, candidate):
            return candidate
    raise RuntimeError(f"No available Qt bridge port from {preferred_port}")


def _resolve_available_port(preferred_port: int, host: Optional[str] = None, blocked_ports: Optional[set] = None) -> int:
    return _resolve_qt_bridge_port(host or QT_BRIDGE_HOST, preferred_port, blocked_ports=blocked_ports)


def _qt_bridge_stop(timeout: float = 5.0) -> dict:
    try:
        return _qt_bridge_request("/stop", method="POST", timeout=timeout)
    except Exception as e:
        return {"success": False, "error": str(e)}


def _qt_bridge_status(timeout: float = 1.0) -> Optional[dict]:
    try:
        return _qt_bridge_request("/status", timeout=timeout)
    except Exception:
        return None


def _qt_bridge_shutdown(timeout: float = 5.0) -> dict:
    try:
        return _qt_bridge_request("/shutdown", method="POST", timeout=timeout)
    except Exception as e:
        return {"success": False, "error": str(e)}


def load_qt_bridge_owner_state() -> Optional[dict]:
    try:
        if not QT_BRIDGE_OWNER_STATE_FILE.exists():
            return None
        payload = json.loads(QT_BRIDGE_OWNER_STATE_FILE.read_text(encoding="utf-8"))
        return payload if isinstance(payload, dict) else None
    except Exception:
        return None


def save_qt_bridge_owner_state(pid: int) -> None:
    payload = {
        "pid": int(pid),
        "port": QT_BRIDGE_PORT,
        "script": str(QT_BRIDGE_SCRIPT),
    }
    QT_BRIDGE_OWNER_STATE_FILE.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")


def clear_qt_bridge_owner_state() -> None:
    with contextlib.suppress(FileNotFoundError):
        QT_BRIDGE_OWNER_STATE_FILE.unlink()


def cleanup_qt_bridge_owner_process() -> None:
    state = load_qt_bridge_owner_state()
    if not state:
        return
    pid = int(state.get("pid") or 0)
    if pid <= 0:
        clear_qt_bridge_owner_state()
        return
    if sys.platform == "win32":
        script = rf"""
$proc = Get-CimInstance Win32_Process -Filter "ProcessId={pid}" -ErrorAction SilentlyContinue
if ($proc) {{
    $cmd = [string]$proc.CommandLine
    if ($cmd -match "qt_camera_bridge\.py") {{
        Stop-Process -Id {pid} -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 300
    }}
}}
"""
        try:
            subprocess.run(
                ["powershell", "-NoProfile", "-Command", script],
                capture_output=True,
                timeout=6,
                check=False,
            )
        except Exception:
            pass
    clear_qt_bridge_owner_state()


def _wait_for_qt_bridge_port_release(timeout: float = 2.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if _qt_bridge_port_available(QT_BRIDGE_HOST, QT_BRIDGE_PORT):
            return True
        time.sleep(0.1)
    return _qt_bridge_port_available(QT_BRIDGE_HOST, QT_BRIDGE_PORT)


def cleanup_qt_bridge_runtime(timeout: float = 5.0) -> dict:
    global _qt_bridge_process

    shutdown_result = _qt_bridge_shutdown(timeout=timeout)
    proc = _qt_bridge_process
    if proc is not None and proc.poll() is None:
        try:
            proc.terminate()
            proc.wait(timeout=timeout)
        except Exception:
            with contextlib.suppress(Exception):
                proc.kill()
                proc.wait(timeout=1.0)
    _qt_bridge_process = None
    cleanup_qt_bridge_owner_process()
    _stop_stale_qt_bridge_on_port()
    released = _wait_for_qt_bridge_port_release(timeout=2.0)
    clear_qt_bridge_owner_state()
    if shutdown_result.get("success"):
        return shutdown_result
    if released:
        return {"success": True, "message": "Qt bridge stopped"}
    return {"success": False, "error": shutdown_result.get("error") or "Qt bridge port still busy"}


def shutdown_qt_bridge(timeout: float = 5.0) -> dict:
    return cleanup_qt_bridge_runtime(timeout=timeout)


def _qt_bridge_is_current(status: Optional[dict]) -> bool:
    if status is None or not status.get("available", False):
        return False
    return int(status.get("bridge_version") or 0) >= QT_BRIDGE_PROTOCOL_VERSION


def summarize_qt_bridge_status(status: Optional[Dict[str, Any]]) -> Dict[str, str]:
    payload = status if isinstance(status, dict) else {}
    if not payload.get("available", False):
        detail = str(payload.get("error") or payload.get("message") or "Qt 相机桥不可用")
        return {"state": "unavailable", "detail": detail}

    if not payload.get("connected", False):
        detail = str(payload.get("message") or "Qt 相机桥未连接设备")
        return {"state": "disconnected", "detail": detail}

    frame_count = int(payload.get("frame_count") or 0)
    detail = str(payload.get("message") or "Qt 相机桥已连接")
    if frame_count <= 0:
        return {"state": "waiting_for_first_frame", "detail": f"{detail}，等待首帧"}
    return {"state": "streaming", "detail": detail}


def classify_qt_bridge_failure(status: Optional[Dict[str, Any]], error_text: str) -> Dict[str, str]:
    payload = status if isinstance(status, dict) else {}
    text = str(error_text or payload.get("error") or payload.get("message") or "Qt 相机桥错误")
    lowered = text.lower()

    if "未找到设备" in text or "device not found" in lowered:
        return {
            "code": "device_not_found",
            "severity": "error",
            "hint": "确认相机已连接，并在 Aurora.exe 打开后重试刷新摄像头。",
            "detail": text,
        }

    if "未收到视频帧" in text or "no frame" in lowered:
        device_name = payload.get("device_name") or "当前设备"
        return {
            "code": "no_frame_after_switch",
            "severity": "error",
            "hint": f"Qt 相机桥已切到 {device_name} 但还没出帧。先确认 Aurora.exe 已打开，再重试刷新摄像头。",
            "detail": text,
        }

    summary = summarize_qt_bridge_status(payload)
    if summary["state"] == "unavailable":
        return {
            "code": "bridge_unavailable",
            "severity": "error",
            "hint": "检查 PySide6 / QtMultimedia 依赖，或重启 Qt 相机桥。",
            "detail": text,
        }

    if summary["state"] == "disconnected":
        return {
            "code": "bridge_disconnected",
            "severity": "error",
            "hint": "确认目标相机存在，然后重新切换设备。",
            "detail": text,
        }

    return {
        "code": "unknown",
        "severity": "error",
        "hint": "查看 Qt 相机桥状态与 Aurora 日志，再重试。",
        "detail": text,
    }


def _python_has_module(python_exe: str, module_name: str) -> bool:
    probe_code = (
        "import importlib; "
        f"importlib.import_module({module_name!r})"
    )
    if module_name == "PySide6":
        probe_code = "import PySide6; from PySide6.QtMultimedia import QMediaDevices, QCamera, QVideoSink"
    try:
        result = subprocess.run(
            [python_exe, "-c", probe_code],
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
    preferred_python = str(os.environ.get(AURORA_PYTHON_ENV_VAR, "")).strip()
    candidates = []
    if preferred_python:
        candidates.append(preferred_python)
    candidates.extend([
        repo_root / "venv_39" / "Scripts" / "python.exe",
        Path(sys.executable),
    ])
    if sys.platform == "win32":
        candidates.append("python")
    seen = set()
    for candidate in candidates:
        python_exe = str(candidate)
        key = python_exe.lower()
        if key in seen:
            continue
        seen.add(key)
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
        Write-Host "[Aurora] Terminating stale Qt camera bridge on port {QT_BRIDGE_PORT} (PID $procId)"
        Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
    }}
}}
Start-Sleep -Milliseconds 400
"""
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", script],
            capture_output=True,
            timeout=8,
            check=False,
        )
        if result.stdout and len(result.stdout) > 0:
            print(result.stdout.decode('utf-8', errors='replace'))
    except Exception as e:
        print(f"[WARN] Stopping stale Qt bridge: {e}")
        pass


def _start_aurora_desktop() -> Dict[str, Any]:
    if sys.platform != "win32":
        return {"started": False, "running": False, "error": "Aurora.exe 启动仅支持 Windows"}
    if not AURORA_EXE_PATH.exists():
        return {"started": False, "running": False, "error": f"未找到 Aurora.exe: {AURORA_EXE_PATH}"}

    script = f"""
$auroraExe = [System.IO.Path]::GetFullPath('{str(AURORA_EXE_PATH).replace("'", "''")}')
$auroraDir = Split-Path -Parent $auroraExe
$existing = Get-Process -Name "Aurora" -ErrorAction SilentlyContinue | Where-Object {{
    try {{ $_.Path -and [System.StringComparer]::OrdinalIgnoreCase.Equals($_.Path, $auroraExe) }} catch {{ $false }}
}} | Select-Object -First 1
if ($existing) {{
    Write-Output "running:$($existing.Id)"
    exit 0
}}
Start-Process -FilePath $auroraExe -WorkingDirectory $auroraDir | Out-Null
Start-Sleep -Seconds 3
Write-Output "started"
"""
    result = subprocess.run(
        ["powershell", "-NoProfile", "-Command", script],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=12,
        check=False,
    )
    output = (result.stdout or result.stderr or "").strip()
    if result.returncode != 0:
        raise RuntimeError(output or "Aurora.exe 启动失败")
    if output.startswith("running:"):
        return {"started": False, "running": True, "path": str(AURORA_EXE_PATH), "pid": output.split(":", 1)[1]}
    return {"started": True, "running": True, "path": str(AURORA_EXE_PATH)}


def ensure_qt_bridge_running(timeout: float = 12.0) -> dict:
    global _qt_bridge_process

    status = _qt_bridge_status(timeout=0.6)
    proc = _qt_bridge_process
    should_cleanup = status is not None and not _qt_bridge_is_current(status)
    if not should_cleanup and proc is not None and proc.poll() is not None:
        should_cleanup = True
    if should_cleanup:
        print("[Aurora] Found stale Qt bridge, stopping...")
        cleanup_qt_bridge_runtime(timeout=3.0)
        time.sleep(0.3)

    with _qt_bridge_lock:
        status = _qt_bridge_status(timeout=0.6)
        proc = _qt_bridge_process
        should_cleanup = status is not None and not _qt_bridge_is_current(status)
        if not should_cleanup and proc is not None and proc.poll() is not None:
            should_cleanup = True
        if _qt_bridge_is_current(status):
            return status
        if should_cleanup:
            cleanup_qt_bridge_runtime(timeout=3.0)
            time.sleep(0.3)

        if not QT_BRIDGE_SCRIPT.exists():
            raise RuntimeError(f"Qt 相机桥脚本不存在: {QT_BRIDGE_SCRIPT}")

        if _qt_bridge_process is None or _qt_bridge_process.poll() is not None:
            bridge_python = _select_qt_bridge_python()
            bridge_port = _resolve_available_port(QT_BRIDGE_PORT, host=QT_BRIDGE_HOST)
            _set_qt_bridge_endpoint(bridge_port)
            kwargs: Dict[str, Any] = {
                "cwd": str(Path(__file__).resolve().parent),
            }
            _qt_bridge_process = subprocess.Popen(
                [bridge_python, str(QT_BRIDGE_SCRIPT.resolve()), "--host", QT_BRIDGE_HOST, "--port", str(QT_BRIDGE_PORT)],
                **kwargs,
            )
            save_qt_bridge_owner_state(_qt_bridge_process.pid)
            print(f"[Aurora] Qt bridge started (PID: {_qt_bridge_process.pid}, port: {QT_BRIDGE_PORT})")

        deadline = time.time() + timeout
        last_status = None
        while time.time() < deadline:
            last_status = _qt_bridge_status(timeout=0.8)
            if last_status is not None:
                return last_status
            if _qt_bridge_process is not None and _qt_bridge_process.poll() is not None:
                raise RuntimeError(f"Qt 相机桥启动失败，进程已退出 (PID: {_qt_bridge_process.pid})")
            time.sleep(0.3)

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
            _device_name_looks_like_a1(item.get("device_name") or ""),
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


def _qt_bridge_wait_for_frame(mode: str = "color", timeout: float = 5.0) -> bool:
    deadline = time.time() + max(0.2, timeout)
    while time.time() < deadline:
        if _qt_bridge_fetch_frame_bytes(mode=mode, timeout=0.9):
            return True
        time.sleep(0.08)
    return False


class QtBridgeCapture:
    """使用独立 QtMultimedia 桥进程获取相机画面。"""

    def __init__(self, device_id: int, source: str):
        self.device_id = int(device_id)
        self.source = source if source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1} else CAMERA_SOURCE_AUTO
        self._last_status: Dict[str, Any] = {}
        self._stream_key = (self.device_id, self.source)
        self._open()

    def stream_key(self) -> Tuple[int, str]:
        return self._stream_key

    def latest_frame_cache(self) -> LatestFrameCache:
        with _qt_stream_threads_lock:
            cache = _qt_stream_latest_frames.get(self._stream_key)
            if cache is None:
                cache = LatestFrameCache()
                _qt_stream_latest_frames[self._stream_key] = cache
            return cache

    def start_stream_worker(self) -> LatestFrameCache:
        cache = self.latest_frame_cache()
        with _qt_stream_threads_lock:
            worker = _qt_stream_threads.get(self._stream_key)
            if worker is not None and worker.is_alive():
                return cache
            worker = threading.Thread(
                target=_qt_bridge_stream_worker,
                args=(self.device_id, self.source, cache),
                daemon=True,
                name=f"qt-bridge-stream-{self.device_id}-{self.source}",
            )
            _qt_stream_threads[self._stream_key] = worker
            worker.start()
        return cache

    def wait_for_next_jpeg(self, last_sequence: int = 0, timeout: float = 1.0) -> Optional[Tuple[int, bytes]]:
        return self.start_stream_worker().wait_for_next(last_sequence=last_sequence, timeout=timeout)

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
        if not _qt_bridge_wait_for_frame(mode="color", timeout=5.0):
            device_name = self._last_status.get("device_name") or f"device {self.device_id}"
            raise RuntimeError(f"Qt 相机桥已切换到 {device_name}，但 5 秒内未收到视频帧")
        self.start_stream_worker()

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
        try:
            result = _qt_bridge_stop(timeout=5.0)
            print(f"[INFO] Camera released: {result.get('message', '')}")
        except Exception as e:
            print(f"[WARN] Error releasing camera: {e}")

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


def _qt_bridge_stream_worker(device_id: int, source: str, cache: LatestFrameCache) -> None:
    stream_key = (int(device_id), source)
    while True:
        with camera_lock:
            cap = camera
        if not isinstance(cap, QtBridgeCapture) or cap.stream_key() != stream_key:
            time.sleep(0.05)
            continue
        ret, payload = cap.read_color_jpeg()
        if ret and payload:
            cache.publish(payload)
            time.sleep(0.001 if source == CAMERA_SOURCE_A1 else 0.005)
            continue
        time.sleep(0.01)


def generate_frames_for_capture(cap: Any):
    last_sequence = 0
    while True:
        if isinstance(cap, QtBridgeCapture):
            item = cap.wait_for_next_jpeg(last_sequence=last_sequence, timeout=1.0)
            if item is None:
                continue
            last_sequence, payload = item
            _mark_camera_connected()
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + payload + b"\r\n")
            continue
        frame = _read_display_frame(cap) if cap else None
        if frame is None:
            break
        quality = 84 if camera_source_global == CAMERA_SOURCE_A1 else 92
        _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, quality])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + buf.tobytes() + b"\r\n")

def probe_camera_device(device_id: int) -> dict:
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


def choose_camera_device(requested_device: int, requested_source: Optional[str] = None) -> Tuple[int, list]:
    """返回 (device_id, candidates)。requested_device=-1 时自动优先 A1。"""
    if requested_device >= 0:
        return requested_device, list_camera_devices()

    candidates = list_camera_devices()
    preferred_source = requested_source if requested_source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AUTO} else None
    if preferred_source is None:
        preferred_source = load_preferred_source()

    def matches_requested_source(info: dict) -> bool:
        if preferred_source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1}:
            return info.get("source") == preferred_source
        return True

    preferred = load_preferred_device()
    if preferred is not None:
        preferred_info = next((c for c in candidates if int(c.get("id", -999)) == int(preferred)), None)
        if preferred_info is None:
            preferred_info = probe_camera_device(preferred)
        auto_has_a1 = (
            preferred_source == CAMERA_SOURCE_AUTO
            and any(c.get("source") == CAMERA_SOURCE_A1 for c in candidates)
            and preferred_info.get("source") != CAMERA_SOURCE_A1
        )
        if (preferred_info["opened"]
                and not auto_has_a1
                and matches_requested_source(preferred_info)
                and (preferred_info.get("has_content") or preferred_info.get("supports_gray_fourcc"))):
            if not any(int(c.get("id", -999)) == int(preferred) for c in candidates):
                candidates.append(preferred_info)
            return preferred, candidates

    if not candidates:
        return 0, []

    if preferred_source in {CAMERA_SOURCE_A1, CAMERA_SOURCE_AUTO, None}:
        a1_candidates = [c for c in candidates if c.get("source") == CAMERA_SOURCE_A1]
        if a1_candidates:
            a1_candidates.sort(
                key=lambda item: (
                    bool(item.get("supports_gray_fourcc")),
                    _device_name_looks_like_a1(item.get("device_name") or ""),
                    int(item.get("score") or 0),
                    -abs(int(item.get("actual_width") or 0) - CAMERA_WIDTH)
                    - abs(int(item.get("actual_height") or 0) - CAMERA_HEIGHT),
                ),
                reverse=True,
            )
            selected = a1_candidates[0]["id"]
            candidates_sorted = sorted(candidates, key=lambda x: (
                x.get("source") == CAMERA_SOURCE_A1,
                bool(x.get("supports_gray_fourcc")),
                _device_name_looks_like_a1(x.get("device_name") or ""),
                int(x.get("score") or 0),
                x["id"],
            ), reverse=True)
            return selected, candidates_sorted

    candidates_sorted = sorted(candidates, key=lambda x: (x["score"], x["id"]), reverse=True)
    return candidates_sorted[0]["id"], candidates_sorted


def open_camera(device_id: int, source: str = CAMERA_SOURCE_AUTO) -> Optional[Any]:
    """打开物理摄像头输入源（仅使用Qt桥）。"""
    if source == CAMERA_SOURCE_AUTO:
        source = camera_source_global if camera_source_global != CAMERA_SOURCE_AUTO else CAMERA_SOURCE_WINDOWS

    cap = QtBridgeCapture(device_id, source)
    if cap.isOpened():
        status = _qt_bridge_status(timeout=0.8) or {}
        msg = status.get("message") or f"Qt 相机桥已连接设备 {device_id}"
        print(f"[INFO] {msg}")
        return cap
    
    raise RuntimeError(f"Qt相机桥无法打开设备 {device_id}")


def bootstrap_camera(requested_device: int, requested_source: str = CAMERA_SOURCE_AUTO) -> None:
    """在后台完成摄像头自动探测与打开，避免阻塞 Web 服务启动。"""
    global camera, device_id_global, camera_source_global, camera_connected, _fail_streak, _consecutive_failures
    global camera_bootstrap_active, camera_devices_snapshot

    camera_bootstrap_active = True
    try:
        selected_device, candidates = choose_camera_device(requested_device, requested_source)
        if candidates:
            selected_info = next((c for c in candidates if c["id"] == selected_device), None)
            if selected_info is None:
                selected_info = probe_camera_device(selected_device)
        else:
            selected_info = probe_camera_device(selected_device)

        camera_devices_snapshot = list(candidates) if candidates else [selected_info]

        if requested_source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1}:
            camera_source_global = requested_source
        else:
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
                and not isinstance(opened_camera, QtBridgeCapture)):
            print("[INFO] 刷新 DirectShow 初始化队列帧...")
            with _suppress_c_stderr():
                for _ in range(5):
                    opened_camera.read()

        with camera_lock:
            if camera:
                camera.release()
            camera = opened_camera

        if opened_camera is not None and opened_camera.isOpened():
            _mark_camera_connected()
        else:
            _mark_camera_disconnected()
            _reset_camera_failure_state()
    except Exception as e:
        print(f"[WARN] 摄像头后台初始化失败: {e}")
        with camera_lock:
            if camera:
                camera.release()
            camera = None
        _mark_camera_disconnected()
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

    current_source = camera_source_global
    if current_source == CAMERA_SOURCE_A1 and len(frame.shape) == 3 and frame.shape[2] >= 3 and not _is_effectively_grayscale(frame):
        frame = _extract_best_mono_channel(frame)
    corrected = _normalize_frame_for_display(frame)
    if corrected is None:
        return None

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
        first_line = text.split('\n')[0].strip()
        return first_line or None
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
    if MODEL_ROOT.exists():
        for suffix in (".onnx", ".pt"):
            for path in sorted(MODEL_ROOT.iterdir(), key=lambda item: item.name.lower()):
                if path.is_file() and path.suffix.lower() == suffix:
                    return path.resolve()
    return _DETECT_MODEL_PATH


try:
    set_detect_model_path(_initial_detect_model_path(), persist=False)
except Exception as exc:
    print(f"[WARN] 初始检测模型不可用: {exc}")


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
    """Legacy square preview helper; A1 mainline uses crop+resize to 640×480."""
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


def _resize_for_a1_train(frame_gray: np.ndarray) -> np.ndarray:
    cropped = crop_center(frame_gray, 720, 540)
    return cv2.resize(cropped, (640, 480))


def _prepare_detect_input(frame_gray: np.ndarray):
    resized = _resize_for_a1_train(frame_gray)
    rgb3 = cv2.cvtColor(resized, cv2.COLOR_GRAY2RGB).astype(np.float32) / 255.0
    inp = np.transpose(rgb3, (2, 0, 1))[np.newaxis]
    return resized, inp


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
    """对灰度帧运行 YOLOv8 推理，返回原始帧坐标系下的检测框。"""
    h, w = frame_gray.shape
    if _DETECT_MODEL_PATH.suffix.lower() == ".pt":
        model = _load_pt_model()
        if model is None:
            return []
        frame_bgr = cv2.cvtColor(frame_gray, cv2.COLOR_GRAY2BGR)
        try:
            # PC preview path only; imgsz=640 does not define the A1 640×480 deployment tensor.
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

    resized, inp = _prepare_detect_input(frame_gray)
    output_names = [o.name for o in sess.get_outputs()]
    outputs = sess.run(output_names, {sess.get_inputs()[0].name: inp})
    detections = _decode_yolov8_outputs(outputs)

    results = []
    x_scale = float(w) / float(resized.shape[1])
    y_scale = float(h) / float(resized.shape[0])
    for box, score, cls_id in detections:
        x1 = max(0.0, box[0] * x_scale)
        y1 = max(0.0, box[1] * y_scale)
        x2 = min(float(w), box[2] * x_scale)
        y2 = min(float(h), box[3] * y_scale)
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
    with _detect_state_lock:
        _last_detect_snapshot.update(summary)
        return dict(_last_detect_snapshot)


def _judge_action_from_snapshot(snapshot: Dict[str, Any]) -> str:
    counts = snapshot.get("class_counts") or {}
    if counts.get("gesture2", 0) > 0:
        return "stop"
    if counts.get("obstacle_box", 0) > 0:
        return "avoid"
    if counts.get("gesture1", 0) > 0:
        return "forward"
    return "idle"


def _judge_risk_from_snapshot(snapshot: Dict[str, Any]) -> str:
    items = snapshot.get("items") or []
    frame_w = float(snapshot.get("frame_width") or CAMERA_WIDTH)
    frame_h = float(snapshot.get("frame_height") or CAMERA_HEIGHT)
    best_score = 0.0
    best_area = 0.0
    best_bottom = 0.0
    for item in items:
        if item.get("class_name") != "obstacle_box":
            continue
        box = item.get("box") or [0, 0, 0, 0]
        if len(box) != 4:
            continue
        x1, y1, x2, y2 = [float(v) for v in box]
        area = max(0.0, x2 - x1) * max(0.0, y2 - y1)
        area_ratio = area / max(1.0, frame_w * frame_h)
        bottom_ratio = y2 / max(1.0, frame_h)
        score = float(item.get("score") or 0.0)
        if area_ratio + score * 0.05 > best_area + best_score * 0.05:
            best_score = score
            best_area = area_ratio
            best_bottom = bottom_ratio
    if best_area >= 0.20 or best_bottom >= 0.72:
        return "high"
    if best_area >= 0.10 or best_bottom >= 0.58:
        return "medium"
    if best_score > 0:
        return "low"
    return "clear"


def _save_capture(frame: np.ndarray, fmt: str) -> dict:
    global capture_count
    tw, th = CAPTURE_FORMATS[fmt]

    if fmt == "640x480":
        # A1 720×1280 灰度帧 → 中心裁剪/缩放到 640×480 训练图
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

    thumb_h = 120 if fmt == "640x480" else 160
    thumb = cv2.resize(out, (90 if fmt == "720x1280" else 160, thumb_h))
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
            item = cap.wait_for_next_jpeg(timeout=1.0)
            if item is not None:
                _sequence, payload = item
                _mark_camera_connected()
                _frame_count += 1
                now = time.time()
                if now - _fps_ts >= 1.0:
                    current_fps = _frame_count / (now - _fps_ts)
                    _frame_count = 0
                    _fps_ts = now
                yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + payload + b"\r\n")
                continue
            frame = None
        else:
            frame = _read_display_frame(cap) if cap else None

        if frame is None:
            _mark_camera_disconnected()
            _fail_streak += 1
            _consecutive_failures += 1
            now = time.time()
            if (not camera_bootstrap_active
                    and _consecutive_failures >= FAIL_THRESHOLD
                    and now - _last_reconnect_time > RECONNECT_INTERVAL):
                _last_reconnect_time = now
                _reset_camera_failure_state()
                print("[INFO] 视频流中断，自动尝试重连摄像头...")
                try:
                    with camera_lock:
                        if camera:
                            camera.release()
                        camera = open_camera(device_id_global, camera_source_global)
                    _mark_camera_connected()
                    print("[INFO] 摄像头自动重连成功")
                except Exception as e:
                    _mark_camera_disconnected()
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

        _mark_camera_connected()
        _frame_count += 1
        now = time.time()
        if now - _fps_ts >= 1.0:
            current_fps = _frame_count / (now - _fps_ts)
            _frame_count = 0
            _fps_ts = now

        quality = 84 if camera_source_global == CAMERA_SOURCE_A1 else 92
        _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, quality])
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


@app.route("/shutdown", methods=["POST"])
def shutdown():
    result = shutdown_qt_bridge()
    return jsonify(result)


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
    snapshot["judge_action"] = _judge_action_from_snapshot(snapshot)
    snapshot["judge_risk"] = _judge_risk_from_snapshot(snapshot)
    return jsonify(snapshot)


@app.route("/api/depth/latest")
def depth_latest():
    return jsonify(get_latest_depth_frame())


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
        "judge_action": _judge_action_from_snapshot(snapshot),
        "judge_risk": _judge_risk_from_snapshot(snapshot),
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


@app.route("/release_camera", methods=["POST"])
def release_camera():
    global camera
    try:
        with camera_lock:
            if camera:
                camera.release()
                camera = None
            _mark_camera_disconnected()
        print("[INFO] 摄像头已释放")
        return jsonify({"success": True, "message": "摄像头已释放"})
    except Exception as e:
        print(f"[WARN] 释放摄像头失败: {e}")
        return jsonify({"success": False, "error": f"释放摄像头失败: {e}"})


@app.route("/refresh_camera", methods=["POST"])
def refresh_camera():
    global camera, _fail_streak, _consecutive_failures, camera_connected
    try:
        with camera_lock:
            if camera:
                camera.release()
            camera = open_camera(device_id_global, camera_source_global)
            ok = camera is not None and camera.isOpened()
    except Exception:
        ok = False

    _reset_camera_failure_state()
    if ok:
        _mark_camera_connected()
        print("[INFO] 摄像头手动刷新成功")
        return jsonify({"success": True, "message": "摄像头已重新连接"})

    _mark_camera_disconnected()
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
        device_name = str(d.get("device_name") or f"device {d['id']}").strip()
        pixel_format = str(d.get("pixel_format") or "").strip()
        format_suffix = f" {pixel_format}" if pixel_format else ""
        display_width, display_height = _display_dims_for_device(d)
        label_kind = "Gray" if d.get("source") == CAMERA_SOURCE_A1 and d.get("supports_gray_fourcc") else kind
        label = f"{device_name} - {display_width}x{display_height} {label_kind} {y8}{format_suffix} score={d['score']}"
        items.append({
            "id": d["id"],
            "label": label,
            "score": d["score"],
            "actual_width": d["actual_width"],
            "actual_height": d["actual_height"],
            "display_width": display_width,
            "display_height": display_height,
            "is_grayscale": d.get("is_grayscale", False),
            "has_content": d.get("has_content", False),
            "supports_gray_fourcc": d.get("supports_gray_fourcc", False),
            "source": d.get("source", CAMERA_SOURCE_WINDOWS),
            "source_label": d.get("source_label", _source_label(CAMERA_SOURCE_WINDOWS)),
            "device_name": d.get("device_name"),
            "pixel_format": d.get("pixel_format"),
            "raw8_hint": d.get("raw8_hint", False),
        })
    return jsonify({
        "current_device": device_id_global,
        "current_source": camera_source_global,
        "devices": items,
        "bootstrapping": bootstrapping or camera_scan_active,
        "scan_error": camera_scan_error,
        "qt_bridge": _qt_bridge_status(timeout=0.5),
    })


@app.route("/switch_camera", methods=["POST"])
def switch_camera():
    global camera, device_id_global, camera_source_global, _fail_streak, _consecutive_failures, camera_connected

    data = request.get_json(silent=True) or {}
    raw_device = data.get("device", device_id_global)

    try:
        new_device = int(raw_device)
    except (TypeError, ValueError):
        return jsonify({"success": False, "error": "device 必须是整数"})

    requested_source = str(data.get("source") or CAMERA_SOURCE_AUTO).strip().lower()
    if requested_source not in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AUTO}:
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
        _mark_camera_connected()
        save_preferred_device(new_device)
        save_preferred_source(requested_source)
        print(f"[INFO] 已切换到摄像头设备 {new_device} ({requested_source})")
        return jsonify({"success": True, "device": new_device, "source": requested_source})
    except Exception as e:
        _mark_camera_disconnected()
        return jsonify({"success": False, "error": str(e)})


@app.route("/recent_captures")
def recent_captures_api():
    return jsonify(list(recent_captures))


@app.route("/status")
def status():
    stream_width = CAMERA_WIDTH
    stream_height = CAMERA_HEIGHT
    qt_status = _qt_bridge_status(timeout=0.5)
    if qt_status:
        stream_width = int(qt_status.get("frame_width") or stream_width)
        stream_height = int(qt_status.get("frame_height") or stream_height)
    connected = bool(camera_connected or (qt_status and qt_status.get("connected")))
    return jsonify({
        "connected": connected,
        "capture_count": capture_count,
        "fps": round(current_fps, 1),
        "output_dir": output_dir,
        "camera_resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
        "stream_width": stream_width,
        "stream_height": stream_height,
        "device": device_id_global,
        "source": camera_source_global,
        "source_label": _source_label(camera_source_global),
        "qt_bridge": qt_status,
        "model_name": _DETECT_MODEL_PATH.name,
        "model_path": str(_DETECT_MODEL_PATH),
        "model_mode": _DETECT_MODEL_MODE,
    })


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    global camera, output_dir, device_id_global, camera_source_global, COMPANION_PORT

    parser = argparse.ArgumentParser(description="Aurora Companion — 原始摄像头/A1 摄像头可视化采集伴侣")
    parser.add_argument("--device", type=int, default=-1,   help="摄像头设备 ID (默认: -1 自动优先 A1)")
    parser.add_argument("--source", type=str, default=CAMERA_SOURCE_AUTO,
                        help="输入源: windows / a1 / auto")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw",
                        help="拍照保存目录")
    parser.add_argument("--port",   type=int, default=6201, help="Web 服务端口 (默认: 6201)")
    parser.add_argument("--host",   type=str, default="127.0.0.1", help="监听地址")
    args = parser.parse_args()
    COMPANION_PORT = int(args.port)

    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    try:
        aurora_status = _start_aurora_desktop()
        if aurora_status.get("running"):
            if aurora_status.get("started"):
                print(f"[INFO] Aurora.exe 已启动: {aurora_status.get('path')}")
            else:
                print(f"[INFO] Aurora.exe 已在运行: {aurora_status.get('path')}")
        elif aurora_status.get("error"):
            print(f"[WARN] Aurora 启动跳过: {aurora_status.get('error')}")
    except Exception as exc:
        print(f"[WARN] Aurora 启动失败，但 Companion 将继续启动: {exc}")

    initial_source = str(args.source or CAMERA_SOURCE_AUTO).strip().lower()
    if initial_source not in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1, CAMERA_SOURCE_AUTO}:
        initial_source = CAMERA_SOURCE_AUTO
    camera_source_global = initial_source
    if initial_source in {CAMERA_SOURCE_WINDOWS, CAMERA_SOURCE_A1}:
        save_preferred_source(initial_source)

    try:
        bridge_status = ensure_qt_bridge_running(timeout=6.0)
        if bridge_status.get("available", False):
            print(f"[INFO] Qt 相机桥已就绪: {QT_BRIDGE_URL}")
        else:
            print(f"[WARN] Qt 相机桥不可用，将自动回退 OpenCV: {bridge_status.get('error')}")
    except Exception as exc:
        print(f"[WARN] Qt 相机桥启动失败，将自动回退 OpenCV: {exc}")

    print(f"[INFO] Aurora Companion 正在启动 Web 服务: http://127.0.0.1:{args.port}")
    print(f"[INFO] 快捷键: 1=720×1280  2=640×480  R=刷新摄像头")
    camera_bootstrap = threading.Thread(target=bootstrap_camera, args=(args.device, initial_source), daemon=True)
    camera_bootstrap.start()
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()

