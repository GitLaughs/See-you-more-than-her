#!/usr/bin/env python3
"""A1 relay control over the local COM13 terminal."""

import time
from typing import Any, Dict, List, Optional

from flask import Blueprint, jsonify, request

import a1_serial as st

a1_bp = Blueprint("a1", __name__, url_prefix="/api/a1")


_PING_WAIT_TOKENS = ["A1_DEBUG", "\"command\":\"ping\"", "\"chassis_ok\":"]
_RPS_SNAPSHOT_WAIT_TOKENS = ["A1_DEBUG", "\"command\":\"rps_snapshot\"", "\"label\":"]
_MOVE_WAIT_TOKENS = ["A1_DEBUG", "\"command\":\"move\"", "\"chassis_ok\":"]
_CHASSIS_TEST_WAIT_TOKENS = ["A1_DEBUG", "\"command\":\"chassis_test\"", "\"chassis_ok\":"]


def _connected() -> bool:
    return st.is_connected()


def _current_port() -> Optional[str]:
    return st.current_port()


def _ensure_connected(baud: Optional[int] = None) -> Dict[str, Any]:
    return st.ensure_connected(baud=baud or 115200)


def _ensure_connected_to(port: Optional[str], baud: Optional[int] = None) -> Dict[str, Any]:
    target_port = str(port or _current_port() or "COM13").strip() or "COM13"
    return st.ensure_connected_to(target_port, baud=baud or 115200)


def _send_cli(line: str, wait_tokens: Optional[List[str]] = None, timeout_sec: float = 0.8) -> Dict[str, Any]:
    return st.send_text_line(line, wait_tokens=wait_tokens, timeout_sec=timeout_sec)


def _send_a1_debug(line: str, wait_tokens: Optional[List[str]] = None, timeout_sec: float = 2.5) -> Dict[str, Any]:
    return st.send_a1_debug_line(line, wait_tokens=wait_tokens, timeout_sec=timeout_sec)


def _looks_like_wait_timeout(result: Dict[str, Any]) -> bool:
    if result.get("success") or not result.get("transport_success") or result.get("response_received"):
        return False
    error = str(result.get("error") or result.get("message") or "")
    return "未在串口输出中等到预期回传" in error


def _classify_ping_failure(result: Dict[str, Any]) -> Dict[str, str]:
    if not _looks_like_wait_timeout(result):
        return {
            "break_stage": _infer_break_stage(result),
            "diagnosis": "transport_error",
            "note": "COM13 命令下发失败，请检查串口 owner 与 A1_TEST 通道。",
        }

    latest_lines = st.latest_lines(limit=3)
    if not latest_lines:
        return {
            "break_stage": "A1 -> ping 回传",
            "diagnosis": "no_a1_reply",
            "note": "A1 未输出任何 ping 回传；若只连接 A1 未连接 STM32，也可能是 A1 侧未生成可识别状态。",
        }

    latest_text = " | ".join(str(item.get("text") or "").strip() for item in latest_lines if str(item.get("text") or "").strip())
    if latest_text:
        return {
            "break_stage": "A1 ping 输出格式",
            "diagnosis": "a1_output_mismatch",
            "note": f"A1 有输出，但不匹配预期关键字。最近输出: {latest_text}",
        }

    return {
        "break_stage": "A1 -> ping 回传",
        "diagnosis": "no_a1_reply",
        "note": "A1 未输出任何可见 ping 回传。",
    }


def _infer_break_stage(result: Dict[str, Any]) -> str:
    if result.get("success"):
        return "STM32 -> A1 -> COM13"
    if result.get("transport_success") and not result.get("response_received"):
        return "A1_TEST -> UART0/STM32"
    return "PC -> COM13"


def _status_payload() -> Dict[str, Any]:
    serial_status = st.serial_status_snapshot(line_limit=12)
    latest_structured = serial_status.get("latest_structured") or {}
    return {
        "success": True,
        "reachable": serial_status.get("connected", False),
        "connected": serial_status.get("connected", False),
        "port": serial_status.get("port"),
        "baud": serial_status.get("baud"),
        "telemetry": {},
        "model_name": "COM13 / A1_TEST",
        "transport": "COM13",
        "serial_owner": "serial_terminal",
        "latest_lines": serial_status.get("latest_lines", []),
        "latest_structured": latest_structured,
        "rx_count": serial_status.get("rx_count", 0),
        "tx_count": serial_status.get("tx_count", 0),
        "latest_rx": serial_status.get("latest_rx"),
        "latest_tx": serial_status.get("latest_tx"),
        "session_summary": {
            "last_command": latest_structured.get("command"),
            "last_action": latest_structured.get("action"),
            "last_gesture": latest_structured.get("gesture"),
            "chassis_ok": bool(latest_structured.get("chassis_ok")),
            "message": latest_structured.get("message") or "",
        },
    }


@a1_bp.route("/config", methods=["GET", "POST"])
def relay_config():
    return jsonify({
        "success": True,
        "base_url": "COM13",
        "timeout_sec": st.current_timeout(),
        "port": _current_port(),
        "baud": st.current_baud(),
        "transport": "COM13",
    })


@a1_bp.route("/status")
def relay_status():
    return jsonify(_status_payload())


@a1_bp.route("/ports")
def relay_ports():
    return jsonify({"success": True, "ports": st.list_ports(), "preferred": _current_port(), "transport": "COM13"})


@a1_bp.route("/connect", methods=["POST"])
def relay_connect():
    data = request.get_json(silent=True) or {}
    baud = int(data.get("baud") or 115200)
    port = str(data.get("port") or _current_port() or "COM13").strip() or "COM13"
    result = _ensure_connected_to(port=port, baud=baud)
    return jsonify({**result, "transport": "COM13", "gesture_map": "P/paper or NoTarget -> stop"})


@a1_bp.route("/disconnect", methods=["POST"])
def relay_disconnect():
    st.disconnect_serial()
    return jsonify({"success": True, "connected": False, "port": _current_port(), "transport": "COM13"})


@a1_bp.route("/move", methods=["POST"])
def relay_move():
    data = request.get_json(silent=True) or {}
    vx = int(data.get("vx") or 0)
    vy = int(data.get("vy") or 0)
    vz = int(data.get("vz") or 0)
    line = f"A1_TEST move {vx} {vy} {vz}"
    result = _send_a1_debug(line, wait_tokens=_MOVE_WAIT_TOKENS, timeout_sec=float(data.get("timeout_sec") or 2.5))
    return jsonify({
        **result,
        "vx": vx,
        "vy": vy,
        "vz": vz,
        "transport": "COM13",
        "gesture_map": "R/rock=forward, S/scissors=backward, P/paper or NoTarget=stop",
    })


@a1_bp.route("/chassis_test", methods=["POST"])
def relay_chassis_test():
    data = request.get_json(silent=True) or {}
    action = str(data.get("action") or "stop").strip() or "stop"
    if action not in {"forward", "stop"}:
        return jsonify({"success": False, "error": "仅支持 forward/stop", "transport": "COM13"})
    line = f"A1_TEST chassis_test {action}"
    result = _send_a1_debug(line, wait_tokens=_CHASSIS_TEST_WAIT_TOKENS, timeout_sec=float(data.get("timeout_sec") or 2.5))
    return jsonify({
        **result,
        "action": action,
        "transport": "COM13",
        "command": "chassis_test",
    })


@a1_bp.route("/stop", methods=["POST"])
def relay_stop():
    result = _send_a1_debug("A1_TEST stop", wait_tokens=_CHASSIS_TEST_WAIT_TOKENS, timeout_sec=2.5)
    return jsonify({**result, "transport": "COM13"})


@a1_bp.route("/snapshot", methods=["POST"])
def relay_snapshot():
    data = request.get_json(silent=True) or {}
    request_id = str(data.get("request_id") or "a1_tool").strip() or "a1_tool"
    line = f"A1_TEST rps_snapshot {request_id}"
    result = _send_a1_debug(line, wait_tokens=_RPS_SNAPSHOT_WAIT_TOKENS, timeout_sec=float(data.get("timeout_sec") or 3.0))
    return jsonify({
        **result,
        "request_id": request_id,
        "transport": "COM13",
        "command": "rps_snapshot",
        "description": "latest board classification snapshot",
    })


@a1_bp.route("/raw_send", methods=["POST"])
def relay_raw_send():
    data = request.get_json(silent=True) or {}
    text = str(data.get("text") or "").strip()
    hex_text = str(data.get("hex") or "").strip()
    if text:
        result = _send_cli(text)
        return jsonify({**result, "transport": "COM13"})
    if not hex_text:
        return jsonify({"success": False, "error": "发送内容不能为空", "transport": "COM13"})
    result = st.send_hex_payload(hex_text)
    return jsonify({**result, "transport": "COM13"})


@a1_bp.route("/ping", methods=["POST"])
def relay_ping():
    result = _send_a1_debug(
        "A1_TEST ping",
        wait_tokens=_PING_WAIT_TOKENS,
        timeout_sec=2.5,
    )
    diagnosis = _classify_ping_failure(result)
    note = "COM13 已收到 A1_DEBUG ping 回传；底盘运动请使用 A1_TEST move/stop 或 chassis_test forward/stop。" if result.get("success") else diagnosis["note"]
    return jsonify({
        **result,
        "connected": bool(result.get("success")),
        "telemetry": {},
        "frame_tx": "A1_TEST ping",
        "note": note,
        "transport": "COM13",
        "serial_owner": "serial_terminal",
        "break_stage": "PC -> COM13 -> A1_TEST -> A1_DEBUG" if result.get("success") else diagnosis["break_stage"],
        "diagnosis": "ok" if result.get("success") else diagnosis["diagnosis"],
        "ts": time.strftime("%H:%M:%S"),
    })


@a1_bp.route("/tx_log")
def relay_tx_log():
    return jsonify(st.tx_log_entries())


@a1_bp.route("/rx_log")
def relay_rx_log():
    return jsonify(st.rx_log_entries())


@a1_bp.route("/logs")
def relay_logs():
    return jsonify({
        "success": True,
        "transport": "COM13",
        "rx": st.rx_log_entries(),
        "tx": st.tx_log_entries(),
    })
