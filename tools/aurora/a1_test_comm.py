#!/usr/bin/env python3
"""a1_test_comm.py — 临时 A1 开发板 TCP 测试桥。"""

import json
import os
import socket
import threading
import time
from collections import deque
from typing import Any, Dict, Optional

from flask import Blueprint, jsonify, request

a1_test_bp = Blueprint("a1_test", __name__, url_prefix="/api/a1_test")

_state_lock = threading.Lock()
_state: Dict[str, Any] = {
    "host": os.environ.get("A1_TEST_HOST", "").strip(),
    "port": int(os.environ.get("A1_TEST_PORT", "9091")),
    "timeout_sec": 2.0,
}
_history = deque(maxlen=20)


def _normalize_host(value: str) -> str:
    text = (value or "").strip()
    text = text.replace("http://", "").replace("https://", "")
    if "/" in text:
        text = text.split("/", 1)[0]
    return text.strip()


def _snapshot_state() -> Dict[str, Any]:
    with _state_lock:
        return dict(_state)


def _append_history(direction: str, payload: Any) -> None:
    _history.appendleft({
        "ts": time.strftime("%H:%M:%S"),
        "direction": direction,
        "payload": payload,
    })


def _update_state(host: Optional[str] = None,
                  port: Optional[int] = None,
                  timeout_sec: Optional[float] = None) -> Dict[str, Any]:
    with _state_lock:
        if host is not None:
            _state["host"] = _normalize_host(host)
        if port is not None:
            _state["port"] = int(port)
        if timeout_sec is not None:
            _state["timeout_sec"] = max(0.5, float(timeout_sec))
        return dict(_state)


def _connect_socket() -> tuple[socket.socket, Dict[str, Any]]:
    state = _snapshot_state()
    host = (state.get("host") or "").strip()
    port = int(state.get("port") or 0)
    if not host:
        raise RuntimeError("A1 测试模块地址未配置")
    if port <= 0:
        raise RuntimeError("A1 测试模块端口无效")

    try:
        sock = socket.create_connection((host, port), timeout=state["timeout_sec"])
        sock.settimeout(state["timeout_sec"])
        return sock, state
    except OSError as exc:
        raise RuntimeError(f"无法连接 A1 测试模块: {exc}") from exc


def _recv_line(sock: socket.socket) -> str:
    chunks = []
    while True:
        block = sock.recv(4096)
        if not block:
            break
        chunks.append(block)
        if b"\n" in block:
            break
    raw = b"".join(chunks).decode("utf-8", errors="replace").strip()
    if not raw:
        raise RuntimeError("A1 测试模块未返回数据")
    return raw.splitlines()[0].strip()


def _send_payload(payload: Dict[str, Any]) -> Dict[str, Any]:
    sock, _ = _connect_socket()
    try:
        encoded = (json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8")
        _append_history("tx", payload)
        sock.sendall(encoded)
        raw = _recv_line(sock)
        try:
            parsed = json.loads(raw)
        except json.JSONDecodeError:
            parsed = {"success": False, "message": raw}
        _append_history("rx", parsed)
        if not isinstance(parsed, dict):
            raise RuntimeError("A1 测试模块响应格式错误")
        return parsed
    except socket.timeout as exc:
        raise RuntimeError("等待 A1 测试模块响应超时") from exc
    finally:
        sock.close()


@a1_test_bp.route("/config", methods=["GET", "POST"])
def a1_test_config():
    if request.method == "GET":
        return jsonify(_snapshot_state())

    data = request.get_json(silent=True) or {}
    try:
        state = _update_state(
            host=data.get("host"),
            port=data.get("port"),
            timeout_sec=data.get("timeout_sec") or data.get("timeout"),
        )
    except (TypeError, ValueError) as exc:
        return jsonify({"success": False, "error": str(exc)})
    return jsonify({"success": True, **state})


@a1_test_bp.route("/status")
def a1_test_status():
    state = _snapshot_state()
    try:
        sock, _ = _connect_socket()
        sock.close()
        return jsonify({"success": True, "reachable": True, **state})
    except RuntimeError as exc:
        return jsonify({
            "success": False,
            "reachable": False,
            "error": str(exc),
            **state,
        })


@a1_test_bp.route("/send_test", methods=["POST"])
def a1_test_send():
    data = request.get_json(silent=True) or {}
    command = str(data.get("command") or "test_echo").strip() or "test_echo"
    payload = {
        "command": command,
        "client": "aurora_companion",
        "message": str(data.get("message") or "pc_frontend_test"),
    }
    try:
        result = _send_payload(payload)
        return jsonify({
            "success": bool(result.get("success", True)),
            "request": payload,
            "response": result,
            "message": result.get("message", ""),
        })
    except RuntimeError as exc:
        return jsonify({"success": False, "error": str(exc)})


@a1_test_bp.route("/history")
def a1_test_history():
    return jsonify(list(_history))
