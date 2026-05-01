# A1 Dataset Conversion Script Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local conversion script that reads `models/best.onnx`, samples images from `data/yolov8_dataset/images/{train,val,test}`, exports `NCHW` float32 `.npy` tensors, packages them into `datasets.zip`, and writes a matching `config.toml` for A1 model conversion.

**Architecture:** Add one focused converter under `tools/convert/` with small helper functions for ONNX inspection, dataset sampling, image preprocessing, `.npy` export, zip packaging, and TOML generation. Keep it single-input only so the script fails fast when the ONNX model does not match the expected A1 conversion path. Reuse the existing dataset layout under `data/yolov8_dataset/` and keep all outputs local to a chosen output directory.

**Tech Stack:** Python 3, `onnx`, `onnxruntime` only if needed for shape inspection fallback, `opencv-python`, `numpy`, `zipfile`, `pathlib`, `tomllib`/`tomli` for validation, `pytest` or plain `unittest` for script tests.

---

## File Structure

- Create: `tools/convert/generate_a1_datasets.py` — main CLI script that inspects ONNX, samples images, writes `.npy`, creates `datasets.zip`, and emits `config.toml`.
- Create: `tools/convert/README.md` — short usage guide with required inputs, outputs, and example command.
- Create: `tools/convert/tests/test_generate_a1_datasets.py` — unit tests for ONNX shape parsing, image preprocessing, sample selection, zip contents, and TOML generation.
- Modify: `docs/15_AI模型转换与部署.md` — add a short note pointing to `tools/convert/generate_a1_datasets.py` for datasets.zip/config.toml generation.
- Modify: `docs/09_AI模型训练.md` — add a small link from export/conversion flow to the new converter so the training doc points at the full packaging step.

## Task 1: Define converter CLI and ONNX input discovery

**Files:**
- Create: `tools/convert/generate_a1_datasets.py`
- Test: `tools/convert/tests/test_generate_a1_datasets.py`

- [ ] **Step 1: Write failing tests for ONNX inspection**

Add tests that cover these exact cases:

```python
from pathlib import Path
import pytest

from tools.convert.generate_a1_datasets import load_single_input_spec


def test_load_single_input_spec_reads_static_shape(tmp_path: Path):
    model = tmp_path / "best.onnx"
    # create a tiny ONNX fixture in test setup
    spec = load_single_input_spec(model)
    assert spec.input_name == "images"
    assert spec.input_shape == (1, 3, 640, 480)


def test_load_single_input_spec_rejects_multi_input(tmp_path: Path):
    model = tmp_path / "multi.onnx"
    with pytest.raises(ValueError, match="single-input"):
        load_single_input_spec(model)


def test_load_single_input_spec_rejects_dynamic_shape(tmp_path: Path):
    model = tmp_path / "dynamic.onnx"
    with pytest.raises(ValueError, match="dynamic"):
        load_single_input_spec(model)
```

- [ ] **Step 2: Run the tests and confirm failure**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: fail because `load_single_input_spec()` does not exist yet.

- [ ] **Step 3: Implement the smallest ONNX reader**

Add a small dataclass and helper functions:

```python
from dataclasses import dataclass
from pathlib import Path
from typing import Tuple

import onnx


@dataclass(frozen=True)
class InputSpec:
    input_name: str
    input_shape: tuple[int, int, int, int]


def load_single_input_spec(model_path: Path) -> InputSpec:
    model = onnx.load(str(model_path))
    inputs = [item for item in model.graph.input if item.name not in {init.name for init in model.graph.initializer}]
    if len(inputs) != 1:
        raise ValueError("Expected single-input ONNX model")
    tensor = inputs[0]
    dims = []
    for dim in tensor.type.tensor_type.shape.dim:
        if dim.dim_value <= 0:
            raise ValueError("ONNX input shape is dynamic or invalid")
        dims.append(int(dim.dim_value))
    if len(dims) != 4:
        raise ValueError("Expected 4D NCHW input")
    return InputSpec(input_name=tensor.name, input_shape=tuple(dims))
```

- [ ] **Step 4: Run the focused tests again**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: ONNX tests pass once fixture setup matches the helper behavior.

- [ ] **Step 5: Commit only after this slice is green**

```bash
git add tools/convert/generate_a1_datasets.py tools/convert/tests/test_generate_a1_datasets.py
git commit -m "feat: inspect single-input onnx for dataset export"
```

## Task 2: Generate NCHW tensors from dataset images

**Files:**
- Modify: `tools/convert/generate_a1_datasets.py`
- Test: `tools/convert/tests/test_generate_a1_datasets.py`

- [ ] **Step 1: Write failing tests for image preprocessing and sampling**

Add tests that verify:
- only `data/yolov8_dataset/images/train|val|test` are scanned;
- images are converted to `float32` NCHW arrays;
- output height/width follow ONNX input shape;
- calibration set gets at least 20 files and evaluation set gets at least 10 files when enough images exist.

Use a test like this:

```python
from pathlib import Path
import numpy as np

from tools.convert.generate_a1_datasets import preprocess_image, collect_dataset_images, split_samples


def test_preprocess_image_returns_nchw_float32(tmp_path: Path):
    image = tmp_path / "sample.png"
    # write a simple RGB test image in fixture setup
    tensor = preprocess_image(image, (640, 480))
    assert tensor.dtype == np.float32
    assert tensor.shape == (1, 3, 480, 640)
    assert tensor.min() >= 0.0
    assert tensor.max() <= 1.0


def test_collect_dataset_images_uses_train_val_test_only(tmp_path: Path):
    dataset_root = tmp_path / "data" / "yolov8_dataset"
    paths = collect_dataset_images(dataset_root)
    assert all("images" in str(path) for path in paths)
    assert all(any(part in str(path) for part in ("train", "val", "test")) for path in paths)
```

- [ ] **Step 2: Run tests and confirm missing helpers fail**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: fail because preprocessing/sampling helpers do not exist yet.

- [ ] **Step 3: Implement image scan, resize, normalize, and split helpers**

Add helpers that:
- walk `data/yolov8_dataset/images/train|val|test` recursively;
- sort paths for stable output;
- shuffle with a fixed seed before splitting;
- read images with OpenCV, convert BGR to RGB, resize to ONNX width/height, divide by 255.0, transpose to NCHW, add batch dimension;
- return `np.float32` arrays.

Use explicit shape handling:

```python
def preprocess_image(image_path: Path, input_shape: tuple[int, int, int, int]) -> np.ndarray:
    _, channels, height, width = input_shape
    if channels != 3:
        raise ValueError("Only 3-channel inputs are supported")
    # read -> resize -> RGB -> /255 -> CHW -> batch
```

- [ ] **Step 4: Run the preprocessing tests again**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: pass once helper logic matches test fixtures.

- [ ] **Step 5: Commit this slice**

```bash
git add tools/convert/generate_a1_datasets.py tools/convert/tests/test_generate_a1_datasets.py
git commit -m "feat: export calibration tensors from dataset images"
```

## Task 3: Write `.npy`, zip archive, and `config.toml`

**Files:**
- Modify: `tools/convert/generate_a1_datasets.py`
- Test: `tools/convert/tests/test_generate_a1_datasets.py`

- [ ] **Step 1: Write failing tests for packaging output**

Add tests that verify:
- `.npy` files are written with `np.save()` output;
- `datasets.zip` contains only `.npy` members;
- `config.toml` contains the actual ONNX input name under `[calibrate.inputs.<name>]`;
- mean/std default to `[0, 0, 0]` and `[1, 1, 1]`.

Example test shape:

```python
from pathlib import Path
import zipfile
import tomllib

from tools.convert.generate_a1_datasets import write_outputs


def test_write_outputs_creates_zip_and_toml(tmp_path: Path):
    out_dir = tmp_path / "out"
    result = write_outputs(...)
    assert (out_dir / "datasets.zip").exists()
    assert (out_dir / "config.toml").exists()
    with zipfile.ZipFile(out_dir / "datasets.zip") as zf:
        assert all(name.endswith(".npy") for name in zf.namelist())
    config = tomllib.loads((out_dir / "config.toml").read_text(encoding="utf-8"))
    assert "calibrate" in config
    assert "inputs" in config["calibrate"]
```

- [ ] **Step 2: Run tests and confirm missing packaging helpers fail**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: fail until write helpers exist.

- [ ] **Step 3: Implement output writer and archive builder**

Add a writer that:
- creates `calibrate_datasets/` and `evaluate_datasets/` under output root;
- saves tensors with `np.save(out_path, tensor)`;
- writes a manifest only if you find it useful for debugging, but do not include it in zip;
- builds `datasets.zip` from those two folders and filters to `.npy` only;
- writes a minimal `config.toml` like:

```toml
[calibrate.inputs.images]
mean = [0, 0, 0]
std = [1, 1, 1]
```

Replace `images` with the real ONNX input name.

- [ ] **Step 4: Run packaging tests again**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
```

Expected: pass, with zip and TOML content verified.

- [ ] **Step 5: Commit this slice**

```bash
git add tools/convert/generate_a1_datasets.py tools/convert/tests/test_generate_a1_datasets.py
git commit -m "feat: package dataset tensors for a1 conversion"
```

## Task 4: Wire CLI, docs, and end-to-end verification

**Files:**
- Modify: `tools/convert/generate_a1_datasets.py`
- Create: `tools/convert/README.md`
- Modify: `docs/15_AI模型转换与部署.md`
- Modify: `docs/09_AI模型训练.md`

- [ ] **Step 1: Add end-to-end CLI test or smoke check**

Add one test that runs the script against a tiny fixture dataset and fixture ONNX model, then checks:
- `datasets.zip` exists;
- `config.toml` exists;
- exactly 20 calibrate samples and 10 evaluate samples are produced when fixture size allows it;
- script exits non-zero with a clear message if the dataset has too few images.

Example command to validate manually:

```bash
python tools/convert/generate_a1_datasets.py \
  --onnx models/best.onnx \
  --dataset-root data/yolov8_dataset \
  --output-dir build/a1_convert
```

- [ ] **Step 2: Add short usage docs**

Write `tools/convert/README.md` with:
- input assumptions: `models/best.onnx`, `data/yolov8_dataset/images/{train,val,test}`;
- output list: `calibrate_datasets/`, `evaluate_datasets/`, `datasets.zip`, `config.toml`;
- one concrete command example;
- note that `.npy` is generated with `np.save()` and zip must contain only tensors.

- [ ] **Step 3: Link docs from existing conversion flow**

Add a brief note to `docs/15_AI模型转换与部署.md` and `docs/09_AI模型训练.md` pointing to the new script as the local way to prepare `datasets.zip` and `config.toml` before A1 conversion.

- [ ] **Step 4: Run final verification**

Run:

```bash
pytest tools/convert/tests/test_generate_a1_datasets.py -v
python tools/convert/generate_a1_datasets.py --onnx models/best.onnx --dataset-root data/yolov8_dataset --output-dir build/a1_convert
```

Expected:
- tests pass;
- converter produces `datasets.zip` and `config.toml`;
- zip has no stray images or metadata files;
- `config.toml` uses the ONNX input name.

- [ ] **Step 5: Final commit**

```bash
git add tools/convert/generate_a1_datasets.py tools/convert/README.md docs/15_AI模型转换与部署.md docs/09_AI模型训练.md tools/convert/tests/test_generate_a1_datasets.py
git commit -m "feat: add a1 dataset conversion workflow"
```

## Self-check coverage map

- ONNX single-input/static-shape parsing → Task 1
- Dataset scan from `data/yolov8_dataset/images/{train,val,test}` → Task 2
- NCHW float32 `.npy` generation → Task 2
- `datasets.zip` with only `.npy` → Task 3
- `config.toml` with real input name and default mean/std → Task 3
- CLI docs and conversion flow docs → Task 4
- End-to-end verification command → Task 4
