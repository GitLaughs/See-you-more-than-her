import importlib
import threading
import unittest
from flask import Flask
from unittest import mock


serial_terminal = importlib.import_module("tools.aurora.serial_terminal")


class SerialTerminalTests(unittest.TestCase):
    def setUp(self):
        self.original_rx_buffer = serial_terminal._rx_buffer
        self.original_rx_log = serial_terminal._rx_log
        self.original_latest_lines = serial_terminal._latest_lines
        self.original_rx_seq = serial_terminal._rx_seq
        self.original_ser = serial_terminal._ser
        self.original_running = serial_terminal._running
        serial_terminal._rx_buffer = bytearray()
        serial_terminal._rx_log.clear()
        serial_terminal._latest_lines.clear()
        serial_terminal._rx_seq = 0
        serial_terminal._ser = None
        serial_terminal._running = False

    def tearDown(self):
        serial_terminal._rx_buffer = self.original_rx_buffer
        serial_terminal._rx_log = self.original_rx_log
        serial_terminal._latest_lines = self.original_latest_lines
        serial_terminal._rx_seq = self.original_rx_seq
        serial_terminal._ser = self.original_ser
        serial_terminal._running = self.original_running

    def test_decode_rx_text_keeps_newline_only_strips_carriage_return(self):
        text = serial_terminal._normalize_text(b"  \t\"command\": \"debug_status\"\r\n")
        self.assertEqual(text, "  \t\"command\": \"debug_status\"\n")

    def test_decode_rx_text_falls_back_to_gb18030(self):
        text = serial_terminal._normalize_text("调试输出".encode("gb18030") + b"\r\n")
        self.assertEqual(text, "调试输出\n")

    def test_split_complete_lines_supports_crlf_lf_and_cr(self):
        serial_terminal._rx_buffer = bytearray(b"one\r\ntwo\nthree\rfour")
        lines = serial_terminal._pop_complete_lines()
        self.assertEqual(lines, [b"one", b"two", b"three"])
        self.assertEqual(bytes(serial_terminal._rx_buffer), b"four")

    def test_decode_rx_line_preserves_leading_spaces_and_tabs(self):
        text = serial_terminal.decode_rx_line(b"  \t{\"command\":\"debug_status\"}\r\n")
        self.assertEqual(text, "  \t{\"command\":\"debug_status\"}")

    def test_decode_partial_rx_line_preserves_trailing_spaces(self):
        text = serial_terminal.decode_partial_rx_line(b"status: OK  ")
        self.assertEqual(text, "status: OK  ")

    def test_complete_line_merges_with_latest_partial_entry(self):
        serial_terminal._append_rx_entry(b"Draw", "Draw", partial=True)
        serial_terminal._rx_buffer = bytearray(b"ing 0 detection boxes\r\n")

        for line in serial_terminal._pop_complete_lines():
            serial_terminal._append_rx_entry(line, serial_terminal.decode_rx_line(line), partial=False)

        texts = [entry["text"] for entry in serial_terminal.rx_log_entries()]
        self.assertEqual(texts[0], "Drawing 0 detection boxes")
        self.assertEqual(len(texts), 1)

    def test_flush_partial_buffer_skips_midword_ascii_fragments(self):
        serial_terminal._rx_buffer = bytearray(b"Draw")

        serial_terminal._flush_partial_buffer(force=True)

        self.assertEqual(list(serial_terminal.rx_log_entries()), [])
        self.assertEqual(bytes(serial_terminal._rx_buffer), b"Draw")

    def test_status_does_not_wait_for_slow_serial_write(self):
        class FakeSerial:
            is_open = True
            port = "COM13"
            baudrate = 115200

            def write(self, payload):
                ready.set()
                release.wait(timeout=1.0)
                return len(payload)

        ready = threading.Event()
        release = threading.Event()
        serial_terminal._ser = FakeSerial()
        app = Flask(__name__)

        result_box = {}

        def send_worker():
            result_box["send"] = serial_terminal._send_payload(b"A1_TEST debug_status\r\n", "A1_TEST debug_status")

        thread = threading.Thread(target=send_worker)
        thread.start()
        self.assertTrue(ready.wait(timeout=0.2))

        with app.app_context():
            response = serial_terminal.status()
            payload = response.get_json()
        self.assertTrue(payload["busy"])
        self.assertEqual(payload["port"], "COM13")
        self.assertEqual(payload["baud"], 115200)

        release.set()
        thread.join(timeout=1.0)
        self.assertFalse(thread.is_alive())
        self.assertTrue(result_box["send"]["success"])


if __name__ == "__main__":
    unittest.main()
