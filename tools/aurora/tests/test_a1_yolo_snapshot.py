import importlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import numpy as np


_REAL_PATH_EXISTS = Path.exists


def _patched_path_exists(self):
    if self.name == "best_a1_formal.onnx":
        return True
    return _REAL_PATH_EXISTS(self)


with mock.patch("pathlib.Path.exists", new=_patched_path_exists):
    aurora_companion = importlib.import_module("tools.aurora.aurora_companion")


class A1YoloSnapshotTests(unittest.TestCase):
    def test_latest_frame_cache_waits_for_new_sequence(self):
        cache = aurora_companion.LatestFrameCache()
        sequence = cache.publish(b"frame-1")

        self.assertEqual(cache.latest(), (sequence, b"frame-1"))
        self.assertIsNone(cache.wait_for_next(last_sequence=sequence, timeout=0.001))

    def test_latest_frame_cache_replaces_stale_payload(self):
        cache = aurora_companion.LatestFrameCache()
        seq1 = cache.publish(b"old")
        seq2 = cache.publish(b"new")

        self.assertGreater(seq2, seq1)
        self.assertEqual(cache.latest(), (seq2, b"new"))

    def test_snapshot_route_draws_board_boxes_on_camera_frame(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "yolo_snapshot",
            "success": True,
            "frame": 42,
            "count": 1,
            "camera_w": 720,
            "camera_h": 1280,
            "threshold": 0.4,
            "raw_candidates": 12,
            "top_score": 0.23,
            "top_class_id": 0,
            "top_class": "person",
            "preprocess_ok": True,
            "inference_ok": True,
            "input_dtype": 1,
            "decoded_candidates": 3,
            "after_nms_count": 1,
            "score_over_005": 8400,
            "score_over_010": 120,
            "score_over_025": 4,
            "score_over_040": 1,
            "head_top_scores": [0.23, 0.19, 0.11],
            "head_top_classes": [0, 2, 3],
            "error_stage": "",
            "error_code": 0,
            "objects": [
                {"class_id": 2, "class": "forward", "score": 0.88, "box": [100.0, 200.0, 240.0, 420.0]}
            ],
            "message": "latest detection snapshot",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [
                {"text": "[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=42"},
                {"text": "Output tensor count: 6"},
                {"text": "Output[0] shape: [1, 80, 80, 4]"},
                {"text": "First 5 values: 0.1 0.2 0.3 0.4 0.5 "},
                {"text": "[YOLOV8_TENSOR_OUTPUT_END] frame=42"},
            ],
        }

        with tempfile.TemporaryDirectory() as tmpdir, \
             mock.patch.object(aurora_companion, "output_dir", tmpdir), \
             mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        self.assertEqual(data["frame"], 42)
        self.assertEqual(data["count"], 1)
        self.assertEqual(data["objects"][0]["class"], "forward")
        self.assertTrue(data["image_b64"])
        self.assertIn("A1_DEBUG", data["raw_line"])
        self.assertEqual(data["diagnostics"]["raw_candidates"], 12)
        self.assertEqual(data["diagnostics"]["top_class"], "person")
        self.assertTrue(data["tensor_dump"].startswith("[YOLOV8_TENSOR_OUTPUT_BEGIN]"))
        self.assertIn("Output tensor count: 6", data["tensor_dump"])
        self.assertTrue(data["diagnostics"]["preprocess_ok"])
        self.assertTrue(data["diagnostics"]["inference_ok"])
        self.assertEqual(data["diagnostics"]["input_dtype"], 1)
        self.assertEqual(data["diagnostics"]["decoded_candidates"], 3)
        self.assertEqual(data["diagnostics"]["after_nms_count"], 1)
        self.assertEqual(data["diagnostics"]["score_over_010"], 120)
        self.assertEqual(data["diagnostics"]["head_top_classes"], [0, 2, 3])
        self.assertIn("近似同步", data["warning"])

    def test_snapshot_route_preserves_preprocess_failure_diagnostics(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "yolo_snapshot",
            "success": True,
            "frame": 43,
            "count": 0,
            "camera_w": 720,
            "camera_h": 1280,
            "threshold": 0.4,
            "raw_candidates": 0,
            "top_score": 0.0,
            "top_class_id": -1,
            "top_class": "unknown",
            "preprocess_ok": False,
            "inference_ok": False,
            "input_dtype": -1,
            "decoded_candidates": 0,
            "after_nms_count": 0,
            "score_over_005": 0,
            "score_over_010": 0,
            "score_over_025": 0,
            "score_over_040": 0,
            "head_top_scores": [0.0, 0.0, 0.0],
            "head_top_classes": [-1, -1, -1],
            "error_stage": "preprocess",
            "error_code": -7,
            "objects": [],
            "message": "latest detection snapshot",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [],
        }

        with tempfile.TemporaryDirectory() as tmpdir, \
             mock.patch.object(aurora_companion, "output_dir", tmpdir), \
             mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertTrue(data["success"])
        self.assertFalse(data["diagnostics"]["preprocess_ok"])
        self.assertFalse(data["diagnostics"]["inference_ok"])
        self.assertEqual(data["diagnostics"]["error_stage"], "preprocess")
        self.assertEqual(data["diagnostics"]["error_code"], -7)
        self.assertEqual(data["diagnostics"]["score_over_005"], 0)

    def test_snapshot_route_returns_error_when_camera_frame_missing(self):
        client = aurora_companion.app.test_client()
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=None):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertIn("无法获取摄像头画面", data["error"])

    def test_snapshot_route_returns_error_for_malformed_a1_debug_payload(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG not-json"},
            "recent_rx": ["A1_DEBUG not-json"],
        }
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/yolo_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertIn("A1_DEBUG", data["error"])
        self.assertEqual(data["recent_rx"], ["A1_DEBUG not-json"])


if __name__ == "__main__":
    unittest.main()
