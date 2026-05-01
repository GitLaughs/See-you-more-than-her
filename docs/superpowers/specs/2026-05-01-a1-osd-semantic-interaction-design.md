# A1 OSD semantic interaction migration design

## Goal

Port useful demo-rps interaction ideas into `ssne_ai_demo`: stabilized visual semantics, product-style OSD bitmaps, and control-friendly state output.

## Scope

Target app: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`.

Assets source: `docs/osd_assets/a1TextureOutput/`.

In scope:

- Copy generated `.ssbmp` assets and `shared_colorLUT.sscl` into `app_assets/`.
- Expand OSD image support from one bitmap layer to layered semantic UI.
- Add stable semantic state before OSD and chassis control.
- Use 5-frame average for gesture/action locking.
- Keep existing YOLOv8 detection backend and chassis UART path.

Out of scope:

- Retraining or converting models.
- Aurora web UI changes.
- Changing camera crop/inference resolution.

## OSD layer design

Use five OSD layers:

- layer 0: detection boxes.
- layer 1: reserved fixed graphics.
- layer 2: long-lived status/background.
- layer 3: action animation.
- layer 4: transient prompt/alert.

`VISUALIZER` should expose layer clearing so state changes can clean layer 3 and layer 4 before drawing new animation/prompt frames.

## Semantic state

Introduce a small state object for downstream OSD, logs, Aurora, and chassis control:

```text
label
confidence
NoTarget
target_locked
action_hint
safe_to_move
```

Labels map from current YOLO classes:

- `person`
- `forward`
- `stop`
- `obstacle`
- `NoTarget`

Action hints:

- `none`
- `hello`
- `forward`
- `stop`
- `avoid_left`
- `avoid_right`
- `blocked`

## Stabilization

For each frame, extract best confidence for person, forward, stop, and obstacle. Maintain a 5-frame ring buffer. Average confidence by class over the buffer.

Decision rules:

1. If all averaged confidences are below threshold, output `NoTarget`.
2. Otherwise choose class by priority and average confidence: obstacle > stop > forward > person.
3. Require consecutive confirmed frames before setting `target_locked`.
4. Hold locked state briefly after lock to avoid rapid flips.
5. Obstacles also use existing area/bottom/center rules for avoid direction.

Chassis control consumes stabilized `action_hint`, not raw single-frame detections.

## OSD behavior

- `NoTarget`: clear layer 3/4; show no transient animation.
- `person`: draw `hello_bubble.ssbmp` and optionally `hello_icon.ssbmp` on layer 4.
- `forward`: loop `car_forward_0.ssbmp` to `car_forward_3.ssbmp` on layer 3; `safe_to_move=true`.
- `stop`: loop `car_stop_0.ssbmp` to `car_stop_2.ssbmp` on layer 3; `safe_to_move=false`.
- `obstacle`: draw `obstacle_alert.ssbmp` on layer 4 and loop `car_detour_0.ssbmp` to `car_detour_5.ssbmp` on layer 3; `safe_to_move=false`.

Use `shared_colorLUT.sscl` for bitmap LUT.

## Integration points

- `osd-device.hpp/cpp`: expand layer count to 5 and create image layers for layer 2/3/4.
- `utils.hpp/cpp`: add bitmap layer clearing and keep `DrawBitmap` path semantics.
- `demo_face.cpp`: add semantic stabilization, asset selection, animation frame update, and chassis control based on stable state.
- `app_assets/`: add converted `.ssbmp` and LUT assets.

## Verification

- Verify copied asset names exist in `app_assets/`.
- Build `ssne_ai_demo` with `bash scripts/build_incremental.sh sdk ssne_ai_demo` if SDK cache/container exists.
- If incremental build cannot run in current environment, report exact blocker.
