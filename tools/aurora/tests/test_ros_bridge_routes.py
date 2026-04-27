import importlib
import unittest
from pathlib import Path
from unittest import mock

from flask import Flask


_REAL_PATH_EXISTS = Path.exists


def _patched_path_exists(self):
    if self.name == "best_a1_formal.onnx":
        return True
    return _REAL_PATH_EXISTS(self)


with mock.patch("pathlib.Path.exists", new=_patched_path_exists):
    ros_bridge = importlib.import_module("tools.aurora.ros_bridge")


class RosBridgeRouteTests(unittest.TestCase):
    def setUp(self):
        app = Flask(__name__)
        app.register_blueprint(ros_bridge.ros_bp)
        self.client = app.test_client()

    def test_stop_motion_returns_json_when_ros_command_unavailable(self):
        with mock.patch.object(ros_bridge, "_dispatch_stop", side_effect=RuntimeError("未找到 ros2 命令")):
            response = self.client.post("/api/ros/stop_motion")

        self.assertEqual(response.status_code, 200)
        payload = response.get_json()
        self.assertEqual(payload["success"], False)
        self.assertIn("未找到 ros2 命令", payload["error"])


if __name__ == "__main__":
    unittest.main()
