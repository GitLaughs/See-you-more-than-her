#!/usr/bin/env python3
"""Direct PC -> STM32 chassis routes for PC tool."""

import struct
import threading
import time
from collections import deque
from typing import Optional

from flask import Blueprint, jsonify, request

try:
    import serial
    import serial.tools.list_ports
    _SERIAL_AVAILABLE = True
except ImportError:
    _SERIAL_AVAILABLE = False

# ─── 协议常量 ─────────────────────────────────────────────────────────────────
FRAME_HEADER = 0x7B
FRAME_TAIL   = 0x7D
CMD_NORMAL   = 0x00
CMD_RECHARGE = 0x01
CMD_NAV      = 0x02
CMD_DOCK     = 0x03

TX_LEN = 11
RX_LEN = 24
_DIRECT_TRANSPORT = "direct_serial"

# ─── 全局串口状态 ─────────────────────────────────────────────────────────────
_ser: Optional["serial.Serial"] = None  # type: ignore
_ser_lock = threading.Lock()
_rx_thread: Optional[threading.Thread] = None
_running = False
_transport_mode = _DIRECT_TRANSPORT
_connected_port: Optional[str] = None
_connected_baud = 115200

_telemetry: dict = {}
_tx_log: deque = deque(maxlen=30)
_rx_log: deque = deque(maxlen=30)
_rx_seq = 0

chassis_bp = Blueprint("chassis", __name__, url_prefix="/api/chassis")


# ─── 协议工具 ─────────────────────────────────────────────────────────────────

def _bcc(data: bytes) -> int:
    r = 0
    for b in data:
        r ^= b
    return r


def build_cmd(vx: int, vy: int, vz: int, cmd: int = CMD_NORMAL) -> bytes:
    frame = bytearray(11)
    frame[0] = FRAME_HEADER
    frame[1] = cmd
    frame[2] = 0x00
    struct.pack_into(">h", frame, 3, max(-32768, min(32767, vx)))
    struct.pack_into(">h", frame, 5, max(-32768, min(32767, vy)))
    struct.pack_into(">h", frame, 7, max(-32768, min(32767, vz)))
    frame[9] = _bcc(bytes(frame[:9]))
    frame[10] = FRAME_TAIL
    return bytes(frame)


def parse_rx(data: bytes) -> Optional[dict]:
    if len(data) != RX_LEN:
        return None
    if data[0] != FRAME_HEADER or data[23] != FRAME_TAIL:
        return None
    if data[22] != _bcc(data[:22]):
        return None

    flag_stop = data[1]
    vx = struct.unpack_from(">h", data, 2)[0]
    vy = struct.unpack_from(">h", data, 4)[0]
    vz = struct.unpack_from(">h", data, 6)[0]
    ax = struct.unpack_from(">h", data, 8)[0]
    ay = struct.unpack_from(">h", data, 10)[0]
    az = struct.unpack_from(">h", data, 12)[0]
    gx = struct.unpack_from(">h", data, 14)[0]
    gy = struct.unpack_from(">h", data, 16)[0]
    gz = struct.unpack_from(">h", data, 18)[0]
    vol = struct.unpack_from(">h", data, 20)[0]

    return {
        "flag_stop": flag_stop,
        "vx": vx,
        "vy": vy,
        "vz": vz,
        "accel_x": round(ax * 0.001, 4),
        "accel_y": round(ay * 0.001, 4),
        "accel_z": round(az * 0.001, 4),
        "gyro_x": round(gx * 0.001, 4),
        "gyro_y": round(gy * 0.001, 4),
        "gyro_z": round(gz * 0.001, 4),
        "voltage": round(vol * 0.001, 3),
        "ts": time.strftime("%H:%M:%S"),
    }


def _direct_connected() -> bool:
    with _ser_lock:
        return _ser is not None and _ser.is_open


def _current_port() -> Optional[str]:
    with _ser_lock:
        if _ser is not None and _ser.is_open:
            return str(_ser.port)
    return _connected_port


def _current_connected() -> bool:
    return _direct_connected()


def _set_direct_backend(port: str, baud: int) -> dict:
    global _ser, _rx_thread, _running, _telemetry, _rx_seq, _transport_mode, _connected_port, _connected_baud
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        try:
            _ser = serial.Serial(port, baud, timeout=0.1)
        except Exception as e:
            return {"success": False, "error": str(e)}
    _running = True
    _telemetry = {}
    _rx_seq = 0
    _transport_mode = _DIRECT_TRANSPORT
    _connected_port = port
    _connected_baud = baud
    _rx_thread = threading.Thread(target=_rx_worker, daemon=True, name="pc-chassis-rx")
    _rx_thread.start()
    return {"success": True, "port": port, "baud": baud, "transport": _DIRECT_TRANSPORT}


def _write_payload(payload: bytes, tx_entry: dict) -> dict:
    with _ser_lock:
        if _ser is None or not _ser.is_open:
            return {"success": False, "error": "未连接串口"}
        try:
            _ser.write(payload)
        except Exception as e:
            return {"success": False, "error": str(e)}
    _tx_log.appendleft(tx_entry)
    return {"success": True}


# ─── 接收线程 ─────────────────────────────────────────────────────────────────

def _rx_worker():
    global _running, _telemetry, _rx_seq
    buf = bytearray()
    while _running:
        with _ser_lock:
            ser = _ser
        if ser is None or not ser.is_open:
            time.sleep(0.1)
            continue
        try:
            waiting = ser.in_waiting
            chunk = ser.read(max(1, min(waiting, 128)))
            if chunk:
                buf.extend(chunk)
                while len(buf) >= RX_LEN:
                    idx = buf.find(FRAME_HEADER)
                    if idx < 0:
                        buf.clear()
                        break
                    if idx > 0:
                        del buf[:idx]
                        continue
                    if len(buf) < RX_LEN:
                        break
                    frame_bytes = bytes(buf[:RX_LEN])
                    del buf[:RX_LEN]
                    parsed = parse_rx(frame_bytes)
                    if parsed:
                        _rx_seq += 1
                        _telemetry = parsed
                        _rx_log.appendleft({
                            "hex": frame_bytes.hex(" ").upper(),
                            "data": parsed,
                            "ts": parsed["ts"],
                        })
        except Exception:
            time.sleep(0.05)


# ─── Flask Blueprint 路由 ─────────────────────────────────────────────────────

@chassis_bp.route("/available")
def available():
    return jsonify({"available": _SERIAL_AVAILABLE})


@chassis_bp.route("/ports")
def list_ports():
    if not _SERIAL_AVAILABLE:
        return jsonify([])
    ports = [
        {"port": p.device, "desc": p.description, "hwid": p.hwid}
        for p in serial.tools.list_ports.comports()
    ]
    return jsonify(ports)


@chassis_bp.route("/connect", methods=["POST"])
def connect():
    global _telemetry, _rx_seq
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装，请运行: pip install pyserial"})

    d = request.get_json(silent=True) or {}
    port = str(d.get("port", "") or "").strip()
    baud = int(d.get("baud", 115200))
    if not port:
        return jsonify({"success": False, "error": "需要指定串口号"})

    _telemetry = {}
    _rx_seq = 0
    result = _set_direct_backend(port, baud)
    if result.get("success"):
        print(f"[PC] 已连接 {result['port']} @ {result['baud']} ({result['transport']})")
    return jsonify(result)


@chassis_bp.route("/disconnect", methods=["POST"])
def disconnect():
    global _ser, _running, _telemetry, _rx_seq, _transport_mode, _connected_port
    _telemetry = {}
    _rx_seq = 0
    _running = False
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = None
    _connected_port = None
    print("[PC] 已断开")
    return jsonify({"success": True, "transport": _DIRECT_TRANSPORT})


@chassis_bp.route("/status")
def status():
    return jsonify({
        "connected": _current_connected(),
        "port": _current_port(),
        "telemetry": _telemetry,
        "transport": _transport_mode,
    })


@chassis_bp.route("/move", methods=["POST"])
def move():
    d = request.get_json(silent=True) or {}
    vx = int(d.get("vx", 0))
    vy = int(d.get("vy", 0))
    vz = int(d.get("vz", 0))
    cmd = int(d.get("cmd", CMD_NORMAL))

    frame = build_cmd(vx, vy, vz, cmd)
    tx_entry = {
        "hex": frame.hex(" ").upper(),
        "vx": vx, "vy": vy, "vz": vz, "cmd": cmd,
        "ts": time.strftime("%H:%M:%S"),
    }
    result = _write_payload(frame, tx_entry)
    if not result.get("success"):
        return jsonify(result)
    return jsonify({"success": True, "frame": frame.hex(" ").upper(), "transport": _transport_mode})


@chassis_bp.route("/stop", methods=["POST"])
def stop():
    frame = build_cmd(0, 0, 0)
    tx_entry = {
        "hex": frame.hex(" ").upper(),
        "vx": 0, "vy": 0, "vz": 0, "cmd": 0,
        "ts": time.strftime("%H:%M:%S"),
    }
    result = _write_payload(frame, tx_entry)
    if not result.get("success"):
        return jsonify(result)
    return jsonify({"success": True, "transport": _transport_mode})


@chassis_bp.route("/tx_log")
def tx_log():
    return jsonify(list(_tx_log))


@chassis_bp.route("/rx_log")
def rx_log():
    return jsonify(list(_rx_log))


@chassis_bp.route("/raw_send", methods=["POST"])
def raw_send():
    d = request.get_json(silent=True) or {}
    hex_str = d.get("hex", "").replace(" ", "")
    try:
        raw = bytes.fromhex(hex_str)
    except ValueError as e:
        return jsonify({"success": False, "error": f"十六进制格式错误: {e}"})

    tx_entry = {
        "hex": raw.hex(" ").upper(),
        "raw": True,
        "ts": time.strftime("%H:%M:%S"),
    }
    result = _write_payload(raw, tx_entry)
    if not result.get("success"):
        return jsonify(result)
    return jsonify({"success": True, "bytes_sent": len(raw), "transport": _transport_mode})


@chassis_bp.route("/ping", methods=["POST"])
def ping():
    import time as _time
    global _rx_seq
    frame = build_cmd(0, 0, 0)
    rx_seq_before = _rx_seq
    tx_entry = {
        "hex": frame.hex(" ").upper(),
        "vx": 0, "vy": 0, "vz": 0, "cmd": 0,
        "ts": _time.strftime("%H:%M:%S"),
    }
    result = _write_payload(frame, tx_entry)
    if not result.get("success"):
        return jsonify(result)

    deadline = _time.monotonic() + 0.6
    while _time.monotonic() < deadline:
        if _rx_seq > rx_seq_before:
            tele = dict(_telemetry) if _telemetry else None
            if tele is None:
                _time.sleep(0.02)
                continue
            return jsonify({
                "success": True,
                "connected": True,
                "frame_tx": frame.hex(" ").upper(),
                "telemetry": tele,
                "transport": _DIRECT_TRANSPORT,
            })
        _time.sleep(0.05)

    return jsonify({
        "success": True,
        "connected": False,
        "frame_tx": frame.hex(" ").upper(),
        "note": "发送成功，未收到 STM32 遥测帧（请检查 RX 接线）",
        "transport": _DIRECT_TRANSPORT,
    })
