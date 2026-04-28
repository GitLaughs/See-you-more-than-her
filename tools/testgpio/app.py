from typing import Optional
from pathlib import Path

from flask import Flask, jsonify, render_template, request

from testgpio.runner import GpioRunner


def create_app(config: Optional[dict] = None) -> Flask:
    app = Flask(__name__, template_folder="templates")
    app.config.setdefault("LOG_DIR", str(Path(__file__).resolve().parent / "logs"))
    app.config.setdefault("A1_SSH_HOST", "")
    app.config.setdefault("A1_REMOTE_WORKDIR", "/root/ssne_ai_demo")
    app.config.setdefault("GPIO_SPAWN_PROCESS", None)
    if config:
        app.config.update(config)

    runner = GpioRunner(
        Path(app.config["LOG_DIR"]),
        ssh_host=str(app.config.get("A1_SSH_HOST") or ""),
        remote_workdir=str(app.config.get("A1_REMOTE_WORKDIR") or ""),
        spawn_process=app.config.get("GPIO_SPAWN_PROCESS"),
    )
    app.config["GPIO_RUNNER"] = runner

    @app.get("/")
    def index():
        return render_template("index.html")

    @app.get("/api/gpio/config")
    def gpio_config_get():
        return jsonify(runner.target_config())

    @app.post("/api/gpio/config")
    def gpio_config_set():
        payload = request.get_json(silent=True) or {}
        runner.update_target(
            ssh_host=str(payload.get("ssh_host") or ""),
            remote_workdir=str(payload.get("remote_workdir") or ""),
        )
        return jsonify(runner.target_config())

    @app.get("/api/gpio/status")
    def gpio_status():
        run = runner.current_run()
        if run and run.get("log_path"):
            log_path = Path(str(run["log_path"]))
            if log_path.exists():
                run["log_excerpt"] = log_path.read_text(encoding="utf-8")[-8000:]
            else:
                run["log_excerpt"] = ""
        return jsonify({"run": run})

    @app.post("/api/gpio/start")
    def gpio_start():
        payload = request.get_json(silent=True) or {}
        run = runner.start_run(
            mode=str(payload.get("mode") or "tx"),
            pin=int(payload.get("pin") or 8),
            period_ms=int(payload.get("period_ms") or 500),
            duration_s=int(payload.get("duration_s") or 10),
        )
        return jsonify({"ok": True, "run": run}), 202

    return app
