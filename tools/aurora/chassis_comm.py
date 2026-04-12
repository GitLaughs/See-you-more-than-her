#!/usr/bin/env python3
"""
chassis_comm.py — WHEELTEC C50X STM32 底盘通信模块

协议参考: WHEELTEC_C50X_2025.12.26 STM32 源码
  - data_task.h / data_task.c   → 发送帧定义 (24字节)
  - uartx_callback.h / .c       → 接收帧解析 (11字节)
  - UART3 (PB10=TX, PB11=RX)    → A1 / ROS 控制通道

发送帧 (A1→STM32): 11字节
  [0]     0x7B            帧头
  [1]     Cmd             命令 (0x00=正常运动, 0x01/0x02=自动回充, 0x03=对接)
  [2]     0x00            保留
  [3..4]  Vx (int16 BE)   X轴线速度 mm/s
  [5..6]  Vy (int16 BE)   Y轴线速度 mm/s
  [7..8]  Vz (int16 BE)   Z轴角速度 mrad/s
  [9]     BCC             XOR(bytes[0..8])
  [10]    0x7D            帧尾

接收帧 (STM32→A1): 24字节
  [0]      0x7B            帧头
  [1]      FlagStop        电机失控标志 (0=正常)
  [2..3]   Vel_X (int16)   X轴当前速度 mm/s
  [4..5]   Vel_Y
  [6..7]   Vel_Z
  [8..9]   Accel_X (int16) 加速度计 (×0.001 → m/s²)
  [10..11] Accel_Y
  [12..13] Accel_Z
  [14..15] Gyro_X (int16)  陀螺仪 (×0.001 → rad/s)
  [16..17] Gyro_Y
  [18..19] Gyro_Z
  [20..21] Voltage (int16) 电池电压 (×0.001 → V)
  [22]     BCC             XOR(bytes[0..21])
  [23]     0x7D            帧尾
"""

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

# ─── 全局串口状态 ─────────────────────────────────────────────────────────────
_ser: Optional["serial.Serial"] = None  # type: ignore
_ser_lock = threading.Lock()
_rx_thread: Optional[threading.Thread] = None
_running = False

_telemetry: dict = {}
_tx_log: deque = deque(maxlen=30)
_rx_log: deque = deque(maxlen=30)

chassis_bp = Blueprint("chassis", __name__, url_prefix="/api/chassis")


# ─── 协议工具 ─────────────────────────────────────────────────────────────────

def _bcc(data: bytes) -> int:
    r = 0
    for b in data:
        r ^= b
    return r


def build_cmd(vx: int, vy: int, vz: int, cmd: int = CMD_NORMAL) -> bytes:
    """构建 11 字节指令帧 (A1→STM32)
    Args:
        vx: X轴线速度 mm/s (int16)
        vy: Y轴线速度 mm/s (int16)
        vz: Z轴角速度 mrad/s (int16)
        cmd: 命令字节
    """
    frame = bytearray(11)
    frame[0] = FRAME_HEADER
    frame[1] = cmd
    frame[2] = 0x00
    struct.pack_into(">h", frame, 3, max(-32768, min(32767, vx)))
    struct.pack_into(">h", frame, 5, max(-32768, min(32767, vy)))
    struct.pack_into(">h", frame, 7, max(-32768, min(32767, vz)))
    frame[9]  = _bcc(bytes(frame[:9]))
    frame[10] = FRAME_TAIL
    return bytes(frame)


def parse_rx(data: bytes) -> Optional[dict]:
    """解析 24 字节遥测帧 (STM32→A1)"""
    if len(data) != RX_LEN:
        return None
    if data[0] != FRAME_HEADER or data[23] != FRAME_TAIL:
        return None
    if data[22] != _bcc(data[:22]):
        return None

    flag_stop = data[1]
    vx  = struct.unpack_from(">h", data,  2)[0]
    vy  = struct.unpack_from(">h", data,  4)[0]
    vz  = struct.unpack_from(">h", data,  6)[0]
    ax  = struct.unpack_from(">h", data,  8)[0]
    ay  = struct.unpack_from(">h", data, 10)[0]
    az  = struct.unpack_from(">h", data, 12)[0]
    gx  = struct.unpack_from(">h", data, 14)[0]
    gy  = struct.unpack_from(">h", data, 16)[0]
    gz  = struct.unpack_from(">h", data, 18)[0]
    vol = struct.unpack_from(">h", data, 20)[0]

    return {
        "flag_stop":  flag_stop,
        "vx":         vx,
        "vy":         vy,
        "vz":         vz,
        "accel_x":    round(ax  * 0.001, 4),
        "accel_y":    round(ay  * 0.001, 4),
        "accel_z":    round(az  * 0.001, 4),
        "gyro_x":     round(gx  * 0.001, 4),
        "gyro_y":     round(gy  * 0.001, 4),
        "gyro_z":     round(gz  * 0.001, 4),
        "voltage":    round(vol * 0.001, 3),
        "ts":         time.strftime("%H:%M:%S"),
    }


# ─── 接收线程 ─────────────────────────────────────────────────────────────────

def _rx_worker():
    global _running, _telemetry
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
                # 扫描完整帧
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
    """检查 pyserial 是否可用"""
    return jsonify({"available": _SERIAL_AVAILABLE})


@chassis_bp.route("/ports")
def list_ports():
    """列出可用串口"""
    if not _SERIAL_AVAILABLE:
        return jsonify([])
    ports = [
        {"port": p.device, "desc": p.description, "hwid": p.hwid}
        for p in serial.tools.list_ports.comports()
    ]
    return jsonify(ports)


@chassis_bp.route("/connect", methods=["POST"])
def connect():
    global _ser, _rx_thread, _running
    if not _SERIAL_AVAILABLE:
        return jsonify({"success": False, "error": "pyserial 未安装，请运行: pip install pyserial"})

    d = request.get_json(silent=True) or {}
    port = d.get("port", "")
    baud = int(d.get("baud", 115200))
    if not port:
        return jsonify({"success": False, "error": "需要指定串口号"})

    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        try:
            _ser = serial.Serial(port, baud, timeout=0.1)
        except Exception as e:
            return jsonify({"success": False, "error": str(e)})

    _running = True
    _rx_thread = threading.Thread(target=_rx_worker, daemon=True, name="chassis-rx")
    _rx_thread.start()
    print(f"[CHASSIS] 已连接 {port} @ {baud}")
    return jsonify({"success": True, "port": port, "baud": baud})


@chassis_bp.route("/disconnect", methods=["POST"])
def disconnect():
    global _ser, _running
    _running = False
    with _ser_lock:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = None
    print("[CHASSIS] 已断开")
    return jsonify({"success": True})


@chassis_bp.route("/status")
def status():
    with _ser_lock:
        connected = _ser is not None and _ser.is_open
        port = _ser.port if connected else None
    return jsonify({
        "connected": connected,
        "port": port,
        "telemetry": _telemetry,
    })


@chassis_bp.route("/move", methods=["POST"])
def move():
    """发送运动指令
    Body: {"vx": mm/s, "vy": mm/s, "vz": mrad/s, "cmd": 0}
    """
    d = request.get_json(silent=True) or {}
    vx  = int(d.get("vx",  0))
    vy  = int(d.get("vy",  0))
    vz  = int(d.get("vz",  0))
    cmd = int(d.get("cmd", CMD_NORMAL))

    frame = build_cmd(vx, vy, vz, cmd)

    with _ser_lock:
        if _ser is None or not _ser.is_open:
            return jsonify({"success": False, "error": "未连接串口"})
        try:
            _ser.write(frame)
        except Exception as e:
            return jsonify({"success": False, "error": str(e)})

    _tx_log.appendleft({
        "hex": frame.hex(" ").upper(),
        "vx": vx, "vy": vy, "vz": vz, "cmd": cmd,
        "ts": time.strftime("%H:%M:%S"),
    })
    return jsonify({"success": True, "frame": frame.hex(" ").upper()})


@chassis_bp.route("/stop", methods=["POST"])
def stop():
    """发送急停指令 (Vx=Vy=Vz=0)"""
    frame = build_cmd(0, 0, 0)
    with _ser_lock:
        if _ser and _ser.is_open:
            try:
                _ser.write(frame)
            except Exception:
                pass
    _tx_log.appendleft({
        "hex": frame.hex(" ").upper(),
        "vx": 0, "vy": 0, "vz": 0, "cmd": 0,
        "ts": time.strftime("%H:%M:%S"),
    })
    return jsonify({"success": True})


@chassis_bp.route("/tx_log")
def tx_log():
    return jsonify(list(_tx_log))


@chassis_bp.route("/rx_log")
def rx_log():
    return jsonify(list(_rx_log))


@chassis_bp.route("/raw_send", methods=["POST"])
def raw_send():
    """发送原始十六进制帧（调试用）
    Body: {"hex": "7B 00 00 ..."}
    """
    d = request.get_json(silent=True) or {}
    hex_str = d.get("hex", "").replace(" ", "")
    try:
        raw = bytes.fromhex(hex_str)
    except ValueError as e:
        return jsonify({"success": False, "error": f"十六进制格式错误: {e}"})

    with _ser_lock:
        if _ser is None or not _ser.is_open:
            return jsonify({"success": False, "error": "未连接串口"})
        try:
            _ser.write(raw)
        except Exception as e:
            return jsonify({"success": False, "error": str(e)})

    _tx_log.appendleft({
        "hex": raw.hex(" ").upper(),
        "raw": True,
        "ts": time.strftime("%H:%M:%S"),
    })
    return jsonify({"success": True, "bytes_sent": len(raw)})
