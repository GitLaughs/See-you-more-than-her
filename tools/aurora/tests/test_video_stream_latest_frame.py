import importlib
import threading
import time
import unittest
from pathlib import Path
from unittest import mock


_REAL_PATH_EXISTS = Path.exists


def _patched_path_exists(self):
    if self.name == "best_a1_formal.onnx":
        return True
    return _REAL_PATH_EXISTS(self)


with mock.patch("pathlib.Path.exists", new=_patched_path_exists):
    aurora_companion = importlib.import_module("tools.aurora.aurora_companion")


class LatestFrameStreamTests(unittest.TestCase):
    def test_qt_bridge_stream_prefers_latest_jpeg_over_old_frames(self):
        cache = aurora_companion.LatestFrameCache()
        cache.publish(b"jpeg-1")
        cache.publish(b"jpeg-2")

        class FakeQtBridgeCapture:
            def wait_for_next_jpeg(self, last_sequence=0, timeout=1.0):
                return cache.wait_for_next(last_sequence=last_sequence, timeout=timeout)

        cap = FakeQtBridgeCapture()
        original_class = aurora_companion.QtBridgeCapture

        try:
            aurora_companion.QtBridgeCapture = FakeQtBridgeCapture
            stream = aurora_companion.generate_frames_for_capture(cap)
            first = next(stream)
        finally:
            aurora_companion.QtBridgeCapture = original_class

        self.assertIn(b"jpeg-2", first)
        self.assertNotIn(b"jpeg-1", first)

    def test_latest_frame_cache_drops_intermediate_frames_for_slow_consumer(self):
        cache = aurora_companion.LatestFrameCache()
        cache.publish(b"frame-1")
        first = cache.wait_for_next(last_sequence=0, timeout=0.01)
        self.assertEqual(first, (1, b"frame-1"))

        cache.publish(b"frame-2")
        cache.publish(b"frame-3")
        latest = cache.wait_for_next(last_sequence=1, timeout=0.01)
        self.assertEqual(latest, (3, b"frame-3"))

    def test_latest_frame_cache_waits_for_new_frame(self):
        cache = aurora_companion.LatestFrameCache()
        result_box = {}

        def reader():
            result_box["value"] = cache.wait_for_next(last_sequence=0, timeout=0.2)

        thread = threading.Thread(target=reader)
        thread.start()
        time.sleep(0.03)
        cache.publish(b"frame-live")
        thread.join(timeout=1.0)

        self.assertEqual(result_box["value"], (1, b"frame-live"))


if __name__ == "__main__":
    unittest.main()
