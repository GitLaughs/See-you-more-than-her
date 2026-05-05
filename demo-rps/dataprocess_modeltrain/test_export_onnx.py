from pathlib import Path
import importlib.util
import sys
import tempfile
import unittest
from unittest.mock import patch

import onnx
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
        self.assertEqual(output.shape, (1, 5, 1, 1))

    def test_default_opset_is_12(self):
        with patch.object(sys, 'argv', ['export_onnx.py']):
            self.assertEqual(export_onnx.parse_args().opset, 12)

    def test_exported_onnx_uses_a1_safe_ops(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            checkpoint_path = tmp / 'best.pt'
            onnx_path = tmp / 'best.onnx'
            model = export_onnx.build_model(image_size=export_onnx.MODEL_IMAGE_SIZE, head_hidden_dim=256, dropout=0.2)
            torch.save(
                {
                    'model_state_dict': model.state_dict(),
                    'image_size': export_onnx.MODEL_IMAGE_SIZE,
                    'head_hidden_dim': 256,
                    'dropout': 0.2,
                },
                checkpoint_path,
            )

            with patch.object(
                sys,
                'argv',
                ['export_onnx.py', '--checkpoint', str(checkpoint_path), '--output_path', str(onnx_path)],
            ):
                export_onnx.main()

            exported = onnx.load(str(onnx_path))
            ops = {node.op_type for node in exported.graph.node}
            self.assertNotIn('Clip', ops)
            self.assertNotIn('Gemm', ops)
            self.assertNotIn('Softmax', ops)
            self.assertNotIn('Sub', ops)
            self.assertNotIn('Div', ops)
            self.assertTrue(ops <= {'Conv', 'AveragePool', 'BatchNormalization', 'Relu', 'GlobalAveragePool', 'Flatten', 'Constant'})


if __name__ == '__main__':
    unittest.main()
