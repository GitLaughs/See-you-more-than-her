import pathlib
import unittest


class DemoFaceOsdConfigTest(unittest.TestCase):
    def test_visualizer_uses_default_lut_without_bitmap_overlay(self):
        source = pathlib.Path(
            "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("visualizer.Initialize(img_shape);", source)
        self.assertNotIn('visualizer.Initialize(img_shape, "shared_colorLUT.sscl");', source)


if __name__ == "__main__":
    unittest.main()
