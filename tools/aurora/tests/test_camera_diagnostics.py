import importlib
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


classify_qt_bridge_failure = aurora_companion.classify_qt_bridge_failure
summarize_qt_bridge_status = aurora_companion.summarize_qt_bridge_status


class CameraDiagnosticsTests(unittest.TestCase):
    def test_classifies_no_frame_after_switch_as_timeout(self):
        result = classify_qt_bridge_failure(
            status={
                "connected": True,
                "device_name": "Smartsens-FlyingChip-A1-1",
                "frame_count": 0,
                "last_frame_ts": 0.0,
                "message": "Qt 相机桥已连接",
            },
            error_text="Qt 相机桥已切换到 Smartsens-FlyingChip-A1-1，但 5 秒内未收到视频帧",
        )
        self.assertEqual(result["code"], "no_frame_after_switch")
        self.assertEqual(result["severity"], "error")
        self.assertIn("hint", result)

    def test_classifies_missing_device(self):
        result = classify_qt_bridge_failure(
            status={"connected": False, "device_name": "", "frame_count": 0},
            error_text="Qt 相机桥未找到设备 0",
        )
        self.assertEqual(result["code"], "device_not_found")
        self.assertEqual(result["severity"], "error")
        self.assertIn("hint", result)

    def test_summarizes_waiting_for_first_frame(self):
        summary = summarize_qt_bridge_status({
            "available": True,
            "connected": True,
            "device_name": "Smartsens-FlyingChip-A1-1",
            "frame_count": 0,
            "last_frame_ts": 0.0,
            "message": "Qt 相机桥已连接: Smartsens-FlyingChip-A1-1 / 1280x720 / YUYV",
        })
        self.assertEqual(summary["state"], "waiting_for_first_frame")
        self.assertIn("首帧", summary["detail"])

    def test_summarizes_streaming_state(self):
        summary = summarize_qt_bridge_status({
            "available": True,
            "connected": True,
            "device_name": "Smartsens-FlyingChip-A1-1",
            "frame_count": 12,
            "last_frame_ts": 1710000123.4,
            "message": "Qt 相机桥已连接: Smartsens-FlyingChip-A1-1 / 1280x720 / YUYV",
        })
        self.assertEqual(summary["state"], "streaming")
        self.assertIn("Qt 相机桥", summary["detail"])


if __name__ == "__main__":
    unittest.main()
