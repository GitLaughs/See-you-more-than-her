#!/usr/bin/env python3
"""
Aurora Flash Web Tool — A1 一键烧录工具

目标:
- 提供可交互的 Web 前端，一键触发 EVB 固件烧录。
- 复用 Aurora 生态与现有 SDK 命令:
  1) 优先走容器内 burn_tool 命令行烧录。
  2) 兜底启动 Aurora GUI（CH347 插件）进行手动确认。

用法:
  python aurora_flash_web.py [--port 5055] [--mode auto] [--firmware <path>]
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

from flask import Flask, jsonify, render_template, request

app = Flask(__name__, template_folder="templates")


def _now_ts() -> float:
    return time.time()


ROOT_DIR = Path(__file__).resolve().parents[2]
AURORA_EXE = ROOT_DIR / "Aurora-2.0.0-ciciec.13" / "Aurora.exe"
OUTPUT_EVB_DIR = ROOT_DIR / "output" / "evb"
DEFAULT_ZIMAGE = "zImage.smartsens-m1-evb"
CONTAINER_ROOT = "/app"
SDK_CONTAINER_DIR = "/app/data/A1_SDK_SC132GS/smartsens_sdk"
BURN_TOOL_CONTAINER_PATH = "./tools/burn_tool/x86_linux/burn_tool"


@dataclass
class FlashTaskState:
    running: bool = False
    mode: str = ""
    firmware_host_path: str = ""
    firmware_container_path: str = ""
    started_at: float = 0.0
    finished_at: float = 0.0
    success: Optional[bool] = None
    return_code: Optional[int] = None
    logs: List[str] = field(default_factory=list)
    process: Optional[subprocess.Popen] = None


STATE = FlashTaskState()
STATE_LOCK = threading.Lock()


def add_log(line: str) -> None:
    with STATE_LOCK:
        STATE.logs.append(line.rstrip("\n"))
        if len(STATE.logs) > 500:
            STATE.logs = STATE.logs[-500:]


def rel_display(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT_DIR)).replace("\\", "/")
    except Exception:
        return str(path)


def list_firmware_candidates() -> List[Dict[str, str]]:
    candidates: List[Path] = []

    latest_path = OUTPUT_EVB_DIR / "latest" / DEFAULT_ZIMAGE
    if latest_path.exists():
        candidates.append(latest_path.resolve())

    if OUTPUT_EVB_DIR.exists():
        for child in OUTPUT_EVB_DIR.iterdir():
            if child.is_dir() and (child.name.isdigit() or "_" in child.name):
                zimg = child / DEFAULT_ZIMAGE
                if zimg.exists():
                    candidates.append(zimg.resolve())

    uniq: Dict[str, Path] = {}
    for item in candidates:
        uniq[str(item)] = item
    items = list(uniq.values())
    items.sort(key=lambda p: p.stat().st_mtime, reverse=True)

    result: List[Dict[str, str]] = []
    for p in items:
        size_mb = p.stat().st_size / (1024 * 1024)
        result.append(
            {
                "path": str(p),
                "display": rel_display(p),
                "sizeMB": f"{size_mb:.2f}",
                "mtime": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(p.stat().st_mtime)),
            }
        )
    return result


def host_to_container_path(host_path: Path) -> Optional[str]:
    try:
        rel = host_path.resolve().relative_to(ROOT_DIR.resolve())
    except Exception:
        return None
    return f"{CONTAINER_ROOT}/{str(rel).replace('\\', '/')}"


def check_container_ready() -> Optional[str]:
    try:
        cmd = ["docker", "exec", "A1_Builder", "bash", "-lc", "echo ready"]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=8)
        if proc.returncode != 0:
            return "A1_Builder 容器不可用，请先启动容器。"

        check_cmd = [
            "docker",
            "exec",
            "A1_Builder",
            "bash",
            "-lc",
            f"cd {SDK_CONTAINER_DIR} && test -x {BURN_TOOL_CONTAINER_PATH}",
        ]
        proc2 = subprocess.run(check_cmd, capture_output=True, text=True, timeout=8)
        if proc2.returncode != 0:
            return "容器内未找到 burn_tool 可执行文件。"
    except Exception as exc:
        return f"检查容器失败: {exc}"

    return None


def start_flash_task(firmware_path: Path, mode: str) -> None:
    def _runner() -> None:
        with STATE_LOCK:
            STATE.running = True
            STATE.mode = mode
            STATE.firmware_host_path = str(firmware_path)
            STATE.firmware_container_path = ""
            STATE.started_at = _now_ts()
            STATE.finished_at = 0.0
            STATE.success = None
            STATE.return_code = None
            STATE.logs = []
            STATE.process = None

        add_log(f"[INFO] 任务开始，模式: {mode}")
        add_log(f"[INFO] 固件: {firmware_path}")

        selected_mode = mode
        if selected_mode == "auto":
            if check_container_ready() is None:
                selected_mode = "docker"
                add_log("[INFO] AUTO 命中: 使用 Docker burn_tool 模式")
            else:
                selected_mode = "aurora"
                add_log("[WARN] AUTO 降级: 使用 Aurora GUI 模式")

        if selected_mode == "docker":
            err = check_container_ready()
            if err:
                add_log(f"[ERROR] {err}")
                with STATE_LOCK:
                    STATE.running = False
                    STATE.finished_at = _now_ts()
                    STATE.success = False
                    STATE.return_code = 1
                return

            container_fw = host_to_container_path(firmware_path)
            if not container_fw:
                add_log("[ERROR] 固件路径不在当前仓库内，无法映射到 /app")
                with STATE_LOCK:
                    STATE.running = False
                    STATE.finished_at = _now_ts()
                    STATE.success = False
                    STATE.return_code = 2
                return

            with STATE_LOCK:
                STATE.firmware_container_path = container_fw

            shell_cmd = (
                f"cd {SDK_CONTAINER_DIR} && "
                f"{BURN_TOOL_CONTAINER_PATH} -f {shlex.quote(container_fw)}"
            )
            cmd = ["docker", "exec", "A1_Builder", "bash", "-lc", shell_cmd]
            add_log(f"[CMD] {' '.join(cmd)}")

            try:
                proc = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
                with STATE_LOCK:
                    STATE.process = proc

                assert proc.stdout is not None
                for line in proc.stdout:
                    add_log(line.rstrip("\n"))

                rc = proc.wait()
                with STATE_LOCK:
                    STATE.return_code = rc
                    STATE.success = (rc == 0)
                    STATE.running = False
                    STATE.finished_at = _now_ts()
                    STATE.process = None

                if rc == 0:
                    add_log("[OK] 烧录完成")
                else:
                    add_log(f"[ERROR] 烧录失败，退出码: {rc}")
            except Exception as exc:
                add_log(f"[ERROR] 执行失败: {exc}")
                with STATE_LOCK:
                    STATE.running = False
                    STATE.finished_at = _now_ts()
                    STATE.success = False
                    STATE.return_code = -1
                    STATE.process = None
            return

        # Aurora GUI fallback
        if not AURORA_EXE.exists():
            add_log(f"[ERROR] 未找到 Aurora 可执行文件: {AURORA_EXE}")
            with STATE_LOCK:
                STATE.running = False
                STATE.finished_at = _now_ts()
                STATE.success = False
                STATE.return_code = 3
            return

        try:
            subprocess.Popen([str(AURORA_EXE)], cwd=str(AURORA_EXE.parent))
            add_log("[INFO] 已启动 Aurora GUI。")
            add_log("[INFO] 请在 CH347/A1BurnTool 中选择同一固件并点击烧录。")
            with STATE_LOCK:
                STATE.running = False
                STATE.finished_at = _now_ts()
                STATE.success = True
                STATE.return_code = 0
        except Exception as exc:
            add_log(f"[ERROR] 启动 Aurora 失败: {exc}")
            with STATE_LOCK:
                STATE.running = False
                STATE.finished_at = _now_ts()
                STATE.success = False
                STATE.return_code = 4

    t = threading.Thread(target=_runner, daemon=True)
    t.start()


@app.route("/")
def index():
    return render_template("flash_ui.html")


@app.route("/api/firmware", methods=["GET"])
def api_firmware():
    return jsonify({"ok": True, "items": list_firmware_candidates()})


@app.route("/api/status", methods=["GET"])
def api_status():
    with STATE_LOCK:
        payload = {
            "ok": True,
            "running": STATE.running,
            "mode": STATE.mode,
            "firmware": STATE.firmware_host_path,
            "containerFirmware": STATE.firmware_container_path,
            "startedAt": STATE.started_at,
            "finishedAt": STATE.finished_at,
            "success": STATE.success,
            "returnCode": STATE.return_code,
            "logs": STATE.logs[-250:],
        }
    return jsonify(payload)


@app.route("/api/flash", methods=["POST"])
def api_flash():
    data = request.get_json(force=True, silent=True) or {}
    firmware = data.get("firmware", "").strip()
    mode = (data.get("mode", "auto").strip() or "auto").lower()

    if mode not in {"auto", "docker", "aurora"}:
        return jsonify({"ok": False, "error": "mode 仅支持 auto/docker/aurora"}), 400

    if not firmware:
        return jsonify({"ok": False, "error": "请选择固件文件"}), 400

    fw = Path(firmware)
    if not fw.exists() or not fw.is_file():
        return jsonify({"ok": False, "error": "固件路径不存在"}), 400

    with STATE_LOCK:
        if STATE.running:
            return jsonify({"ok": False, "error": "已有烧录任务在执行中"}), 409

    start_flash_task(fw, mode)
    return jsonify({"ok": True, "message": "烧录任务已启动"})


@app.route("/api/stop", methods=["POST"])
def api_stop():
    with STATE_LOCK:
        proc = STATE.process

    if proc is None:
        return jsonify({"ok": False, "error": "当前没有可停止的后台进程"}), 400

    try:
        proc.terminate()
        add_log("[WARN] 已请求停止当前烧录任务")
        return jsonify({"ok": True})
    except Exception as exc:
        return jsonify({"ok": False, "error": str(exc)}), 500


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Aurora Flash Web Tool")
    parser.add_argument("--port", type=int, default=5055, help="Web 端口")
    parser.add_argument("--mode", type=str, default="auto", choices=["auto", "docker", "aurora"], help="默认烧录模式")
    parser.add_argument("--firmware", type=str, default="", help="启动时预选固件路径")
    return parser.parse_args()


def bootstrap_state(default_mode: str, default_firmware: str) -> None:
    with STATE_LOCK:
        STATE.mode = default_mode
        if default_firmware:
            STATE.firmware_host_path = default_firmware


def main() -> None:
    args = parse_args()
    bootstrap_state(args.mode, args.firmware)

    print("[INFO] Aurora Flash Web Tool 已启动")
    print(f"[INFO] 打开: http://localhost:{args.port}")
    app.run(host="0.0.0.0", port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
