from pathlib import Path


TEMPLATE = Path(__file__).resolve().parents[1] / "templates" / "companion_ui.html"


def test_tensor_output_panel_is_present():
    html = TEMPLATE.read_text(encoding="utf-8")

    assert "YOLO Tensor Output" in html
    assert "tensorOutputText" in html
    assert "copyTensorOutput" in html


def test_tensor_output_parser_looks_for_click_triggered_markers():
    html = TEMPLATE.read_text(encoding="utf-8")

    assert "[YOLOV8_TENSOR_OUTPUT_BEGIN]" in html
    assert "[YOLOV8_TENSOR_OUTPUT_END]" in html
    assert "frame=100" not in html
    assert "d.tensor_dump" in html
    assert "extractTensorOutput" in html
    assert "updateTensorOutput" in html
