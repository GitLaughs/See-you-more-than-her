import importlib
import sys
import unittest
from unittest import mock

from flask import Flask


serial_terminal = importlib.import_module("tools.aurora.serial_terminal")
sys.modules.setdefault("serial_terminal", serial_terminal)
relay_comm = importlib.import_module("tools.aurora.relay_comm")


class RelayDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.app = Flask(__name__)
        self.app.register_blueprint(relay_comm.relay_bp)
        self.client = self.app.test_client()

    def test_status_surfaces_shared_serial_owner_state(self):
        with mock.patch.object(relay_comm.st, "is_connected", return_value=True), \
             mock.patch.object(relay_comm.st, "current_port", return_value="COM13"), \
             mock.patch.object(relay_comm.st, "current_baud", return_value=115200), \
             mock.patch.object(relay_comm.st, "latest_lines", return_value=[{"text": "{\"success\":true}", "partial": False}]):
            payload = relay_comm._status_payload()

        self.assertTrue(payload["connected"])
        self.assertEqual(payload["serial_owner"], "serial_terminal")
        self.assertEqual(payload["port"], "COM13")
        self.assertEqual(payload["latest_lines"][0]["text"], '{"success":true}')

    def test_ping_reports_break_stage_when_a1_reply_missing(self):
        with mock.patch.object(relay_comm, "_send_cli", return_value={
            "success": False,
            "transport_success": True,
            "response_received": False,
            "error": "未在串口输出中等到预期回传",
            "port": "COM13",
            "command_line": "A1_TEST debug_status",
        }), mock.patch.object(relay_comm.st, "latest_lines", return_value=[]):
            response = self.client.post("/api/relay/ping")
            payload = response.get_json()

        self.assertTrue(payload["transport_success"])
        self.assertFalse(payload["connected"])
        self.assertEqual(payload["break_stage"], "A1 -> debug_status 回传")
        self.assertEqual(payload["frame_tx"], "A1_TEST debug_status")
        self.assertEqual(payload["diagnosis"], "no_a1_reply")

    def test_ping_explains_timeout_without_any_a1_output(self):
        with mock.patch.object(relay_comm, "_send_cli", return_value={
            "success": False,
            "transport_success": True,
            "response_received": False,
            "error": "未在串口输出中等到预期回传结果",
            "port": "COM13",
            "command_line": "A1_TEST debug_status",
        }), mock.patch.object(relay_comm.st, "latest_lines", return_value=[]):
            response = self.client.post("/api/relay/ping")
            payload = response.get_json()

        self.assertEqual(payload["break_stage"], "A1 -> debug_status 回传")
        self.assertIn("A1 未输出任何 debug_status 回传", payload["note"])
        self.assertEqual(payload["diagnosis"], "no_a1_reply")

    def test_ping_explains_output_mismatch_when_a1_printed_other_text(self):
        with mock.patch.object(relay_comm, "_send_cli", return_value={
            "success": False,
            "transport_success": True,
            "response_received": False,
            "error": "未在串口输出中等到预期回传结果",
            "port": "COM13",
            "command_line": "A1_TEST debug_status",
        }), mock.patch.object(relay_comm.st, "latest_lines", return_value=[{"text": "status: chassis offline", "partial": False}]):
            response = self.client.post("/api/relay/ping")
            payload = response.get_json()

        self.assertEqual(payload["break_stage"], "A1 debug_status 输出格式")
        self.assertIn("A1 有输出，但不匹配预期关键字", payload["note"])
        self.assertIn("status: chassis offline", payload["note"])
        self.assertEqual(payload["diagnosis"], "a1_output_mismatch")


if __name__ == "__main__":
    unittest.main()
