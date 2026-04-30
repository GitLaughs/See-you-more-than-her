#!/usr/bin/env python3
"""A1 Tool — COM13 -> A1_TEST -> STM32 relay control."""

import argparse

from flask import Flask, jsonify, render_template

from a1_relay import a1_bp
from a1_serial import serial_term_bp

app = Flask(__name__, template_folder="templates")
app.register_blueprint(a1_bp)
app.register_blueprint(serial_term_bp)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/status")
def status():
    return jsonify({"success": True, "tool": "A1", "purpose": "COM13 经 A1_TEST 中继控制 STM32"})


def main() -> None:
    parser = argparse.ArgumentParser(description="A1 relay control tool")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6203)
    args = parser.parse_args()
    print(f"[A1] Web listening on http://{args.host}:{args.port}")
    print("[A1] COM13 / A1_TEST relay routes loaded")
    app.run(host=args.host, port=args.port, threaded=True, debug=False)


if __name__ == "__main__":
    main()
