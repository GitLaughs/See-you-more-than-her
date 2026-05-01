#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

import timm
import torch
from PIL import Image
from torch import nn
from torchvision import transforms

MODEL_IMAGE_SIZE = 320


class MobileNetSigmoidClassifier(nn.Module):
    def __init__(self, output_classes: list[str], image_size: int, head_hidden_dim: int, dropout: float):
        super().__init__()
        self.backbone = timm.create_model(
            "mobilenetv1_100",
            pretrained=False,
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
    parser = argparse.ArgumentParser(description="Inference for MobileNet sigmoid classifier")
    parser.add_argument("--image_path", required=True, help="Path to one image for inference")
    parser.add_argument("--checkpoint", required=True, help="Path to trained checkpoint .pt")
    parser.add_argument("--threshold", type=float, default=None, help="Override checkpoint threshold")
    return parser.parse_args()


def build_transform(image_size: int):
    return transforms.Compose(
        [
            transforms.Resize((image_size, image_size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
        ]
    )


def load_metadata(checkpoint_path: Path):
    metadata_path = checkpoint_path.parent / "metadata.json"
    if metadata_path.exists():
        return json.loads(metadata_path.read_text(encoding="utf-8"))
    return {}


def main():
    args = parse_args()
    checkpoint_path = Path(args.checkpoint)
    image_path = Path(args.image_path)
    if not checkpoint_path.exists():
        raise RuntimeError(f"checkpoint not found: {checkpoint_path}")
    if not image_path.exists():
        raise RuntimeError(f"image not found: {image_path}")

    checkpoint = torch.load(checkpoint_path, map_location="cpu")
    metadata = load_metadata(checkpoint_path)
    output_classes = checkpoint.get("output_classes", metadata.get("output_classes", ["P", "R", "S"]))
    image_size = checkpoint.get("image_size") or metadata.get("image_size", MODEL_IMAGE_SIZE)
    if image_size != MODEL_IMAGE_SIZE:
        raise RuntimeError(
            f"this inference script only supports fixed image size {MODEL_IMAGE_SIZE}, got {image_size}"
        )
    threshold = args.threshold
    if threshold is None:
        threshold = checkpoint.get("threshold", metadata.get("threshold", 0.5))
    head_hidden_dim = checkpoint.get("head_hidden_dim", metadata.get("head_hidden_dim", 256))
    dropout = checkpoint.get("dropout", metadata.get("dropout", 0.2))

    model = MobileNetSigmoidClassifier(
        output_classes=output_classes,
        image_size=MODEL_IMAGE_SIZE,
        head_hidden_dim=head_hidden_dim,
        dropout=dropout,
    )
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()

    image = Image.open(image_path).convert("RGB")
    tensor = build_transform(MODEL_IMAGE_SIZE)(image).unsqueeze(0)
    with torch.no_grad():
        probs = torch.sigmoid(model(tensor))[0].tolist()

    max_index = max(range(len(probs)), key=lambda index: probs[index])
    max_score = float(probs[max_index])
    label = output_classes[max_index] if max_score > threshold else "NoTarget"
    result = {
        "label": label,
        "score": max_score,
        "threshold": float(threshold),
        "confidence": {
            class_name: float(score) for class_name, score in zip(output_classes, probs)
        },
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
