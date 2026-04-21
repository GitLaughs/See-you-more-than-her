#!/usr/bin/env python3
"""A1 Companion — A1 开发板专用摄像头 / OSD / 底盘伴侣入口。

这个入口复用 aurora_companion.py 的摄像头、YOLOv8 检测和底盘控制能力，
但默认绑定 A1 专用的 head6 模型与真实类别名，并把默认端口分离出来，
方便和现有 Aurora 伴侣工具并行调试。
"""

import os
from pathlib import Path
import sys


def _resolve_local_port(argv):
    for index, argument in enumerate(argv):
        if argument.startswith("--port="):
            try:
                return int(argument.split("=", 1)[1])
            except ValueError:
                break
        if argument == "--port" and index + 1 < len(argv):
            try:
                return int(argv[index + 1])
            except ValueError:
                break
    return 5803


os.environ.setdefault("A1_COMPANION_URL", f"http://127.0.0.1:{_resolve_local_port(sys.argv[1:])}")

import aurora_companion as base

base.set_detect_model_path(Path(__file__).parent.parent.parent / "models" / "best_a1_formal_head6.onnx", persist=False)
base._CLASS_NAMES = {0: "person", 1: "forward", 2: "stop", 3: "obstacle_box"}

if not any(arg == "--port" or arg.startswith("--port=") for arg in sys.argv[1:]):
    sys.argv.extend(["--port", "5803"])


if __name__ == "__main__":
    base.main()
