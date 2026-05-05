import argparse
import copy
import json
import random
from pathlib import Path

import timm
import torch
from PIL import Image
from torch import nn
from torch.utils.data import DataLoader, Dataset
from torchvision import transforms


CLASS_NAMES = ["person", "stop", "forward", "obstacle", "NoTarget"]
CLASS_TO_INDEX = {name: idx for idx, name in enumerate(CLASS_NAMES)}
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}
MODEL_IMAGE_SIZE = 320
NUM_CLASSES = len(CLASS_NAMES)
MEAN = [0.485, 0.456, 0.406]
STD = [0.229, 0.224, 0.225]


class A1Classifier(nn.Module):
    def __init__(self, pretrained: bool, image_size: int, head_hidden_dim: int, dropout: float):
        super().__init__()
        self.backbone = timm.create_model(
            "mobilenetv1_100",
            pretrained=pretrained,
            num_classes=0,
            global_pool="",
        )
        feature_channels = self._infer_feature_channels(image_size)
        self.head = nn.Sequential(
            nn.Conv2d(feature_channels, head_hidden_dim, kernel_size=1, bias=False),
            nn.BatchNorm2d(head_hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout(p=dropout),
            nn.AdaptiveAvgPool2d(1),
            nn.Flatten(1),
            nn.Linear(head_hidden_dim, NUM_CLASSES),
        )

    def _infer_feature_channels(self, image_size: int):
        was_training = self.backbone.training
        self.backbone.eval()
        with torch.no_grad():
            dummy = torch.zeros(1, 3, image_size, image_size)
            features = self.backbone(dummy)
        if was_training:
            self.backbone.train()
        return features.shape[1]

    def forward(self, x):
        return self.head(self.backbone(x))


class ClassImageDataset(Dataset):
    def __init__(self, samples, transform):
        self.samples = samples
        self.transform = transform

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, index):
        image_path, label = self.samples[index]
        image = Image.open(image_path).convert("RGB")
        return self.transform(image), label, str(image_path)


def parse_args():
    parser = argparse.ArgumentParser(description="Train A1 5-class single-label classifier")
    parser.add_argument("--dataset_dir", required=True, help="Dataset root with train/val/test/<class> folders")
    parser.add_argument("--output_dir", default="outputs/a1_5class_mobilenetv1")
    parser.add_argument("--batch_size", type=int, default=64)
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight_decay", type=float, default=1e-4)
    parser.add_argument("--num_workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--head_hidden_dim", type=int, default=256)
    parser.add_argument("--dropout", type=float, default=0.2)
    parser.add_argument("--export_onnx", action="store_true")
    parser.add_argument("--onnx_path", default=None)
    return parser.parse_args()


def seed_everything(seed: int) -> None:
    random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def list_split_samples(split_dir: Path):
    samples_by_class = {}
    for class_name in CLASS_NAMES:
        class_dir = split_dir / class_name
        items = []
        if class_dir.exists():
            for path in sorted(class_dir.rglob("*")):
                if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                    items.append((path, CLASS_TO_INDEX[class_name]))
        samples_by_class[class_name] = items
    return samples_by_class


def flatten_samples(samples_by_class):
    samples = []
    for class_name in CLASS_NAMES:
        samples.extend(samples_by_class.get(class_name, []))
    return samples


def summarize_split(split_name: str, samples_by_class):
    counts = {class_name: len(samples_by_class.get(class_name, [])) for class_name in CLASS_NAMES}
    total = sum(counts.values())
    counts_text = " ".join(f"{name}={counts[name]}" for name in CLASS_NAMES)
    print(f"{split_name}: total={total} {counts_text}")


def verify_required_classes(split_name: str, samples_by_class):
    missing = [class_name for class_name in CLASS_NAMES if not samples_by_class.get(class_name)]
    if missing:
        raise RuntimeError(f"{split_name} split missing class data: {missing}")


def build_transforms(image_size: int):
    train_transform = transforms.Compose(
        [
            transforms.Resize((image_size + 16, image_size + 16)),
            transforms.RandomResizedCrop(image_size, scale=(0.9, 1.0), ratio=(0.95, 1.05)),
            transforms.RandomHorizontalFlip(p=0.5),
            transforms.ColorJitter(brightness=0.12, contrast=0.12, saturation=0.08, hue=0.03),
            transforms.ToTensor(),
            transforms.Normalize(mean=MEAN, std=STD),
        ]
    )
    eval_transform = transforms.Compose(
        [
            transforms.Resize((image_size, image_size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=MEAN, std=STD),
        ]
    )
    return train_transform, eval_transform


def build_model(pretrained: bool, image_size: int, head_hidden_dim: int, dropout: float):
    return A1Classifier(pretrained, image_size, head_hidden_dim, dropout)


def confusion_matrix(preds: torch.Tensor, targets: torch.Tensor, num_classes: int):
    matrix = torch.zeros((num_classes, num_classes), dtype=torch.int64)
    for target, pred in zip(targets.tolist(), preds.tolist()):
        matrix[target, pred] += 1
    return matrix


def accuracy_from_logits(logits: torch.Tensor, targets: torch.Tensor):
    preds = logits.argmax(dim=1)
    return (preds == targets).float().mean().item()


def run_epoch(model, loader, criterion, optimizer, device):
    is_train = optimizer is not None
    model.train(is_train)
    total_loss = 0.0
    total_items = 0
    all_logits = []
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
        all_logits.append(logits.detach().cpu())
        all_targets.append(targets.detach().cpu())

    logits = torch.cat(all_logits, dim=0) if all_logits else torch.empty(0, NUM_CLASSES)
    targets = torch.cat(all_targets, dim=0) if all_targets else torch.empty(0, dtype=torch.long)
    return {
        "loss": total_loss / max(total_items, 1),
        "top1": accuracy_from_logits(logits, targets) if total_items else 0.0,
        "preds": logits.argmax(dim=1) if total_items else torch.empty(0, dtype=torch.long),
        "targets": targets,
    }


def format_confusion_matrix(matrix: torch.Tensor):
    header = "          " + " ".join(f"{name:>9}" for name in CLASS_NAMES)
    rows = [header]
    for idx, name in enumerate(CLASS_NAMES):
        values = " ".join(f"{int(v):9d}" for v in matrix[idx].tolist())
        rows.append(f"{name:>9} {values}")
    return "\n".join(rows)


def save_metadata(output_dir: Path, args, train_count: int, val_count: int, test_count: int):
    metadata = {
        "model_name": "mobilenetv1_100",
        "class_names": CLASS_NAMES,
        "num_classes": NUM_CLASSES,
        "image_size": MODEL_IMAGE_SIZE,
        "dataset_dir": args.dataset_dir,
        "head_hidden_dim": args.head_hidden_dim,
        "dropout": args.dropout,
        "train_count": train_count,
        "val_count": val_count,
        "test_count": test_count,
        "lr": args.lr,
        "weight_decay": args.weight_decay,
        "batch_size": args.batch_size,
        "seed": args.seed,
    }
    (output_dir / "metadata.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")


def export_onnx_model(checkpoint_path: Path, onnx_path: Path, pretrained: bool, image_size: int, head_hidden_dim: int, dropout: float):
    checkpoint = torch.load(checkpoint_path, map_location="cpu")
    model = build_model(pretrained, image_size, head_hidden_dim, dropout)
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()
    dummy_input = torch.randn(1, 3, image_size, image_size)
    onnx_path.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        torch.onnx.export(
            model,
            dummy_input,
            str(onnx_path),
            input_names=["input"],
            output_names=["logits"],
            opset_version=12,
            do_constant_folding=True,
            dynamic_axes=None,
        )


def main():
    args = parse_args()
    seed_everything(args.seed)
    dataset_dir = Path(args.dataset_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not dataset_dir.exists():
        raise RuntimeError(f"dataset directory not found: {dataset_dir}")

    split_dirs = {split_name: dataset_dir / split_name for split_name in ["train", "val", "test"]}
    train_samples_by_class = list_split_samples(split_dirs["train"])
    val_samples_by_class = list_split_samples(split_dirs["val"])
    test_samples_by_class = list_split_samples(split_dirs["test"])

    verify_required_classes("train", train_samples_by_class)
    verify_required_classes("val", val_samples_by_class)
    summarize_split("train", train_samples_by_class)
    summarize_split("val", val_samples_by_class)
    summarize_split("test", test_samples_by_class)

    train_samples = flatten_samples(train_samples_by_class)
    val_samples = flatten_samples(val_samples_by_class)
    test_samples = flatten_samples(test_samples_by_class)
    train_transform, eval_transform = build_transforms(MODEL_IMAGE_SIZE)

    train_loader = DataLoader(ClassImageDataset(train_samples, train_transform), batch_size=args.batch_size, shuffle=True, num_workers=args.num_workers, pin_memory=True)
    val_loader = DataLoader(ClassImageDataset(val_samples, eval_transform), batch_size=args.batch_size, shuffle=False, num_workers=args.num_workers, pin_memory=True)
    test_loader = DataLoader(ClassImageDataset(test_samples, eval_transform), batch_size=args.batch_size, shuffle=False, num_workers=args.num_workers, pin_memory=True) if test_samples else None

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = build_model(args.pretrained, MODEL_IMAGE_SIZE, args.head_hidden_dim, args.dropout).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=max(args.epochs, 1))

    best_metric = -1.0
    best_checkpoint_path = output_dir / "best.pt"
    last_checkpoint_path = output_dir / "last.pt"
    best_state_dict = None
    save_metadata(output_dir, args, len(train_samples), len(val_samples), len(test_samples))

    print(f"Device      : {device}")
    print(f"Classes     : {CLASS_NAMES}")
    print(f"Image size  : {MODEL_IMAGE_SIZE}")
    print(f"Output dir  : {output_dir}")

    for epoch in range(1, args.epochs + 1):
        train_metrics = run_epoch(model, train_loader, criterion, optimizer, device)
        val_metrics = run_epoch(model, val_loader, criterion, None, device)
        scheduler.step()

        checkpoint = {
            "model_state_dict": copy.deepcopy(model.state_dict()),
            "epoch": epoch,
            "image_size": MODEL_IMAGE_SIZE,
            "class_names": CLASS_NAMES,
            "head_hidden_dim": args.head_hidden_dim,
            "dropout": args.dropout,
        }
        torch.save(checkpoint, last_checkpoint_path)
        if val_metrics["top1"] > best_metric:
            best_metric = val_metrics["top1"]
            best_state_dict = copy.deepcopy(model.state_dict())
            torch.save(checkpoint, best_checkpoint_path)

        val_matrix = confusion_matrix(val_metrics["preds"], val_metrics["targets"], NUM_CLASSES)
        print(
            f"Epoch {epoch:03d}/{args.epochs:03d} "
            f"train_loss={train_metrics['loss']:.4f} "
            f"val_loss={val_metrics['loss']:.4f} "
            f"val_top1={val_metrics['top1']:.4f}"
        )
        print(format_confusion_matrix(val_matrix))

    if best_state_dict is not None:
        model.load_state_dict(best_state_dict)
    if test_loader is not None:
        test_metrics = run_epoch(model, test_loader, criterion, None, device)
        test_matrix = confusion_matrix(test_metrics["preds"], test_metrics["targets"], NUM_CLASSES)
        print(f"Test top1    : {test_metrics['top1']:.4f}")
        print(format_confusion_matrix(test_matrix))

    if args.export_onnx:
        onnx_path = Path(args.onnx_path) if args.onnx_path else output_dir / "best.onnx"
        export_onnx_model(best_checkpoint_path, onnx_path, args.pretrained, MODEL_IMAGE_SIZE, args.head_hidden_dim, args.dropout)
        print(f"ONNX export: {onnx_path}")

    print(f"Best checkpoint: {best_checkpoint_path}")
    print(f"Last checkpoint: {last_checkpoint_path}")


if __name__ == "__main__":
    main()
