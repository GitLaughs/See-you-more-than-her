#!/usr/bin/env python3
"""PC Tool — direct STM32 serial and ROS debug."""

import argparse
from pathlib import Path

from flask import Flask, jsonify, render_template

from pc_chassis import chassis_bp
from pc_ros import ros_bp

app = Flask(__name__, template_folder="templates")
app.register_blueprint(chassis_bp)
app.register_blueprint(ros_bp)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/status")
def status():
    return jsonify({"success": True, "tool": "PC", "purpose": "电脑直连 STM32 / ROS 调试"})


def main() -> None:
    parser = argparse.ArgumentParser(description="PC direct STM32 and ROS debug tool")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6202)
    args = parser.parse_args()
    print(f"[PC] Web listening on http://{args.host}:{args.port}")
    print("[PC] Direct STM32 serial routes loaded")
    print("[PC] ROS debug routes loaded")
    app.run(host=args.host, port=args.port, threaded=True, debug=False)


if __name__ == "__main__":
    main()
