#!/usr/bin/env python3
"""
Qt Camera Bridge

使用 QtMultimedia 直接枚举并读取 Windows 相机，尽量贴近 Aurora 的
QtMultimediaCameraBackend 行为；对外暴露本地 HTTP 接口，供 Flask 页面消费。
"""

import argparse
import json
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import parse_qs, urlparse

try:
    from PySide6.QtCore import QByteArray, QBuffer, QIODevice, QObject, Qt, QTimer, Signal, Slot
    from PySide6.QtGui import QGuiApplication, QImage
    from PySide6.QtMultimedia import QCamera, QCameraDevice, QCameraFormat, QMediaCaptureSession, QMediaDevices, QVideoFrame, QVideoSink

    QT_AVAILABLE = True
    QT_IMPORT_ERROR = ""
except Exception as exc:  # pragma: no cover - 无依赖环境下也需要正常返回错误
    QT_AVAILABLE = False
    QT_IMPORT_ERROR = str(exc)
    QByteArray = QBuffer = QIODevice = QObject = Qt = QTimer = Signal = Slot = None  # type: ignore[assignment]
    QGuiApplication = QImage = None  # type: ignore[assignment]
    QCamera = QCameraDevice = QCameraFormat = QMediaCaptureSession = QMediaDevices = QVideoFrame = QVideoSink = None  # type: ignore[assignment]


CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30.0
A1_CAMERA_FPS = 90.0
DEFAULT_JPEG_QUALITY = 92
A1_JPEG_QUALITY = 84
BRIDGE_PROTOCOL_VERSION = 2

SOURCE_WINDOWS = "windows"
SOURCE_A1 = "a1"
SOURCE_AUTO = "auto"

_GRAY_HINTS = ("y8", "y16", "grayscale", "gray", "mono", "l8", "l16")
_RAW_HINTS = ("raw", "raw8", "raw10", "raw12", "raw16")
_RGB_HINTS = ("rgb", "bgr", "argb", "xrgb")
_YUV_HINTS = ("nv12", "yuv", "uyvy", "yuyv", "p010", "p016")
_A1_NAME_HINTS = (
    "smartsens-flyingchip-a1",
    "flyingchip",
    "smartsens",
    "sc132",
)


def _enum_name(value: Any) -> str:
    text = str(value)
    if "." in text:
        text = text.split(".")[-1]
    return text


def _safe_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except Exception:
        return default


def _safe_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except Exception:
        return default


def _looks_like_a1(description: str, pixel_formats: List[str]) -> bool:
    desc = (description or "").strip().lower()
    if any(token in desc for token in _A1_NAME_HINTS):
        return True
    blob = " ".join((pixel_formats or [])).lower()
    if any(token in blob for token in _RAW_HINTS + _GRAY_HINTS):
        return True
    # 某些 A1 驱动只暴露 YUV 格式，但设备名仍包含 SmartSens 线索。
    if "smart" in desc and any(token in blob for token in _YUV_HINTS):
        return True
    return False


def _guess_source(description: str, pixel_formats: List[str]) -> str:
    return SOURCE_A1 if _looks_like_a1(description, pixel_formats) else SOURCE_WINDOWS


def _pixel_format_score(pixel_name: str, requested_source: str, device_source: str) -> int:
    text = (pixel_name or "").lower()
    score = 0
    if requested_source == SOURCE_A1 or device_source == SOURCE_A1:
        if any(token in text for token in _RAW_HINTS):
            score += 56
        if any(token in text for token in _GRAY_HINTS):
            score += 44
        if any(token in text for token in _YUV_HINTS):
            score += 30
        if "mjpeg" in text or "jpeg" in text:
            score -= 16
        if any(token in text for token in _RGB_HINTS):
            score -= 32
    else:
        if any(token in text for token in _RGB_HINTS):
            score += 24
        if "mjpeg" in text or "jpeg" in text:
            score += 18
        if any(token in text for token in _YUV_HINTS):
            score += 10
        if any(token in text for token in _GRAY_HINTS):
            score += 4
    return score


def _resolution_score(width: int, height: int) -> int:
    area = width * height
    score = 0
    if (width, height) == (720, 1280):
        score += 32
    elif (width, height) == (1280, 720):
        score += 30
    elif area == CAMERA_WIDTH * CAMERA_HEIGHT:
        score += 24
    else:
        delta = abs(area - (CAMERA_WIDTH * CAMERA_HEIGHT))
        score += max(0, 16 - delta // 120000)
    if height > width:
        score += 6
    ratio = width / max(1, height)
    if 1.65 <= ratio <= 1.85 or 0.54 <= ratio <= 0.62:
        score += 4
    return score


def _fps_score(min_fps: float, max_fps: float, requested_source: str = SOURCE_WINDOWS) -> int:
    target_fps = A1_CAMERA_FPS if requested_source == SOURCE_A1 else CAMERA_FPS
    if min_fps <= target_fps <= max_fps and max_fps > 0:
        return 8
    if max_fps <= 0:
        return 0
    return max(0, 5 - int(abs(max_fps - target_fps)))


if QT_AVAILABLE:
    class BridgeController(QObject):
        _invoke_signal = Signal(object, object)

        def __init__(self):
            super().__init__()
            self._invoke_signal.connect(self._on_invoke, Qt.ConnectionType.QueuedConnection)

        @Slot(object, object)
        def _on_invoke(self, func, holder):
            try:
                holder["value"] = func()
            except Exception as exc:  # pragma: no cover - 运行态异常兜底
                holder["error"] = exc
            finally:
                holder["event"].set()

        def call(self, func, timeout: float = 8.0):
            holder = {"event": threading.Event()}
            self._invoke_signal.emit(func, holder)
            if not holder["event"].wait(timeout):
                raise TimeoutError("Qt 相机桥调用超时")
            if "error" in holder:
                raise holder["error"]
            return holder.get("value")
else:
    class BridgeController:
        def call(self, func, timeout: float = 8.0):
            _ = timeout
            return func()


class CameraBridgeState:
    def __init__(self):
        self.lock = threading.Lock()
        self.started_at = time.time()
        self.error = QT_IMPORT_ERROR
        self.available = QT_AVAILABLE
        self.connected = False
        self.device_id = -1
        self.requested_source = SOURCE_AUTO
        self.active_source = SOURCE_AUTO
        self.device_name = ""
        self.pixel_format = ""
        self.raw8_hint = False
        self.frame_width = 0
        self.frame_height = 0
        self.frame_count = 0
        self.last_frame_ts = 0.0
        self.last_encode_error_ts = 0.0
        self.fps = 0.0
        self.devices_cache: List[Dict[str, Any]] = []
        self.latest_image = None
        self.latest_color_jpeg: Optional[bytes] = None
        self.latest_gray_jpeg: Optional[bytes] = None
        self.status_message = "Qt 相机桥未初始化"

        self.controller = BridgeController()
        self.camera = None
        self.capture_session = None
        self.video_sink = None
        self.media_devices = None
        self._camera_device_objects: List[Any] = []
        self._current_format = None
        self._jpeg_quality = DEFAULT_JPEG_QUALITY

        if not QT_AVAILABLE:
            return

        self.status_message = "Qt 相机桥已初始化"
        self.media_devices = QMediaDevices()
        self.capture_session = QMediaCaptureSession()
        self.video_sink = QVideoSink()
        self.video_sink.videoFrameChanged.connect(self._on_frame)
        self.capture_session.setVideoSink(self.video_sink)
        try:
            self.media_devices.videoInputsChanged.connect(self._refresh_devices)
        except Exception:
            pass
        self._refresh_devices()

    def _qimage_format(self, name: str):
        if hasattr(QImage, name):
            return getattr(QImage, name)
        return getattr(QImage.Format, name)

    def _jpeg_bytes(self, image: Any, grayscale: bool = False) -> bytes:
        if image.isNull():
            return b""
        if grayscale:
            image = image.convertToFormat(self._qimage_format("Format_Grayscale8"))
        else:
            image = image.convertToFormat(self._qimage_format("Format_RGB888"))
        qbytes = QByteArray()
        buffer = QBuffer(qbytes)
        buffer.open(QIODevice.OpenModeFlag.WriteOnly)
        if not image.save(buffer, "JPEG", self._jpeg_quality):
            image.save(buffer, "JPG", self._jpeg_quality)
        buffer.close()
        return bytes(qbytes)

    def _frame_plane_bytes(self, frame: Any, mapped_bytes: int) -> Optional[memoryview]:
        try:
            bits = frame.bits(0)
        except Exception:
            return None
        if bits is None:
            return None
        if hasattr(bits, "setsize"):
            try:
                bits.setsize(mapped_bytes)
            except Exception:
                pass
        if hasattr(bits, "asstring"):
            try:
                return memoryview(bits.asstring(mapped_bytes))
            except Exception:
                return None
        try:
            view = memoryview(bits)
            if mapped_bytes > 0 and len(view) > mapped_bytes:
                return view[:mapped_bytes]
            return view
        except Exception:
            return None

    def _a1_raw_y8_image(self, frame: Any) -> Optional[Any]:
        if self.active_source != SOURCE_A1 or not QT_AVAILABLE:
            return None
        pixel = (self.pixel_format or "").lower()
        if not any(token in pixel for token in _RAW_HINTS + _GRAY_HINTS + _YUV_HINTS):
            return None

        try:
            map_mode = QVideoFrame.MapMode.ReadOnly
            if not frame.map(map_mode):
                return None
        except Exception:
            return None

        try:
            src_w = _safe_int(frame.width())
            src_h = _safe_int(frame.height())
            if src_w <= 0 or src_h <= 0:
                return None
            try:
                bpl = _safe_int(frame.bytesPerLine(0))
            except Exception:
                bpl = 0
            try:
                mapped_bytes = _safe_int(frame.mappedBytes(0))
            except Exception:
                mapped_bytes = 0
            if bpl <= 0 and mapped_bytes > 0:
                bpl = mapped_bytes // max(1, src_h)
            if bpl <= 0:
                return None
            view = self._frame_plane_bytes(frame, mapped_bytes or bpl * src_h)
            if view is None or len(view) < bpl:
                return None

            if any(token in pixel for token in ("uyvy", "yuyv", "yuv")) and bpl >= src_w * 2:
                out_w = src_w * 2
            else:
                out_w = src_w
            if src_w <= 720 <= bpl and any(token in pixel for token in ("uyvy", "yuyv", "yuv")):
                out_w = 720
            out_w = max(1, min(out_w, bpl))

            raw = bytearray(out_w * src_h)
            for y in range(src_h):
                src_start = y * bpl
                src_end = src_start + out_w
                dst_start = y * out_w
                if src_end > len(view):
                    return None
                raw[dst_start:dst_start + out_w] = view[src_start:src_end]
            image = QImage(bytes(raw), out_w, src_h, out_w, self._qimage_format("Format_Grayscale8"))
            return image.copy()
        finally:
            try:
                frame.unmap()
            except Exception:
                pass

    def _device_formats(self, device: Any) -> Tuple[List[Dict[str, Any]], Optional[Dict[str, Any]], Optional[Any]]:
        formats_meta: List[Dict[str, Any]] = []
        best_meta = None
        best_format = None
        description = device.description()
        pixel_names = [_enum_name(fmt.pixelFormat()) for fmt in device.videoFormats()]
        device_source = _guess_source(description, pixel_names)
        for fmt in device.videoFormats():
            resolution = fmt.resolution()
            width = _safe_int(resolution.width())
            height = _safe_int(resolution.height())
            pixel_name = _enum_name(fmt.pixelFormat())
            min_fps = _safe_float(fmt.minFrameRate())
            max_fps = _safe_float(fmt.maxFrameRate())
            meta = {
                "width": width,
                "height": height,
                "pixel_format": pixel_name,
                "min_fps": round(min_fps, 2),
                "max_fps": round(max_fps, 2),
                "score_windows": _pixel_format_score(pixel_name, SOURCE_WINDOWS, device_source) + _resolution_score(width, height) + _fps_score(min_fps, max_fps, SOURCE_WINDOWS),
                "score_a1": _pixel_format_score(pixel_name, SOURCE_A1, device_source) + _resolution_score(width, height) + _fps_score(min_fps, max_fps, SOURCE_A1),
            }
            formats_meta.append(meta)

        def _pick(requested_source: str):
            local_best = None
            local_format = None
            for idx, fmt in enumerate(device.videoFormats()):
                meta = formats_meta[idx]
                score = meta["score_a1"] if requested_source == SOURCE_A1 else meta["score_windows"]
                if local_best is None or score > local_best["score"]:
                    local_best = dict(meta)
                    local_best["score"] = score
                    local_format = fmt
            return local_best, local_format

        best_meta, best_format = _pick(device_source if device_source in {SOURCE_A1, SOURCE_WINDOWS} else SOURCE_WINDOWS)
        return formats_meta, best_meta, best_format

    def _refresh_devices(self):
        if not QT_AVAILABLE or self.media_devices is None:
            return []
        devices: List[Dict[str, Any]] = []
        self._camera_device_objects = list(self.media_devices.videoInputs())
        for idx, device in enumerate(self._camera_device_objects):
            formats, best_meta, _best_format = self._device_formats(device)
            pixel_names = [item["pixel_format"] for item in formats]
            device_source = _guess_source(device.description(), pixel_names)
            score = int(best_meta["score"]) if best_meta else 0
            actual_width = int(best_meta["width"]) if best_meta else 0
            actual_height = int(best_meta["height"]) if best_meta else 0
            pixel_format = best_meta["pixel_format"] if best_meta else ""
            devices.append({
                "id": idx,
                "opened": True,
                "score": score,
                "actual_width": actual_width,
                "actual_height": actual_height,
                "is_grayscale": any(token in pixel_format.lower() for token in _GRAY_HINTS + _RAW_HINTS),
                "has_content": True,
                "supports_gray_fourcc": device_source == SOURCE_A1,
                "source": device_source,
                "source_label": "A1 开发板" if device_source == SOURCE_A1 else "Windows 摄像头",
                "device_name": device.description(),
                "pixel_format": pixel_format,
                "formats": formats[:12],
                "raw8_hint": any(token in pixel_format.lower() for token in _RAW_HINTS),
            })
        with self.lock:
            self.devices_cache = devices
        return devices

    def list_devices(self) -> List[Dict[str, Any]]:
        if QT_AVAILABLE:
            return self.controller.call(self._refresh_devices)
        with self.lock:
            return list(self.devices_cache)

    def _select_format_for_source(self, device: Any, requested_source: str) -> Tuple[Optional[Any], Optional[Dict[str, Any]], str]:
        formats, _default_best, _default_fmt = self._device_formats(device)
        pixel_names = [item["pixel_format"] for item in formats]
        device_source = _guess_source(device.description(), pixel_names)
        actual_source = requested_source if requested_source in {SOURCE_A1, SOURCE_WINDOWS} else device_source

        best_meta = None
        best_format = None
        candidates = list(zip(device.videoFormats(), formats))
        if actual_source == SOURCE_A1:
            preferred = []
            fallback = []
            for fmt, meta in candidates:
                pixel_name = str(meta.get("pixel_format") or "").lower()
                if any(token in pixel_name for token in _RAW_HINTS + _GRAY_HINTS + _YUV_HINTS):
                    preferred.append((fmt, meta))
                else:
                    fallback.append((fmt, meta))
            if preferred:
                candidates = preferred
            elif fallback:
                candidates = fallback

        for fmt, meta in candidates:
            score = meta["score_a1"] if actual_source == SOURCE_A1 else meta["score_windows"]
            if best_meta is None or score > best_meta["score"]:
                best_meta = dict(meta)
                best_meta["score"] = score
                best_format = fmt
        return best_format, best_meta, actual_source

    def _stop_camera(self):
        if self.camera is not None:
            try:
                self.camera.stop()
            except Exception:
                pass
            try:
                self.camera.deleteLater()
            except Exception:
                pass
        if self.capture_session is not None:
            try:
                self.capture_session.setCamera(None)
            except Exception:
                pass
        self.camera = None
        self.connected = False

    def stop_camera(self) -> Dict[str, Any]:
        if QT_AVAILABLE:
            return self.controller.call(self._stop_camera_internal)
        return {"success": True, "message": "Qt 不可用"}

    def _stop_camera_internal(self) -> Dict[str, Any]:
        print("[DEBUG] Stop camera requested")
        self._stop_camera()
        self.status_message = "Qt 相机桥已停止"
        return {"success": True, "message": "相机已释放"}

    def _open_camera(self, device_id: int, requested_source: str) -> Dict[str, Any]:
        if not QT_AVAILABLE:
            raise RuntimeError(f"PySide6 不可用: {self.error}")

        self._refresh_devices()
        if device_id < 0 or device_id >= len(self._camera_device_objects):
            raise RuntimeError(f"Qt 相机桥未找到设备 {device_id}")

        device = self._camera_device_objects[device_id]
        selected_format, meta, actual_source = self._select_format_for_source(device, requested_source)
        
        print(f"[DEBUG] Opening camera: device_id={device_id}, device={device.description()}")
        if selected_format is not None:
            res = selected_format.resolution()
            print(f"[DEBUG] Selected format: {_enum_name(selected_format.pixelFormat())} @ {res.width()}x{res.height()}")
        if meta:
            print(f"[DEBUG] Format metadata: {meta}")
        
        # 确保完全停止并释放之前的相机
        self._stop_camera()
        # 给足够的时间让 Qt 清理资源
        QThread = None
        try:
            from PySide6.QtCore import QThread
            QThread.msleep(200)
        except Exception:
            pass
        
        # 尝试打开相机，增加重试机制
        max_retries = 3
        last_error = None
        for retry in range(max_retries):
            try:
                print(f"[DEBUG] Attempting to start camera (retry {retry + 1}/{max_retries})")
                self.camera = QCamera(device)
                if selected_format is not None:
                    try:
                        self.camera.setCameraFormat(selected_format)
                        print(f"[DEBUG] Camera format set successfully")
                    except Exception as e:
                        print(f"[DEBUG] Could not set camera format: {e}")
                
                # 确保video sink被正确设置
                if self.video_sink is None:
                    self.video_sink = QVideoSink()
                    self.video_sink.videoFrameChanged.connect(self._on_frame)
                self.capture_session.setCamera(self.camera)
                self.capture_session.setVideoSink(self.video_sink)  # 确保每次都重新设置sink
                print(f"[DEBUG] Video sink set to capture session")
                
                self.camera.start()
                print(f"[DEBUG] Camera started successfully")
                break
            except Exception as e:
                last_error = e
                print(f"[ERROR] Failed to open camera: {e}")
                if retry < max_retries - 1:
                    print(f"[WARN] Opening camera failed (retry {retry + 1}/{max_retries}): {e}")
                    try:
                        self._stop_camera()
                        if QThread:
                            QThread.msleep(300)
                    except Exception:
                        pass
                else:
                    raise
                    
        if last_error is not None and self.camera is None:
            raise RuntimeError(f"无法打开相机: {last_error}")

        self.device_id = device_id
        self.requested_source = requested_source
        self.active_source = actual_source
        self.device_name = device.description()
        self.pixel_format = meta.get("pixel_format", "") if meta else ""
        self.raw8_hint = any(token in self.pixel_format.lower() for token in _RAW_HINTS + _GRAY_HINTS)
        self.frame_width = int(meta.get("width", 0)) if meta else 0
        self.frame_height = int(meta.get("height", 0)) if meta else 0
        self._jpeg_quality = A1_JPEG_QUALITY if actual_source == SOURCE_A1 else DEFAULT_JPEG_QUALITY
        self.frame_count = 0
        self.last_frame_ts = 0.0
        self.fps = 0.0
        self.latest_image = None
        self.latest_color_jpeg = None
        self.latest_gray_jpeg = None
        self.started_at = time.time()
        self.connected = True
        self.status_message = (
            f"Qt 相机桥已连接: {self.device_name} / {self.frame_width}x{self.frame_height} / {self.pixel_format}"
        )
        print(f"[DEBUG] Camera bridge status: connected={self.connected}, active_source={self.active_source}")
        return self.status()

    def switch_camera(self, device_id: int, requested_source: str) -> Dict[str, Any]:
        if QT_AVAILABLE:
            return self.controller.call(lambda: self._open_camera(device_id, requested_source))
        raise RuntimeError(f"PySide6 不可用: {self.error}")

    def _on_frame(self, frame):
        now = time.time()
        
        # 调试信息：记录接收到的帧
        if self.frame_count == 0:
            print(f"[DEBUG] Qt camera bridge: first frame received at {now}")
            print(f"[DEBUG] Frame details: width={frame.width()}, height={frame.height()}, pixelFormat={_enum_name(frame.pixelFormat())}")
        
        raw_y8 = self._a1_raw_y8_image(frame)
        try:
            image = raw_y8 if raw_y8 is not None else frame.toImage()
        except Exception as e:
            if self.frame_count < 5:  # 只在前几帧打印错误
                print(f"[DEBUG] Qt frame conversion failed: {e}")
            image = None
        if image is None or image.isNull():
            if self.frame_count < 5:  # 只在前几帧打印警告
                print(f"[DEBUG] Qt frame is null")
            return
        
        latest = image.copy()
        color_jpeg = None
        gray_jpeg = None
        try:
            if raw_y8 is not None:
                gray_jpeg = self._jpeg_bytes(latest, grayscale=True)
                color_jpeg = gray_jpeg
            else:
                color_jpeg = self._jpeg_bytes(latest, grayscale=False)
                gray_jpeg = self._jpeg_bytes(latest, grayscale=True)
        except Exception as exc:
            if now - self.last_encode_error_ts >= 5.0:
                self.last_encode_error_ts = now
                print(f"[WARN] Qt frame encode failed: {exc}")
        with self.lock:
            self.latest_image = latest
            self.latest_color_jpeg = color_jpeg
            self.latest_gray_jpeg = gray_jpeg
            self.frame_width = _safe_int(latest.width(), self.frame_width)
            self.frame_height = _safe_int(latest.height(), self.frame_height)
            self.frame_count += 1
            self.last_frame_ts = now
            if self.frame_count > 1 and self.started_at < now:
                elapsed = max(0.001, now - self.started_at)
                self.fps = self.frame_count / elapsed

    def frame_bytes(self, mode: str = "color") -> Optional[bytes]:
        with self.lock:
            cached = self.latest_gray_jpeg if mode == "gray" else self.latest_color_jpeg
            image = self.latest_image.copy() if self.latest_image is not None else None
        if cached:
            return cached
        if image is None or image.isNull():
            return None
        now = time.time()
        try:
            return self._jpeg_bytes(image, grayscale=(mode == "gray"))
        except Exception as exc:
            if now - self.last_encode_error_ts >= 5.0:
                self.last_encode_error_ts = now
                print(f"[WARN] Qt frame encode failed: {exc}")
            return None

    def status(self) -> Dict[str, Any]:
        with self.lock:
            return {
                "bridge_version": BRIDGE_PROTOCOL_VERSION,
                "available": self.available,
                "error": self.error,
                "connected": self.connected,
                "device": self.device_id,
                "requested_source": self.requested_source,
                "active_source": self.active_source,
                "device_name": self.device_name,
                "pixel_format": self.pixel_format,
                "raw8_hint": self.raw8_hint,
                "frame_width": self.frame_width,
                "frame_height": self.frame_height,
                "frame_count": self.frame_count,
                "fps": round(self.fps, 1),
                "last_frame_ts": self.last_frame_ts,
                "uptime": round(time.time() - self.started_at, 1),
                "message": self.status_message,
            }


BRIDGE = CameraBridgeState()


class BridgeHandler(BaseHTTPRequestHandler):
    server_version = "QtCameraBridge/1.0"

    def _read_json(self) -> Dict[str, Any]:
        length = _safe_int(self.headers.get("Content-Length"))
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        if not raw:
            return {}
        try:
            return json.loads(raw.decode("utf-8"))
        except Exception:
            return {}

    def _write_json(self, payload: Dict[str, Any], status: int = 200):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _write_bytes(self, payload: bytes, mime: str = "image/jpeg", status: int = 200):
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("X-Accel-Buffering", "no")
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt: str, *args):  # pragma: no cover - 减少控制台噪声
        _ = (fmt, args)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path in {"/", "/health", "/status"}:
            status = BRIDGE.status()
            status["success"] = True
            self._write_json(status)
            return
        if parsed.path == "/devices":
            try:
                items = BRIDGE.list_devices()
                self._write_json({"success": True, "devices": items, "status": BRIDGE.status()})
            except Exception as exc:
                self._write_json({"success": False, "error": str(exc), "devices": [], "status": BRIDGE.status()}, status=500)
            return
        if parsed.path == "/frame.jpg":
            query = parse_qs(parsed.query or "")
            mode = str((query.get("mode") or ["color"])[0]).strip().lower()
            payload = None
            for _ in range(20):
                payload = BRIDGE.frame_bytes("gray" if mode == "gray" else "color")
                if payload:
                    break
                time.sleep(0.05)
            if payload:
                self._write_bytes(payload)
            else:
                self._write_json({"success": False, "error": "尚未收到视频帧"}, status=HTTPStatus.SERVICE_UNAVAILABLE)
            return
        self._write_json({"success": False, "error": "unknown endpoint"}, status=404)

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/switch":
            data = self._read_json()
            device_id = _safe_int(data.get("device"), -1)
            source = str(data.get("source") or SOURCE_AUTO).strip().lower()
            if source not in {SOURCE_WINDOWS, SOURCE_A1, SOURCE_AUTO}:
                source = SOURCE_AUTO
            try:
                status = BRIDGE.switch_camera(device_id, source)
                self._write_json({"success": True, "status": status})
            except Exception as exc:
                self._write_json({"success": False, "error": str(exc), "status": BRIDGE.status()}, status=500)
            return
        if parsed.path == "/stop":
            try:
                result = BRIDGE.stop_camera()
                self._write_json(result)
            except Exception as exc:
                self._write_json({"success": False, "error": str(exc), "status": BRIDGE.status()}, status=500)
            return
        self._write_json({"success": False, "error": "unknown endpoint"}, status=404)


def main():
    parser = argparse.ArgumentParser(description="Qt 相机桥")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP 监听地址")
    parser.add_argument("--port", type=int, default=5911, help="HTTP 监听端口")
    args = parser.parse_args()

    if QT_AVAILABLE:
        app = QGuiApplication.instance() or QGuiApplication([])
        server = ThreadingHTTPServer((args.host, args.port), BridgeHandler)
        server_thread = threading.Thread(target=server.serve_forever, daemon=True)
        server_thread.start()
        print(f"[INFO] Qt Camera Bridge 已启动: http://{args.host}:{args.port}")
        print("[INFO] 等待 Flask Companion 连接并下发设备选择...")
        app.exec()
        server.shutdown()
        server.server_close()
    else:
        server = ThreadingHTTPServer((args.host, args.port), BridgeHandler)
        print(f"[WARN] Qt Camera Bridge 启动，但 PySide6 不可用: {QT_IMPORT_ERROR}")
        server.serve_forever()


if __name__ == "__main__":
    main()
