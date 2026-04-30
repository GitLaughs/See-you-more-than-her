#!/usr/bin/env python3
"""PC Tool — direct STM32 serial debug."""

import argparse

from flask import Flask, jsonify, render_template

from pc_chassis import chassis_bp

app = Flask(__name__, template_folder="templates")
app.register_blueprint(chassis_bp)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/status")
def status():
    return jsonify({"success": True, "tool": "PC", "purpose": "电脑直连 STM32 调试"})


def main() -> None:
    parser = argparse.ArgumentParser(description="PC direct STM32 serial debug tool")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6202)
    args = parser.parse_args()
    print(f"[PC] Web listening on http://{args.host}:{args.port}")
    print("[PC] Direct STM32 serial routes loaded")
    app.run(host=args.host, port=args.port, threaded=True, debug=False)


if __name__ == "__main__":
    main()
