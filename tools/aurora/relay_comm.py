#!/usr/bin/env python3
"""relay_comm.py — A1 chassis control over the local COM13 terminal."""

import time
from typing import Any, Dict, List, Optional

from flask import Blueprint, jsonify, request

import serial_terminal as st

relay_bp = Blueprint("relay", __name__, url_prefix="/api/relay")


def _connected() -> bool:
    return st.is_connected()


def _current_port() -> Optional[str]:
    return st.current_port()


def _ensure_connected(baud: Optional[int] = None) -> Dict[str, Any]:
    return st.ensure_connected(baud=baud or 115200)


def _send_cli(line: str, wait_tokens: Optional[List[str]] = None, timeout_sec: float = 0.8) -> Dict[str, Any]:
    return st.send_text_line(line, wait_tokens=wait_tokens, timeout_sec=timeout_sec)


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
        "latest_lines": st.latest_lines(),
    }


@relay_bp.route("/config", methods=["GET", "POST"])
def relay_config():
    return jsonify({
        "success": True,
        "base_url": "COM13",
        "timeout_sec": st.current_timeout(),
        "port": _current_port(),
        "baud": st.current_baud(),
        "transport": "COM13",
    })


@relay_bp.route("/status")
def relay_status():
    return jsonify(_status_payload())


@relay_bp.route("/ports")
def relay_ports():
    return jsonify({"success": True, "ports": st.list_ports(), "preferred": _current_port(), "transport": "COM13"})


@relay_bp.route("/connect", methods=["POST"])
def relay_connect():
    data = request.get_json(silent=True) or {}
    baud = int(data.get("baud") or 115200)
    result = _ensure_connected(baud=baud)
    return jsonify({**result, "transport": "COM13"})


@relay_bp.route("/disconnect", methods=["POST"])
def relay_disconnect():
    # Keep COM13 alive for the shared terminal; this action only clears the logical relay target.
    return jsonify({"success": True, "connected": _connected(), "port": _current_port(), "transport": "COM13"})


@relay_bp.route("/move", methods=["POST"])
def relay_move():
    data = request.get_json(silent=True) or {}
    vx = int(data.get("vx") or 0)
    vy = int(data.get("vy") or 0)
    vz = int(data.get("vz") or 0)
    line = f"A1_TEST move {vx} {vy} {vz}"
    result = _send_cli(line)
    return jsonify({**result, "vx": vx, "vy": vy, "vz": vz, "transport": "COM13"})


@relay_bp.route("/stop", methods=["POST"])
def relay_stop():
    result = _send_cli("A1_TEST stop")
    return jsonify({**result, "transport": "COM13"})


@relay_bp.route("/raw_send", methods=["POST"])
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


@relay_bp.route("/ping", methods=["POST"])
def relay_ping():
    result = _send_cli(
        "A1_TEST debug_status",
        wait_tokens=["\"success\":true", "\"command\":\"debug_status\"", "\"chassis_ok\":"],
        timeout_sec=1.8,
    )
    return jsonify({
        **result,
        "connected": bool(result.get("success")),
        "telemetry": {},
        "frame_tx": "A1_TEST debug_status",
        "note": "COM13 已收到 A1 debug_status 回传；底盘运动请使用 A1_TEST move/stop。",
        "transport": "COM13",
        "ts": time.strftime("%H:%M:%S"),
    })


@relay_bp.route("/tx_log")
def relay_tx_log():
    return jsonify(st.tx_log_entries())


@relay_bp.route("/rx_log")
def relay_rx_log():
    return jsonify(st.rx_log_entries())
