#!/usr/bin/env python3
from __future__ import annotations

import argparse
import random
from pathlib import Path

import cv2
import numpy as np

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
MODEL_IMAGE_SIZE = 320
NORM_MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32).reshape(3, 1, 1)
NORM_STD = np.array([0.229, 0.224, 0.225], dtype=np.float32).reshape(3, 1, 1)


def preprocess_image(img_path: Path, image_size: int) -> np.ndarray:
    img_bgr = cv2.imread(str(img_path), cv2.IMREAD_COLOR)
    if img_bgr is None:
        raise RuntimeError(f"Failed to read image: {img_path}")
    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    img_rgb = cv2.resize(img_rgb, (image_size, image_size), interpolation=cv2.INTER_LINEAR)
    img_rgb = img_rgb.astype(np.float32) / 255.0
    img_chw = np.transpose(img_rgb, (2, 0, 1))
    img_chw = (img_chw - NORM_MEAN) / NORM_STD
    img_nchw = np.expand_dims(img_chw, axis=0)
    return img_nchw.astype(np.float32)


def collect_images_from_dataset(dataset_dir: Path) -> list[Path]:
    if not dataset_dir.exists():
        raise RuntimeError(f"Dataset directory not found: {dataset_dir}")
    return [
        path
        for path in sorted(dataset_dir.rglob("*"))
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
    ]


def save_tensor_set(
    image_paths: list[Path],
    output_dir: Path,
    image_size: int,
    set_name: str,
    output_format: str,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_lines = []
    for idx, img_path in enumerate(image_paths):
        tensor = preprocess_image(img_path, image_size)
        if output_format == "npy":
            out_path = output_dir / f"{idx:04d}_{img_path.stem}.npy"
            np.save(out_path, tensor)
        else:
            out_path = output_dir / f"{idx:04d}_{img_path.stem}.bin"
            tensor.tofile(out_path)
        manifest_lines.append(
            f"{out_path.name}\t{img_path}\tshape={tuple(tensor.shape)}\tdtype=float32"
        )

    manifest_path = output_dir / f"{set_name}_manifest.txt"
    manifest_path.write_text("\n".join(manifest_lines), encoding="utf-8")
    print(f"=== {set_name.capitalize()} set generated ===")
    print(f"  Image count : {len(image_paths)}")
    print(f"  Output dir  : {output_dir}")
    print(f"  Tensor shape: (1, 3, {image_size}, {image_size})")
    print("  Tensor dtype: float32")
    print("  Layout      : NCHW")
    print("  Normalize   : mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]")
    print(f"  Format      : {output_format}")
    print(f"  Manifest    : {manifest_path}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate calibration and evaluation tensors with MobileNet sigmoid preprocessing"
    )
    parser.add_argument("--dataset_dir", required=True, help="Dataset root containing class subdirectories")
    parser.add_argument("--output_dir", required=True, help="Root dir for calibrate/evaluate outputs")
    parser.add_argument("--image_size", type=int, default=MODEL_IMAGE_SIZE)
    parser.add_argument("--output_format", default="npy", choices=["npy", "bin"])
    parser.add_argument("--cal_num", type=int, default=50)
    parser.add_argument("--eval_num", type=int, default=20)
    parser.add_argument("--seed", type=int, default=42)
    return parser.parse_args()


def main():
    args = parse_args()
    if args.image_size != MODEL_IMAGE_SIZE:
        raise RuntimeError(
            f"Current model preprocessing is fixed to {MODEL_IMAGE_SIZE}, got {args.image_size}"
        )
    all_images = collect_images_from_dataset(Path(args.dataset_dir))
    if not all_images:
        raise RuntimeError(f"No images found under: {args.dataset_dir}")
    total = len(all_images)
    if total < args.cal_num + args.eval_num:
        raise RuntimeError(
            f"Not enough images ({total}) for cal({args.cal_num}) + eval({args.eval_num})"
        )

    print(f"Total images found: {total}")
    rng = random.Random(args.seed)
    rng.shuffle(all_images)
    output_dir = Path(args.output_dir)
    save_tensor_set(
        image_paths=all_images[: args.cal_num],
        output_dir=output_dir / "calibrate_datasets",
        image_size=MODEL_IMAGE_SIZE,
        set_name="calibration",
        output_format=args.output_format,
    )
    save_tensor_set(
        image_paths=all_images[args.cal_num : args.cal_num + args.eval_num],
        output_dir=output_dir / "evaluate_datasets",
        image_size=MODEL_IMAGE_SIZE,
        set_name="evaluation",
        output_format=args.output_format,
    )


if __name__ == "__main__":
    main()
