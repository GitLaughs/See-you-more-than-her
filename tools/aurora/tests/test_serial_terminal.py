import importlib
import unittest


serial_terminal = importlib.import_module("tools.aurora.serial_terminal")


class SerialTerminalTests(unittest.TestCase):
    def test_decode_rx_text_keeps_newline_only_strips_carriage_return(self):
        text = serial_terminal._normalize_text(b"  \t\"command\": \"debug_status\"\r\n")
        self.assertEqual(text, "  \t\"command\": \"debug_status\"\n")

    def test_decode_rx_text_falls_back_to_gb18030(self):
        text = serial_terminal._normalize_text("调试输出".encode("gb18030") + b"\r\n")
        self.assertEqual(text, "调试输出\n")

    def test_split_complete_lines_supports_crlf_lf_and_cr(self):
        original = serial_terminal._rx_buffer
        try:
            serial_terminal._rx_buffer = bytearray(b"one\r\ntwo\nthree\rfour")
            lines = serial_terminal._pop_complete_lines()
            self.assertEqual(lines, [b"one", b"two", b"three"])
            self.assertEqual(bytes(serial_terminal._rx_buffer), b"four")
        finally:
            serial_terminal._rx_buffer = original

    def test_decode_rx_line_preserves_leading_spaces_and_tabs(self):
        text = serial_terminal.decode_rx_line(b"  \t{\"command\":\"debug_status\"}\r\n")
        self.assertEqual(text, "  \t{\"command\":\"debug_status\"}")

    def test_decode_partial_rx_line_preserves_trailing_spaces(self):
        text = serial_terminal.decode_partial_rx_line(b"status: OK  ")
        self.assertEqual(text, "status: OK  ")


if __name__ == "__main__":
    unittest.main()
