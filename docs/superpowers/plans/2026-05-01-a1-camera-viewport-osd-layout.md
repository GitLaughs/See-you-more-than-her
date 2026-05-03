# A1 Camera Viewport OSD Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the board display use a 1920x1080 OSD canvas with full-screen background/animations and a centered 360x640 camera viewport placed 150px from the left.

**Architecture:** Keep demo-rps OSD layering: background on layer 2, transient animation/prompt on layers 3/4, detection boxes on layer 0. Move all camera-specific coordinates into explicit layout constants, configure the online image path to crop the center 360x640 camera region, and map YOLO boxes only into that viewport. Background and right-side animations stay in full 1920x1080 OSD coordinates.

**Tech Stack:** C++17-style app code in SmartSens A1 SDK, SSNE online pipeline APIs (`OnlineSetCrop`, `OnlineSetOutputImage`, `UpdateOnlineParam`), OSD APIs through `VISUALIZER` and `OsdDevice`, repo build wrappers.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/project_paths.hpp`
  - Owns display, sensor, online crop, camera viewport, and model input constants.
  - Add explicit `CAMERA_VIEW_*` constants and switch source/crop constants to the 720x1280 camera path described by the existing README and YOLO comments.

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp`
  - Owns online pipeline configuration.
  - Add `OnlineSetCrop(kPipeline0, ...)`, output cropped 360x640, call `UpdateOnlineParam()`, and log exact layout.

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
  - Owns UI state and box-to-OSD coordinate mapping.
  - Replace full-screen box mapping with viewport mapping: crop-space boxes become OSD-space boxes inside `(150,220)-(510,860)`.

- No new runtime files.
- No broad refactor.

---

### Task 1: Add explicit camera viewport constants

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/project_paths.hpp:4-43`

- [ ] **Step 1: Replace the resolution block with camera/display split constants**

Change the top resolution constants to this exact structure:

```cpp
/**
 * project_paths.hpp — ssne_ai_demo 全局配置
 *
 * 分辨率设计说明:
 *   - OSD 画布: 1920 × 1080，背景和右侧动画使用全画布绝对坐标
 *   - 摄像头源: 720 × 1280，取中心 360 × 640 作为左侧视频窗口
 *   - 摄像头窗口: OSD 坐标 (150, 220)，尺寸 360 × 640
 *   - 推理输入: 640 × 480，RunAiPreprocessPipe 将摄像头窗口缩放到模型输入
 */
```

Replace constants `SENSOR_WIDTH`, `SENSOR_HEIGHT`, `OSD_WIDTH`, `OSD_HEIGHT`, `PIPE_CROP_*`, `DET_WIDTH`, and `DET_HEIGHT` with:

```cpp
// ─── 摄像头源分辨率 ────────────────────────────────────────────────────────
constexpr int SENSOR_WIDTH  = 720;
constexpr int SENSOR_HEIGHT = 1280;

// ─── OSD 显示画布 ────────────────────────────────────────────────────────────
constexpr int OSD_WIDTH  = 1920;
constexpr int OSD_HEIGHT = 1080;

// ─── 摄像头视频窗口：中心裁剪后贴到背景左侧 ───────────────────────────────────
constexpr int CAMERA_VIEW_X = 150;
constexpr int CAMERA_VIEW_WIDTH = 360;
constexpr int CAMERA_VIEW_HEIGHT = 640;
constexpr int CAMERA_VIEW_Y = (OSD_HEIGHT - CAMERA_VIEW_HEIGHT) / 2;
constexpr int CAMERA_VIEW_RIGHT = CAMERA_VIEW_X + CAMERA_VIEW_WIDTH;
constexpr int CAMERA_VIEW_BOTTOM = CAMERA_VIEW_Y + CAMERA_VIEW_HEIGHT;

// ─── 在线裁剪区域：720×1280 源图中心 360×640 ────────────────────────────────
constexpr int PIPE_CROP_WIDTH  = CAMERA_VIEW_WIDTH;
constexpr int PIPE_CROP_HEIGHT = CAMERA_VIEW_HEIGHT;
constexpr int PIPE_CROP_X1 = (SENSOR_WIDTH - PIPE_CROP_WIDTH) / 2;
constexpr int PIPE_CROP_X2 = PIPE_CROP_X1 + PIPE_CROP_WIDTH;
constexpr int PIPE_CROP_Y1 = (SENSOR_HEIGHT - PIPE_CROP_HEIGHT) / 2;
constexpr int PIPE_CROP_Y2 = PIPE_CROP_Y1 + PIPE_CROP_HEIGHT;

// ─── 当前模型推理输入分辨率 ──────────────────────────────────────────────────
constexpr int DET_WIDTH  = 640;
constexpr int DET_HEIGHT = 480;
```

- [ ] **Step 2: Verify constants by quick mental expansion**

Expected values:

```text
CAMERA_VIEW_X=150
CAMERA_VIEW_Y=220
CAMERA_VIEW_WIDTH=360
CAMERA_VIEW_HEIGHT=640
PIPE_CROP_X1=180
PIPE_CROP_X2=540
PIPE_CROP_Y1=320
PIPE_CROP_Y2=960
```

- [ ] **Step 3: Commit task 1**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/project_paths.hpp
git commit -m "feat: define A1 camera viewport layout"
```

---

### Task 2: Configure online pipeline to output the viewport crop

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp:19-38`

- [ ] **Step 1: Update includes if needed**

`common.hpp` already includes `project_paths.hpp`, so no new include is needed.

- [ ] **Step 2: Replace online configuration body**

In `IMAGEPROCESSOR::Initialize`, replace lines that set `img_width`, `img_height`, and call `OnlineSetOutputImage` with:

```cpp
    uint16_t img_width = static_cast<uint16_t>(cfg::PIPE_CROP_WIDTH);
    uint16_t img_height = static_cast<uint16_t>(cfg::PIPE_CROP_HEIGHT);
    format_online = SSNE_YUV422_16;

    int crop_ret = OnlineSetCrop(kPipeline0,
                                 static_cast<uint16_t>(cfg::PIPE_CROP_X1),
                                 static_cast<uint16_t>(cfg::PIPE_CROP_X2),
                                 static_cast<uint16_t>(cfg::PIPE_CROP_Y1),
                                 static_cast<uint16_t>(cfg::PIPE_CROP_Y2));
    if (crop_ret != 0) {
        printf("[ERROR] OnlineSetCrop failed ret=%d crop=(%d,%d)-(%d,%d)\n",
               crop_ret,
               cfg::PIPE_CROP_X1,
               cfg::PIPE_CROP_Y1,
               cfg::PIPE_CROP_X2,
               cfg::PIPE_CROP_Y2);
        return;
    }

    int output_ret = OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
    if (output_ret != 0) {
        printf("[ERROR] OnlineSetOutputImage failed ret=%d size=%dx%d\n",
               output_ret,
               img_width,
               img_height);
        return;
    }

    int update_ret = UpdateOnlineParam();
    if (update_ret != 0) {
        printf("[ERROR] UpdateOnlineParam failed ret=%d\n", update_ret);
        return;
    }

    printf("[IMAGEPROCESSOR] online crop source=%dx%d crop=(%d,%d)-(%d,%d) output=%dx%d view=(%d,%d,%d,%d)\n",
           cfg::SENSOR_WIDTH,
           cfg::SENSOR_HEIGHT,
           cfg::PIPE_CROP_X1,
           cfg::PIPE_CROP_Y1,
           cfg::PIPE_CROP_X2,
           cfg::PIPE_CROP_Y2,
           img_width,
           img_height,
           cfg::CAMERA_VIEW_X,
           cfg::CAMERA_VIEW_Y,
           cfg::CAMERA_VIEW_WIDTH,
           cfg::CAMERA_VIEW_HEIGHT);
```

Keep existing `OpenOnlinePipeline(kPipeline0)` block after this.

- [ ] **Step 3: Update `GetImage` comments**

Replace comments saying `1920×1080` with:

```cpp
/**
 * @brief 从pipeline获取中心裁剪后的 360×640 摄像头窗口图像
 * @param img_sensor 输出参数：存储从pipe0获取的显示/推理图像
 */
```

And replace inline comment with:

```cpp
    // 从pipe0获取 360×640 在线图像数据
```

- [ ] **Step 4: Commit task 2**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp
git commit -m "feat: crop A1 online camera feed to viewport"
```

---

### Task 3: Map YOLO boxes into the camera viewport only

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp:307-322`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp:458-465`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp:501-502`

- [ ] **Step 1: Replace `to_osd_boxes` signature and body**

Replace the whole function:

```cpp
std::vector<std::array<float, 4>> to_osd_boxes(const FaceDetectionResult& det_result,
                                               float crop_offset_y,
                                               float osd_scale_x,
                                               float osd_scale_y)
{
    std::vector<std::array<float, 4>> boxes_original_coord;
    boxes_original_coord.reserve(det_result.boxes.size());
    for (const auto& box : det_result.boxes) {
        boxes_original_coord.push_back({
            box[0] * osd_scale_x,
            (box[1] + crop_offset_y) * osd_scale_y,
            box[2] * osd_scale_x,
            (box[3] + crop_offset_y) * osd_scale_y,
        });
    }
    return boxes_original_coord;
}
```

With:

```cpp
std::vector<std::array<float, 4>> to_osd_boxes(const FaceDetectionResult& det_result)
{
    constexpr float scale_x = static_cast<float>(cfg::CAMERA_VIEW_WIDTH) /
                              static_cast<float>(cfg::PIPE_CROP_WIDTH);
    constexpr float scale_y = static_cast<float>(cfg::CAMERA_VIEW_HEIGHT) /
                              static_cast<float>(cfg::PIPE_CROP_HEIGHT);

    std::vector<std::array<float, 4>> boxes_in_view;
    boxes_in_view.reserve(det_result.boxes.size());
    for (const auto& box : det_result.boxes) {
        boxes_in_view.push_back({
            cfg::CAMERA_VIEW_X + box[0] * scale_x,
            cfg::CAMERA_VIEW_Y + box[1] * scale_y,
            cfg::CAMERA_VIEW_X + box[2] * scale_x,
            cfg::CAMERA_VIEW_Y + box[3] * scale_y,
        });
    }
    return boxes_in_view;
}
```

- [ ] **Step 2: Remove unused crop/full-screen scale variables in `main`**

Replace:

```cpp
    array<int, 2> crop_shape = {cfg::PIPE_CROP_WIDTH, cfg::PIPE_CROP_HEIGHT};
    const float crop_offset_y = static_cast<float>(cfg::PIPE_CROP_Y1);
    const float osd_scale_x = static_cast<float>(cfg::OSD_WIDTH) /
                              static_cast<float>(cfg::SENSOR_WIDTH);
    const float osd_scale_y = static_cast<float>(cfg::OSD_HEIGHT) /
                              static_cast<float>(cfg::SENSOR_HEIGHT);
```

With:

```cpp
    array<int, 2> crop_shape = {cfg::PIPE_CROP_WIDTH, cfg::PIPE_CROP_HEIGHT};
```

- [ ] **Step 3: Update draw call**

Replace:

```cpp
            visualizer.Draw(to_osd_boxes(*det_result, crop_offset_y, osd_scale_x, osd_scale_y));
```

With:

```cpp
            visualizer.Draw(to_osd_boxes(*det_result));
```

- [ ] **Step 4: Add one startup layout log near background draw**

Before:

```cpp
    cout << "[A1] draw startup background" << endl;
```

Add:

```cpp
    cout << "[A1] OSD canvas=" << cfg::OSD_WIDTH << "x" << cfg::OSD_HEIGHT
         << " camera_view=(" << cfg::CAMERA_VIEW_X << "," << cfg::CAMERA_VIEW_Y
         << "," << cfg::CAMERA_VIEW_WIDTH << "," << cfg::CAMERA_VIEW_HEIGHT << ")"
         << endl;
```

- [ ] **Step 5: Commit task 3**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp
git commit -m "feat: map detections into camera viewport"
```

---

### Task 4: Verify build and expected runtime logs

**Files:**
- No code files.

- [ ] **Step 1: Run C++ incremental build for app**

Run from repo root inside available environment:

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
```

If SDK build must run through container, run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"
```

Expected result:

```text
ssne_ai_demo build completes without compiler errors
```

- [ ] **Step 2: If full firmware packaging needed, build app-only EVB image**

Run only if prior full SDK cache exists:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected result:

```text
output/evb/<timestamp>/ contains updated zImage.smartsens-m1-evb
```

- [ ] **Step 3: Board runtime smoke test**

After flashing/copying updated app, run on board:

```bash
/app_demo/scripts/run.sh
```

Expected startup log snippets:

```text
[IMAGEPROCESSOR] online crop source=720x1280 crop=(180,320)-(540,960) output=360x640 view=(150,220,360,640)
[VISUALIZER] Initialize OSD 1920x1080 lut=/app_demo/app_assets/shared_colorLUT.sscl
[A1] OSD canvas=1920x1080 camera_view=(150,220,360,640)
[A1] draw startup background
[OsdDevice] DrawTexture bitmap=/app_demo/app_assets/background.ssbmp lut=/app_demo/app_assets/shared_colorLUT.sscl layer=2 pos=(0,0)
[OsdDevice] osd_flush_texture_layer succeeded layer=2
```

- [ ] **Step 4: Visual acceptance check**

In Aurora/device preview, verify:

```text
background fills 1920x1080 canvas
camera image appears x=150, y=220, width=360, height=640
YOLO boxes align with camera image only
right-side animation/prompt appears outside camera area and is not clipped by camera crop
```

- [ ] **Step 5: Commit verification notes only if code changed during verification**

If no code changes happened during verification, do not commit.

If verification required fixes, stage exact modified files and commit:

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/project_paths.hpp \
        data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp \
        data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp
git commit -m "fix: align A1 viewport OSD layout"
```

---

## Self-Review

- Spec coverage: plan implements scheme 1: full 1920x1080 OSD canvas, background not cropped, camera viewport at `150,220,360,640`, YOLO boxes mapped only into that viewport, animations stay on full-canvas OSD coordinates.
- Placeholder scan: no `TBD`, `TODO`, or open-ended implementation steps remain.
- Type consistency: all new constants live in `cfg::`; touched files already include `project_paths.hpp` through `common.hpp` or direct include path. `to_osd_boxes` call signature matches replacement body.
- Risk called out: if board video plane ignores online crop for preview/composition, OSD boxes will be correct but camera plane placement may still require lower-level display/ISP configuration not present in current app code. Runtime visual acceptance step catches this.
