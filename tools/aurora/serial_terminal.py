#!/usr/bin/env python3
"""serial_terminal.py — A1 调试串口实时终端 / 简易 CLI。"""

import threading
import time
from collections import deque
from typing import Any, Dict, List, Optional

from flask import Blueprint, jsonify, request

try:
    import serial
    import serial.tools.list_ports

    _SERIAL_AVAILABLE = True
except ImportError:
    _SERIAL_AVAILABLE = False


serial_term_bp = Blueprint("serial_term", __name__, url_prefix="/api/serial_term")

_DEFAULT_PORT = "COM13"
_DEFAULT_DESC_HINTS = ("usb-hispeed-serial-a", "ch347f", "smartsens", "flyingchip")

_state_lock = threading.Lock()
_state: Dict[str, Any] = {
    "port": _DEFAULT_PORT,
    "baud": 115200,
    "timeout": 0.05,
    "append_newline": True,
}

_ser: Optional["serial.Serial"] = None  # type: ignore[name-defined]
_ser_lock = threading.Lock()
_running = False
_rx_thread: Optional[threading.Thread] = None

_rx_log: deque = deque(maxlen=120)
_tx_log: deque = deque(maxlen=80)
_latest_lines: deque = deque(maxlen=60)
_rx_buffer = bytearray()
_rx_seq = 0
_rx_cond = threading.Condition()


def _snapshot_state() -> Dict[str, Any]:
    with _state_lock:
        return dict(_state)


def _score_port(port_info: Dict[str, Any]) -> int:
    score = 0
    device = str(port_info.get("port") or "").lower()
    desc = str(port_info.get("desc") or "").lower()
    hwid = str(port_info.get("hwid") or "").lower()
    blob = f"{device} {desc} {hwid}"
    if device == _DEFAULT_PORT.lower():
        score += 100
    for hint in _DEFAULT_DESC_HINTS:
        if hint in blob:
            score += 40
    if "usb" in blob:
        score += 10
    return score


def _list_ports() -> list:
    if not _SERIAL_AVAILABLE:
        return []
    items = [
        {"port": p.device, "desc": p.description, "hwid": p.hwid}
        for p in serial.tools.list_ports.comports()
    ]
    items.sort(key=_score_port, reverse=True)
    return items


def _normalize_text(raw: bytes) -> str:
    if not raw:
        return ""
    return raw.decode("utf-8", errors="replace").replace("\r", "")


def _append_rx_entry(raw: bytes, text: str, partial: bool = False) -> None:
    global _rx_seq
    entry = {
        "ts": time.strftime("%H:%M:%S"),
        "hex": raw.hex(" ").upper(),
        "text": text,
        "partial": partial,
    }
    _rx_log.appendleft(entry)
    if text:
        _latest_lines.appendleft(entry)
    with _rx_cond:
        _rx_seq += 1
        _rx_cond.notify_all()


def _append_tx_entry(text: str, payload: bytes, hex_mode: bool = False) -> None:
    _tx_log.appendleft({
        "ts": time.strftime("%H:%M:%S"),
        "text": text,
        "hex": payload.hex(" ").upper(),
        "hex_mode": hex_mode,
    })


def _rx_worker() -> None:
    global _running, _rx_buffer
    while _running:
        with _ser_lock:
            ser = _ser
        if ser is None or not ser.is_open:
            time.sleep(0.05)
            continue
        try:
            waiting = getattr(ser, "in_waiting", 0)
            chunk = ser.read(max(1, min(waiting or 1, 256)))
            if not chunk:
                continue
            _rx_buffer.extend(chunk)
            while b"\n" in _rx_buffer:
                line, _, rest = _rx_buffer.partition(b"\n")
                _rx_buffer = bytearray(rest)
                text = _normalize_text(line).strip()
                _append_rx_entry(bytes(line), text, partial=False)
            if len(_rx_buffer) > 512:
                raw = bytes(_rx_buffer)
                text = _normalize_text(raw).strip()
                _append_rx_entry(raw, text, partial=True)
                _rx_buffer.clear()
        except Exception:
            time.sleep(0.05)


def _connect_serial(port: str, baud: int, timeout: float) -> None:
    global _ser, _running, _rx_thread, _rx_buffer
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = serial.Serial(port, baud, timeout=timeout)
    _rx_buffer = bytearray()
    _running = True
    _rx_thread = threading.Thread(target=_rx_worker, daemon=True, name="serial-term-rx")
    _rx_thread.start()


def _disconnect_serial() -> None:
    global _ser, _running, _rx_buffer
    _running = False
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = None
    _rx_buffer = bytearray()


def _send_payload(payload: bytes, text: str, hex_mode: bool = False) -> Dict[str, Any]:
    with _ser_lock:
        if _ser is None or not _ser.is_open:
            return {"success": False, "error": "串口未连接"}
        try:
            _ser.write(payload)
        except Exception as exc:
            return {"success": False, "error": str(exc)}
    _append_tx_entry(text, payload, hex_mode=hex_mode)
    return {"success": True, "bytes_sent": len(payload), "hex": payload.hex(' ').upper()}


def _auto_connect(baud: Optional[int] = None, timeout: Optional[float] = None) -> Dict[str, Any]:
    state = _snapshot_state()
    baudrate = int(baud or state.get("baud") or 115200)
    serial_timeout = float(timeout or state.get("timeout") or 0.05)
    ports = _list_ports()
    if not ports:
        return {"success": False, "error": "未发现可用串口"}
    last_error = "未能连接任何串口"
    for item in ports:
        try:
            _connect_serial(item["port"], baudrate, serial_timeout)
            with _state_lock:
                _state["port"] = item["port"]
                _state["baud"] = baudrate
                _state["timeout"] = serial_timeout
            print(f"[SERIAL_TERM] 自动连接成功 {item['port']} @ {baudrate}")
            return {"success": True, "port": item["port"], "baud": baudrate, "desc": item.get("desc")}
        except Exception as exc:
            last_error = f"{item['port']}: {exc}"
            continue
    return {"success": False, "error": last_error}


def _wait_for_text(tokens: List[str], timeout_sec: float = 1.8, after_seq: Optional[int] = None) -> Dict[str, Any]:
    deadline = time.time() + max(0.1, timeout_sec)
    start_seq = _rx_seq if after_seq is None else after_seq
    lowered = [token.lower() for token in tokens if token]
    while time.time() < deadline:
        with _rx_cond:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            _rx_cond.wait(timeout=min(0.2, remaining))
        current_items = list(_latest_lines)
        if _rx_seq <= start_seq:
            continue
        for item in current_items:
            text = str(item.get("text") or "").lower()
            if any(token in text for token in lowered):
                return {"success": True, "matched": item}
    return {"success": False, "error": "未在串口输出中等到预期回传"}


@serial_term_bp.route("/available")
def available():
    return jsonify({"available": _SERIAL_AVAILABLE})


@serial_term_bp.route("/ports")
def ports():
    return jsonify({"ports": _list_ports(), "preferred": _snapshot_state().get("port", _DEFAULT_PORT)})


@serial_term_bp.route("/config", methods=["GET", "POST"])
def config():
    if request.method == "GET":
        state = _snapshot_state()
        state["ports"] = _list_ports()
        return jsonify(state)

    data = request.get_json(silent=True) or {}
    with _state_lock:
        if data.get("port") is not None:
            _state["port"] = str(data.get("port") or "").strip() or _DEFAULT_PORT
        if data.get("baud") is not None:
            _state["baud"] = int(data.get("baud") or 115200)
        if data.get("timeout") is not None:
            _state["timeout"] = max(0.01, float(data.get("timeout")))
        if data.get("append_newline") is not None:
            _state["append_newline"] = bool(data.get("append_newline"))
        state = dict(_state)
    state["success"] = True
    return jsonify(state)


@serial_term_bp.route("/status")
def status():
    state = _snapshot_state()
    with _ser_lock:
        connected = _ser is not None and _ser.is_open
        port = _ser.port if connected else state.get("port")
        baud = _ser.baudrate if connected else state.get("baud")
    return jsonify({
        "success": True,
        "available": _SERIAL_AVAILABLE,
        "connected": connected,
        "port": port,
        "baud": baud,
        "append_newline": state.get("append_newline", True),
        "latest_lines": list(_latest_lines),
        "rx_count": len(_rx_log),
        "tx_count": len(_tx_log),
    })


@serial_term_bp.route("/connect", methods=["POST"])
def connect():
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装"})
    data = request.get_json(silent=True) or {}
    state = _snapshot_state()
    port = str(data.get("port") or state.get("port") or _DEFAULT_PORT).strip()
    baud = int(data.get("baud") or state.get("baud") or 115200)
    timeout = float(data.get("timeout") or state.get("timeout") or 0.05)
    if not port:
        return jsonify({"success": False, "error": "需要指定串口号"})
    try:
        _connect_serial(port, baud, timeout)
        with _state_lock:
            _state["port"] = port
            _state["baud"] = baud
            _state["timeout"] = timeout
        print(f"[SERIAL_TERM] 已连接 {port} @ {baud}")
        return jsonify({"success": True, "port": port, "baud": baud})
    except Exception as exc:
        return jsonify({"success": False, "error": str(exc)})


@serial_term_bp.route("/auto_connect", methods=["POST"])
def auto_connect():
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装"})
    data = request.get_json(silent=True) or {}
    result = _auto_connect(baud=data.get("baud"), timeout=data.get("timeout"))
    return jsonify(result)


@serial_term_bp.route("/disconnect", methods=["POST"])
def disconnect():
    _disconnect_serial()
    print("[SERIAL_TERM] 已断开")
    return jsonify({"success": True})


@serial_term_bp.route("/send", methods=["POST"])
def send():
    data = request.get_json(silent=True) or {}
    text = str(data.get("text") or "")
    hex_mode = bool(data.get("hex_mode"))
    append_newline = bool(data.get("append_newline", _snapshot_state().get("append_newline", True)))

    if not text:
        return jsonify({"success": False, "error": "发送内容不能为空"})
    try:
        if hex_mode:
            payload = bytes.fromhex(text.replace(" ", ""))
        else:
            payload = text.encode("utf-8")
            if append_newline:
                payload += b"\r\n"
    except Exception as exc:
        return jsonify({"success": False, "error": str(exc)})
    return jsonify(_send_payload(payload, text, hex_mode=hex_mode))


@serial_term_bp.route("/send_test", methods=["POST"])
def send_test():
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装"})
    data = request.get_json(silent=True) or {}
    auto_connect_first = bool(data.get("auto_connect", True))
    command = str(data.get("command") or "test_echo").strip() or "test_echo"
    message = str(data.get("message") or "pc_frontend_test").strip() or "pc_frontend_test"
    timeout_sec = max(0.3, float(data.get("timeout_sec") or 1.8))
    wait_tokens = data.get("wait_tokens") or [
        "\"success\":true",
        f"\"command\":\"{command}\"",
        "测试回传成功",
    ]

    with _ser_lock:
        connected = _ser is not None and _ser.is_open
    if not connected and auto_connect_first:
        result = _auto_connect()
        if not result.get("success"):
            return jsonify(result)

    line = f"A1_TEST {command} {message}"
    payload = (line + "\r\n").encode("utf-8")
    start_seq = _rx_seq
    result = _send_payload(payload, line, hex_mode=False)
    if not result.get("success"):
        return jsonify(result)
    waited = _wait_for_text(list(wait_tokens), timeout_sec=timeout_sec, after_seq=start_seq)
    response_received = bool(waited.get("success"))
    return jsonify({
        **result,
        "success": bool(result.get("success")) and response_received,
        "transport_success": bool(result.get("success")),
        "response_received": response_received,
        "wait": waited,
        "matched": waited.get("matched"),
        "message": waited.get("matched", {}).get("text", "") if waited.get("success") else waited.get("error", ""),
    })


@serial_term_bp.route("/logs")
def logs():
    return jsonify({
        "rx": list(_rx_log),
        "tx": list(_tx_log),
    })


@serial_term_bp.route("/clear", methods=["POST"])
def clear():
    _rx_log.clear()
    _tx_log.clear()
    _latest_lines.clear()
    return jsonify({"success": True})
