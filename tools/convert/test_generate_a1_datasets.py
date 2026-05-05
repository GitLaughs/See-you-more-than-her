from pathlib import Path
import importlib.util
import sys
import tempfile
import unittest
import zipfile

import numpy as np
import onnx
from onnx import TensorProto, helper
from PIL import Image


SPEC = importlib.util.spec_from_file_location(
    'generate_a1_datasets', Path(__file__).with_name('generate_a1_datasets.py')
)
mod = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = mod
SPEC.loader.exec_module(mod)


class GenerateA1DatasetsTest(unittest.TestCase):
    def test_load_single_input_spec_accepts_single_channel(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            model_path = Path(tmpdir) / 'model.onnx'
            input_tensor = helper.make_tensor_value_info('input', TensorProto.FLOAT, [1, 1, 320, 320])
            output_tensor = helper.make_tensor_value_info('logits', TensorProto.FLOAT, [1, 5])
            node = helper.make_node('Identity', ['input'], ['logits'])
            graph = helper.make_graph([node], 'g', [input_tensor], [output_tensor])
            model = helper.make_model(graph)
            onnx.save(model, model_path)

            spec = mod.load_single_input_spec(model_path)

            self.assertEqual(spec.input_shape, (1, 1, 320, 320))

    def test_collect_dataset_images_reads_processed_dataset_layout(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            dataset_root = Path(tmpdir)
            image_path = dataset_root / 'processed_dataset' / 'train' / 'person' / 'sample.png'
            image_path.parent.mkdir(parents=True, exist_ok=True)
            Image.fromarray(np.full((10, 10), 128, dtype=np.uint8)).save(image_path)

            paths = mod.collect_dataset_images(dataset_root / 'processed_dataset')

            self.assertEqual(paths, [image_path])

    def test_preprocess_image_returns_single_channel_tensor(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            image_path = Path(tmpdir) / 'sample.png'
            Image.fromarray(np.full((10, 10), 128, dtype=np.uint8)).save(image_path)

            tensor = mod.preprocess_image(image_path, (1, 1, 320, 320))

            self.assertEqual(tensor.shape, (1, 1, 320, 320))

    def test_build_zip_preserves_dataset_directory_names(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            cal = root / 'calibrate_datasets'
            eva = root / 'evaluate_datasets'
            cal.mkdir()
            eva.mkdir()
            np.save(cal / 'cal.npy', np.zeros((1, 1, 2, 2), dtype=np.float32))
            np.save(eva / 'eval.npy', np.zeros((1, 1, 2, 2), dtype=np.float32))
            zip_path = root / 'datasets.zip'

            mod.build_zip(zip_path, [cal, eva])

            with zipfile.ZipFile(zip_path) as zf:
                self.assertEqual(
                    sorted(zf.namelist()),
                    ['calibrate_datasets/cal.npy', 'evaluate_datasets/eval.npy'],
                )


if __name__ == '__main__':
    unittest.main()
