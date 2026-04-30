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


class AuroraStartupTests(unittest.TestCase):
    def test_launch_script_looks_for_aurora_exe_in_repo_root(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        self.assertIn(
            '$repoRoot = Split-Path (Split-Path $ScriptDir -Parent) -Parent',
            launch_script,
        )
        self.assertIn(
            '$auroraExe = Join-Path $repoRoot "Aurora-2.0.0-ciciec.16\\Aurora.exe"',
            launch_script,
        )

    def test_main_bootstraps_aurora_before_qt_bridge_on_windows(self):
        events = []

        def record_start_aurora():
            events.append("start_aurora")

        def record_ensure_bridge(*, timeout):
            events.append("ensure_qt_bridge_running")
            return {"available": False, "error": "qt down"}

        with mock.patch.object(aurora_companion, "AURORA_EXE_PATH", Path("C:/Aurora/Aurora.exe"), create=True), \
             mock.patch.object(aurora_companion, "_start_aurora_desktop", side_effect=record_start_aurora, create=True) as start_aurora, \
             mock.patch.object(aurora_companion, "ensure_qt_bridge_running", side_effect=record_ensure_bridge) as ensure_bridge, \
             mock.patch.object(aurora_companion, "bootstrap_camera") as bootstrap_camera, \
             mock.patch.object(aurora_companion.threading, "Thread") as thread_cls, \
             mock.patch.object(aurora_companion.app, "run") as app_run, \
             mock.patch.object(aurora_companion.os, "makedirs") as makedirs, \
             mock.patch.object(aurora_companion.argparse.ArgumentParser, "parse_args", return_value=type("Args", (), {
                 "device": -1,
                 "source": "auto",
                 "output": "../../data/yolov8_dataset/raw",
                 "port": 5801,
                 "host": "127.0.0.1",
             })()):
            thread_instance = mock.Mock()
            thread_cls.return_value = thread_instance

            aurora_companion.main()

        start_aurora.assert_called_once_with()
        ensure_bridge.assert_called_once_with(timeout=6.0)
        self.assertEqual(events[:2], ["start_aurora", "ensure_qt_bridge_running"])
        thread_cls.assert_called_once()
        thread_instance.start.assert_called_once_with()
        app_run.assert_called_once_with(host="127.0.0.1", port=5801, debug=False, threaded=True)
        makedirs.assert_called_once()
        bootstrap_camera.assert_not_called()

    def test_main_continues_when_aurora_bootstrap_fails(self):
        with mock.patch.object(aurora_companion, "AURORA_EXE_PATH", Path("C:/Aurora/Aurora.exe"), create=True), \
             mock.patch.object(aurora_companion, "_start_aurora_desktop", side_effect=RuntimeError("boom"), create=True), \
             mock.patch.object(aurora_companion, "ensure_qt_bridge_running", return_value={"available": False, "error": "qt down"}) as ensure_bridge, \
             mock.patch.object(aurora_companion.threading, "Thread") as thread_cls, \
             mock.patch.object(aurora_companion.app, "run") as app_run, \
             mock.patch.object(aurora_companion.os, "makedirs"), \
             mock.patch.object(aurora_companion.argparse.ArgumentParser, "parse_args", return_value=type("Args", (), {
                 "device": -1,
                 "source": "auto",
                 "output": "../../data/yolov8_dataset/raw",
                 "port": 5801,
                 "host": "127.0.0.1",
             })()):
            thread_instance = mock.Mock()
            thread_cls.return_value = thread_instance

            aurora_companion.main()

        ensure_bridge.assert_called_once_with(timeout=6.0)
        thread_instance.start.assert_called_once_with()
        app_run.assert_called_once_with(host="127.0.0.1", port=5801, debug=False, threaded=True)

    def test_main_waits_longer_for_qt_bridge_startup(self):
        with mock.patch.object(aurora_companion, "AURORA_EXE_PATH", Path("C:/Aurora/Aurora.exe"), create=True), \
             mock.patch.object(aurora_companion, "_start_aurora_desktop", return_value={"running": True}, create=True), \
             mock.patch.object(aurora_companion, "ensure_qt_bridge_running", return_value={"available": True}) as ensure_bridge, \
             mock.patch.object(aurora_companion.threading, "Thread") as thread_cls, \
             mock.patch.object(aurora_companion.app, "run") as app_run, \
             mock.patch.object(aurora_companion.os, "makedirs"), \
             mock.patch.object(aurora_companion.argparse.ArgumentParser, "parse_args", return_value=type("Args", (), {
                 "device": -1,
                 "source": "auto",
                 "output": "../../data/yolov8_dataset/raw",
                 "port": 5801,
                 "host": "127.0.0.1",
             })()):
            thread_instance = mock.Mock()
            thread_cls.return_value = thread_instance

            aurora_companion.main()

        ensure_bridge.assert_called_once_with(timeout=6.0)
        thread_instance.start.assert_called_once_with()
        app_run.assert_called_once_with(host="127.0.0.1", port=5801, debug=False, threaded=True)

    def test_qt_bridge_launch_uses_absolute_script_path(self):
        bridge_process = mock.Mock(pid=43210)
        bridge_process.poll.return_value = None
        ready_status = {"available": True, "bridge_version": aurora_companion.QT_BRIDGE_PROTOCOL_VERSION}
        relative_script = Path("tools/aurora/qt_camera_bridge.py")

        with mock.patch.object(aurora_companion, "QT_BRIDGE_SCRIPT", relative_script), \
             mock.patch.object(aurora_companion, "_qt_bridge_process", None), \
             mock.patch.object(aurora_companion, "_qt_bridge_status", side_effect=[None, None, ready_status]), \
             mock.patch.object(aurora_companion, "_resolve_available_port", return_value=5930), \
             mock.patch.object(aurora_companion, "save_qt_bridge_owner_state"), \
             mock.patch.object(aurora_companion, "cleanup_qt_bridge_owner_process"), \
             mock.patch.object(aurora_companion, "_stop_stale_qt_bridge_on_port"), \
             mock.patch.object(aurora_companion, "_select_qt_bridge_python", return_value="python.exe"), \
             mock.patch.object(aurora_companion.subprocess, "Popen", return_value=bridge_process) as popen:
            status = aurora_companion.ensure_qt_bridge_running(timeout=0.1)

        self.assertEqual(status, ready_status)
        popen.assert_called_once_with(
            [
                "python.exe",
                str(relative_script.resolve()),
                "--host",
                aurora_companion.QT_BRIDGE_HOST,
                "--port",
                "5930",
            ],
            cwd=str(Path(aurora_companion.__file__).resolve().parent),
        )
    def test_qt_bridge_python_selector_ignores_dot_venv39(self):
        repo_root = Path(aurora_companion.__file__).resolve().parents[2]
        canonical_python = repo_root / "venv_39" / "Scripts" / "python.exe"
        duplicate_python = repo_root / ".venv39" / "Scripts" / "python.exe"
        current_python = Path(aurora_companion.sys.executable)

        def has_module(python_exe, module_name):
            self.assertEqual(module_name, "PySide6")
            if Path(python_exe) == canonical_python:
                return False
            if Path(python_exe) == current_python:
                return False
            if Path(python_exe) == duplicate_python:
                return True
            return False

        with mock.patch.object(aurora_companion, "_python_has_module", side_effect=has_module):
            selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(selected, aurora_companion.sys.executable)

    def test_qt_bridge_python_selector_prefers_launch_env(self):
        repo_root = Path(aurora_companion.__file__).resolve().parents[2]
        preferred_python = repo_root / ".venv39" / "Scripts" / "python.exe"

        with mock.patch.dict(aurora_companion.os.environ, {"AURORA_PYTHON": str(preferred_python)}, clear=False), \
             mock.patch.object(aurora_companion, "_python_has_module", return_value=True) as has_module:
            selected = aurora_companion._select_qt_bridge_python()

        self.assertEqual(Path(selected), preferred_python)
        has_module.assert_called_once_with(str(preferred_python), "PySide6")

    def test_launch_script_defines_qt_bridge_owner_state_path_helper(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        self.assertIn("function Get-QtBridgeOwnerStatePath", launch_script)
        self.assertIn('return Join-Path $ScriptDir ".qt_bridge_owner.json"', launch_script)

    def test_launch_script_uses_single_repo_python_env(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        self.assertNotIn('.venv39\\Scripts\\python.exe', launch_script)
        self.assertIn('$env:AURORA_PYTHON = $Python', launch_script)
        self.assertIn('Set-AuroraPythonEnvironment -PythonExecutable $Python', launch_script)
        self.assertIn('& $env:AURORA_PYTHON aurora_companion.py', launch_script)

    def test_launch_script_uses_base_python_with_venv_packages(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        self.assertIn("pyvenv.cfg", launch_script)
        self.assertIn("$env:VIRTUAL_ENV", launch_script)
        self.assertIn("$env:PYTHONPATH", launch_script)

    def test_launch_script_port_probe_can_escape_windows_reserved_range(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        start = launch_script.index("function Resolve-AvailablePort")
        end = launch_script.index("function Stop-StaleCompanionOnPort")
        resolve_block = launch_script[start:end]
        self.assertNotIn("$PreferredPort + 30", resolve_block)
        self.assertIn("65535", resolve_block)

    def test_launch_script_companion_cleanup_avoids_pid_variable_collision(self):
        launch_script = Path("tools/aurora/launch.ps1").read_text(encoding="utf-8")
        start = launch_script.index("function Stop-StaleCompanionOnPort")
        end = launch_script.index("function Stop-StaleQtBridge")
        cleanup_block = launch_script[start:end]
        self.assertNotIn("$pId = [int]$conn.OwningProcess", cleanup_block)
        self.assertIn("$ownerPid = [int]$conn.OwningProcess", cleanup_block)
        self.assertIn("$ownerPid -le 0 -or $ownerPid -eq $PID", cleanup_block)


if __name__ == "__main__":
    unittest.main()
