from pathlib import Path
import importlib.util
import sys
import unittest

import torch


SPEC = importlib.util.spec_from_file_location(
    'export_onnx', Path(__file__).with_name('export_onnx.py')
)
export_onnx = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = export_onnx
SPEC.loader.exec_module(export_onnx)


class ExportOnnxTest(unittest.TestCase):
    def test_build_model_accepts_single_channel_and_outputs_five_classes(self):
        model = export_onnx.build_model(image_size=export_onnx.MODEL_IMAGE_SIZE, head_hidden_dim=256, dropout=0.2)
        output = model(torch.zeros(1, 1, export_onnx.MODEL_IMAGE_SIZE, export_onnx.MODEL_IMAGE_SIZE))
        self.assertEqual(output.shape, (1, 5))


if __name__ == '__main__':
    unittest.main()
