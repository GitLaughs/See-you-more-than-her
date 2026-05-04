# A1 YOLO Snapshot Overlay Design

## Goal

Add an Aurora button that captures the current PC preview frame, asks the A1 board for its latest YOLO detection snapshot, overlays the board-reported boxes on the saved preview frame, and shows the result in the Aurora UI.

This is a diagnostic path for checking whether board-side YOLO boxes are plausible without transferring full images from the board.

## Scope

In scope:
- Aurora frontend button and result panel.
- Aurora backend endpoint that captures the latest preview frame, sends an `A1_TEST` command over COM13, parses the A1 response, draws boxes, and returns the result.
- A1 board command `A1_TEST yolo_snapshot` that returns the latest main-loop detection result.

Out of scope:
- Pixel-perfect same-frame synchronization.
- SCP/SSH image transfer from the board.
- JPEG/PNG encoding on the board.
- Changing model, thresholds, or YOLO postprocess behavior.

## Data Flow

1. User clicks `A1 拍照检测` in Aurora.
2. Aurora backend snapshots the most recent preview frame from its camera path.
3. Aurora sends `A1_TEST yolo_snapshot` over COM13.
4. A1 returns one `A1_DEBUG` JSON line with latest detection metadata.
5. Aurora parses objects from the JSON, draws them on the saved preview frame, saves a JPEG under the Aurora output directory, and returns image metadata plus detection summary.
6. UI displays the annotated image, frame index, target count, and object list.

The preview frame and board detection frame are approximate, not guaranteed pixel-identical. The UI must say this explicitly.

## A1 Board Interface

Command:

```text
A1_TEST yolo_snapshot
```

Response format:

```text
A1_DEBUG {"command":"yolo_snapshot","success":true,"frame":1234,"count":2,"camera_w":720,"camera_h":1280,"objects":[{"class_id":2,"class":"forward","score":0.83,"box":[120.0,300.0,260.0,520.0]}],"message":"latest detection snapshot"}
```

Behavior:
- Do not grab a new frame in the command handler.
- Do not run extra inference in the command handler.
- Return the latest detection result copied from the main loop.
- Protect shared latest snapshot state with a mutex because keyboard command handling runs in a separate thread.
- If no detection frame has been produced yet, return `success:false` with a clear message.

Coordinates:
- Board reports boxes in the 720×1280 camera coordinate system used by `det_result.boxes`.
- Aurora draws those boxes directly after resizing to its captured frame dimensions if needed.

## Aurora Backend

Add endpoint:

```text
POST /api/a1/yolo_snapshot
```

Endpoint steps:
1. Read the latest display frame from the existing camera object.
2. Send `A1_TEST yolo_snapshot` using existing serial terminal helpers.
3. Wait for `A1_DEBUG` containing `"command":"yolo_snapshot"`.
4. Parse JSON payload from returned line.
5. If board response fails, return JSON error and preserve recent serial lines for diagnosis.
6. Draw boxes on the captured frame using existing class colors/names where possible.
7. Save annotated JPEG to `output_dir` with a timestamped filename.
8. Append result to existing recent captures gallery if appropriate.
9. Return `success`, filename/path, thumbnail/base64 image, frame index, count, objects, and any warning text.

Error cases:
- COM13 unavailable or not connected.
- A1 command timeout.
- A1 response malformed.
- No camera frame available.
- A1 reports no snapshot yet.

## Aurora Frontend

Add UI under `摄像头与拍照`:
- Button: `A1 拍照检测`.
- Result panel showing:
  - annotated image
  - A1 frame index
  - target count
  - object list with class, score, and box
  - warning: `预览帧与板端检测帧为近似同步，不保证像素级同帧`

Frontend behavior:
- Disable or show busy state while request is in flight.
- Use toast for success/failure.
- Render server error details if available.

## Verification

Run:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py
```

Manual checks:
- Page loads and button is visible.
- COM13 disconnected path shows useful error.
- Unsupported or malformed A1 response shows useful error.
- Valid `A1_DEBUG yolo_snapshot` response produces annotated image.
- Box scaling looks correct for 720×1280 frame coordinates.
