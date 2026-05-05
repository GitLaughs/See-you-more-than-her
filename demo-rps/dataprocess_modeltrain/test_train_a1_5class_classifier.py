from pathlib import Path
import importlib.util
import sys
import tempfile
import unittest

import torch


SPEC = importlib.util.spec_from_file_location(
    'train_a1_5class_classifier', Path(__file__).with_name('train_a1_5class_classifier.py')
)
train = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = train
SPEC.loader.exec_module(train)


class TrainingMetricsExportTest(unittest.TestCase):
    def test_save_training_reports_writes_csv_and_plots(self):
        history = [
            {'epoch': 1, 'train_loss': 1.2, 'val_loss': 1.4, 'train_top1': 0.4, 'val_top1': 0.3},
            {'epoch': 2, 'train_loss': 0.9, 'val_loss': 1.0, 'train_top1': 0.6, 'val_top1': 0.5},
            {'epoch': 3, 'train_loss': 0.7, 'val_loss': 0.8, 'train_top1': 0.7, 'val_top1': 0.65},
        ]
        val_matrix = torch.tensor([
            [8, 1, 0],
            [0, 7, 2],
            [1, 0, 9],
        ])
        test_matrix = torch.tensor([
            [7, 2, 0],
            [1, 6, 2],
            [0, 1, 8],
        ])
        class_names = ['person', 'stop', 'forward']

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir)
            train.save_training_reports(output_dir, history, val_matrix, test_matrix, class_names)

            expected_files = [
                'results.csv',
                'accuracy_loss_curve.png',
                'confusion_matrix_val.png',
                'confusion_matrix_test.png',
                'per_class_metrics.csv',
            ]
            for name in expected_files:
                self.assertTrue((output_dir / name).exists(), name)

            csv_text = (output_dir / 'results.csv').read_text(encoding='utf-8')
            self.assertIn('epoch,train_loss,val_loss,train_top1,val_top1', csv_text)
            self.assertIn('3,0.7,0.8,0.7,0.65', csv_text)

    def test_compute_per_class_metrics_uses_precision_recall_f1(self):
        matrix = torch.tensor([
            [8, 1, 1],
            [2, 5, 1],
            [0, 2, 6],
        ])
        metrics = train.compute_per_class_metrics(matrix, ['a', 'b', 'c'])
        self.assertEqual(metrics[0]['class_name'], 'a')
        self.assertAlmostEqual(metrics[0]['precision'], 8 / 10)
        self.assertAlmostEqual(metrics[0]['recall'], 8 / 10)
        self.assertAlmostEqual(metrics[0]['f1'], 0.8)
        self.assertAlmostEqual(metrics[1]['precision'], 5 / 8)
        self.assertAlmostEqual(metrics[1]['recall'], 5 / 8)


if __name__ == '__main__':
    unittest.main()
