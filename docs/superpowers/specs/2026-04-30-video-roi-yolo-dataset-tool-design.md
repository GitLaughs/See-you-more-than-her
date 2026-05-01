# Video ROI YOLO Dataset Tool Design

## Goal

Add a local video labeling tool under `tools/video/` that extracts frames from a video, resizes every output image to `640x480`, and writes YOLO labels using user-entered fixed ROI coordinates. Update `tools/yolo/` docs and local paths so workflow covers video capture -> raw images/labels -> YOLOv8 training with `third_party/ultralytics` -> ONNX export -> head6 crop -> A1 conversion.

## Scope

In scope:

- Local browser UI to enter:
  - input video path, default `video.mp4` at repo root
  - class name
  - class id
  - ROI coordinates `x1 y1 x2 y2` in source video coordinates
  - frame step
  - optional output prefix
- Backend frame extraction:
  - read video via OpenCV
  - save frames to `tools/yolo/raw/images/`
  - save labels to `tools/yolo/raw/labels/`
  - resize output images to `640x480`
  - scale ROI from source frame size to `640x480`, then convert to YOLO normalized labels
- `tools/yolo/` local workflow:
  - `dataset.yaml` local to `tools/yolo/`
  - split raw data into train/val/test under `tools/yolo/images/` and `tools/yolo/labels/`
  - commands for training using `third_party/ultralytics`
  - commands for ONNX export and references to existing model conversion/head6 docs

Out of scope:

- Automatic object detection.
- Mouse drawing UI.
- Editing individual frame labels.
- Changing Aurora tools.

## Architecture

`tools/video/video_label_tool.py` serves a small local HTML page and exposes one API endpoint. It keeps the UI dependency-light and does the processing in the same process. `tools/video/launch.ps1` starts the server from repo root.

`tools/yolo/split_dataset.py` localizes the train/val/test split for the `tools/yolo/` dataset layout. `tools/yolo/dataset.yaml` points to `tools/yolo` and uses the default class list until user edits it.

## Data Flow

```text
video.mp4
-> tools/video UI fixed ROI + class name/id
-> tools/yolo/raw/images/*.jpg 640x480
-> tools/yolo/raw/labels/*.txt YOLO normalized boxes
-> tools/yolo/split_dataset.py
-> tools/yolo/images/train|val|test
-> tools/yolo/labels/train|val|test
-> third_party/ultralytics YOLOv8 train
-> best.pt
-> best.onnx
-> head6 cropped onnx
-> m1model conversion
```

## Error Handling

- Missing video path returns UI-visible error.
- Invalid ROI coordinates return error before processing.
- ROI outside source frame returns error.
- Failed frame read stops with error summary.
- Existing output files are not deleted automatically; new files use prefix and frame index.

## Verification

- `python -m py_compile tools/video/video_label_tool.py tools/yolo/split_dataset.py`
- Optional smoke test with tiny generated video: run extractor endpoint or CLI helper if present.
- Confirm README paths reference `tools/yolo`, `tools/video`, and `third_party/ultralytics`.
