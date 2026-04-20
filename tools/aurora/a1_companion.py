#!/usr/bin/env python3
"""A1 Companion — A1 开发板专用摄像头 / OSD / 底盘伴侣入口。

这个入口复用 aurora_companion.py 的摄像头、YOLOv8 检测和底盘控制能力，
但默认绑定 A1 专用的 head6 模型与真实类别名，并把默认端口分离出来，
方便和现有 Aurora 伴侣工具并行调试。
"""

from pathlib import Path
import sys

import aurora_companion as base

base._DETECT_MODEL_PATH = Path(__file__).parent.parent.parent / "models" / "best_a1_formal_head6.onnx"
base._CLASS_NAMES = {0: "person", 1: "forward", 2: "stop", 3: "obstacle_box"}

if not any(arg == "--port" or arg.startswith("--port=") for arg in sys.argv[1:]):
    sys.argv.extend(["--port", "5803"])


if __name__ == "__main__":
    base.main()
