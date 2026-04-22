#!/usr/bin/env python3
"""Split YOLO dataset from raw/ into train/val/test with a fixed seed."""

import argparse
import random
import shutil
from pathlib import Path

IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def reset_dir(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def collect_images(raw_images: Path) -> list[Path]:
    files = [p for p in raw_images.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTS]
    files.sort()
    return files


def copy_pair(img_path: Path, raw_labels: Path, img_out: Path, lbl_out: Path) -> None:
    shutil.copy2(img_path, img_out / img_path.name)
    label_src = raw_labels / f"{img_path.stem}.txt"
    if not label_src.exists():
        raise FileNotFoundError(f"Missing label for {img_path.name}: {label_src}")
    shutil.copy2(label_src, lbl_out / label_src.name)


def split_counts(total: int, train: float, val: float) -> tuple[int, int, int]:
    n_train = int(total * train)
    n_val = int(total * val)
    n_test = total - n_train - n_val
    return n_train, n_val, n_test


def main() -> None:
    parser = argparse.ArgumentParser(description="Split YOLO raw dataset into train/val/test")
    parser.add_argument("--dataset-root", default="data/yolov8_dataset", help="Dataset root path")
    parser.add_argument("--train", type=float, default=0.8, help="Train ratio")
    parser.add_argument("--val", type=float, default=0.1, help="Val ratio")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    if args.train <= 0 or args.val <= 0 or args.train + args.val >= 1:
        raise ValueError("Ratios must satisfy: train > 0, val > 0, train + val < 1")

    root = Path(args.dataset_root)
    raw_images = root / "raw" / "images"
    raw_labels = root / "raw" / "labels"

    if not raw_images.exists() or not raw_labels.exists():
        raise FileNotFoundError("raw/images or raw/labels not found")

    images = collect_images(raw_images)
    if not images:
        raise ValueError("No images found in raw/images")

    random.seed(args.seed)
    random.shuffle(images)

    n_train, n_val, _ = split_counts(len(images), args.train, args.val)
    train_set = images[:n_train]
    val_set = images[n_train : n_train + n_val]
    test_set = images[n_train + n_val :]

    out_dirs = {
        "train": (root / "images" / "train", root / "labels" / "train", train_set),
        "val": (root / "images" / "val", root / "labels" / "val", val_set),
        "test": (root / "images" / "test", root / "labels" / "test", test_set),
    }

    for img_dir, lbl_dir, _ in out_dirs.values():
        reset_dir(img_dir)
        reset_dir(lbl_dir)

    for split, (img_dir, lbl_dir, items) in out_dirs.items():
        for img in items:
            copy_pair(img, raw_labels, img_dir, lbl_dir)
        print(f"{split}: {len(items)}")

    print("Split complete")


if __name__ == "__main__":
    main()
