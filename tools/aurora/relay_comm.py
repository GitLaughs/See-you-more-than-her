#!/usr/bin/env python3
"""relay_comm.py — PC -> A1 Companion -> STM32 relay proxy."""

import json
import os
import threading
import urllib.error
import urllib.request
from typing import Any, Dict, Optional

from flask import Blueprint, jsonify, request

relay_bp = Blueprint("relay", __name__, url_prefix="/api/relay")

_relay_lock = threading.Lock()
_relay_state: Dict[str, Any] = {
    "base_url": os.environ.get("A1_COMPANION_URL", "http://127.0.0.1:5803"),
    "timeout_sec": 2.5,
}


def _normalize_base_url(value: str) -> str:
    text = (value or "").strip()
    if not text:
        raise ValueError("A1 Companion 地址不能为空")
    if not text.startswith("http://") and not text.startswith("https://"):
        text = f"http://{text}"
    return text.rstrip("/")


def _snapshot_state() -> Dict[str, Any]:
    with _relay_lock:
        return dict(_relay_state)


def _update_state(base_url: Optional[str] = None,
                  timeout_sec: Optional[float] = None) -> Dict[str, Any]:
    with _relay_lock:
        if base_url is not None:
            _relay_state["base_url"] = _normalize_base_url(base_url)
        if timeout_sec is not None:
            _relay_state["timeout_sec"] = max(0.5, float(timeout_sec))
        return dict(_relay_state)


def _relay_json(method: str, path: str,
                payload: Optional[Dict[str, Any]] = None) -> Any:
    state = _snapshot_state()
    target = f"{state['base_url']}{path}"
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {"Content-Type": "application/json"} if body is not None else {}
    req = urllib.request.Request(target, data=body, headers=headers,
                                 method=method.upper())
    try:
        with urllib.request.urlopen(req, timeout=state["timeout_sec"]) as resp:
            raw = resp.read()
            if not raw:
                return {}
            charset = resp.headers.get_content_charset() or "utf-8"
            text = raw.decode(charset, errors="replace")
            return json.loads(text)
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        if detail:
            try:
                parsed = json.loads(detail)
                if isinstance(parsed, dict) and parsed.get("error"):
                    raise RuntimeError(str(parsed["error"]))
            except json.JSONDecodeError:
                pass
        raise RuntimeError(f"远端返回 HTTP {exc.code}") from exc
    except urllib.error.URLError as exc:
        reason = getattr(exc, "reason", exc)
        raise RuntimeError(f"无法连接 A1 Companion: {reason}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError("远端响应不是合法 JSON") from exc


def _proxy_json(method: str, remote_path: str,
                payload: Optional[Dict[str, Any]] = None):
    try:
        result = _relay_json(method, remote_path, payload)
        return jsonify(result)
    except RuntimeError as exc:
        return jsonify({"success": False, "error": str(exc)})


@relay_bp.route("/config", methods=["GET", "POST"])
def relay_config():
    if request.method == "GET":
        return jsonify(_snapshot_state())

    data = request.get_json(silent=True) or {}
    try:
        state = _update_state(
            base_url=data.get("base_url") or data.get("url"),
            timeout_sec=data.get("timeout_sec") or data.get("timeout"),
        )
    except (TypeError, ValueError) as exc:
        return jsonify({"success": False, "error": str(exc)})
    return jsonify({"success": True, **state})


@relay_bp.route("/status")
def relay_status():
    state = _snapshot_state()
    try:
        companion = _relay_json("GET", "/status")
        chassis = _relay_json("GET", "/api/chassis/status")
        return jsonify({
            "success": True,
            "reachable": True,
            "base_url": state["base_url"],
            "connected": bool(chassis.get("connected")),
            "port": chassis.get("port"),
            "telemetry": chassis.get("telemetry") or {},
            "companion": companion,
            "source": companion.get("source"),
            "model_name": companion.get("model_name"),
        })
    except RuntimeError as exc:
        return jsonify({
            "success": False,
            "reachable": False,
            "base_url": state["base_url"],
            "connected": False,
            "error": str(exc),
            "telemetry": {},
        })


@relay_bp.route("/ports")
def relay_ports():
    try:
        return jsonify(_relay_json("GET", "/api/chassis/ports"))
    except RuntimeError as exc:
        return jsonify({"success": False, "error": str(exc), "ports": []})


@relay_bp.route("/connect", methods=["POST"])
def relay_connect():
    return _proxy_json("POST", "/api/chassis/connect", request.get_json(silent=True) or {})


@relay_bp.route("/disconnect", methods=["POST"])
def relay_disconnect():
    return _proxy_json("POST", "/api/chassis/disconnect", request.get_json(silent=True) or {})


@relay_bp.route("/move", methods=["POST"])
def relay_move():
    return _proxy_json("POST", "/api/chassis/move", request.get_json(silent=True) or {})


@relay_bp.route("/stop", methods=["POST"])
def relay_stop():
    return _proxy_json("POST", "/api/chassis/stop", request.get_json(silent=True) or {})


@relay_bp.route("/raw_send", methods=["POST"])
def relay_raw_send():
    return _proxy_json("POST", "/api/chassis/raw_send", request.get_json(silent=True) or {})


@relay_bp.route("/ping", methods=["POST"])
def relay_ping():
    return _proxy_json("POST", "/api/chassis/ping", request.get_json(silent=True) or {})


@relay_bp.route("/tx_log")
def relay_tx_log():
    return _proxy_json("GET", "/api/chassis/tx_log")


@relay_bp.route("/rx_log")
def relay_rx_log():
    return _proxy_json("GET", "/api/chassis/rx_log")