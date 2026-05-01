# Video ROI Preview and OSD Asset Conversion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add first-frame preview and mouse drag ROI selection to `tools/video`, convert generated OSD SVG assets into BMP files beside their sources, then commit and push all local work into a PR.

**Architecture:** Extend the existing single-file `tools/video/video_label_tool.py` with a `/api/preview` endpoint that returns the first video frame as a 640x480 data URL plus source dimensions. Update the embedded HTML/JS to load the frame, draw a canvas overlay, support drag-to-box, and keep coordinate inputs synced in source-frame coordinates for fine adjustment. Convert `docs/osd_assets/*.svg` to same-directory `.bmp` files so the user can manually convert them with the SmartSens OSD toolchain.

**Tech Stack:** Python standard library HTTP server, OpenCV, browser canvas, Python SVG conversion tool if available, Git/GitHub CLI.

---

## Tasks

- [x] Inspect current `tools/video/video_label_tool.py` and existing `.ssbmp` format enough to choose conversion output.
- [x] Add first-frame preview API and mouse drag box UI, keeping 640x480 preview and source-coordinate inputs in sync.
- [x] Convert `docs/osd_assets/*.svg` to BMP assets in the same directory; user will manually convert BMP to `.ssbmp`.
- [x] Update docs to explain preview/drag workflow and BMP conversion output.
- [x] Verify Python scripts compile and generated assets exist.
- [ ] Stage all local changes requested by user, commit, push branch, and update/create PR.

## Verification

```bash
python -m py_compile tools/video/video_label_tool.py tools/yolo/split_dataset.py
```

Expected: no output.
