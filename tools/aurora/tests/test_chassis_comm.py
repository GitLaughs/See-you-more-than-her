import importlib
import sys
import threading
import unittest
from unittest import mock

from flask import Flask


serial_terminal = importlib.import_module("tools.aurora.serial_terminal")
sys.modules.setdefault("serial_terminal", serial_terminal)
chassis_comm = importlib.import_module("tools.aurora.chassis_comm")


class ChassisCommBackendTests(unittest.TestCase):
    def setUp(self):
        self.app = Flask(__name__)
        self.app.register_blueprint(chassis_comm.chassis_bp)
        self.client = self.app.test_client()

        self.original_ser = chassis_comm._ser
        self.original_rx_thread = chassis_comm._rx_thread
        self.original_running = chassis_comm._running
        self.original_transport_mode = chassis_comm._transport_mode
        self.original_connected_port = chassis_comm._connected_port
        self.original_connected_baud = chassis_comm._connected_baud
        self.original_telemetry = chassis_comm._telemetry
        self.original_rx_seq = chassis_comm._rx_seq
        self.original_tx_log = chassis_comm._tx_log
        self.original_rx_log = chassis_comm._rx_log

        chassis_comm._ser = None
        chassis_comm._rx_thread = None
        chassis_comm._running = False
        chassis_comm._transport_mode = chassis_comm._DIRECT_TRANSPORT
        chassis_comm._connected_port = None
        chassis_comm._connected_baud = 115200
        chassis_comm._telemetry = {}
        chassis_comm._rx_seq = 0
        chassis_comm._tx_log.clear()
        chassis_comm._rx_log.clear()

    def tearDown(self):
        chassis_comm._ser = self.original_ser
        chassis_comm._rx_thread = self.original_rx_thread
        chassis_comm._running = self.original_running
        chassis_comm._transport_mode = self.original_transport_mode
        chassis_comm._connected_port = self.original_connected_port
        chassis_comm._connected_baud = self.original_connected_baud
        chassis_comm._telemetry = self.original_telemetry
        chassis_comm._rx_seq = self.original_rx_seq
        chassis_comm._tx_log = self.original_tx_log
        chassis_comm._rx_log = self.original_rx_log

    def test_connect_uses_shared_backend_for_shared_port(self):
        with mock.patch.object(chassis_comm.shared_serial, "is_shared_port", return_value=True), \
             mock.patch.object(chassis_comm.shared_serial, "ensure_connected_to", return_value={"success": True, "port": "COM13", "baud": 115200}), \
             mock.patch.object(chassis_comm.serial, "Serial") as serial_ctor:
            response = self.client.post("/api/chassis/connect", json={"port": "COM13", "baud": 115200})
            payload = response.get_json()

        self.assertTrue(payload["success"])
        self.assertEqual(payload["transport"], chassis_comm._SHARED_TRANSPORT)
        self.assertEqual(chassis_comm._transport_mode, chassis_comm._SHARED_TRANSPORT)
        serial_ctor.assert_not_called()

    def test_disconnect_in_shared_mode_keeps_shared_owner_alive(self):
        chassis_comm._transport_mode = chassis_comm._SHARED_TRANSPORT
        chassis_comm._connected_port = "COM13"

        fake_serial = mock.Mock()
        fake_serial.is_open = True
        chassis_comm._ser = fake_serial

        response = self.client.post("/api/chassis/disconnect")
        payload = response.get_json()

        self.assertTrue(payload["success"])
        self.assertEqual(payload["transport"], chassis_comm._SHARED_TRANSPORT)
        self.assertEqual(payload["shared_owner"], "serial_terminal")
        fake_serial.close.assert_not_called()
        self.assertEqual(chassis_comm._transport_mode, chassis_comm._DIRECT_TRANSPORT)
        self.assertIsNone(chassis_comm._connected_port)

    def test_connect_uses_direct_backend_for_non_shared_port(self):
        fake_serial = mock.Mock()
        fake_serial.is_open = True
        fake_serial.port = "COM7"
        fake_serial.baudrate = 115200

        started = []

        class FakeThread:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def start(self):
                started.append(True)

        with mock.patch.object(chassis_comm.shared_serial, "is_shared_port", return_value=False), \
             mock.patch.object(chassis_comm.serial, "Serial", return_value=fake_serial) as serial_ctor, \
             mock.patch.object(chassis_comm.threading, "Thread", FakeThread):
            response = self.client.post("/api/chassis/connect", json={"port": "COM7", "baud": 115200})
            payload = response.get_json()

        self.assertTrue(payload["success"])
        self.assertEqual(payload["transport"], chassis_comm._DIRECT_TRANSPORT)
        self.assertEqual(chassis_comm._transport_mode, chassis_comm._DIRECT_TRANSPORT)
        self.assertEqual(chassis_comm._connected_port, "COM7")
        serial_ctor.assert_called_once_with("COM7", 115200, timeout=0.1)
        self.assertEqual(started, [True])

    def test_status_keeps_existing_keys_and_transport(self):
        chassis_comm._transport_mode = chassis_comm._SHARED_TRANSPORT
        chassis_comm._connected_port = "COM13"
        chassis_comm._telemetry = {"vx": 12}

        with mock.patch.object(chassis_comm.shared_serial, "current_port", return_value="COM13"), \
             mock.patch.object(chassis_comm.shared_serial, "is_shared_port", return_value=True), \
             mock.patch.object(chassis_comm, "_shared_snapshot", return_value={"connected": True, "port": "COM13", "baud": 115200, "busy": False}):
            response = self.client.get("/api/chassis/status")
            payload = response.get_json()

        self.assertTrue(payload["connected"])
        self.assertEqual(payload["port"], "COM13")
        self.assertEqual(payload["telemetry"], {"vx": 12})
        self.assertEqual(payload["transport"], chassis_comm._SHARED_TRANSPORT)

    def test_ping_in_shared_mode_reports_transport_only_contract(self):
        chassis_comm._transport_mode = chassis_comm._SHARED_TRANSPORT
        chassis_comm._connected_port = "COM13"

        with mock.patch.object(chassis_comm.shared_serial, "is_shared_port", return_value=True), \
             mock.patch.object(chassis_comm, "_shared_snapshot", return_value={"connected": True, "port": "COM13", "baud": 115200, "busy": False}):
            response = self.client.post("/api/chassis/ping")
            payload = response.get_json()

        self.assertTrue(payload["success"])
        self.assertTrue(payload["connected"])
        self.assertIsNone(payload["frame_tx"])
        self.assertEqual(payload["transport"], chassis_comm._SHARED_TRANSPORT)
        self.assertIn("relay debug_status", payload["note"])

    def test_move_in_shared_mode_uses_shared_serial_sender(self):
        chassis_comm._transport_mode = chassis_comm._SHARED_TRANSPORT
        with mock.patch.object(chassis_comm.shared_serial, "send_raw_payload", return_value={"success": True}) as sender:
            response = self.client.post("/api/chassis/move", json={"vx": 200, "vy": 0, "vz": 0, "cmd": 0})
            payload = response.get_json()

        self.assertTrue(payload["success"])
        sender.assert_called_once()
        sent_payload = sender.call_args.args[0]
        self.assertEqual(sent_payload, chassis_comm.build_cmd(200, 0, 0, 0))
        self.assertEqual(payload["transport"], chassis_comm._SHARED_TRANSPORT)


if __name__ == "__main__":
    unittest.main()
