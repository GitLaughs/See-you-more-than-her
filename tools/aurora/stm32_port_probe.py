#!/usr/bin/env python3
"""STM32 串口端口探测器。

在 Windows 上枚举可用串口，向每个端口发送低速前进指令，
并等待 STM32 回传 24 字节遥测帧，以确认当前硬件接线对应的 COM 口。

协议与 WHEELTEC_C50X_2025.12.26 保持一致：
  - 11 字节控制帧（A1/PC -> STM32）
  - 24 字节遥测帧（STM32 -> A1/PC）
  - BCC 为 bytes[0..N-2] 的 XOR
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from flask import Flask, jsonify, render_template, request

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    serial = None  # type: ignore[assignment]
    SERIAL_AVAILABLE = False

try:
    from chassis_comm import build_cmd, parse_rx
except ImportError as exc:
    raise RuntimeError(
        "无法导入 chassis_comm.py，请确认脚本位于 tools/aurora/ 目录下运行。"
    ) from exc


APP_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = APP_DIR / "templates"

FRAME_HEADER = 0x7B
TELEMETRY_LEN = 24

app = Flask(__name__, template_folder=str(TEMPLATE_DIR))


def _frame_hex(frame: bytes) -> str:
    return frame.hex(" ").upper()


def _port_dict(info: Any) -> Dict[str, Any]:
    return {
        "port": getattr(info, "device", ""),
        "description": getattr(info, "description", "") or "",
        "hwid": getattr(info, "hwid", "") or "",
        "manufacturer": getattr(info, "manufacturer", "") or "",
        "product": getattr(info, "product", "") or "",
        "serial_number": getattr(info, "serial_number", "") or "",
        "location": getattr(info, "location", "") or "",
        "vid": getattr(info, "vid", None),
        "pid": getattr(info, "pid", None),
    }


def _list_ports() -> List[Dict[str, Any]]:
    if not SERIAL_AVAILABLE:
        return []
    return [_port_dict(info) for info in serial.tools.list_ports.comports()]


def _preview_hex(data: bytes, limit: int = 48) -> str:
    if not data:
        return ""
    preview = data[:limit].hex(" ").upper()
    if len(data) > limit:
        preview += " …"
    return preview


def _extract_telemetry(buffer: bytearray) -> Tuple[Optional[Dict[str, Any]], Optional[bytes]]:
    """从缓冲区中提取第一帧有效 24 字节遥测帧。"""
    header = bytes([FRAME_HEADER])
    while len(buffer) >= TELEMETRY_LEN:
        idx = buffer.find(header)
        if idx < 0:
            buffer.clear()
            return None, None
        if idx > 0:
            del buffer[:idx]
            continue
        if len(buffer) < TELEMETRY_LEN:
            return None, None

        candidate = bytes(buffer[:TELEMETRY_LEN])
        parsed = parse_rx(candidate)
        if parsed:
            del buffer[:TELEMETRY_LEN]
            return parsed, candidate

        # 当前帧不是有效遥测，继续向后滑动一个字节寻找下一帧。
        del buffer[0]

    return None, None


def _probe_port(
    port_info: Dict[str, Any],
    baud: int,
    vx: int,
    read_ms: int,
    settle_ms: int,
    send_stop: bool = True,
    collect_response: bool = True,
) -> Dict[str, Any]:
    port_name = port_info.get("port", "")
    forward_frame = build_cmd(vx, 0, 0, 0x00)
    stop_frame = build_cmd(0, 0, 0, 0x00)

    result: Dict[str, Any] = {
        **port_info,
        "baud": baud,
        "vx": vx,
        "tx_hex": _frame_hex(forward_frame),
        "stop_hex": _frame_hex(stop_frame),
        "rx_hex": "",
        "rx_total_bytes": 0,
        "status": "pending",
        "status_text": "等待测试",
        "stop_sent": False,
        "opened": False,
        "error": "",
        "telemetry": None,
    }

    if not SERIAL_AVAILABLE:
        result["status"] = "pyserial_missing"
        result["status_text"] = "缺少 pyserial"
        result["error"] = "请先安装 pyserial"
        return result

    timeout_sec = max(0.02, min(0.2, read_ms / 1000.0 / 6.0 if read_ms > 0 else 0.02))
    write_timeout = max(0.2, read_ms / 1000.0 if read_ms > 0 else 0.2)
    serial_handle = None
    rx_buffer = bytearray()

    try:
        serial_handle = serial.Serial(
            port=port_name,
            baudrate=baud,
            timeout=timeout_sec,
            write_timeout=write_timeout,
        )
        result["opened"] = True

        try:
            serial_handle.reset_input_buffer()
            serial_handle.reset_output_buffer()
        except Exception:
            pass

        if settle_ms > 0:
            time.sleep(settle_ms / 1000.0)

        serial_handle.write(forward_frame)
        serial_handle.flush()

        if collect_response and read_ms > 0:
            deadline = time.monotonic() + read_ms / 1000.0
            while time.monotonic() < deadline:
                waiting = getattr(serial_handle, "in_waiting", 0)
                chunk = serial_handle.read(waiting or 1)
                if chunk:
                    result["rx_total_bytes"] += len(chunk)
                    rx_buffer.extend(chunk)
                    parsed, frame_bytes = _extract_telemetry(rx_buffer)
                    if parsed:
                        result["status"] = "matched"
                        result["status_text"] = "有效回包"
                        result["telemetry"] = parsed
                        result["rx_hex"] = _frame_hex(frame_bytes or b"")
                        break
                else:
                    time.sleep(0.02)
            else:
                if result["rx_total_bytes"] > 0:
                    result["status"] = "raw_response"
                    result["status_text"] = "收到数据但未成帧"
                    result["rx_hex"] = _preview_hex(bytes(rx_buffer))
                else:
                    result["status"] = "no_response"
                    result["status_text"] = "无响应"
        else:
            result["status"] = "sent_only"
            result["status_text"] = "已发送停车帧"

        if send_stop:
            try:
                serial_handle.write(stop_frame)
                serial_handle.flush()
                time.sleep(0.02)
                result["stop_sent"] = True
            except Exception as stop_exc:
                result["stop_sent"] = False
                result["error"] = str(stop_exc)

    except Exception as exc:
        result["status"] = "open_failed"
        result["status_text"] = "打开失败"
        result["error"] = str(exc)
    finally:
        if serial_handle is not None:
            try:
                serial_handle.close()
            except Exception:
                pass

    return result


@app.route("/")
def index():
    return render_template(
        "stm32_port_probe.html",
        serial_available=SERIAL_AVAILABLE,
        default_baud=115200,
        default_vx=80,
        default_read_ms=450,
        default_settle_ms=80,
        port_hint="WHEELTEC C50X: UART3 = PB10(TX) / PB11(RX) · A1 ssne_ai_demo: GPIO_PIN_0 / GPIO_PIN_2",
    )


@app.route("/api/ports")
def api_ports():
    ports = _list_ports()
    return jsonify({
        "available": SERIAL_AVAILABLE,
        "ports": ports,
        "count": len(ports),
    })


@app.route("/api/scan", methods=["POST"])
def api_scan():
    if not SERIAL_AVAILABLE:
        return jsonify({
            "success": False,
            "error": "pyserial 未安装，请先执行: pip install pyserial",
            "results": [],
        })

    payload = request.get_json(silent=True) or {}
    baud = int(payload.get("baud", 115200))
    vx = int(payload.get("vx", 80))
    read_ms = int(payload.get("read_ms", 450))
    settle_ms = int(payload.get("settle_ms", 80))

    requested_port = (payload.get("port") or "").strip()
    available_ports = _list_ports()
    if requested_port:
        candidate_ports = [p for p in available_ports if p["port"] == requested_port]
        if not candidate_ports:
            return jsonify({
                "success": False,
                "error": f"未找到串口 {requested_port}",
                "results": [],
            })
    else:
        candidate_ports = available_ports

    start_ts = time.monotonic()
    stop_mode = vx == 0 and read_ms == 0
    results = [
        _probe_port(
            port_info=port_info,
            baud=baud,
            vx=vx,
            read_ms=read_ms,
            settle_ms=settle_ms,
            send_stop=True,
            collect_response=read_ms > 0,
        )
        for port_info in candidate_ports
    ]
    elapsed_ms = round((time.monotonic() - start_ts) * 1000.0, 1)

    matched = [item for item in results if item.get("status") == "matched"]
    if stop_mode:
        summary = (
            f"全体停车完成，共 {len(results)} 个端口"
            if not requested_port
            else f"单端口停车完成：{requested_port}"
        )
    else:
        summary = (
            f"扫描完成，共 {len(results)} 个端口，找到 {len(matched)} 个有效回包"
            if not requested_port
            else f"单端口测试完成：{requested_port}"
        )

    return jsonify({
        "success": True,
        "mode": "stop" if stop_mode else "scan",
        "summary": summary,
        "elapsed_ms": elapsed_ms,
        "requested_port": requested_port,
        "baud": baud,
        "vx": vx,
        "read_ms": read_ms,
        "settle_ms": settle_ms,
        "results": results,
        "matched_ports": [item["port"] for item in matched],
        "tx_hex": results[0]["tx_hex"] if results else _frame_hex(build_cmd(vx, 0, 0, 0x00)),
    })


def main() -> None:
    parser = argparse.ArgumentParser(description="STM32 串口端口探测器")
    parser.add_argument("--host", default="127.0.0.1", help="监听地址，默认 127.0.0.1")
    parser.add_argument("--port", type=int, default=5006, help="Web 服务端口，默认 5006")
    parser.add_argument("--baud", type=int, default=115200, help="串口波特率，默认 115200")
    parser.add_argument("--vx", type=int, default=80, help="扫描时发送的前进速度 (mm/s)")
    parser.add_argument("--read-ms", type=int, default=450, help="每个端口等待回包时间 (ms)")
    parser.add_argument("--settle-ms", type=int, default=80, help="串口打开后等待稳定时间 (ms)")
    args = parser.parse_args()

    print("[INFO] STM32 串口端口探测器")
    print(f"[INFO] 串口库状态: {'可用' if SERIAL_AVAILABLE else '不可用'}")
    print(f"[INFO] 默认前进速度: {args.vx} mm/s")
    print(f"[INFO] 监听地址: http://{args.host}:{args.port}")
    if not SERIAL_AVAILABLE:
        print("[WARN] pyserial 未安装，页面将无法枚举或测试串口")

    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()