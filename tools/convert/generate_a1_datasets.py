#!/usr/bin/env python3
"""
A1 校准/评估数据集打包工具

从 YOLO 标注数据集生成 A1 SSNE 推理所需的校准和评估数据：
1. 随机选取 calibrate/evaluate 图像子集
2. 裁剪 ROI 区域并做预处理
3. 打包为 datasets.zip（含 calibrate_datasets/ 和 evaluate_datasets/）
"""

from __future__ import annotations

import argparse
import random
import shutil
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import cv2
import numpy as np
import onnx

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
DEFAULT_CALIBRATE_COUNT = 20
DEFAULT_EVALUATE_COUNT = 10
DEFAULT_SEED = 42
DEFAULT_MEAN = [0]
DEFAULT_STD = [1]


@dataclass(frozen=True)
class InputSpec:
    input_name: str
    input_shape: tuple[int, int, int, int]


def load_single_input_spec(model_path: Path) -> InputSpec:
    model = onnx.load(str(model_path))
    graph_inputs = [item for item in model.graph.input if item.name not in {init.name for init in model.graph.initializer}]
    if len(graph_inputs) != 1:
        raise ValueError("Expected single-input ONNX model")

    tensor = graph_inputs[0]
    dims: list[int] = []
    for dim in tensor.type.tensor_type.shape.dim:
        if not dim.HasField("dim_value") or dim.dim_value <= 0:
            raise ValueError("ONNX input shape is dynamic or invalid")
        dims.append(int(dim.dim_value))

    if len(dims) != 4:
        raise ValueError("Expected 4D NCHW input")
    if dims[1] not in {1, 3}:
        raise ValueError("Only 1-channel or 3-channel inputs are supported")

    return InputSpec(input_name=tensor.name, input_shape=tuple(dims))


def collect_dataset_images(dataset_root: Path) -> list[Path]:
    search_root = dataset_root / "images" if (dataset_root / "images").exists() else dataset_root
    if not any((search_root / split).exists() for split in ("train", "val", "test")):
        raise FileNotFoundError(f"Missing dataset split dirs under: {search_root}")

    paths: list[Path] = []
    for split in ("train", "val", "test"):
        split_dir = search_root / split
        if not split_dir.exists():
            continue
        for path in sorted(split_dir.rglob("*")):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                paths.append(path)
    return paths


def preprocess_image(image_path: Path, input_shape: tuple[int, int, int, int]) -> np.ndarray:
    _, channels, height, width = input_shape
    if channels == 1:
        image = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        if image is None:
            raise RuntimeError(f"Failed to read image: {image_path}")
        image = cv2.resize(image, (width, height), interpolation=cv2.INTER_LINEAR)
        tensor = image.astype(np.float32) / 255.0
        tensor = np.expand_dims(tensor, axis=0)
        tensor = np.expand_dims(tensor, axis=0)
        return tensor.astype(np.float32)
    if channels == 3:
        image_bgr = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
        if image_bgr is None:
            raise RuntimeError(f"Failed to read image: {image_path}")
        image_rgb = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)
        image_rgb = cv2.resize(image_rgb, (width, height), interpolation=cv2.INTER_LINEAR)
        tensor = image_rgb.astype(np.float32) / 255.0
        tensor = np.transpose(tensor, (2, 0, 1))
        tensor = np.expand_dims(tensor, axis=0)
        return tensor.astype(np.float32)
    raise ValueError("Only 1-channel or 3-channel inputs are supported")


def split_samples(paths: list[Path], calibrate_count: int, evaluate_count: int, seed: int) -> tuple[list[Path], list[Path]]:
    total = calibrate_count + evaluate_count
    if len(paths) < total:
        raise ValueError(f"Not enough images: need at least {total}, found {len(paths)}")

    shuffled = list(paths)
    random.Random(seed).shuffle(shuffled)
    return shuffled[:calibrate_count], shuffled[calibrate_count:total]


def save_tensor(path: Path, tensor: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    np.save(path, tensor)


def build_zip(zip_path: Path, dataset_dirs: Iterable[Path]) -> None:
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for dataset_dir in dataset_dirs:
            for npy_path in sorted(dataset_dir.rglob("*.npy")):
                zf.write(npy_path, arcname=str(npy_path.relative_to(dataset_dir.parent)))


def write_config(config_path: Path, input_name: str) -> None:
    config_path.write_text(
        "\n".join(
            [
                f"[calibrate.inputs.{input_name}]",
                f"mean = {DEFAULT_MEAN}",
                f"std = {DEFAULT_STD}",
                "",
            ]
        ),
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate datasets.zip and config.toml for A1 conversion")
    parser.add_argument("--onnx", required=True, help="Path to best.onnx")
    parser.add_argument(
        "--dataset-root",
        default="data/rps_dataset/processed_dataset_regularized",
        help="Path to processed train/val/test dataset",
    )
    parser.add_argument("--output-dir", required=True, help="Output directory")
    parser.add_argument("--calibrate-count", type=int, default=DEFAULT_CALIBRATE_COUNT)
    parser.add_argument("--evaluate-count", type=int, default=DEFAULT_EVALUATE_COUNT)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    model_path = Path(args.onnx)
    dataset_root = Path(args.dataset_root)
    output_dir = Path(args.output_dir)

    spec = load_single_input_spec(model_path)
    image_paths = collect_dataset_images(dataset_root)
    calibrate_paths, evaluate_paths = split_samples(image_paths, args.calibrate_count, args.evaluate_count, args.seed)

    calibrate_dir = output_dir / "calibrate_datasets"
    evaluate_dir = output_dir / "evaluate_datasets"
    if calibrate_dir.exists():
        shutil.rmtree(calibrate_dir)
    if evaluate_dir.exists():
        shutil.rmtree(evaluate_dir)
    calibrate_dir.mkdir(parents=True, exist_ok=True)
    evaluate_dir.mkdir(parents=True, exist_ok=True)

    for idx, image_path in enumerate(calibrate_paths):
        tensor = preprocess_image(image_path, spec.input_shape)
        save_tensor(calibrate_dir / f"{idx:04d}_{image_path.stem}.npy", tensor)

    for idx, image_path in enumerate(evaluate_paths):
        tensor = preprocess_image(image_path, spec.input_shape)
        save_tensor(evaluate_dir / f"{idx:04d}_{image_path.stem}.npy", tensor)

    output_dir.mkdir(parents=True, exist_ok=True)
    build_zip(output_dir / "datasets.zip", [calibrate_dir, evaluate_dir])
    write_config(output_dir / "config.toml", spec.input_name)


if __name__ == "__main__":
    main()
