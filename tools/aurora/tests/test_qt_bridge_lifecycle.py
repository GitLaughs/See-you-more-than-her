import importlib
import json
import tempfile
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
    qt_camera_bridge = importlib.import_module("tools.aurora.qt_camera_bridge")


class QtBridgeLifecycleTests(unittest.TestCase):
    def test_shutdown_qt_bridge_terminates_owned_process_and_clears_owner_state(self):
        class FakeProcess:
            def __init__(self):
                self.pid = 43210
                self.terminate_called = False
                self.kill_called = False
                self.wait_called = False
                self._poll_value = None

            def poll(self):
                return self._poll_value

            def terminate(self):
                self.terminate_called = True
                self._poll_value = 0

            def wait(self, timeout=None):
                self.wait_called = True
                return 0

            def kill(self):
                self.kill_called = True
                self._poll_value = 0

        fake_process = FakeProcess()
        with tempfile.TemporaryDirectory() as tmpdir:
            owner_state_path = Path(tmpdir) / "qt_bridge_owner.json"
            owner_state_path.write_text(json.dumps({"pid": fake_process.pid}), encoding="utf-8")
            with mock.patch.object(aurora_companion, "QT_BRIDGE_OWNER_STATE_FILE", owner_state_path), \
                 mock.patch.object(aurora_companion, "_qt_bridge_process", fake_process), \
                 mock.patch.object(aurora_companion, "_qt_bridge_stop", return_value={"success": True}):
                result = aurora_companion.shutdown_qt_bridge()

        self.assertTrue(result["success"])
        self.assertTrue(fake_process.terminate_called)
        self.assertTrue(fake_process.wait_called)
        self.assertFalse(owner_state_path.exists())
        self.assertIsNone(aurora_companion._qt_bridge_process)

    def test_ensure_qt_bridge_running_cleans_owned_stale_state_before_respawn(self):
        stale_status = {"available": False, "bridge_version": 0}
        healthy_status = {"available": True, "bridge_version": aurora_companion.QT_BRIDGE_PROTOCOL_VERSION}
        fake_process = mock.Mock(pid=54321)
        fake_process.poll.return_value = None
        with tempfile.TemporaryDirectory() as tmpdir:
            owner_state_path = Path(tmpdir) / "qt_bridge_owner.json"
            owner_state_path.write_text(json.dumps({"pid": 99999}), encoding="utf-8")
            with mock.patch.object(aurora_companion, "QT_BRIDGE_OWNER_STATE_FILE", owner_state_path), \
                 mock.patch.object(aurora_companion, "_qt_bridge_process", None), \
                 mock.patch.object(aurora_companion, "_qt_bridge_status", side_effect=[stale_status, None, healthy_status]), \
                 mock.patch.object(aurora_companion, "cleanup_qt_bridge_owner_process") as cleanup_mock, \
                 mock.patch.object(aurora_companion, "_select_qt_bridge_python", return_value="python"), \
                 mock.patch.object(aurora_companion.subprocess, "Popen", return_value=fake_process), \
                 mock.patch.object(aurora_companion.time, "sleep"):
                status = aurora_companion.ensure_qt_bridge_running(timeout=0.1)

        self.assertEqual(status, healthy_status)
        cleanup_mock.assert_called_once()

    def test_shutdown_endpoint_invokes_qt_bridge_cleanup(self):
        client = aurora_companion.app.test_client()
        with mock.patch.object(aurora_companion, "shutdown_qt_bridge", return_value={"success": True, "message": "stopped"}) as cleanup_mock:
            response = client.post("/shutdown")

        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.get_json()["success"])
        cleanup_mock.assert_called_once()


    def test_ensure_qt_bridge_running_uses_available_port_when_default_reserved(self):
        healthy_status = {"available": True, "bridge_version": aurora_companion.QT_BRIDGE_PROTOCOL_VERSION}
        fake_process = mock.Mock(pid=54321)
        fake_process.poll.return_value = None
        with tempfile.TemporaryDirectory() as tmpdir:
            owner_state_path = Path(tmpdir) / "qt_bridge_owner.json"
            original_port = aurora_companion.QT_BRIDGE_PORT
            original_url = aurora_companion.QT_BRIDGE_URL
            try:
                with mock.patch.object(aurora_companion, "QT_BRIDGE_OWNER_STATE_FILE", owner_state_path), \
                     mock.patch.object(aurora_companion, "_qt_bridge_process", None), \
                     mock.patch.object(aurora_companion, "_qt_bridge_status", side_effect=[None, None, healthy_status]), \
                     mock.patch.object(aurora_companion, "_resolve_available_port", return_value=5930, create=True) as resolve_port, \
                     mock.patch.object(aurora_companion, "save_qt_bridge_owner_state"), \
                     mock.patch.object(aurora_companion, "cleanup_qt_bridge_owner_process"), \
                     mock.patch.object(aurora_companion, "_stop_stale_qt_bridge_on_port"), \
                     mock.patch.object(aurora_companion, "_select_qt_bridge_python", return_value="python"), \
                     mock.patch.object(aurora_companion.subprocess, "Popen", return_value=fake_process) as popen:
                    status = aurora_companion.ensure_qt_bridge_running(timeout=0.1)
                    selected_port = aurora_companion.QT_BRIDGE_PORT
                    selected_url = aurora_companion.QT_BRIDGE_URL
            finally:
                aurora_companion.QT_BRIDGE_PORT = original_port
                aurora_companion.QT_BRIDGE_URL = original_url

        self.assertEqual(status, healthy_status)
        resolve_port.assert_called_once_with(aurora_companion.QT_BRIDGE_HOST, original_port, blocked_ports={aurora_companion.COMPANION_PORT})
        self.assertEqual(selected_port, 5930)
        self.assertEqual(selected_url, f"http://{aurora_companion.QT_BRIDGE_HOST}:5930")
        popen.assert_called_once_with(
            [
                "python",
                str(aurora_companion.QT_BRIDGE_SCRIPT.resolve()),
                "--host",
                aurora_companion.QT_BRIDGE_HOST,
                "--port",
                "5930",
            ],
            cwd=str(Path(aurora_companion.__file__).resolve().parent),
        )


    def test_bridge_write_json_ignores_client_abort(self):
        handler = object.__new__(qt_camera_bridge.BridgeHandler)
        handler.send_response = mock.Mock()
        handler.send_header = mock.Mock()
        handler.end_headers = mock.Mock()
        handler.wfile = mock.Mock()
        handler.wfile.write.side_effect = ConnectionAbortedError("client disconnected")

        qt_camera_bridge.BridgeHandler._write_json(handler, {"success": False}, status=503)

        handler.send_response.assert_called_once_with(503)
        handler.end_headers.assert_called_once_with()
        handler.wfile.write.assert_called_once()
    def test_bridge_write_bytes_ignores_client_abort(self):
        handler = object.__new__(qt_camera_bridge.BridgeHandler)
        handler.send_response = mock.Mock()
        handler.send_header = mock.Mock()
        handler.end_headers = mock.Mock()
        handler.wfile = mock.Mock()
        handler.wfile.write.side_effect = ConnectionAbortedError("client disconnected")

        qt_camera_bridge.BridgeHandler._write_bytes(handler, b"jpeg-data")

        handler.send_response.assert_called_once_with(200)
        handler.end_headers.assert_called_once_with()
        handler.wfile.write.assert_called_once_with(b"jpeg-data")


if __name__ == "__main__":
    unittest.main()
