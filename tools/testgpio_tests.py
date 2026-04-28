from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

from testgpio.app import create_app
from testgpio.runner import GpioRunner


class FakePopen:
    def __init__(self, command, stdout_lines=None):
        self.command = command
        self.stdout_lines = list(stdout_lines or [])
        self.returncode = 0

    def wait(self):
        return self.returncode


class TestCommandBuilder(unittest.TestCase):
    def test_build_gpio_command_includes_expected_flags(self):
        app = create_app({"TESTING": True})
        runner = app.config["GPIO_RUNNER"]

        command = runner.build_gpio_command(mode="tx", pin=8, period_ms=250, duration_s=12)

        self.assertEqual(
            command,
            [
                "./ssne_ai_demo",
                "--gpio-test",
                "--pin",
                "8",
                "--mode",
                "tx",
                "--period-ms",
                "250",
                "--duration-s",
                "12",
            ],
        )

    def test_build_launch_command_wraps_ssh_when_host_configured(self):
        runner = GpioRunner(
            log_dir=Path(tempfile.mkdtemp()),
            ssh_host="192.168.1.10",
            remote_workdir="/tmp/demo",
        )

        command = runner.build_launch_command(mode="loop", pin=10, period_ms=100, duration_s=3)

        self.assertEqual(
            command,
            [
                "ssh",
                "192.168.1.10",
                "cd /tmp/demo && ./ssne_ai_demo --gpio-test --pin 10 --mode loop --period-ms 100 --duration-s 3",
            ],
        )


class TestAppConfig(unittest.TestCase):
    def test_create_app_passes_ssh_and_spawn_config_to_runner(self):
        fake_spawn = lambda command: FakePopen(command, stdout_lines=[])
        app = create_app(
            {
                "TESTING": True,
                "A1_SSH_HOST": "board.local",
                "A1_REMOTE_WORKDIR": "/opt/demo",
                "GPIO_SPAWN_PROCESS": fake_spawn,
            }
        )
        runner = app.config["GPIO_RUNNER"]

        command = runner.build_launch_command(mode="tx", pin=8, period_ms=500, duration_s=5)
        run = runner.start_run(mode="tx", pin=8, period_ms=500, duration_s=5)
        runner.wait_for_completion(timeout=2.0)

        self.assertEqual(command[0], "ssh")
        self.assertEqual(command[1], "board.local")
        self.assertIn("cd /opt/demo &&", command[2])
        self.assertEqual(run["status"], "running")

    def test_config_endpoint_reads_and_updates_board_target(self):
        app = create_app({"TESTING": True})
        client = app.test_client()

        get_response = client.get("/api/gpio/config")
        self.assertEqual(get_response.status_code, 200)
        original = get_response.get_json()
        self.assertIn("ssh_host", original)
        self.assertIn("remote_workdir", original)

        post_response = client.post(
            "/api/gpio/config",
            json={"ssh_host": "10.0.0.8", "remote_workdir": "/root/demo"},
        )
        self.assertEqual(post_response.status_code, 200)
        updated = post_response.get_json()
        self.assertEqual(updated["ssh_host"], "10.0.0.8")
        self.assertEqual(updated["remote_workdir"], "/root/demo")

        runner = app.config["GPIO_RUNNER"]
        command = runner.build_launch_command(mode="tx", pin=8, period_ms=500, duration_s=5)
        self.assertEqual(command[1], "10.0.0.8")
        self.assertIn("cd /root/demo &&", command[2])


class TestRunLifecycle(unittest.TestCase):
    def test_start_run_persists_log_file_and_status(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            app = create_app({"TESTING": True, "LOG_DIR": temp_dir, "GPIO_SPAWN_PROCESS": lambda command: FakePopen(command, stdout_lines=[])})
            client = app.test_client()

            response = client.post(
                "/api/gpio/start",
                json={"mode": "rx", "pin": 9, "period_ms": 500, "duration_s": 5},
            )

            self.assertEqual(response.status_code, 202)
            payload = response.get_json()
            self.assertEqual(payload["run"]["mode"], "rx")
            self.assertEqual(payload["run"]["pin"], 9)
            self.assertEqual(payload["run"]["status"], "running")
            self.assertTrue(payload["run"]["log_path"].endswith(".log"))
            self.assertTrue(Path(payload["run"]["log_path"]).exists())

            status_response = client.get("/api/gpio/status")
            self.assertEqual(status_response.status_code, 200)
            status_payload = status_response.get_json()
            self.assertEqual(status_payload["run"]["id"], payload["run"]["id"])
            self.assertIn(status_payload["run"]["status"], ["running", "passed"])
            self.assertIn("log_excerpt", status_payload["run"])

    def test_start_run_streams_output_to_log_and_marks_exit_code(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            spawned = []

            def fake_spawn(command):
                proc = FakePopen(command, stdout_lines=["line one\n", "line two\n"])
                spawned.append(proc)
                return proc

            runner = GpioRunner(log_dir=Path(temp_dir), spawn_process=fake_spawn)

            run = runner.start_run(mode="tx", pin=8, period_ms=500, duration_s=5)
            runner.wait_for_completion(timeout=2.0)
            finished = runner.current_run()

            self.assertEqual(len(spawned), 1)
            self.assertEqual(finished["status"], "passed")
            self.assertEqual(finished["exit_code"], 0)
            log_text = Path(run["log_path"]).read_text(encoding="utf-8")
            self.assertIn("line one", log_text)
            self.assertIn("line two", log_text)

    def test_index_page_renders_panel_title(self):
        app = create_app({"TESTING": True})
        client = app.test_client()

        response = client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn("A1 GPIO Test", response.get_data(as_text=True))


if __name__ == "__main__":
    unittest.main()
