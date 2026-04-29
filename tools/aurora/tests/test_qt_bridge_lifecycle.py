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

    def test_select_qt_bridge_python_prefers_aurora_python_when_pyside6_available(self):
        aurora_python = r"C:\AuroraPython\python.exe"
        with mock.patch.dict(aurora_companion.os.environ, {"AURORA_PYTHON": aurora_python}), \
             mock.patch.object(aurora_companion.Path, "exists", return_value=True), \
             mock.patch.object(aurora_companion, "_python_has_module", return_value=True) as has_module_mock:
            selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(selected, aurora_python)
        has_module_mock.assert_called_once_with(aurora_python, "PySide6")

    def test_select_qt_bridge_python_falls_back_when_aurora_python_lacks_pyside6(self):
        aurora_python = r"C:\BadPython\python.exe"
        fallback_python = r"C:\FallbackPython\python.exe"

        def fake_has_module(candidate, module_name):
            return candidate == fallback_python and module_name == "PySide6"

        def fake_exists(path):
            return str(path) in {aurora_python, fallback_python}

        with mock.patch.dict(aurora_companion.os.environ, {"AURORA_PYTHON": aurora_python}), \
             mock.patch.object(aurora_companion.Path, "exists", fake_exists), \
             mock.patch.object(aurora_companion.sys, "executable", fallback_python), \
             mock.patch.object(aurora_companion.sys, "platform", "linux"), \
             mock.patch.object(aurora_companion, "_python_has_module", side_effect=fake_has_module):
            selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(selected, fallback_python)

    def test_stop_stale_qt_bridge_on_port_does_not_sweep_all_python_processes(self):
        captured = {}

        def fake_run(command, **kwargs):
            captured["script"] = command[-1]
            return mock.Mock(stdout=b"", returncode=0)

        with mock.patch.object(aurora_companion.sys, "platform", "win32"), \
             mock.patch.object(aurora_companion.subprocess, "run", side_effect=fake_run):
            aurora_companion._stop_stale_qt_bridge_on_port()

        self.assertIn("Get-NetTCPConnection", captured["script"])
        self.assertNotIn("Name='python.exe' OR Name='pythonw.exe'", captured["script"])


if __name__ == "__main__":
    unittest.main()
