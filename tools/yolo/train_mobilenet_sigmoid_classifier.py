#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import random
from collections import defaultdict
from pathlib import Path

import timm
import torch
from PIL import Image
from torch import nn
from torch.utils.data import DataLoader, Dataset
from torchvision import transforms

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}
MODEL_IMAGE_SIZE = 320


class MobileNetSigmoidClassifier(nn.Module):
    def __init__(
        self,
        output_classes: list[str],
        pretrained: bool,
        image_size: int,
        head_hidden_dim: int,
        dropout: float,
    ):
        super().__init__()
        self.backbone = timm.create_model(
            "mobilenetv1_100",
            pretrained=pretrained,
            num_classes=0,
            global_pool="",
        )
        feature_channels, feature_height, feature_width = self._infer_feature_shape(image_size)
        self.head = nn.Sequential(
            nn.Conv2d(feature_channels, head_hidden_dim, kernel_size=1, bias=False),
            nn.BatchNorm2d(head_hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout2d(p=dropout),
            nn.Conv2d(
                head_hidden_dim,
                len(output_classes),
                kernel_size=(feature_height, feature_width),
                bias=True,
            ),
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
    parser = argparse.ArgumentParser(
        description="Train MobileNetV1 sigmoid classifier with optional negative class"
    )
    parser.add_argument("--dataset_dir", required=True, help="Dataset root with class subdirectories")
    parser.add_argument(
        "--output_dir",
        default="outputs/mobilenet_sigmoid",
        help="Checkpoint output directory",
    )
    parser.add_argument(
        "--classes",
        nargs="+",
        default=["P", "R", "S"],
        help="Positive output classes in output order",
    )
    parser.add_argument(
        "--negative_class",
        default="N",
        help="Negative class directory; target is all zeros",
    )
    parser.add_argument("--batch_size", type=int, default=64)
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight_decay", type=float, default=1e-4)
    parser.add_argument("--val_ratio", type=float, default=0.2)
    parser.add_argument("--num_workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--threshold", type=float, default=0.5)
    parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--head_hidden_dim", type=int, default=256)
    parser.add_argument("--dropout", type=float, default=0.2)
    return parser.parse_args()


def seed_everything(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def class_targets(output_classes: list[str], negative_class: str) -> dict[str, list[float]]:
    targets = {}
    for index, class_name in enumerate(output_classes):
        target = [0.0] * len(output_classes)
        target[index] = 1.0
        targets[class_name] = target
    targets[negative_class] = [0.0] * len(output_classes)
    return targets


def list_samples(dataset_dir: Path, targets: dict[str, list[float]]):
    samples_by_class = defaultdict(list)
    for class_name, target in targets.items():
        class_dir = dataset_dir / class_name
        if not class_dir.exists():
            continue
        for path in sorted(class_dir.rglob("*")):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                samples_by_class[class_name].append(
                    (path, torch.tensor(target, dtype=torch.float32))
                )
    return samples_by_class


def split_samples(samples_by_class, val_ratio: float, seed: int):
    rng = random.Random(seed)
    train_samples = []
    val_samples = []
    for class_name, items in samples_by_class.items():
        items = items.copy()
        rng.shuffle(items)
        if not items:
            continue
        val_count = int(len(items) * val_ratio)
        if len(items) > 1:
            val_count = max(1, val_count)
            val_count = min(len(items) - 1, val_count)
        else:
            val_count = 0
        val_samples.extend(items[:val_count])
        train_samples.extend(items[val_count:])
        print(
            f"{class_name}: total={len(items)} "
            f"train={len(items[val_count:])} val={len(items[:val_count])}"
        )
    return train_samples, val_samples


class ImageFolderMultiLabelDataset(Dataset):
    def __init__(self, samples, transform):
        self.samples = samples
        self.transform = transform

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, index):
        image_path, target = self.samples[index]
        image = Image.open(image_path).convert("RGB")
        return self.transform(image), target, str(image_path)


def build_transforms(image_size: int):
    train_transform = transforms.Compose(
        [
            transforms.Resize((image_size + 32, image_size + 32)),
            transforms.RandomResizedCrop(image_size, scale=(0.75, 1.0), ratio=(0.9, 1.1)),
            transforms.RandomHorizontalFlip(p=0.5),
            transforms.RandomRotation(degrees=18),
            transforms.ColorJitter(
                brightness=0.2, contrast=0.2, saturation=0.15, hue=0.05
            ),
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
    dataset_dir = Path(args.dataset_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    if not dataset_dir.exists():
        raise RuntimeError(f"dataset directory not found: {dataset_dir}")

    targets = class_targets(args.classes, args.negative_class)
    samples_by_class = list_samples(dataset_dir, targets)
    missing = [class_name for class_name in args.classes if not samples_by_class.get(class_name)]
    if missing:
        raise RuntimeError(f"missing required class data: {missing}")
    if not samples_by_class.get(args.negative_class):
        print(
            f"Warning: negative class {args.negative_class} not found; "
            "training without negative samples."
        )

    train_samples, val_samples = split_samples(samples_by_class, args.val_ratio, args.seed)
    if not train_samples or not val_samples:
        raise RuntimeError("train/val split failed, please check dataset size and val_ratio")

    train_transform, val_transform = build_transforms(MODEL_IMAGE_SIZE)
    train_loader = DataLoader(
        ImageFolderMultiLabelDataset(train_samples, train_transform),
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=True,
    )
    val_loader = DataLoader(
        ImageFolderMultiLabelDataset(val_samples, val_transform),
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=True,
    )
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = MobileNetSigmoidClassifier(
        args.classes,
        args.pretrained,
        MODEL_IMAGE_SIZE,
        args.head_hidden_dim,
        args.dropout,
    ).to(device)
    criterion = nn.BCEWithLogitsLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    metadata = {
        "model_name": "mobilenetv1_100",
        "output_classes": args.classes,
        "negative_class_dir": args.negative_class,
        "image_size": MODEL_IMAGE_SIZE,
        "dataset_dir": str(dataset_dir),
        "head_hidden_dim": args.head_hidden_dim,
        "dropout": args.dropout,
        "threshold": args.threshold,
        "train_count": len(train_loader.dataset),
        "val_count": len(val_loader.dataset),
    }
    (output_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8"
    )

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
            "negative_class_dir": args.negative_class,
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
            f"val_neg_recall={val_metrics['negative_recall']:.4f}"
        )
    print(f"Best checkpoint: {output_dir / 'best.pt'}")
    print(f"Last checkpoint: {output_dir / 'last.pt'}")


if __name__ == "__main__":
    main()
