# Grayscale A1 5-Class Dataset Training Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert `demo-rps/dataprocess_modeltrain` to make and train a true grayscale 5-class dataset for `person`, `stop`, `forward`, `obstacle`, and `NoTarget`.

**Architecture:** Keep the existing video-to-image dataset maker and 5-class MobileNetV1 trainer. Dataset images are saved as grayscale, training reads images as single-channel tensors, and the model accepts `1x1x320x320` input without pretrained weights.

**Tech Stack:** Python, OpenCV, PyTorch, torchvision, timm, Pillow, ONNX.

---

## File Structure

- Modify `demo-rps/dataprocess_modeltrain/prepare_video_dataset.py`
  - Ensure every saved crop is grayscale even if OpenCV returns a 3-channel frame.
  - Keep class folders fixed to `person`, `stop`, `forward`, `obstacle`, `NoTarget`.
- Modify `demo-rps/dataprocess_modeltrain/train_a1_5class_classifier.py`
  - Load images as `L` grayscale.
  - Normalize using single-channel mean/std.
  - Build a true 1-channel MobileNetV1 classifier.
  - Disable pretrained weights by default.
  - Export ONNX with dummy input shape `1x1x320x320`.
- Create `demo-rps/dataprocess_modeltrain/requirements.txt`
  - Record training and dataset dependencies.
- Optionally update `demo-rps/dataprocess_modeltrain/README.md`
  - Document grayscale five-class usage and install command.

## Task 1: Add Grayscale Dataset Saving

**Files:**
- Modify: `demo-rps/dataprocess_modeltrain/prepare_video_dataset.py`

- [ ] **Step 1: Add grayscale conversion helper**

Add this helper after `center_crop`:

```python
def to_grayscale(frame):
    if len(frame.shape) == 2:
        return frame
    if frame.shape[2] == 1:
        return frame[:, :, 0]
    return cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
```

- [ ] **Step 2: Save grayscale crops**

Change `process_video` crop save path from:

```python
cropped = center_cropped[y1:y2, x1:x2]
```

to:

```python
cropped = to_grayscale(center_cropped[y1:y2, x1:x2])
```

- [ ] **Step 3: Run syntax check**

Run:

```bash
python -m py_compile demo-rps/dataprocess_modeltrain/prepare_video_dataset.py
```

Expected: command exits with code `0`.

## Task 2: Convert Trainer To True 1-Channel Input

**Files:**
- Modify: `demo-rps/dataprocess_modeltrain/train_a1_5class_classifier.py`

- [ ] **Step 1: Change normalization constants**

Change:

```python
MEAN = [0.485, 0.456, 0.406]
STD = [0.229, 0.224, 0.225]
```

to:

```python
MEAN = [0.5]
STD = [0.5]
```

- [ ] **Step 2: Add input channel constant**

After `NUM_CLASSES = len(CLASS_NAMES)`, add:

```python
INPUT_CHANNELS = 1
```

- [ ] **Step 3: Make MobileNetV1 accept one channel**

Change `timm.create_model(...)` in `A1Classifier.__init__` from:

```python
self.backbone = timm.create_model(
    "mobilenetv1_100",
    pretrained=pretrained,
    num_classes=0,
    global_pool="",
)
```

to:

```python
self.backbone = timm.create_model(
    "mobilenetv1_100",
    pretrained=pretrained,
    num_classes=0,
    global_pool="",
    in_chans=INPUT_CHANNELS,
)
```

- [ ] **Step 4: Change dummy feature input to one channel**

Change:

```python
dummy = torch.zeros(1, 3, image_size, image_size)
```

to:

```python
dummy = torch.zeros(1, INPUT_CHANNELS, image_size, image_size)
```

- [ ] **Step 5: Read images as grayscale**

Change:

```python
image = Image.open(image_path).convert("RGB")
```

to:

```python
image = Image.open(image_path).convert("L")
```

- [ ] **Step 6: Disable pretrained by default**

Change parser line:

```python
parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=True)
```

to:

```python
parser.add_argument("--pretrained", action=argparse.BooleanOptionalAction, default=False)
```

- [ ] **Step 7: Export ONNX with one-channel dummy input**

Change:

```python
dummy_input = torch.randn(1, 3, image_size, image_size)
```

to:

```python
dummy_input = torch.randn(1, INPUT_CHANNELS, image_size, image_size)
```

- [ ] **Step 8: Save metadata input channels**

Add this key in `metadata`:

```python
"input_channels": INPUT_CHANNELS,
```

- [ ] **Step 9: Print input channels**

After image size print, add:

```python
print(f"Input chans : {INPUT_CHANNELS}")
```

- [ ] **Step 10: Run syntax check**

Run:

```bash
python -m py_compile demo-rps/dataprocess_modeltrain/train_a1_5class_classifier.py
```

Expected: command exits with code `0`.

## Task 3: Add Requirements File

**Files:**
- Create: `demo-rps/dataprocess_modeltrain/requirements.txt`

- [ ] **Step 1: Write dependency list**

Create file with:

```txt
torch
torchvision
timm
opencv-python-headless
numpy
Pillow
onnx
```

- [ ] **Step 2: Verify installed imports**

Run:

```bash
python - <<'PY'
mods = ['cv2', 'onnx', 'torch', 'torchvision', 'timm', 'numpy', 'PIL']
for mod in mods:
    __import__(mod)
    print(f'{mod} OK')
PY
```

Expected output includes:

```text
cv2 OK
onnx OK
torch OK
torchvision OK
timm OK
numpy OK
PIL OK
```

## Task 4: Update README Usage

**Files:**
- Modify: `demo-rps/dataprocess_modeltrain/README.md`

- [ ] **Step 1: Update dependency install command**

Use:

```bash
pip install -r requirements.txt
```

- [ ] **Step 2: Update class folder docs**

Document input video folders:

```text
datasets/
├── person/
├── stop/
├── forward/
├── obstacle/
└── NoTarget/
```

Document output image folders:

```text
processed_dataset/
├── train/person/
├── train/stop/
├── train/forward/
├── train/obstacle/
├── train/NoTarget/
├── val/<class>/
└── test/<class>/
```

- [ ] **Step 3: Update model description**

State:

```text
Input: 1 x 1 x 320 x 320 grayscale tensor
Output: 5-class logits [person, stop, forward, obstacle, NoTarget]
Loss: CrossEntropyLoss
Pretrained: disabled by default
```

## Task 5: Final Verification

**Files:**
- Verify only.

- [ ] **Step 1: Compile all changed scripts**

Run:

```bash
python -m py_compile demo-rps/dataprocess_modeltrain/prepare_video_dataset.py demo-rps/dataprocess_modeltrain/train_a1_5class_classifier.py
```

Expected: command exits with code `0`.

- [ ] **Step 2: Verify model can construct one-channel input**

Run:

```bash
python - <<'PY'
import importlib.util
from pathlib import Path
spec = importlib.util.spec_from_file_location('train_a1', Path('demo-rps/dataprocess_modeltrain/train_a1_5class_classifier.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
model = mod.build_model(False, mod.MODEL_IMAGE_SIZE, 256, 0.2)
print(mod.INPUT_CHANNELS)
print(model(torch.zeros(1, 1, mod.MODEL_IMAGE_SIZE, mod.MODEL_IMAGE_SIZE)).shape)
PY
```

Expected output:

```text
1
torch.Size([1, 5])
```

## Self-Review

- Spec coverage: five classes, grayscale dataset save, true one-channel model, no pretrained default, requirements file, and verification are covered.
- Placeholder scan: no placeholders remain.
- Type consistency: `INPUT_CHANNELS`, `CLASS_NAMES`, `NUM_CLASSES`, and model/export dummy input all use same one-channel design.
