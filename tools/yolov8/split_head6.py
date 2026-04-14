#!/usr/bin/env python3
"""
YOLOv8 ONNX 模型裁剪工具 — 移除 Detect Head 后的后处理部分

原始模型: models/best.onnx
  - 输入: images [1, 3, 640, 640]
  - 输出: output0 [1, 8, 8400]  (4 bbox + 4 classes, 含完整后处理)

裁剪后模型: models/best_head6.onnx
  - 输入: images [1, 3, 640, 640]
  - 输出: 6 个 Detect Head 原始输出，移除 DFL decode / bbox decode / stride 缩放

部署预处理说明:
  A1 开发板输入流为 1280×720 灰度图，预处理流程:
    1280×720 → resize 640×360 → letterbox 补零 140px(上)/140px(下) → 640×640 → 归一化 [0,1]
  补零边距: pad_top = pad_bottom = (640 - 360) / 2 = 140 px

用法:
    python split_head6.py
"""

import onnx
from onnx.utils import extract_model


INPUT_MODEL = "models/best.onnx"
OUTPUT_MODEL = "models/best_head6.onnx"

# 输入节点
INPUT_NAMES = ["images"]

# 6 个 Detect Head 输出节点 (cv3=分类, cv2=回归, 3 个 FPN 尺度)
#   对于 640×640 输入:
#     scale 0 → 80×80 (stride=8)
#     scale 1 → 40×40 (stride=16)
#     scale 2 → 20×20 (stride=32)
OUTPUT_NAMES = [
    "/model.22/cv3.0/cv3.0.2/Conv_output_0",  # cls  [1, num_cls, 80, 80]
    "/model.22/cv3.1/cv3.1.2/Conv_output_0",  # cls  [1, num_cls, 40, 40]
    "/model.22/cv3.2/cv3.2.2/Conv_output_0",  # cls  [1, num_cls, 20, 20]
    "/model.22/cv2.0/cv2.0.2/Conv_output_0",  # reg  [1, 64,      80, 80]
    "/model.22/cv2.1/cv2.1.2/Conv_output_0",  # reg  [1, 64,      40, 40]
    "/model.22/cv2.2/cv2.2.2/Conv_output_0",  # reg  [1, 64,      20, 20]
]


def main():
    print(f"[split_head6] 输入模型: {INPUT_MODEL}")

    model = onnx.load(INPUT_MODEL)
    inp = model.graph.input[0]
    shape = [
        d.dim_value if d.dim_value > 0 else d.dim_param
        for d in inp.type.tensor_type.shape.dim
    ]
    print(f"[split_head6] 原始输入形状: {shape}")

    extract_model(
        INPUT_MODEL,
        OUTPUT_MODEL,
        input_names=INPUT_NAMES,
        output_names=OUTPUT_NAMES,
    )

    # 验证输出
    out_model = onnx.load(OUTPUT_MODEL)
    print(f"[split_head6] 裁剪完成: {OUTPUT_MODEL}")
    print("[split_head6] 输出节点:")
    for out in out_model.graph.output:
        shape = [
            d.dim_value if d.dim_value > 0 else d.dim_param
            for d in out.type.tensor_type.shape.dim
        ]
        print(f"  {out.name}: {shape}")

    print()
    print("部署预处理配置:")
    print("  传感器输出: 1280×720 灰度图")
    print("  resize  →  640×360 (保持 16:9 宽高比)")
    print("  letterbox → 640×640 (上下各补零 140px)")
    print("  模型输入:   640×640")


if __name__ == "__main__":
    main()
