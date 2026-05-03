#!/usr/bin/env python3
"""A1 relay control over the local COM13 terminal."""

import time
from typing import Any, Dict, List, Optional

from flask import Blueprint, jsonify, request

import a1_serial as st

a1_bp = Blueprint("a1", __name__, url_prefix="/api/a1")


_DEBUG_STATUS_WAIT_TOKENS = ["\"success\":true", "\"command\":\"debug_status\"", "\"chassis_ok\":"]


def _connected() -> bool:
    return st.is_connected()


def _current_port() -> Optional[str]:
    return st.current_port()


def _ensure_connected(baud: Optional[int] = None) -> Dict[str, Any]:
    return st.ensure_connected(baud=baud or 115200)


def _send_cli(line: str, wait_tokens: Optional[List[str]] = None, timeout_sec: float = 0.8) -> Dict[str, Any]:
    return st.send_text_line(line, wait_tokens=wait_tokens, timeout_sec=timeout_sec)


def _looks_like_wait_timeout(result: Dict[str, Any]) -> bool:
    if result.get("success") or not result.get("transport_success") or result.get("response_received"):
        return False
    error = str(result.get("error") or result.get("message") or "")
    return "未在串口输出中等到预期回传" in error


def _classify_debug_status_failure(result: Dict[str, Any]) -> Dict[str, str]:
    if not _looks_like_wait_timeout(result):
        return {
            "break_stage": _infer_break_stage(result),
            "diagnosis": "transport_error",
            "note": "COM13 命令下发失败，请检查串口 owner 与 A1_TEST 通道。",
        }

    latest_lines = st.latest_lines(limit=3)
    if not latest_lines:
        return {
            "break_stage": "A1 -> debug_status 回传",
            "diagnosis": "no_a1_reply",
            "note": "A1 未输出任何 debug_status 回传；若只连接 A1 未连接 STM32，也可能是 A1 侧未生成可识别状态。",
        }

    latest_text = " | ".join(str(item.get("text") or "").strip() for item in latest_lines if str(item.get("text") or "").strip())
    if latest_text:
        return {
            "break_stage": "A1 debug_status 输出格式",
            "diagnosis": "a1_output_mismatch",
            "note": f"A1 有输出，但不匹配预期关键字。最近输出: {latest_text}",
        }

    return {
        "break_stage": "A1 -> debug_status 回传",
        "diagnosis": "no_a1_reply",
        "note": "A1 未输出任何可见 debug_status 回传。",
    }


def _infer_break_stage(result: Dict[str, Any]) -> str:
    if result.get("success"):
        return "STM32 -> A1 -> COM13"
    if result.get("transport_success") and not result.get("response_received"):
        return "A1_TEST -> UART0/STM32"
    return "PC -> COM13"


def _status_payload() -> Dict[str, Any]:
    return {
        "success": True,
        "reachable": _connected(),
        "connected": _connected(),
        "port": _current_port(),
        "baud": st.current_baud(),
        "telemetry": {},
        "model_name": "COM13 / A1_TEST",
        "transport": "COM13",
        "serial_owner": "serial_terminal",
        "latest_lines": st.latest_lines(),
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
    result = _ensure_connected(baud=baud)
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
    result = _send_cli(line)
    return jsonify({**result, "vx": vx, "vy": vy, "vz": vz, "transport": "COM13", "gesture_map": "R/rock=forward, S/scissors=backward, P/paper or NoTarget=stop"})


@a1_bp.route("/stop", methods=["POST"])
def relay_stop():
    result = _send_cli("A1_TEST stop")
    return jsonify({**result, "transport": "COM13"})


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
    result = _send_cli(
        "A1_TEST debug_status",
        wait_tokens=_DEBUG_STATUS_WAIT_TOKENS,
        timeout_sec=1.8,
    )
    diagnosis = _classify_debug_status_failure(result)
    note = "COM13 已收到 A1 debug_status 回传；底盘运动请使用 A1_TEST move/stop。" if result.get("success") else diagnosis["note"]
    return jsonify({
        **result,
        "connected": bool(result.get("success")),
        "telemetry": {},
        "frame_tx": "A1_TEST debug_status",
        "note": note,
        "transport": "COM13",
        "serial_owner": "serial_terminal",
        "break_stage": "STM32 -> A1 -> COM13" if result.get("success") else diagnosis["break_stage"],
        "diagnosis": "ok" if result.get("success") else diagnosis["diagnosis"],
        "ts": time.strftime("%H:%M:%S"),
    })


@a1_bp.route("/tx_log")
def relay_tx_log():
    return jsonify(st.tx_log_entries())


@a1_bp.route("/rx_log")
def relay_rx_log():
    return jsonify(st.rx_log_entries())
