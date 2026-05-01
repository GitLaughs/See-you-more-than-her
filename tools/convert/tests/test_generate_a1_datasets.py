import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

import cv2
import numpy as np
import onnx
from onnx import TensorProto, helper

from tools.convert.generate_a1_datasets import (
    InputSpec,
    build_zip,
    collect_dataset_images,
    load_single_input_spec,
    preprocess_image,
    split_samples,
    write_config,
)


class GenerateA1DatasetsTests(unittest.TestCase):
    def _write_onnx(self, path: Path, input_name: str = "images", shape=(1, 3, 480, 640), inputs: int = 1):
        input_tensors = []
        for idx in range(inputs):
            name = input_name if idx == 0 else f"input_{idx}"
            input_tensors.append(
                helper.make_tensor_value_info(name, TensorProto.FLOAT, list(shape))
            )
        output_tensor = helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, 1])
        node = helper.make_node("Relu", [input_tensors[0].name], ["output"])
        graph = helper.make_graph([node], "test", input_tensors, [output_tensor])
        model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
        onnx.save(model, path)

    def _write_image(self, path: Path, size=(10, 8)):
        path.parent.mkdir(parents=True, exist_ok=True)
        arr = np.zeros((size[1], size[0], 3), dtype=np.uint8)
        arr[:, :, 0] = 255
        cv2.imwrite(str(path), arr)

    def test_load_single_input_spec_reads_static_shape(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            model = Path(tmpdir) / "best.onnx"
            self._write_onnx(model, shape=(1, 3, 480, 640))
            spec = load_single_input_spec(model)

        self.assertEqual(spec.input_name, "images")
        self.assertEqual(spec.input_shape, (1, 3, 480, 640))

    def test_load_single_input_spec_rejects_multi_input(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            model = Path(tmpdir) / "multi.onnx"
            self._write_onnx(model, inputs=2)
            with self.assertRaisesRegex(ValueError, "single-input"):
                load_single_input_spec(model)

    def test_load_single_input_spec_rejects_dynamic_shape(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            model = Path(tmpdir) / "dynamic.onnx"
            self._write_onnx(model, shape=(1, 3, 0, 640))
            with self.assertRaisesRegex(ValueError, "dynamic"):
                load_single_input_spec(model)

    def test_collect_dataset_images_uses_train_val_test_only(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            dataset_root = Path(tmpdir)
            self._write_image(dataset_root / "images" / "train" / "a.png")
            self._write_image(dataset_root / "images" / "val" / "b.png")
            self._write_image(dataset_root / "images" / "test" / "c.png")
            self._write_image(dataset_root / "images" / "ignore" / "d.png")

            paths = collect_dataset_images(dataset_root)

        self.assertEqual(len(paths), 3)
        self.assertTrue(all(any(part in str(path) for part in ("train", "val", "test")) for path in paths))

    def test_preprocess_image_returns_nchw_float32(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            image = Path(tmpdir) / "sample.png"
            self._write_image(image, size=(10, 8))
            tensor = preprocess_image(image, (1, 3, 12, 14))

        self.assertEqual(tensor.dtype, np.float32)
        self.assertEqual(tensor.shape, (1, 3, 12, 14))
        self.assertGreaterEqual(float(tensor.min()), 0.0)
        self.assertLessEqual(float(tensor.max()), 1.0)

    def test_split_samples_returns_requested_counts(self):
        paths = [Path(f"img_{idx}.png") for idx in range(30)]
        cal, eva = split_samples(paths, 20, 10, 42)
        self.assertEqual(len(cal), 20)
        self.assertEqual(len(eva), 10)
        self.assertEqual(len(set(cal) & set(eva)), 0)

    def test_write_config_uses_input_name(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = Path(tmpdir) / "config.toml"
            write_config(config_path, "images")
            text = config_path.read_text(encoding="utf-8")

        self.assertIn("[calibrate.inputs.images]", text)
        self.assertIn("mean = [0, 0, 0]", text)
        self.assertIn("std = [1, 1, 1]", text)

    def test_build_zip_contains_only_npy_files(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            cal = root / "calibrate_datasets"
            eva = root / "evaluate_datasets"
            cal.mkdir()
            eva.mkdir()
            np.save(cal / "a.npy", np.zeros((1, 3, 4, 4), dtype=np.float32))
            np.save(eva / "b.npy", np.zeros((1, 3, 4, 4), dtype=np.float32))
            (cal / "ignore.txt").write_text("x", encoding="utf-8")
            zip_path = root / "datasets.zip"

            build_zip(zip_path, [cal, eva])

            with zipfile.ZipFile(zip_path) as zf:
                names = zf.namelist()

        self.assertEqual(sorted(names), ["a.npy", "b.npy"])


if __name__ == "__main__":
    unittest.main()
