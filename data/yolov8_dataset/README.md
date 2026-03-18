# YOLOv8 Dataset Template

This folder keeps a lightweight, versioned dataset template.

## Structure

- raw/images: original images before split
- raw/labels: YOLO txt labels matching raw/images
- images/train|val|test: split image sets
- labels/train|val|test: split label sets

## Notes

- Keep large raw data out of git when possible.
- Use tools/yolov8/split_dataset.py to perform deterministic split.
- Update data/yolov8_dataset/dataset.yaml class names before training.
