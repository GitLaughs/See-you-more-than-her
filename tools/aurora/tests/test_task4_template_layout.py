import importlib
import re
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


class Task4VideoFirstTemplateTests(unittest.TestCase):
    def setUp(self):
        self.client = aurora_companion.app.test_client()

    def test_render_uses_video_first_workspace_shell(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn('class="ops-layout"', html)
        self.assertIn('class="workspace-main"', html)
        self.assertRegex(html, r'class="[^"]*control-sidebar[^"]*"')
        self.assertIn('class="console-shell"', html)

    def test_render_exposes_terminal_console_toolbar(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn('id="serialTermRxArea"', html)
        self.assertIn('class="console-toolbar"', html)
        self.assertRegex(html, r'class="[^"]*console-output[^"]*"')
        self.assertRegex(html, r'class="[^"]*console-input[^"]*"')

    def test_render_uses_light_commercial_theme_tokens(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn('--color-bg: #f3f6fb;', html)
        self.assertIn('--color-surface: #ffffff;', html)
        self.assertIn('--color-surface-hover: #eef4ff;', html)
        self.assertIn('--color-border: #d7e3f4;', html)
        self.assertIn('linear-gradient(180deg, #f8fbff 0%, #eef3f9 100%)', html)

    def test_render_defines_ros_runtime_handlers(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        for name in (
            'useCurrentDirectPortForRos',
            'applyRosConfig',
            'loadRosStatus',
            'startRosNode',
            'stopRosNode',
            'stopRosMotion',
            'testRosForward',
        ):
            self.assertIn(f'function {name}(', html)

    def test_render_promotes_serial_terminal_to_full_width_primary_card(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertRegex(html, r'class="[^"]*workflow-grid[^\"]*shared-workflow-grid[^\"]*"')
        self.assertRegex(html, r'class="[^"]*serial-console-card[^\"]*span-all[^\"]*terminal-primary-card[^\"]*"')
        self.assertIn('.terminal-primary-card .console-output-shell', html)

    def test_render_expands_terminal_output_and_wider_desktop_workspace(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn('grid-template-columns:minmax(0, 1.2fr) minmax(360px, 1.5fr);', html)
        self.assertIn('.shared-workflow-grid{grid-template-columns:repeat(2, minmax(0, 1fr));}', html)
        self.assertIn('#serialTermRxArea{min-height:320px;max-height:420px;}', html)
        self.assertIn('.workflow-grid-stm32{', html)
        self.assertIn('grid-template-columns:repeat(2, minmax(0, 1fr));', html)

    def test_render_shows_chain_diagnostics_fields(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn('id="diagSerialOwner"', html)
        self.assertIn('id="diagBreakStage"', html)
        self.assertIn('id="diagChainView"', html)
        self.assertIn('function updatePingDiagnostics(', html)
        self.assertIn('function renderBreakChain(', html)

    def test_render_distinguishes_a1_reply_from_stm32_telemetry_in_ping_ui(self):
        response = self.client.get("/")
        html = response.get_data(as_text=True)

        self.assertIn("const hasTelemetry = t && t.vx !== undefined && t.vy !== undefined && t.vz !== undefined", html)
        self.assertIn("✅ 收到 A1 debug_status 回传", html)
        self.assertIn("⚠ 未包含 STM32 遥测字段", html)
        self.assertNotIn("✅ 控制下发成功\n✅ 收到 STM32 遥测", html)


if __name__ == "__main__":
    unittest.main()
