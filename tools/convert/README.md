# A1 Dataset Convert Tool

Generate `datasets.zip` and `config.toml` for A1 conversion.

## Inputs

- `--onnx models/best.onnx`
- `--dataset-root data/yolov8_dataset`

Script scans:

- `data/yolov8_dataset/images/train`
- `data/yolov8_dataset/images/val`
- `data/yolov8_dataset/images/test`

## Outputs

- `calibrate_datasets/*.npy`
- `evaluate_datasets/*.npy`
- `datasets.zip`
- `config.toml`

## Example

```powershell
python tools/convert/generate_a1_datasets.py \
  --onnx models/best.onnx \
  --dataset-root data/yolov8_dataset \
  --output-dir build/a1_convert
```

## Notes

- `.npy` files are written with `np.save()`.
- Zip archive contains `.npy` only.
- `config.toml` uses the ONNX input name under `[calibrate.inputs.<name>]`.
- Script expects single-input ONNX with static 4D NCHW shape.
