#!/usr/bin/env python3
"""ros_bridge.py — lightweight ROS2 bridge for direct STM32 debugging."""

import os
import shlex
import shutil
import subprocess
import threading
import time
from collections import deque
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from flask import Blueprint, jsonify, request

ros_bp = Blueprint("ros_bridge", __name__, url_prefix="/api/ros")

_REPO_ROOT = Path(__file__).resolve().parents[2]
_ROS_WS = _REPO_ROOT / "src" / "ros2_ws"
_ROS_INSTALL = _ROS_WS / "install"
_ROS_LOCAL_SETUP_BAT = _ROS_INSTALL / "local_setup.bat"
_ROS_LOCAL_SETUP_BASH = _ROS_INSTALL / "local_setup.bash"
_ROS_SETUP_BASH = _ROS_INSTALL / "setup.bash"
_ROS_DEFAULT_SETUP_BASH = Path("/opt/ros/jazzy/setup.bash")

_ROS_LOCK = threading.Lock()
_ROS_PROCESSES: Dict[str, subprocess.Popen] = {}
_ROS_LOGS: Dict[str, deque] = {}
_ROS_CONFIG: Dict[str, Any] = {
    "serial_port": "COM7" if os.name == "nt" else "/dev/wheeltec_controller",
    "serial_baud": 115200,
    "forward_linear_x": 0.18,
    "forward_angular_z": 0.0,
    "backward_linear_x": -0.18,
    "backward_angular_z": 0.0,
    "idle_stop_delay_sec": 1.0,
    "trigger_cooldown_sec": 0.8,
    "camera_hfov_deg": 60.0,
    "obstacle_distance": 0.28,
    "obstacle_clear_distance": 100.0,
    "automation_enabled": False,
    "forward_enabled": True,
    "backward_enabled": True,
    "stop_enabled": True,
    "obstacle_enabled": True,
    "last_action": "idle",
    "last_reason": "未启用视觉联动",
    "last_dispatch_ts": 0.0,
}

_BOOL_FIELDS = {
    "automation_enabled",
    "forward_enabled",
    "backward_enabled",
    "stop_enabled",
    "obstacle_enabled",
}
_FLOAT_FIELDS = {
    "forward_linear_x",
    "forward_angular_z",
    "backward_linear_x",
    "backward_angular_z",
    "idle_stop_delay_sec",
    "trigger_cooldown_sec",
    "camera_hfov_deg",
    "obstacle_distance",
    "obstacle_clear_distance",
}
_INT_FIELDS = {"serial_baud"}
_STR_FIELDS = {"serial_port"}


def _to_bash_path(path: Path) -> str:
    text = path.resolve().as_posix()
    if len(text) >= 3 and text[1] == ":":
        return f"/{text[0].lower()}{text[2:]}"
    return text


def _setup_candidates() -> List[str]:
    items = []
    if _ROS_DEFAULT_SETUP_BASH.exists():
        items.append(str(_ROS_DEFAULT_SETUP_BASH))
    if _ROS_SETUP_BASH.exists():
        items.append(str(_ROS_SETUP_BASH))
    if _ROS_LOCAL_SETUP_BASH.exists():
        items.append(str(_ROS_LOCAL_SETUP_BASH))
    if _ROS_LOCAL_SETUP_BAT.exists():
        items.append(str(_ROS_LOCAL_SETUP_BAT))
    return items


def _build_ros_command(command_parts: List[str]) -> List[str]:
    if os.name == "nt" and _ROS_LOCAL_SETUP_BAT.exists():
        cmdline = subprocess.list2cmdline(command_parts)
        return ["cmd.exe", "/c", f'call "{_ROS_LOCAL_SETUP_BAT}" && ros2 {cmdline}']

    bash_exec = shutil.which("bash")
    if bash_exec:
        setup_cmds = []
        if _ROS_DEFAULT_SETUP_BASH.exists():
            setup_cmds.append(f"source {shlex.quote(_ROS_DEFAULT_SETUP_BASH.as_posix())} >/dev/null 2>&1")
        if _ROS_SETUP_BASH.exists():
            setup_cmds.append(f"source {shlex.quote(_to_bash_path(_ROS_SETUP_BASH))} >/dev/null 2>&1")
        elif _ROS_LOCAL_SETUP_BASH.exists():
            setup_cmds.append(f"source {shlex.quote(_to_bash_path(_ROS_LOCAL_SETUP_BASH))} >/dev/null 2>&1")
        if setup_cmds:
            setup_cmds.append("ros2 " + shlex.join(command_parts))
            return [bash_exec, "-lc", "; ".join(setup_cmds)]

    ros2_exec = shutil.which("ros2") or shutil.which("ros2.exe")
    if ros2_exec:
        return [ros2_exec, *command_parts]

    raise RuntimeError("未找到 ros2 命令，请先安装 ROS2 并配置工作空间")


def _process_running(name: str) -> bool:
    with _ROS_LOCK:
        proc = _ROS_PROCESSES.get(name)
        return proc is not None and proc.poll() is None


def _append_log(name: str, line: str) -> None:
    if not line:
        return
    with _ROS_LOCK:
        log = _ROS_LOGS.setdefault(name, deque(maxlen=120))
        log.append(line.rstrip())


def _read_process_output(name: str, proc: subprocess.Popen) -> None:
    if proc.stdout is None:
        return
    for line in proc.stdout:
        _append_log(name, line)


def _start_process(name: str, command_parts: List[str]) -> Tuple[bool, str]:
    with _ROS_LOCK:
        proc = _ROS_PROCESSES.get(name)
        if proc is not None and proc.poll() is None:
            return True, f"{name} 已在运行"

    invocation = _build_ros_command(command_parts)
    creationflags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    proc = subprocess.Popen(
        invocation,
        cwd=str(_ROS_WS),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
        creationflags=creationflags,
    )
    with _ROS_LOCK:
        _ROS_PROCESSES[name] = proc
        _ROS_LOGS[name] = deque(maxlen=120)
    thread = threading.Thread(target=_read_process_output, args=(name, proc), daemon=True)
    thread.start()
    time.sleep(0.6)
    if proc.poll() is not None:
        output = "\n".join(list(_ROS_LOGS.get(name, []))[-10:]).strip()
        return False, output or f"{name} 启动失败"
    return True, f"{name} 已启动"


def _stop_process(name: str) -> Tuple[bool, str]:
    with _ROS_LOCK:
        proc = _ROS_PROCESSES.get(name)
    if proc is None:
        return True, f"{name} 未运行"
    if proc.poll() is not None:
        return True, f"{name} 已停止"

    proc.terminate()
    try:
        proc.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2.0)
    return True, f"{name} 已停止"


def _run_ros_once(command_parts: List[str], timeout: float = 8.0) -> Tuple[bool, str]:
    invocation = _build_ros_command(command_parts)
    completed = subprocess.run(
        invocation,
        cwd=str(_ROS_WS),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    output = (completed.stdout or completed.stderr or "").strip()
    if completed.returncode != 0:
        return False, output or "ROS 命令执行失败"
    return True, output


def _publish_twist(topic: str, linear_x: float, angular_z: float) -> Tuple[bool, str]:
    message = (
        "{linear: {x: %.4f, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: %.4f}}"
        % (linear_x, angular_z)
    )
    return _run_ros_once(
        ["topic", "pub", "--once", topic, "geometry_msgs/msg/Twist", message],
        timeout=8.0,
    )


def _publish_position(angle_x: float, distance: float) -> Tuple[bool, str]:
    message = "{angle_x: %.4f, angle_y: 0.0, distance: %.4f}" % (angle_x, distance)
    return _run_ros_once(
        [
            "topic",
            "pub",
            "--once",
            "/object_tracker/current_position",
            "turn_on_wheeltec_robot/msg/Position",
            message,
        ],
        timeout=8.0,
    )


def _publish_clear_obstacle() -> Tuple[bool, str]:
    with _ROS_LOCK:
        distance = float(_ROS_CONFIG["obstacle_clear_distance"])
    return _publish_position(0.0, distance)


def _status_snapshot() -> Dict[str, Any]:
    with _ROS_LOCK:
        process_states = {}
        for name in ("wheeltec_robot_node", "multi_avoidance"):
            proc = _ROS_PROCESSES.get(name)
            running = proc is not None and proc.poll() is None
            process_states[name] = {
                "running": running,
                "exit_code": None if running or proc is None else proc.returncode,
                "logs": list(_ROS_LOGS.get(name, []))[-20:],
            }
        return {
            "workspace": str(_ROS_WS),
            "workspace_exists": _ROS_WS.exists(),
            "setup_candidates": _setup_candidates(),
            "ros2_available": bool(shutil.which("ros2") or shutil.which("ros2.exe") or shutil.which("bash")),
            "config": dict(_ROS_CONFIG),
            "processes": process_states,
        }


def _update_config(data: Dict[str, Any]) -> Dict[str, Any]:
    with _ROS_LOCK:
        for key in _BOOL_FIELDS:
            if key in data:
                _ROS_CONFIG[key] = bool(data[key])
        for key in _FLOAT_FIELDS:
            if key in data and data[key] is not None:
                _ROS_CONFIG[key] = float(data[key])
        for key in _INT_FIELDS:
            if key in data and data[key] is not None:
                _ROS_CONFIG[key] = int(data[key])
        for key in _STR_FIELDS:
            if key in data and data[key] is not None:
                _ROS_CONFIG[key] = str(data[key]).strip()
        return dict(_ROS_CONFIG)


def _set_last_action(action: str, reason: str) -> None:
    with _ROS_LOCK:
        _ROS_CONFIG["last_action"] = action
        _ROS_CONFIG["last_reason"] = reason
        _ROS_CONFIG["last_dispatch_ts"] = time.time()


def _get_config_copy() -> Dict[str, Any]:
    with _ROS_LOCK:
        return dict(_ROS_CONFIG)


def _build_direct_node_command(config: Dict[str, Any]) -> List[str]:
    return [
        "run",
        "turn_on_wheeltec_robot",
        "wheeltec_robot_node",
        "--ros-args",
        "-p",
        f"usart_port_name:={config['serial_port']}",
        "-p",
        f"serial_baud_rate:={int(config['serial_baud'])}",
    ]


def _build_avoidance_command() -> List[str]:
    return [
        "run",
        "wheeltec_multi",
        "multi_avoidance",
    ]


def _manual_direct_serial_busy() -> bool:
    try:
        import chassis_comm  # pylint: disable=import-outside-toplevel

        serial_obj = getattr(chassis_comm, "_ser", None)
        return bool(serial_obj and getattr(serial_obj, "is_open", False))
    except Exception:
        return False


def _extract_boxes(detections: Iterable[Tuple[float, float, float, float, float, int]]) -> Dict[int, List[Tuple[float, float, float, float, float, int]]]:
    buckets: Dict[int, List[Tuple[float, float, float, float, float, int]]] = {}
    for det in detections:
        try:
            x1, y1, x2, y2, score, cls_id = det
            cls_key = int(cls_id)
            buckets.setdefault(cls_key, []).append((float(x1), float(y1), float(x2), float(y2), float(score), cls_key))
        except (TypeError, ValueError):
            continue
    return buckets


def _largest_box(boxes: List[Tuple[float, float, float, float, float, int]]) -> Optional[Tuple[float, float, float, float, float, int]]:
    if not boxes:
        return None
    return max(boxes, key=lambda item: ((item[2] - item[0]) * (item[3] - item[1]), item[4]))


def _box_to_angle(box: Tuple[float, float, float, float, float, int], frame_w: float, hfov_deg: float) -> float:
    x1, _, x2, _, _, _ = box
    center_x = (x1 + x2) * 0.5
    normalized = (center_x / max(frame_w, 1.0)) - 0.5
    hfov_rad = max(1.0, hfov_deg) * 3.141592653589793 / 180.0
    return normalized * hfov_rad


def _dispatch_stop(reason: str) -> Tuple[bool, str]:
    ok1, msg1 = _publish_twist("/cmd_vel", 0.0, 0.0)
    ok2, msg2 = _publish_twist("/cmd_vel_ori", 0.0, 0.0)
    if _process_running("multi_avoidance"):
        _publish_clear_obstacle()
    if ok1 or ok2:
        _set_last_action("stop", reason)
        return True, msg1 or msg2 or "已发送停车命令"
    return False, msg1 or msg2 or "停车命令发送失败"


def _dispatch_forward(config: Dict[str, Any], reason: str) -> Tuple[bool, str]:
    topic = "/cmd_vel_ori" if _process_running("multi_avoidance") else "/cmd_vel"
    if _process_running("multi_avoidance"):
        _publish_clear_obstacle()
    ok, message = _publish_twist(topic, config["forward_linear_x"], config["forward_angular_z"])
    if ok:
        _set_last_action("forward", reason)
    return ok, message


def _dispatch_backward(config: Dict[str, Any], reason: str) -> Tuple[bool, str]:
    topic = "/cmd_vel_ori" if _process_running("multi_avoidance") else "/cmd_vel"
    if _process_running("multi_avoidance"):
        _publish_clear_obstacle()
    ok, message = _publish_twist(topic, config["backward_linear_x"], config["backward_angular_z"])
    if ok:
        _set_last_action("backward", reason)
    return ok, message


def _dispatch_obstacle(config: Dict[str, Any],
                       box: Tuple[float, float, float, float, float, int],
                       frame_shape: Tuple[int, int]) -> Tuple[bool, str]:
    if not _process_running("multi_avoidance"):
        return _dispatch_stop("检测到 obstacle_box，但避障节点未启动，已退回停车")

    frame_h, frame_w = frame_shape
    del frame_h
    angle_x = _box_to_angle(box, float(frame_w), float(config["camera_hfov_deg"]))
    ok_cmd, msg_cmd = _publish_twist("/cmd_vel_ori", config["forward_linear_x"], config["forward_angular_z"])
    ok_obs, msg_obs = _publish_position(angle_x, float(config["obstacle_distance"]))
    if ok_cmd and ok_obs:
        _set_last_action("obstacle", f"触发 obstacle_box 避障，angle_x={angle_x:.3f}")
        return True, msg_obs or msg_cmd or "已发送避障输入"
    return False, msg_obs or msg_cmd or "避障输入发送失败"


def handle_yolo_detections(detections: Iterable[Tuple[float, float, float, float, float, int]],
                           frame_shape: Tuple[int, int]) -> Dict[str, Any]:
    config = _get_config_copy()
    now = time.time()
    result = {
        "enabled": bool(config["automation_enabled"]),
        "action": "idle",
        "reason": config["last_reason"],
        "dispatched": False,
    }
    if not config["automation_enabled"]:
        return result

    buckets = _extract_boxes(detections)
    has_stop = bool(config["stop_enabled"] and buckets.get(2))
    obstacle_box = _largest_box(buckets.get(3, [])) if config["obstacle_enabled"] else None
    has_forward = bool(config["forward_enabled"] and buckets.get(1))
    has_backward = bool(config["backward_enabled"] and buckets.get(4))

    action = "idle"
    if has_stop:
        action = "stop"
    elif obstacle_box is not None:
        action = "obstacle"
    elif has_backward:
        action = "backward"
    elif has_forward:
        action = "forward"

    result["action"] = action

    last_action = config["last_action"]
    last_dispatch_ts = float(config["last_dispatch_ts"])
    cooldown = float(config["trigger_cooldown_sec"])
    if action == last_action and (now - last_dispatch_ts) < cooldown:
        result["reason"] = config["last_reason"]
        return result

    if action == "stop":
        ok, message = _dispatch_stop("检测到 stop 手势")
    elif action == "obstacle":
        ok, message = _dispatch_obstacle(config, obstacle_box, frame_shape)
    elif action == "backward":
        ok, message = _dispatch_backward(config, "检测到 backward 手势")
    elif action == "forward":
        ok, message = _dispatch_forward(config, "检测到 forward 手势")
    else:
        should_idle_stop = last_action in {"forward", "backward", "obstacle"} and (now - last_dispatch_ts) >= float(config["idle_stop_delay_sec"])
        if should_idle_stop:
            ok, message = _dispatch_stop("手势消失，自动停车")
        else:
            ok, message = True, config["last_reason"]
            _set_last_action("idle", "等待 forward/backward/stop/obstacle_box")

    result["reason"] = message
    result["dispatched"] = ok
    return result


@ros_bp.route("/status")
def ros_status():
    return jsonify({"success": True, **_status_snapshot()})


@ros_bp.route("/config", methods=["POST"])
def ros_config():
    data = request.get_json(silent=True) or {}
    try:
        config = _update_config(data)
    except (TypeError, ValueError) as exc:
        return jsonify({"success": False, "error": str(exc)})
    return jsonify({"success": True, "config": config})


@ros_bp.route("/start_node", methods=["POST"])
def ros_start_node():
    data = request.get_json(silent=True) or {}
    node = str(data.get("node") or "").strip()
    if node not in {"wheeltec_robot_node", "multi_avoidance"}:
        return jsonify({"success": False, "error": "未知节点"})
    if node == "wheeltec_robot_node" and _manual_direct_serial_busy():
        return jsonify({"success": False, "error": "本机串口调试模块仍占用串口，请先断开直连串口"})

    if data:
        _update_config(data)
    config = _get_config_copy()
    command = _build_direct_node_command(config) if node == "wheeltec_robot_node" else _build_avoidance_command()
    try:
        ok, message = _start_process(node, command)
    except RuntimeError as exc:
        return jsonify({"success": False, "error": str(exc)})
    return jsonify({"success": ok, "message": message, **_status_snapshot()})


@ros_bp.route("/stop_node", methods=["POST"])
def ros_stop_node():
    data = request.get_json(silent=True) or {}
    node = str(data.get("node") or "").strip()
    if node not in {"wheeltec_robot_node", "multi_avoidance"}:
        return jsonify({"success": False, "error": "未知节点"})
    ok, message = _stop_process(node)
    return jsonify({"success": ok, "message": message, **_status_snapshot()})


@ros_bp.route("/move", methods=["POST"])
def ros_move():
    data = request.get_json(silent=True) or {}
    linear_x = float(data.get("linear_x", 0.0))
    angular_z = float(data.get("angular_z", 0.0))
    topic = "/cmd_vel_ori" if _process_running("multi_avoidance") else "/cmd_vel"
    ok, message = _publish_twist(topic, linear_x, angular_z)
    return jsonify({"success": ok, "message": message, "topic": topic})


@ros_bp.route("/stop_motion", methods=["POST"])
def ros_stop_motion():
    ok, message = _dispatch_stop("前端手动停车")
    return jsonify({"success": ok, "message": message})


@ros_bp.route("/obstacle", methods=["POST"])
def ros_obstacle():
    data = request.get_json(silent=True) or {}
    angle_x = float(data.get("angle_x", 0.0))
    distance = float(data.get("distance", _ROS_CONFIG["obstacle_distance"]))
    ok, message = _publish_position(angle_x, distance)
    return jsonify({"success": ok, "message": message})