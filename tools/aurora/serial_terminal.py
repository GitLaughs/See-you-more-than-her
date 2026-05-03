#!/usr/bin/env python3
"""serial_terminal.py — A1 调试串口实时终端 / 简易 CLI。"""

import codecs
import threading
import time
from collections import deque
from typing import Any, Dict, List, Optional, Tuple

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
_A1_DEBUG_PREFIX = "A1_TEST"
_A1_DEBUG_WAIT_PREFIX = "A1_DEBUG"
_A1_DEBUG_COMMANDS = {
    "ping": "ping",
    "osd_status": "osd_status",
    "uart_status": "uart_status",
    "chassis_stop": "chassis_test stop",
    "chassis_forward": "chassis_test forward",
    "chassis_backward": "chassis_test backward",
}
_A1_DEBUG_DESCRIPTIONS = {
    "chassis_forward": "R/rock -> forward",
    "chassis_backward": "S/scissors -> backward",
    "chassis_stop": "P/paper or NoTarget -> stop",
}

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
_rx_last_data_ts = 0.0

_PARTIAL_IDLE_FLUSH_SEC = 3.0  # 增加到3秒，避免频繁刷新部分行
_PARTIAL_BUFFER_LIMIT = 8192
_LOCK_SNAPSHOT_TIMEOUT_SEC = 0.02


def _serial_snapshot() -> Dict[str, Any]:
    state = _snapshot_state()
    acquired = _ser_lock.acquire(timeout=_LOCK_SNAPSHOT_TIMEOUT_SEC)
    if not acquired:
        return {
            "connected": False,
            "port": state.get("port") or _DEFAULT_PORT,
            "baud": int(state.get("baud") or 115200),
            "busy": True,
        }
    try:
        connected = _ser is not None and _ser.is_open
        return {
            "connected": connected,
            "port": str(_ser.port) if connected else (state.get("port") or _DEFAULT_PORT),
            "baud": int(_ser.baudrate) if connected else int(state.get("baud") or 115200),
            "busy": False,
        }
    finally:
        _ser_lock.release()


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
    for encoding in ("utf-8", "gb18030"):
        try:
            return raw.decode(encoding, errors="strict").replace("\r", "")
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace").replace("\r", "")


def decode_rx_line(raw: bytes) -> str:
    return _normalize_text(raw).rstrip("\n")


def decode_partial_rx_line(raw: bytes) -> str:
    return _normalize_text(raw)


def _split_decodable_prefix(raw: bytes) -> Tuple[bytes, bytes]:
    if not raw:
        return b"", b""
    best_prefix = b""
    best_tail = raw
    for encoding in ("utf-8", "gb18030"):
        try:
            decoder = codecs.getincrementaldecoder(encoding)(errors="strict")
            decoder.decode(raw, final=False)
            pending, _ = decoder.getstate()
            tail_len = len(pending or b"")
            prefix = raw if tail_len == 0 else raw[:-tail_len]
            tail = raw[len(prefix):]
            if len(prefix) > len(best_prefix):
                best_prefix = prefix
                best_tail = tail
        except Exception:
            continue
    if best_prefix:
        return best_prefix, best_tail
    return b"", raw


def _pop_complete_lines() -> List[bytes]:
    global _rx_buffer
    lines: List[bytes] = []
    start = 0
    idx = 0
    size = len(_rx_buffer)
    
    # 先检查是否有完整的换行符 (\n 或 \r\n)
    while idx < size:
        byte = _rx_buffer[idx]
        if byte == 10:  # \n
            # 只添加非空行
            if idx > start:
                lines.append(bytes(_rx_buffer[start:idx]))
            start = idx + 1
            idx += 1
        elif byte == 13:  # \r
            # 检查是否是 \r\n
            if idx + 1 < size and _rx_buffer[idx + 1] == 10:
                if idx > start:
                    lines.append(bytes(_rx_buffer[start:idx]))
                start = idx + 2
                idx += 2
            else:
                # 单独的 \r，也当作换行处理
                if idx > start:
                    lines.append(bytes(_rx_buffer[start:idx]))
                start = idx + 1
                idx += 1
        else:
            idx += 1
    
    if start > 0:
        _rx_buffer = _rx_buffer[start:]
    return lines


def _flush_partial_buffer(force: bool = False) -> None:
    global _rx_buffer
    if not _rx_buffer:
        return
    if not force and len(_rx_buffer) < _PARTIAL_BUFFER_LIMIT:
        return
    # 对于 force=True，我们会在较长空闲后才输出部分行，确保用户体验
    # 并且只输出看起来是完整消息的内容（避免太短的片段）
    if force and len(_rx_buffer) >= 5:  # 避免太短的片段，至少5个字节
        safe_raw, tail = _split_decodable_prefix(bytes(_rx_buffer))
        if safe_raw:
            text = decode_partial_rx_line(safe_raw)
            if text:
                _append_rx_entry(safe_raw, text, partial=True)
            _rx_buffer = bytearray(tail)
            return
    # 默认情况下，不轻易输出部分行，等待完整行到来
    pass


def _should_merge_partial(previous_text: str, next_text: str) -> bool:
    if not previous_text or not next_text:
        return False
    if previous_text[-1].isspace() or next_text[0].isspace():
        return False
    if previous_text[-1].isascii() and previous_text[-1].isalnum() and next_text[0].isascii() and next_text[0].isalnum():
        return True
    return False


def _append_rx_entry(raw: bytes, text: str, partial: bool = False) -> None:
    global _rx_seq
    if text and not partial and _rx_log:
        previous = _rx_log[0]
        prev_text = str(previous.get("text") or "")
        if previous.get("partial") and _should_merge_partial(prev_text, text):
            merged_text = prev_text + text
            merged_raw = bytes.fromhex(str(previous.get("hex") or "").replace(" ", "")) + raw
            entry = {
                "ts": time.strftime("%H:%M:%S"),
                "hex": merged_raw.hex(" ").upper(),
                "text": merged_text,
                "partial": False,
            }
            _rx_log.popleft()
            if _latest_lines and _latest_lines[0] is previous:
                _latest_lines.popleft()
            _rx_log.appendleft(entry)
            _latest_lines.appendleft(entry)
            with _rx_cond:
                _rx_seq += 1
                _rx_cond.notify_all()
            return
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
    global _running, _rx_buffer, _rx_last_data_ts
    while _running:
        with _ser_lock:
            ser = _ser
        if ser is None or not ser.is_open:
            time.sleep(0.05)
            continue
        try:
            waiting = getattr(ser, "in_waiting", 0)
            # 增加单次读取大小，尽可能读取更完整的数据
            chunk = ser.read(max(1, min(waiting or 1, 1024)))
            if not chunk:
                if _rx_buffer and (time.monotonic() - _rx_last_data_ts) >= _PARTIAL_IDLE_FLUSH_SEC:
                    _flush_partial_buffer(force=True)
                continue
            _rx_buffer.extend(chunk)
            _rx_last_data_ts = time.monotonic()
            # 优先处理完整的行，避免输出片段
            for line in _pop_complete_lines():
                text = decode_rx_line(line)
                if text:
                    _append_rx_entry(bytes(line), text, partial=False)
            # 不再在每次读取后都尝试刷新部分行，减少片段输出
        except Exception:
            time.sleep(0.05)


def _connect_serial(port: str, baud: int, timeout: float) -> None:
    global _ser, _running, _rx_thread, _rx_buffer, _rx_last_data_ts
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = serial.Serial(port, baud, timeout=timeout)
    _rx_buffer = bytearray()
    _rx_last_data_ts = 0.0
    _running = True
    _rx_thread = threading.Thread(target=_rx_worker, daemon=True, name="serial-term-rx")
    _rx_thread.start()


def _disconnect_serial() -> None:
    global _ser, _running, _rx_buffer, _rx_last_data_ts
    _running = False
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = None
    _rx_buffer = bytearray()
    _rx_last_data_ts = 0.0


def is_connected() -> bool:
    return bool(_serial_snapshot()["connected"])


def current_port() -> Optional[str]:
    return str(_serial_snapshot()["port"])


def current_baud() -> int:
    return int(_serial_snapshot()["baud"])


def current_timeout() -> float:
    return float(_snapshot_state().get("timeout") or 0.05)


def latest_lines(limit: int = 10) -> List[Dict[str, Any]]:
    return list(_rx_log)[:max(0, limit)]


def tx_log_entries() -> List[Dict[str, Any]]:
    return list(_tx_log)


def rx_log_entries() -> List[Dict[str, Any]]:
    return list(_rx_log)


def list_ports() -> list:
    return _list_ports()


def ensure_connected(baud: Optional[int] = None, timeout: Optional[float] = None) -> Dict[str, Any]:
    if is_connected():
        return {"success": True, "port": current_port(), "baud": current_baud()}
    return _auto_connect(baud=baud or 115200, timeout=timeout)


def ensure_connected_to(port: str, baud: Optional[int] = None, timeout: Optional[float] = None) -> Dict[str, Any]:
    target_port = str(port or "").strip() or _DEFAULT_PORT
    serial_timeout = float(timeout if timeout is not None else current_timeout())
    baudrate = int(baud or current_baud() or 115200)
    snapshot = _serial_snapshot()
    if snapshot["connected"] and str(snapshot["port"]).lower() == target_port.lower():
        return {"success": True, "port": snapshot["port"], "baud": snapshot["baud"]}
    try:
        _connect_serial(target_port, baudrate, serial_timeout)
        with _state_lock:
            _state["port"] = target_port
            _state["baud"] = baudrate
            _state["timeout"] = serial_timeout
        return {"success": True, "port": target_port, "baud": baudrate}
    except Exception as exc:
        return {"success": False, "error": str(exc), "port": target_port}


def is_shared_port(port: Optional[str]) -> bool:
    target = str(port or "").strip().lower()
    if not target:
        return False
    return target == str(_snapshot_state().get("port") or _DEFAULT_PORT).strip().lower() or target == _DEFAULT_PORT.lower()


def send_raw_payload(payload: bytes, text: Optional[str] = None) -> Dict[str, Any]:
    display = text if text is not None else payload.hex(" ").upper()
    return _send_payload(payload, display, hex_mode=text is None)


def build_a1_debug_line(command_key: str) -> Dict[str, Any]:
    key = str(command_key or "").strip()
    command = _A1_DEBUG_COMMANDS.get(key)
    if command is None:
        return {"success": False, "error": f"不支持的调试命令: {key}"}
    return {
        "success": True,
        "key": key,
        "command": command,
        "line": f"{_A1_DEBUG_PREFIX} {command}",
        "description": _A1_DEBUG_DESCRIPTIONS.get(key, ""),
        "wait_tokens": [
            _A1_DEBUG_WAIT_PREFIX,
            f'"command":"{command.split()[0]}"',
        ],
    }


def send_text_line(line: str, wait_tokens: Optional[List[str]] = None, timeout_sec: float = 0.8) -> Dict[str, Any]:
    ready = ensure_connected()
    if not ready.get("success"):
        return ready
    payload = (line.rstrip() + "\r\n").encode("utf-8")
    result = _send_payload(payload, line, hex_mode=False)
    if not result.get("success") or not wait_tokens:
        return {**result, "port": current_port(), "command_line": line}
    waited = _wait_for_text(wait_tokens, timeout_sec=timeout_sec)
    return {
        **result,
        "success": bool(result.get("success")) and bool(waited.get("success")),
        "transport_success": bool(result.get("success")),
        "response_received": bool(waited.get("success")),
        "matched": waited.get("matched"),
        "message": waited.get("matched", {}).get("text", "") if waited.get("success") else waited.get("error", ""),
        "port": current_port(),
        "command_line": line,
    }


def send_hex_payload(hex_text: str) -> Dict[str, Any]:
    ready = ensure_connected()
    if not ready.get("success"):
        return ready
    try:
        payload = bytes.fromhex(str(hex_text or "").replace(" ", ""))
    except Exception as exc:
        return {"success": False, "error": str(exc)}
    return _send_payload(payload, str(hex_text or "").strip(), hex_mode=True)


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


def _recent_rx_lines(limit: int = 8) -> List[str]:
    return [str(item.get("text") or item.get("hex") or "") for item in list(_latest_lines)[-limit:]]


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
            if lowered and all(token in text for token in lowered):
                return {"success": True, "matched": item}
    return {"success": False, "error": "未在串口输出中等到预期回传", "recent_rx": _recent_rx_lines()}


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
    serial_state = _serial_snapshot()
    return jsonify({
        "success": True,
        "available": _SERIAL_AVAILABLE,
        "connected": serial_state["connected"],
        "port": serial_state["port"],
        "baud": serial_state["baud"],
        "busy": serial_state["busy"],
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

    serial_state = _serial_snapshot()
    connected = serial_state["connected"]
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


@serial_term_bp.route("/a1_debug", methods=["POST"])
def a1_debug():
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装"})
    data = request.get_json(silent=True) or {}
    built = build_a1_debug_line(str(data.get("command") or ""))
    if not built.get("success"):
        return jsonify(built)

    timeout_sec = max(0.3, float(data.get("timeout_sec") or 2.5))
    ready = ensure_connected()
    if not ready.get("success"):
        return jsonify(ready)

    start_seq = _rx_seq
    result = _send_payload((built["line"] + "\r\n").encode("utf-8"), built["line"], hex_mode=False)
    if not result.get("success"):
        return jsonify(result)

    waited = _wait_for_text(list(built["wait_tokens"]), timeout_sec=timeout_sec, after_seq=start_seq)
    response_received = bool(waited.get("success"))
    return jsonify({
        **result,
        "success": bool(result.get("success")) and response_received,
        "transport_success": bool(result.get("success")),
        "response_received": response_received,
        "command": built["command"],
        "key": built["key"],
        "sent_line": built["line"],
        "description": built.get("description", ""),
        "wait_tokens": built["wait_tokens"],
        "matched": waited.get("matched"),
        "message": waited.get("matched", {}).get("text", "") if response_received else waited.get("error", ""),
        "recent_rx": waited.get("recent_rx", _recent_rx_lines()),
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
