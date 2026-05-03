# Aurora OSD debug bridge and demo-rps board display design

## Goal

Rework Aurora and the A1 board app into one fast verification loop:

- Aurora shows a horizontal official-style `1920x1080` OSD preview.
- Aurora exposes Chinese debug buttons for board connectivity and status checks.
- Board-side OSD returns to the official demo-rps layer model.
- Board-side debug commands return machine-readable status through the existing COM13 / `A1_TEST` path.
- The final step rebuilds the board app so the user can flash/check visual output.

## Scope

In scope:

- `tools/aurora/aurora_companion.py`
- `tools/aurora/serial_terminal.py`
- `tools/aurora/templates/companion_ui.html`
- Board app under `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
- Build verification through existing scripts and Docker container

Out of scope:

- Replacing the Qt camera bridge implementation.
- Adding a board-side HTTP/WebSocket server.
- Rebuilding OSD assets from source art.
- Reintroducing the previous vertical camera viewport layout on the board.
- Direct PC-to-STM32 debug flow; that remains in `tools/PC/`.

## Architecture

Use the existing Aurora Flask app and serial terminal as the host-side control plane. Keep camera acquisition in the current OpenCV/Qt bridge path, and add an official-style preview mode on top of the browser UI.

Use the board app as the source of truth for actual display behavior. The board outputs the official demo-rps `1920x1080` composition: background on layer 2, transient game/status bitmaps on layers 3/4, with chassis control still driven by stabilized R/P/S labels.

Use COM13 and `A1_TEST` for debug transport. Aurora sends whitelisted commands through `serial_terminal.py`; the board prints one-line `A1_DEBUG` responses that the frontend can show and optionally parse.

## Aurora UI design

The UI becomes a two-column horizontal workstation:

- Left column: 16:9 OSD preview area.
  - Stream remains the live A1 camera feed.
  - The preview area is framed as `1920x1080` to match board OSD behavior.
  - A browser overlay approximates the official background/camera-window composition.
  - This overlay is a verification aid, not a substitute for board-side OSD.
- Right column: debug and connectivity panel.
  - Shows COM13 state, A1_TEST response state, OSD status, and UART/chassis status.
  - Adds Chinese buttons:
    - `连通测试` -> `ping`
    - `OSD状态` -> `osd_status`
    - `串口状态` -> `uart_status`
    - `前进测试` -> `chassis_test forward`
    - `后退测试` -> `chassis_test backward`
    - `停止` -> `chassis_test stop`
  - Shows latest response log with raw text preserved.

Keep existing camera controls, capture buttons, model switching, and advanced serial terminal available. Do not remove manual serial input; hide no capability that is useful during bench debug.

## Board OSD design

Return board display to the official demo-rps model:

- `img_shape = {1920, 1080}`.
- `VISUALIZER::Initialize(img_shape, "shared_colorLUT.sscl")`.
- Draw `background.ssbmp` at `(0,0)` on layer 2 once after startup delay.
- Use layer 3/4 for transient ready/countdown/RPS/result/status bitmaps.
- Keep P/R/S -> chassis mapping:
  - `P` -> forward
  - `R` -> stop
  - `S` -> backward
- Do not adapt the board to vertical preview screenshots. If host preview is vertical or cropped, fix the host display path.

Add startup and draw diagnostics around:

- `VISUALIZER::Initialize` canvas size and LUT path.
- `DrawBitmap` bitmap path, LUT path, layer, and position.
- `osd_add_texture_layer` return value.
- `osd_flush_texture_layer` return value.
- Background drawn flag used by `osd_status`.

## Debug command design

Debug transport uses the existing COM13 serial terminal path:

`Aurora frontend -> serial_terminal.py -> COM13 -> A1_TEST -> board debug handler -> serial output -> Aurora response log`

Supported first-version commands:

- `ping`
  - Returns app uptime/frame count and `pong`.
- `osd_status`
  - Returns canvas size, background drawn flag, last layer 2 add/flush return values.
- `uart_status`
  - Returns chassis initialization state, last telemetry snapshot, last TX/RX status if available.
- `chassis_test stop|forward|backward`
  - Sends a short safe test command and returns result.
  - `stop` sends zero velocity.
  - `forward` and `backward` use the same conservative velocities as the runtime gesture mapping, then stop after the test interval if the implementation has a blocking test helper; otherwise send a single command and expose status.

Response format:

```text
A1_DEBUG {"command":"osd_status","success":true,"canvas":"1920x1080","background":true}
```

The prefix stays human-greppable. The JSON body lets Aurora parse success and fields later without changing the transport.

## Safety and error handling

- Aurora debug buttons send only whitelisted commands.
- Manual serial terminal remains available for advanced checks.
- `chassis_test` defaults to safe, short commands and includes a visible `停止` button.
- If COM13 is disconnected, buttons show a connection error instead of pretending to send.
- If no expected `A1_DEBUG` line arrives before timeout, the UI shows timeout and preserves any raw serial output.
- Board OSD failures print exact API return values rather than only screenshots.

## Verification plan

Host-side checks:

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py
```

Manual Aurora checks:

- Launch `tools/aurora/launch.ps1`.
- Verify page uses horizontal 16:9 OSD preview.
- Verify Chinese debug buttons exist.
- Connect COM13.
- Click `连通测试`, `OSD状态`, `串口状态`, and `停止`; confirm response log updates.

Board build:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Fallback if app-only cache is missing:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
```

Board/runtime checks after flash:

- Startup log shows OSD init with `1920x1080`.
- Background draw log shows layer 2 add/flush success.
- Aurora OSD preview is horizontal and no longer implies a vertical board output.
- Debug buttons return `A1_DEBUG` lines.
- P/R/S chassis behavior still works.

## Implementation notes

Keep edits narrow. Do not rewrite camera bridge lifecycle code unless required by UI integration. Prefer adding a small board debug handler around existing runtime state over scattering command parsing across unrelated files. Keep status state in simple structs guarded by existing runtime flow; do not introduce long-lived background service threads unless required by the current A1_TEST integration.
