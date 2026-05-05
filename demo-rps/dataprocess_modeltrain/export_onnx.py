import argparse
import json
from pathlib import Path

import onnx
import timm
import torch
from torch import nn

MODEL_IMAGE_SIZE = 320


class RPSClassifier(nn.Module):
    def __init__(self, image_size: int, head_hidden_dim: int, dropout: float):
        super().__init__()
        self.backbone = timm.create_model(
            "mobilenetv1_100",
            pretrained=False,
            num_classes=0,
            global_pool="",
        )
        feature_channels, feature_height, feature_width = self._infer_feature_shape(
            image_size
        )
        self.head = nn.Sequential(
            nn.Conv2d(feature_channels, head_hidden_dim, kernel_size=1, bias=False),
            nn.BatchNorm2d(head_hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout2d(p=dropout),
            nn.Conv2d(
                head_hidden_dim,
                3,
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
        features = self.backbone(x)
        logits = self.head(features)
        return logits


class RPSOnnxWrapper(nn.Module):
    def __init__(self, model: nn.Module):
        super().__init__()
        self.model = model

    def forward(self, x):
        logits = self.model(x)
        return torch.sigmoid(logits)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Export fixed-shape RPS classifier to ONNX"
    )
    parser.add_argument(
        "--checkpoint",
        type=str,
        default="outputs/rps_mobilenetv1/best.pt",
        help="Path to PyTorch checkpoint",
    )
    parser.add_argument(
        "--output_path",
        type=str,
        default="outputs/rps_mobilenetv1/best.onnx",
        help="Path to exported ONNX model",
    )
    parser.add_argument(
        "--opset",
        type=int,
        default=18,
        help="ONNX opset version",
    )
    return parser.parse_args()


def build_model(image_size: int, head_hidden_dim: int, dropout: float):
    return RPSClassifier(
        image_size=image_size, head_hidden_dim=head_hidden_dim, dropout=dropout
    )


def load_metadata(checkpoint_path: Path):
    metadata_path = checkpoint_path.parent / "metadata.json"
    if metadata_path.exists():
        return json.loads(metadata_path.read_text(encoding="utf-8"))
    return {}


def merge_external_data_if_needed(output_path: Path) -> None:
    external_data_path = output_path.with_suffix(output_path.suffix + ".data")
    if not external_data_path.exists():
        return

    model = onnx.load(str(output_path), load_external_data=True)
    onnx.save_model(
        model,
        str(output_path),
        save_as_external_data=False,
    )
    external_data_path.unlink()


def main():
    args = parse_args()
    checkpoint_path = Path(args.checkpoint)
    output_path = Path(args.output_path)

    if not checkpoint_path.exists():
        raise RuntimeError(f"checkpoint not found: {checkpoint_path}")

    checkpoint = torch.load(checkpoint_path, map_location="cpu")
    metadata = load_metadata(checkpoint_path)
    image_size = checkpoint.get("image_size") or metadata.get("image_size", MODEL_IMAGE_SIZE)
    if image_size != MODEL_IMAGE_SIZE:
        raise RuntimeError(
            f"this export script only supports fixed image size {MODEL_IMAGE_SIZE}, got {image_size}"
        )
    head_hidden_dim = checkpoint.get(
        "head_hidden_dim", metadata.get("head_hidden_dim", 256)
    )
    dropout = checkpoint.get("dropout", metadata.get("dropout", 0.2))

    model = build_model(
        image_size=MODEL_IMAGE_SIZE,
        head_hidden_dim=head_hidden_dim,
        dropout=dropout,
    )
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()

    export_model = RPSOnnxWrapper(model).eval()
    dummy_input = torch.randn(
        1, 3, MODEL_IMAGE_SIZE, MODEL_IMAGE_SIZE, dtype=torch.float32
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        torch.onnx.export(
            export_model,
            dummy_input,
            str(output_path),
            input_names=["input"],
            output_names=["confidence"],
            opset_version=args.opset,
            external_data=False,
            do_constant_folding=True,
            dynamic_axes=None,
        )

    merge_external_data_if_needed(output_path)

    print(f"Checkpoint : {checkpoint_path}")
    print(f"Image size : 1x3x{MODEL_IMAGE_SIZE}x{MODEL_IMAGE_SIZE}")
    print(f"Head dim   : {head_hidden_dim}")
    print(f"Dropout    : {dropout}")
    print(f"Output     : {output_path}")
    print("Dynamic axes disabled")


if __name__ == "__main__":
    main()
