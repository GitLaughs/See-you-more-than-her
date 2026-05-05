import importlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import numpy as np


_REAL_PATH_EXISTS = Path.exists


def _patched_path_exists(self):
    if self.name == "model_rps.m1model":
        return True
    return _REAL_PATH_EXISTS(self)


with mock.patch("pathlib.Path.exists", new=_patched_path_exists):
    aurora_companion = importlib.import_module("tools.aurora.aurora_companion")


class A1RpsSnapshotTests(unittest.TestCase):
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

    def test_snapshot_route_returns_rps_classification_payload(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "rps_snapshot",
            "success": True,
            "request": "req42abcdef0",
            "frame": 42,
            "camera_w": 720,
            "camera_h": 1280,
            "roi": {"x": 210, "y": 270, "w": 540, "h": 540},
            "label": "P",
            "label_name": "paper",
            "confidence": 0.91,
            "scores": [0.91, 0.05, 0.04],
            "action": "forward",
            "message": "latest classification snapshot",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [{"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))}],
        }

        with tempfile.TemporaryDirectory() as tmpdir, \
             mock.patch.object(aurora_companion, "output_dir", tmpdir), \
             mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.uuid, "uuid4", return_value=mock.Mock(hex="req42abcdef0xxxx")), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/rps_snapshot")

        data = response.get_json()
        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        self.assertEqual(data["request"], "req42abcdef0")
        self.assertEqual(data["frame"], 42)
        self.assertEqual(data["label"], "P")
        self.assertEqual(data["label_name"], "paper")
        self.assertEqual(data["action"], "forward")
        self.assertEqual(data["roi"], {"x": 210, "y": 270, "w": 540, "h": 540})
        self.assertEqual(data["scores"], [0.91, 0.05, 0.04])
        self.assertTrue(data["image_b64"])
        self.assertIn("A1_DEBUG", data["raw_line"])
        self.assertIn("近似同步", data["warning"])

    def test_snapshot_route_updates_latest_classification_state(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "rps_snapshot",
            "success": True,
            "request": "req43abcdef0",
            "frame": 43,
            "camera_w": 720,
            "camera_h": 1280,
            "roi": {"x": 210, "y": 270, "w": 540, "h": 540},
            "label": "R",
            "label_name": "rock",
            "confidence": 0.82,
            "scores": [0.05, 0.82, 0.13],
            "action": "stop",
            "message": "latest classification snapshot",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [{"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))}],
        }

        with tempfile.TemporaryDirectory() as tmpdir, \
             mock.patch.object(aurora_companion, "output_dir", tmpdir), \
             mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.uuid, "uuid4", return_value=mock.Mock(hex="req43abcdef0xxxx")), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/rps_snapshot")

        self.assertTrue(response.get_json()["success"])
        latest = client.get("/api/detect/latest").get_json()
        self.assertEqual(latest["label"], "R")
        self.assertEqual(latest["label_name"], "rock")
        self.assertEqual(latest["action"], "stop")
        self.assertEqual(latest["roi"], {"x": 210, "y": 270, "w": 540, "h": 540})

    def test_snapshot_route_rejects_mismatched_request(self):
        client = aurora_companion.app.test_client()
        frame = np.zeros((1280, 720, 3), dtype=np.uint8)
        payload = {
            "command": "rps_snapshot",
            "success": True,
            "request": "otherreq",
            "frame": 44,
            "label": "S",
            "confidence": 0.77,
            "scores": [0.1, 0.13, 0.77],
            "action": "stop",
        }
        serial_result = {
            "success": True,
            "matched": {"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))},
            "recent_rx": [{"text": "A1_DEBUG " + json.dumps(payload, separators=(",", ":"))}],
        }
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=frame), \
             mock.patch.object(aurora_companion.uuid, "uuid4", return_value=mock.Mock(hex="reqbadxxxxxxzz")), \
             mock.patch.object(aurora_companion.serial_terminal, "send_text_line", return_value=serial_result):
            response = client.post("/api/a1/rps_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertEqual(data["error"], "A1 rps_snapshot request mismatch")

    def test_snapshot_route_returns_error_when_camera_frame_missing(self):
        client = aurora_companion.app.test_client()
        with mock.patch.object(aurora_companion, "camera", object()), \
             mock.patch.object(aurora_companion, "_read_display_frame", return_value=None):
            response = client.post("/api/a1/rps_snapshot")

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
            response = client.post("/api/a1/rps_snapshot")

        data = response.get_json()
        self.assertFalse(data["success"])
        self.assertIn("A1_DEBUG", data["error"])
        self.assertEqual(data["recent_rx"], ["A1_DEBUG not-json"])


if __name__ == "__main__":
    unittest.main()
