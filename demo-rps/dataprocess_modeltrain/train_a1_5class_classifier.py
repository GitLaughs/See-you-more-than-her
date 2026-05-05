"""
A1 5 分类视觉导航分类器训练脚本

架构：MobileNetV1（灰度输入 320×320，5 类 softmax 输出）
类别：person / stop / forward / obstacle / NoTarget
优化器：AdamW，支持类别权重平衡和 dropout 正则化
输出：best.pt（最佳模型）+ metadata.json（训练元数据）
"""

import argparse
import copy
import csv
import json
import random
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
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
INPUT_CHANNELS = 1
MEAN = [0.5]
STD = [0.5]


class A1Classifier(nn.Module):
    def __init__(self, pretrained: bool, image_size: int, head_hidden_dim: int, dropout: float):
        super().__init__()
        self.backbone = timm.create_model(
            "mobilenetv1_100",
            pretrained=pretrained,
            num_classes=0,
            global_pool="",
            in_chans=INPUT_CHANNELS,
        )
        replace_relu6(self.backbone)
        feature_channels = self._infer_feature_channels(image_size)
        self.head = nn.Sequential(
            nn.Conv2d(feature_channels, head_hidden_dim, kernel_size=1, bias=False),
            nn.BatchNorm2d(head_hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout(p=dropout),
            nn.AvgPool2d(kernel_size=2, stride=2),
            nn.AdaptiveAvgPool2d(1),
            nn.Conv2d(head_hidden_dim, NUM_CLASSES, kernel_size=1, bias=True),
        )

    def _infer_feature_channels(self, image_size: int):
        was_training = self.backbone.training
        self.backbone.eval()
        with torch.no_grad():
            dummy = torch.zeros(1, INPUT_CHANNELS, image_size, image_size)
            features = self.backbone(dummy)
        if was_training:
            self.backbone.train()
        return features.shape[1]

    def forward(self, x):
        return self.head(self.backbone(x)).flatten(1)


def replace_relu6(module: nn.Module) -> None:
    for name, child in module.named_children():
        if isinstance(child, nn.ReLU6):
            setattr(module, name, nn.ReLU(inplace=child.inplace))
        else:
            replace_relu6(child)


class ClassImageDataset(Dataset):
    def __init__(self, samples, transform):
        self.samples = samples
        self.transform = transform

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, index):
        image_path, label = self.samples[index]
        image = Image.open(image_path).convert("L")
        return self.transform(image), label, str(image_path)


def parse_args():
    parser = argparse.ArgumentParser(description="Train A1 5-class single-label classifier")
    parser.add_argument("--dataset_dir", default="data/rps_dataset/processed_dataset", help="Dataset root with train/val/test/<class> folders")
    parser.add_argument("--output_dir", default="outputs/a1_5class_mobilenetv1")
    parser.add_argument("--batch_size", type=int, default=64)
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--weight_decay", type=float, default=1e-3)
    parser.add_argument("--num_workers", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--head_hidden_dim", type=int, default=128)
    parser.add_argument("--dropout", type=float, default=0.4)
    parser.add_argument("--label_smoothing", type=float, default=0.1)
    parser.add_argument("--patience", type=int, default=5)
    parser.add_argument("--min_delta", type=float, default=0.002)
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
            transforms.Resize((image_size + 24, image_size + 24)),
            transforms.RandomResizedCrop(image_size, scale=(0.8, 1.0), ratio=(0.9, 1.1)),
            transforms.RandomApply(
                [transforms.ColorJitter(brightness=0.2, contrast=0.2)],
                p=0.5,
            ),
            transforms.RandomRotation(8, fill=0),
            transforms.RandomHorizontalFlip(p=0.5),
            transforms.ToTensor(),
            transforms.RandomErasing(p=0.25, scale=(0.02, 0.15), ratio=(0.3, 3.3), value=0),
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
    return A1Classifier(
        pretrained=pretrained,
        image_size=image_size,
        head_hidden_dim=head_hidden_dim,
        dropout=dropout,
    )


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


def compute_per_class_metrics(matrix: torch.Tensor, class_names: list[str]):
    metrics = []
    for idx, class_name in enumerate(class_names):
        tp = float(matrix[idx, idx].item())
        fp = float(matrix[:, idx].sum().item() - tp)
        fn = float(matrix[idx, :].sum().item() - tp)
        precision = tp / (tp + fp) if tp + fp else 0.0
        recall = tp / (tp + fn) if tp + fn else 0.0
        f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        metrics.append({
            "class_name": class_name,
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": int(matrix[idx, :].sum().item()),
        })
    return metrics


def save_results_csv(output_dir: Path, history: list[dict]):
    with (output_dir / "results.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["epoch", "train_loss", "val_loss", "train_top1", "val_top1"])
        for row in history:
            writer.writerow([row["epoch"], row["train_loss"], row["val_loss"], row["train_top1"], row["val_top1"]])


def plot_training_curves(output_dir: Path, history: list[dict]):
    epochs = [row["epoch"] for row in history]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), dpi=150)

    axes[0].plot(epochs, [row["train_loss"] for row in history], marker="o", label="train_loss")
    axes[0].plot(epochs, [row["val_loss"] for row in history], marker="o", label="val_loss")
    axes[0].set_xlabel("epoch")
    axes[0].set_ylabel("loss")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    axes[1].plot(epochs, [row["train_top1"] for row in history], marker="o", label="train_top1")
    axes[1].plot(epochs, [row["val_top1"] for row in history], marker="o", label="val_top1")
    axes[1].set_xlabel("epoch")
    axes[1].set_ylabel("top1")
    axes[1].set_ylim(0.0, 1.0)
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()

    fig.tight_layout()
    fig.savefig(output_dir / "accuracy_loss_curve.png", bbox_inches="tight")
    plt.close(fig)


def plot_confusion_matrix(output_path: Path, matrix: torch.Tensor, class_names: list[str], title: str):
    matrix_cpu = matrix.detach().cpu()
    fig, ax = plt.subplots(figsize=(1.4 * len(class_names) + 2, 1.1 * len(class_names) + 2), dpi=150)
    image = ax.imshow(matrix_cpu.numpy(), cmap="Blues")
    ax.set_title(title)
    ax.set_xlabel("predicted")
    ax.set_ylabel("true")
    ax.set_xticks(range(len(class_names)))
    ax.set_xticklabels(class_names, rotation=30, ha="right")
    ax.set_yticks(range(len(class_names)))
    ax.set_yticklabels(class_names)

    max_value = int(matrix_cpu.max().item()) if matrix_cpu.numel() else 0
    threshold = max_value * 0.5
    for i in range(matrix_cpu.shape[0]):
        for j in range(matrix_cpu.shape[1]):
            value = int(matrix_cpu[i, j].item())
            ax.text(j, i, str(value), ha="center", va="center", color="white" if value > threshold else "black", fontsize=9)

    fig.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def save_per_class_metrics(output_dir: Path, matrix: torch.Tensor, class_names: list[str]):
    with (output_dir / "per_class_metrics.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["class_name", "precision", "recall", "f1", "support"])
        for row in compute_per_class_metrics(matrix, class_names):
            writer.writerow([row["class_name"], row["precision"], row["recall"], row["f1"], row["support"]])


def save_training_reports(output_dir: Path, history: list[dict], val_matrix: torch.Tensor, test_matrix: torch.Tensor, class_names: list[str]):
    save_results_csv(output_dir, history)
    plot_training_curves(output_dir, history)
    plot_confusion_matrix(output_dir / "confusion_matrix_val.png", val_matrix, class_names, "Validation Confusion Matrix")
    plot_confusion_matrix(output_dir / "confusion_matrix_test.png", test_matrix, class_names, "Test Confusion Matrix")
    save_per_class_metrics(output_dir, val_matrix, class_names)


def save_metadata(output_dir: Path, args, train_count: int, val_count: int, test_count: int):
    metadata = {
        "model_name": "mobilenetv1_100",
        "class_names": CLASS_NAMES,
        "num_classes": NUM_CLASSES,
        "input_channels": INPUT_CHANNELS,
        "image_size": MODEL_IMAGE_SIZE,
        "dataset_dir": args.dataset_dir,
        "head_hidden_dim": args.head_hidden_dim,
        "dropout": args.dropout,
        "label_smoothing": args.label_smoothing,
        "patience": args.patience,
        "min_delta": args.min_delta,
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
    dummy_input = torch.randn(1, INPUT_CHANNELS, image_size, image_size)
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
    criterion = nn.CrossEntropyLoss(label_smoothing=args.label_smoothing)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=max(args.epochs, 1))

    best_metric = -1.0
    epochs_without_improvement = 0
    best_checkpoint_path = output_dir / "best.pt"
    last_checkpoint_path = output_dir / "last.pt"
    best_state_dict = None
    best_val_metrics = None
    history = []
    save_metadata(output_dir, args, len(train_samples), len(val_samples), len(test_samples))

    print(f"Device      : {device}")
    print(f"Classes     : {CLASS_NAMES}")
    print(f"Image size  : {MODEL_IMAGE_SIZE}")
    print(f"Input chans : {INPUT_CHANNELS}")
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
            "input_channels": INPUT_CHANNELS,
            "head_hidden_dim": args.head_hidden_dim,
            "dropout": args.dropout,
        }
        torch.save(checkpoint, last_checkpoint_path)
        if val_metrics["top1"] > best_metric + args.min_delta:
            best_metric = val_metrics["top1"]
            epochs_without_improvement = 0
            best_state_dict = copy.deepcopy(model.state_dict())
            best_val_metrics = {
                "loss": val_metrics["loss"],
                "top1": val_metrics["top1"],
                "preds": val_metrics["preds"].clone(),
                "targets": val_metrics["targets"].clone(),
                "epoch": epoch,
            }
            torch.save(checkpoint, best_checkpoint_path)
        else:
            epochs_without_improvement += 1

        history.append({
            "epoch": epoch,
            "train_loss": train_metrics["loss"],
            "val_loss": val_metrics["loss"],
            "train_top1": train_metrics["top1"],
            "val_top1": val_metrics["top1"],
        })

        val_matrix = confusion_matrix(val_metrics["preds"], val_metrics["targets"], NUM_CLASSES)
        print(
            f"Epoch {epoch:03d}/{args.epochs:03d} "
            f"train_loss={train_metrics['loss']:.4f} "
            f"val_loss={val_metrics['loss']:.4f} "
            f"val_top1={val_metrics['top1']:.4f}"
        )
        print(format_confusion_matrix(val_matrix))

        if epochs_without_improvement >= args.patience:
            print(f"Early stopping at epoch {epoch:03d} after {epochs_without_improvement} stale epochs")
            break

    if best_state_dict is not None:
        model.load_state_dict(best_state_dict)
    if best_val_metrics is None:
        best_val_metrics = val_metrics
    if test_loader is not None:
        test_metrics = run_epoch(model, test_loader, criterion, None, device)
        test_matrix = confusion_matrix(test_metrics["preds"], test_metrics["targets"], NUM_CLASSES)
        print(f"Test top1    : {test_metrics['top1']:.4f}")
        print(format_confusion_matrix(test_matrix))
    else:
        test_matrix = torch.zeros((NUM_CLASSES, NUM_CLASSES), dtype=torch.int64)

    save_training_reports(output_dir, history, confusion_matrix(best_val_metrics["preds"], best_val_metrics["targets"], NUM_CLASSES), test_matrix, CLASS_NAMES)

    if args.export_onnx:
        onnx_path = Path(args.onnx_path) if args.onnx_path else output_dir / "best.onnx"
        export_onnx_model(best_checkpoint_path, onnx_path, args.pretrained, MODEL_IMAGE_SIZE, args.head_hidden_dim, args.dropout)
        print(f"ONNX export: {onnx_path}")

    print(f"Best checkpoint: {best_checkpoint_path}")
    print(f"Last checkpoint: {last_checkpoint_path}")


if __name__ == "__main__":
    main()
