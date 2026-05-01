#!/usr/bin/env python3
from __future__ import annotations

import argparse
import random
import shutil
from pathlib import Path

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}


def collect_pairs(raw_images: Path, raw_labels: Path) -> list[tuple[Path, Path]]:
    pairs = []
    for image_path in sorted(raw_images.rglob("*")):
        if not image_path.is_file() or image_path.suffix.lower() not in IMAGE_EXTENSIONS:
            continue
        label_path = raw_labels / f"{image_path.stem}.txt"
        if not label_path.exists():
            raise RuntimeError(f"missing label for image: {image_path}")
        pairs.append((image_path, label_path))
    if not pairs:
        raise RuntimeError(f"no images found under: {raw_images}")
    return pairs


def reset_split_dirs(dataset_root: Path) -> None:
    for kind in ["images", "labels"]:
        for split in ["train", "val", "test"]:
            path = dataset_root / kind / split
            if path.exists():
                shutil.rmtree(path)
            path.mkdir(parents=True, exist_ok=True)


def split_pairs(pairs: list[tuple[Path, Path]], train: float, val: float):
    if train <= 0 or val < 0 or train + val >= 1:
        raise RuntimeError("expected train > 0, val >= 0, and train + val < 1")
    train_count = int(len(pairs) * train)
    val_count = int(len(pairs) * val)
    train_items = pairs[:train_count]
    val_items = pairs[train_count : train_count + val_count]
    test_items = pairs[train_count + val_count :]
    if not train_items or not val_items or not test_items:
        raise RuntimeError(
            f"split too small: train={len(train_items)} val={len(val_items)} test={len(test_items)}"
        )
    return {"train": train_items, "val": val_items, "test": test_items}


def copy_split(dataset_root: Path, splits: dict[str, list[tuple[Path, Path]]]) -> None:
    for split, items in splits.items():
        for image_path, label_path in items:
            shutil.copy2(image_path, dataset_root / "images" / split / image_path.name)
            shutil.copy2(label_path, dataset_root / "labels" / split / label_path.name)
        print(f"{split}: {len(items)}")


def parse_args():
    parser = argparse.ArgumentParser(description="Split tools/yolo raw dataset into train/val/test")
    parser.add_argument("--dataset-root", default="tools/yolo")
    parser.add_argument("--train", type=float, default=0.8)
    parser.add_argument("--val", type=float, default=0.1)
    parser.add_argument("--seed", type=int, default=42)
    return parser.parse_args()


def main():
    args = parse_args()
    dataset_root = Path(args.dataset_root)
    pairs = collect_pairs(dataset_root / "raw" / "images", dataset_root / "raw" / "labels")
    rng = random.Random(args.seed)
    rng.shuffle(pairs)
    reset_split_dirs(dataset_root)
    splits = split_pairs(pairs, args.train, args.val)
    copy_split(dataset_root, splits)


if __name__ == "__main__":
    main()
