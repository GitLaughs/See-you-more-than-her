# 4-State MobileNet Sigmoid Training Script Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `third_party/ultralytics/train.py` that trains a 4-output MobileNetV1 sigmoid classifier from `data/yolov8_dataset` YOLO labels.

**Architecture:** The script is standalone and does not depend on Ultralytics trainer internals. It reads YOLO images/labels from train/val splits, converts class ids 0/1/2/3 to multi-label targets `[person, stop, forward, obstacle]`, treats empty or missing label files as negative `[0,0,0,0]`, trains a timm MobileNetV1 backbone with full-map convolution head, and saves `last.pt` plus best `best.pt` by `(positive_top1 + negative_recall) / 2`.

**Tech Stack:** Python, PyTorch, torchvision transforms, timm, PyYAML, PIL.

---

### Task 1: Create standalone training script

**Files:**
- Create: `third_party/ultralytics/train.py`

- [ ] **Step 1: Create `MobileNetSigmoidClassifier` and CLI args**

Add imports, constants, model class, and `parse_args()`.

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import random
from pathlib import Path

import timm
import torch
import yaml
from PIL import Image
from torch import nn
from torch.utils.data import DataLoader, Dataset
from torchvision import transforms

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}
MODEL_IMAGE_SIZE = 320
DEFAULT_OUTPUT_CLASSES = ["person", "stop", "forward", "obstacle"]


class MobileNetSigmoidClassifier(nn.Module):
    def __init__(self, output_classes: list[str], pretrained: bool, image_size: int, head_hidden_dim: int, dropout: float):
        super().__init__()
        self.backbone = timm.create_model("mobilenetv1_100", pretrained=pretrained, num_classes=0, global_pool="")
        feature_channels, feature_height, feature_width = self._infer_feature_shape(image_size)
        self.head = nn.Sequential(
            nn.Conv2d(feature_channels, head_hidden_dim, kernel_size=1, bias=False),
            nn.BatchNorm2d(head_hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout2d(p=dropout),
            nn.Conv2d(head_hidden_dim, len(output_classes), kernel_size=(feature_height, feature_width), bias=True),
            nn.Flatten(1),
        )

    def _infer_feature_shape(self, image_size: int):
        was_training = self.backbone.training
        self.backbone.eval()
        with torch.no_grad():
            dummy = torch.zeros(1, 3, image_size, image_size)
            features = self.backbone(dummy)
        if was_training:
            self.backbone.train()
        return features.shape[1], features.shape[2], features.shape[3]

    def forward(self, x):
        return self.head(self.backbone(x))


def parse_args():
    parser = argparse.ArgumentParser(description="Train 4-state MobileNetV1 sigmoid classifier from YOLO labels")
    parser.add_argument("--data", default="data/yolov8_dataset/dataset.yaml", help="YOLO dataset yaml")
    parser.add_argument("--output_dir", default="runs/mobilenet_sigmoid_4state", help="Checkpoint output directory")
    parser.add_argument("--classes", nargs="+", default=DEFAULT_OUTPUT_CLASSES, help="Output class names in order")
    parser.add_argument("--batch_size", type=int, default=64)
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight_decay", type=float, default=1e-4)
    parser.add_argument("--num_workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--threshold", type=float, default=0.5)
    parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--head_hidden_dim", type=int, default=256)
    parser.add_argument("--dropout", type=float, default=0.2)
    return parser.parse_args()
```

- [ ] **Step 2: Add YOLO dataset loading**

Add dataset config parsing, label parsing, and dataset class.

```python
def seed_everything(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def load_dataset_config(data_yaml: Path):
    if not data_yaml.exists():
        raise RuntimeError(f"dataset yaml not found: {data_yaml}")
    config = yaml.safe_load(data_yaml.read_text(encoding="utf-8"))
    dataset_root = Path(config["path"])
    if not dataset_root.is_absolute():
        dataset_root = data_yaml.parent / dataset_root if not dataset_root.exists() else dataset_root
    names = config.get("names", {})
    id_to_name = {int(class_id): name for class_id, name in names.items()}
    return dataset_root, config, id_to_name


def resolve_split_dir(dataset_root: Path, split_value: str) -> Path:
    split_dir = Path(split_value)
    return split_dir if split_dir.is_absolute() else dataset_root / split_dir


def label_path_for_image(image_path: Path, images_dir: Path, labels_dir: Path) -> Path:
    relative = image_path.relative_to(images_dir)
    return labels_dir / relative.with_suffix(".txt")


def target_from_label_file(label_path: Path, class_to_index: dict[int, int], output_count: int) -> torch.Tensor:
    target = torch.zeros(output_count, dtype=torch.float32)
    if not label_path.exists():
        return target
    for raw_line in label_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        class_id = int(float(line.split()[0]))
        if class_id in class_to_index:
            target[class_to_index[class_id]] = 1.0
    return target


def list_yolo_samples(dataset_root: Path, config: dict, split: str, id_to_name: dict[int, str], output_classes: list[str]):
    images_dir = resolve_split_dir(dataset_root, config[split])
    labels_dir = dataset_root / "labels" / Path(config[split]).name
    if not images_dir.exists():
        raise RuntimeError(f"{split} images directory not found: {images_dir}")
    class_to_index = {class_id: output_classes.index(name) for class_id, name in id_to_name.items() if name in output_classes}
    samples = []
    for image_path in sorted(images_dir.rglob("*")):
        if image_path.is_file() and image_path.suffix.lower() in IMAGE_EXTENSIONS:
            label_path = label_path_for_image(image_path, images_dir, labels_dir)
            samples.append((image_path, target_from_label_file(label_path, class_to_index, len(output_classes))))
    if not samples:
        raise RuntimeError(f"no images found for split: {split}")
    return samples


class YoloStateDataset(Dataset):
    def __init__(self, samples, transform):
        self.samples = samples
        self.transform = transform

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, index):
        image_path, target = self.samples[index]
        image = Image.open(image_path).convert("RGB")
        return self.transform(image), target, str(image_path)
```

- [ ] **Step 3: Add transforms and metrics**

Add requested augmentation and validation metrics.

```python
def build_transforms(image_size: int):
    train_transform = transforms.Compose(
        [
            transforms.Resize((352, 352)),
            transforms.RandomResizedCrop(image_size, scale=(0.75, 1.0), ratio=(0.9, 1.1)),
            transforms.RandomHorizontalFlip(p=0.5),
            transforms.RandomRotation(degrees=18),
            transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.15, hue=0.05),
            transforms.RandomPerspective(distortion_scale=0.15, p=0.2),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
            transforms.RandomErasing(p=0.2, scale=(0.02, 0.12), ratio=(0.3, 3.0)),
        ]
    )
    val_transform = transforms.Compose(
        [
            transforms.Resize((image_size, image_size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
        ]
    )
    return train_transform, val_transform


def exact_match(probs: torch.Tensor, targets: torch.Tensor, threshold: float):
    preds = (probs >= threshold).float()
    return (preds == targets).all(dim=1).float().mean().item()


def negative_recall(probs: torch.Tensor, targets: torch.Tensor, threshold: float):
    negative_mask = targets.sum(dim=1) == 0
    if negative_mask.sum() == 0:
        return 0.0
    preds = (probs[negative_mask] >= threshold).float()
    return (preds.sum(dim=1) == 0).float().mean().item()


def positive_top1_accuracy(probs: torch.Tensor, targets: torch.Tensor):
    positive_mask = targets.sum(dim=1) > 0
    if positive_mask.sum() == 0:
        return 0.0
    pred_idx = probs[positive_mask].argmax(dim=1)
    target_idx = targets[positive_mask].argmax(dim=1)
    return (pred_idx == target_idx).float().mean().item()
```

- [ ] **Step 4: Add training loop and checkpointing**

Add epoch runner, main function, optimizer, scheduler, and checkpoint saving.

```python
def run_epoch(model, loader, criterion, optimizer, device, threshold):
    is_train = optimizer is not None
    model.train(is_train)
    total_loss = 0.0
    total_items = 0
    all_probs = []
    all_targets = []
    for images, targets, _ in loader:
        images = images.to(device, non_blocking=True)
        targets = targets.to(device, non_blocking=True)
        with torch.set_grad_enabled(is_train):
            logits = model(images)
            loss = criterion(logits, targets)
            if is_train:
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                optimizer.step()
        batch_size = images.size(0)
        total_loss += loss.item() * batch_size
        total_items += batch_size
        all_probs.append(torch.sigmoid(logits).detach().cpu())
        all_targets.append(targets.detach().cpu())
    probs = torch.cat(all_probs, dim=0)
    targets = torch.cat(all_targets, dim=0)
    return {
        "loss": total_loss / max(total_items, 1),
        "exact_match": exact_match(probs, targets, threshold),
        "positive_top1": positive_top1_accuracy(probs, targets),
        "negative_recall": negative_recall(probs, targets, threshold),
    }


def main():
    args = parse_args()
    seed_everything(args.seed)
    data_yaml = Path(args.data)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    dataset_root, config, id_to_name = load_dataset_config(data_yaml)
    train_samples = list_yolo_samples(dataset_root, config, "train", id_to_name, args.classes)
    val_samples = list_yolo_samples(dataset_root, config, "val", id_to_name, args.classes)
    train_transform, val_transform = build_transforms(MODEL_IMAGE_SIZE)

    train_loader = DataLoader(YoloStateDataset(train_samples, train_transform), batch_size=args.batch_size, shuffle=True, num_workers=args.num_workers, pin_memory=True)
    val_loader = DataLoader(YoloStateDataset(val_samples, val_transform), batch_size=args.batch_size, shuffle=False, num_workers=args.num_workers, pin_memory=True)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = MobileNetSigmoidClassifier(args.classes, args.pretrained, MODEL_IMAGE_SIZE, args.head_hidden_dim, args.dropout).to(device)
    criterion = nn.BCEWithLogitsLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    metadata = {
        "model_name": "mobilenetv1_100",
        "output_classes": args.classes,
        "image_size": MODEL_IMAGE_SIZE,
        "dataset_yaml": str(data_yaml),
        "dataset_root": str(dataset_root),
        "threshold": args.threshold,
        "head_hidden_dim": args.head_hidden_dim,
        "dropout": args.dropout,
        "train_count": len(train_loader.dataset),
        "val_count": len(val_loader.dataset),
    }
    (output_dir / "metadata.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")

    best_metric = -1.0
    print(f"Device      : {device}")
    print(f"Classes     : {args.classes}")
    print(f"Image size  : {MODEL_IMAGE_SIZE}")
    print(f"Output dir  : {output_dir}")
    for epoch in range(1, args.epochs + 1):
        train_metrics = run_epoch(model, train_loader, criterion, optimizer, device, args.threshold)
        val_metrics = run_epoch(model, val_loader, criterion, None, device, args.threshold)
        scheduler.step()
        checkpoint = {
            "model_state_dict": model.state_dict(),
            "epoch": epoch,
            "image_size": MODEL_IMAGE_SIZE,
            "output_classes": args.classes,
            "threshold": args.threshold,
            "head_hidden_dim": args.head_hidden_dim,
            "dropout": args.dropout,
        }
        torch.save(checkpoint, output_dir / "last.pt")
        score = (val_metrics["positive_top1"] + val_metrics["negative_recall"]) / 2.0
        if score > best_metric:
            best_metric = score
            torch.save(checkpoint, output_dir / "best.pt")
        print(
            f"Epoch {epoch:03d}/{args.epochs:03d} "
            f"train_loss={train_metrics['loss']:.4f} "
            f"val_loss={val_metrics['loss']:.4f} "
            f"val_exact={val_metrics['exact_match']:.4f} "
            f"val_pos_top1={val_metrics['positive_top1']:.4f} "
            f"val_neg_recall={val_metrics['negative_recall']:.4f} "
            f"score={score:.4f}"
        )
    print(f"Best checkpoint: {output_dir / 'best.pt'}")
    print(f"Last checkpoint: {output_dir / 'last.pt'}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Run syntax verification**

Run:

```bash
python -m py_compile third_party/ultralytics/train.py
```

Expected: command exits 0 with no output.

- [ ] **Step 6: Run help smoke test**

Run:

```bash
python third_party/ultralytics/train.py --help
```

Expected: usage text includes `--data`, `--classes`, `--threshold`, `--head_hidden_dim`, and `--dropout`.

- [ ] **Step 7: Commit only if user asks**

Do not commit automatically. If user explicitly asks for commit, stage only:

```bash
git add third_party/ultralytics/train.py docs/superpowers/plans/2026-04-30-ultralytics-4state-train-script.md
git commit -m "feat: add four-state MobileNet training script"
```

## Self-review

- Spec coverage: 4-output sigmoid, YOLO dataset, empty negative samples, requested architecture, loss, optimizer, scheduler, augmentations, validation metrics, and best checkpoint rule are covered.
- Placeholder scan: no TBD/TODO/implement-later placeholders.
- Type consistency: `output_classes`, `class_to_index`, target tensors, and metric functions use consistent 4-output tensors.
