# A1 OSD Semantic Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port demo-rps semantic OSD behavior into A1 `ssne_ai_demo`, using stabilized visual state and `.ssbmp` product UI assets.

**Architecture:** Keep current YOLOv8 detector and chassis controller. Add small semantic stabilizer in `demo_face.cpp`, expand OSD to five layers, copy `.ssbmp` assets into `app_assets`, and route OSD/chassis through stable `VisionUiState`.

**Tech Stack:** C++17-style project code, SmartSens SSNE/OSD APIs, existing `VISUALIZER`, existing incremental SDK build scripts.

---

## File Structure

- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
  - Increase OSD layer count from 3 to 5.
  - Add layer clearing API.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`
  - Create TYPE_IMAGE layers for layer 2, 3, and 4.
  - Implement layer clearing.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
  - Expose `VISUALIZER::ClearLayer(int layer_id)`.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`
  - Forward clear calls to `OsdDevice`.
- Modify `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
  - Add `VisionUiState`, 5-frame average stabilizer, OSD animation selection, stable chassis action.
- Copy from `docs/osd_assets/a1TextureOutput/` to `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/`
  - `hello_bubble.ssbmp`, `hello_icon.ssbmp`, `obstacle_alert.ssbmp`
  - `car_forward_0.ssbmp` through `car_forward_3.ssbmp`
  - `car_stop_0.ssbmp` through `car_stop_2.ssbmp`
  - `car_detour_0.ssbmp` through `car_detour_5.ssbmp`
  - `shared_colorLUT.sscl`

---

### Task 1: Copy OSD assets

**Files:**
- Copy from: `docs/osd_assets/a1TextureOutput/*`
- Copy to: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/`

- [ ] **Step 1: Copy exact assets**

Run:

```bash
cp "docs/osd_assets/a1TextureOutput/"*.ssbmp "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/" && cp "docs/osd_assets/a1TextureOutput/shared_colorLUT.sscl" "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/"
```

Expected: command exits 0.

- [ ] **Step 2: Verify asset names**

Run:

```bash
ls "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/" | sort | grep -E 'hello_|car_forward_|car_stop_|car_detour_|obstacle_alert|shared_colorLUT'
```

Expected includes all copied files:

```text
car_detour_0.ssbmp
car_detour_1.ssbmp
car_detour_2.ssbmp
car_detour_3.ssbmp
car_detour_4.ssbmp
car_detour_5.ssbmp
car_forward_0.ssbmp
car_forward_1.ssbmp
car_forward_2.ssbmp
car_forward_3.ssbmp
car_stop_0.ssbmp
car_stop_1.ssbmp
car_stop_2.ssbmp
hello_bubble.ssbmp
hello_icon.ssbmp
obstacle_alert.ssbmp
shared_colorLUT.sscl
```

---

### Task 2: Expand OSD layers and add clearing API

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`

- [ ] **Step 1: Update header**

Change `OSD_LAYER_SIZE` and add method declaration:

```cpp
#define OSD_LAYER_SIZE 5
```

Add public method near `DrawTexture`:

```cpp
    void ClearLayer(int layer_id);
```

- [ ] **Step 2: Replace image layer creation loop**

In `OsdDevice::Initialize`, replace single layer-2 image block with loop:

```cpp
    for (int layer_index = 2; layer_index < OSD_LAYER_SIZE; layer_index++) {
        int texture_dma_size = 0x20000;
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma, texture_dma_size);
        sleep(0.25);
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma_2, texture_dma_size);
        int dma_fd = osd_get_buffer_fd(m_osd_handle, m_layer_dma[layer_index].dma);

        LAYER_ATTR_S osd_layer;
        osd_layer.codeTYPE = SS_TYPE_RLE;
        osd_layer.layer_data_RLE.osd_buf.buf_type = BUFFER_TYPE_DMABUF;
        osd_layer.layer_data_RLE.osd_buf.buf.fd_dmabuf = dma_fd;
        osd_layer.layerStart.layer_start_x = 0;
        osd_layer.layerStart.layer_start_y = 0;
        osd_layer.layerSize.layer_width = m_width;
        osd_layer.layerSize.layer_height = m_height;
        osd_layer.layer_rgn = {TYPE_IMAGE, {m_width, m_height}};

        int ret = osd_create_layer(m_osd_handle, (ssLAYER_HANDLE)layer_index, &osd_layer);
        if (ret != 0) {
            std::cerr << "[OsdDevice] ERROR: osd_create_layer failed! ret=" << ret
                      << ", layer_index=" << layer_index << std::endl;
        }

        ret = osd_set_layer_buffer(m_osd_handle, (ssLAYER_HANDLE)layer_index, m_layer_dma[layer_index]);
        if (ret != 0) {
            std::cerr << "[OsdDevice] ERROR: osd_set_layer_buffer failed! ret=" << ret
                      << ", layer_index=" << layer_index << std::endl;
        }
    }
```

- [ ] **Step 3: Implement `ClearLayer`**

Add near `DrawTexture`:

```cpp
void OsdDevice::ClearLayer(int layer_id) {
    if (layer_id < 0 || layer_id >= OSD_LAYER_SIZE) {
        return;
    }
    osd_clean_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
}
```

- [ ] **Step 4: Compile check target later**

Do not build yet. Build after all code changes to avoid repeated SDK cycles.

---

### Task 3: Expose visualizer layer clearing

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`

- [ ] **Step 1: Add `VISUALIZER::ClearLayer` declaration**

In `utils.hpp`, add after `DrawBitmap`:

```cpp
    void ClearLayer(int layer_id);
```

- [ ] **Step 2: Add implementation**

In `utils.cpp`, add after `DrawBitmap`:

```cpp
void VISUALIZER::ClearLayer(int layer_id) {
    osd_device.ClearLayer(layer_id);
}
```

---

### Task 4: Add semantic stabilization structures

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`

- [ ] **Step 1: Add includes**

Add near existing includes:

```cpp
#include <array>
#include <deque>
```

- [ ] **Step 2: Add semantic enums and structs**

Add after `ActionState`:

```cpp
enum class SemanticLabel {
    NoTarget,
    Person,
    Forward,
    Stop,
    Obstacle,
};

struct FrameScores {
    float person = 0.f;
    float forward = 0.f;
    float stop = 0.f;
    float obstacle = 0.f;
};

struct VisionUiState {
    SemanticLabel label = SemanticLabel::NoTarget;
    float confidence = 0.f;
    bool NoTarget = true;
    bool target_locked = false;
    ActionState action_hint = ActionState::Idle;
    bool safe_to_move = false;
};

struct SemanticStabilizer {
    std::deque<FrameScores> history;
    SemanticLabel candidate = SemanticLabel::NoTarget;
    SemanticLabel locked = SemanticLabel::NoTarget;
    int candidate_frames = 0;
    int hold_frames = 0;
};
```

- [ ] **Step 3: Add constants**

Add in anonymous namespace before helper functions:

```cpp
constexpr int kSemanticAverageFrames = 5;
constexpr int kSemanticLockFrames = 3;
constexpr int kSemanticHoldFrames = 6;
constexpr float kSemanticNoTargetThreshold = 0.35f;
constexpr int kLayerDetection = 0;
constexpr int kLayerAnimation = 3;
constexpr int kLayerPrompt = 4;
```

---

### Task 5: Add semantic helpers

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`

- [ ] **Step 1: Add label name helper**

Add in anonymous namespace:

```cpp
const char* semantic_label_name(SemanticLabel label) {
    switch (label) {
        case SemanticLabel::NoTarget: return "NoTarget";
        case SemanticLabel::Person: return "person";
        case SemanticLabel::Forward: return "forward";
        case SemanticLabel::Stop: return "stop";
        case SemanticLabel::Obstacle: return "obstacle";
    }
    return "unknown";
}
```

- [ ] **Step 2: Add frame score extraction**

Add in anonymous namespace:

```cpp
FrameScores extract_frame_scores(const FaceDetectionResult& det_result) {
    FrameScores scores;
    for (size_t i = 0; i < det_result.class_ids.size() && i < det_result.scores.size(); ++i) {
        const float score = det_result.scores[i];
        switch (det_result.class_ids[i]) {
            case cfg::TARGET_CLASS_PERSON:
                scores.person = std::max(scores.person, score);
                break;
            case cfg::TARGET_CLASS_FORWARD:
                scores.forward = std::max(scores.forward, score);
                break;
            case cfg::TARGET_CLASS_STOP:
                scores.stop = std::max(scores.stop, score);
                break;
            case cfg::TARGET_CLASS_OBSTACLE_BOX:
                scores.obstacle = std::max(scores.obstacle, score);
                break;
        }
    }
    return scores;
}
```

- [ ] **Step 3: Add averaging helper**

Add in anonymous namespace:

```cpp
FrameScores average_scores(const std::deque<FrameScores>& history) {
    FrameScores avg;
    if (history.empty()) {
        return avg;
    }

    for (const auto& scores : history) {
        avg.person += scores.person;
        avg.forward += scores.forward;
        avg.stop += scores.stop;
        avg.obstacle += scores.obstacle;
    }

    const float inv = 1.0f / static_cast<float>(history.size());
    avg.person *= inv;
    avg.forward *= inv;
    avg.stop *= inv;
    avg.obstacle *= inv;
    return avg;
}
```

- [ ] **Step 4: Add best semantic selection**

Add in anonymous namespace:

```cpp
SemanticLabel choose_semantic_label(const FrameScores& scores, float* confidence) {
    *confidence = scores.obstacle;
    if (scores.obstacle >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Obstacle;
    }

    *confidence = scores.stop;
    if (scores.stop >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Stop;
    }

    *confidence = scores.forward;
    if (scores.forward >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Forward;
    }

    *confidence = scores.person;
    if (scores.person >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Person;
    }

    *confidence = 0.f;
    return SemanticLabel::NoTarget;
}
```

---

### Task 6: Convert stabilized label to UI/control state

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`

- [ ] **Step 1: Add action conversion helper**

Add in anonymous namespace after `decide_action`:

```cpp
ActionState semantic_action_hint(SemanticLabel label, ActionState obstacle_action) {
    switch (label) {
        case SemanticLabel::Forward:
            return ActionState::Forward;
        case SemanticLabel::Stop:
            return ActionState::StopGesture;
        case SemanticLabel::Obstacle:
            if (obstacle_action == ActionState::AvoidLeft ||
                obstacle_action == ActionState::AvoidRight ||
                obstacle_action == ActionState::Blocked) {
                return obstacle_action;
            }
            return ActionState::Blocked;
        case SemanticLabel::Person:
        case SemanticLabel::NoTarget:
            return ActionState::Idle;
    }
    return ActionState::Idle;
}
```

- [ ] **Step 2: Add stabilizer update helper**

Add in anonymous namespace:

```cpp
VisionUiState update_semantic_state(SemanticStabilizer* stabilizer,
                                    const FaceDetectionResult& det_result,
                                    ActionState obstacle_action) {
    stabilizer->history.push_back(extract_frame_scores(det_result));
    while (static_cast<int>(stabilizer->history.size()) > kSemanticAverageFrames) {
        stabilizer->history.pop_front();
    }

    const FrameScores avg = average_scores(stabilizer->history);
    float confidence = 0.f;
    const SemanticLabel candidate = choose_semantic_label(avg, &confidence);

    if (candidate == stabilizer->candidate) {
        stabilizer->candidate_frames += 1;
    } else {
        stabilizer->candidate = candidate;
        stabilizer->candidate_frames = 1;
    }

    if (candidate != SemanticLabel::NoTarget && stabilizer->candidate_frames >= kSemanticLockFrames) {
        stabilizer->locked = candidate;
        stabilizer->hold_frames = kSemanticHoldFrames;
    } else if (stabilizer->hold_frames > 0) {
        stabilizer->hold_frames -= 1;
    } else if (candidate == SemanticLabel::NoTarget) {
        stabilizer->locked = SemanticLabel::NoTarget;
    }

    VisionUiState state;
    state.label = stabilizer->locked;
    state.confidence = confidence;
    state.NoTarget = state.label == SemanticLabel::NoTarget;
    state.target_locked = !state.NoTarget;
    state.action_hint = semantic_action_hint(state.label, obstacle_action);
    state.safe_to_move = state.action_hint == ActionState::Forward ||
                         state.action_hint == ActionState::AvoidLeft ||
                         state.action_hint == ActionState::AvoidRight;
    return state;
}
```

---

### Task 7: Add OSD animation rendering

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`

- [ ] **Step 1: Add render helper**

Add in anonymous namespace:

```cpp
void render_semantic_osd(VISUALIZER* visualizer,
                         const VisionUiState& state,
                         uint64_t frame_index,
                         SemanticLabel* last_label) {
    if (state.label != *last_label) {
        visualizer->ClearLayer(kLayerAnimation);
        visualizer->ClearLayer(kLayerPrompt);
        *last_label = state.label;
    }

    switch (state.label) {
        case SemanticLabel::NoTarget:
            visualizer->ClearLayer(kLayerAnimation);
            visualizer->ClearLayer(kLayerPrompt);
            break;
        case SemanticLabel::Person:
            visualizer->DrawBitmap("hello_bubble.ssbmp", "shared_colorLUT.sscl", 260, 40, kLayerPrompt);
            visualizer->DrawBitmap("hello_icon.ssbmp", "shared_colorLUT.sscl", 220, 52, kLayerPrompt);
            break;
        case SemanticLabel::Forward: {
            const int idx = static_cast<int>((frame_index / 5) % 4);
            visualizer->DrawBitmap("car_forward_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 160, 280, kLayerAnimation);
            break;
        }
        case SemanticLabel::Stop: {
            const int idx = static_cast<int>((frame_index / 5) % 3);
            visualizer->DrawBitmap("car_stop_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 160, 280, kLayerAnimation);
            break;
        }
        case SemanticLabel::Obstacle: {
            const int idx = static_cast<int>((frame_index / 5) % 6);
            visualizer->DrawBitmap("obstacle_alert.ssbmp", "shared_colorLUT.sscl", 80, 40, kLayerPrompt);
            visualizer->DrawBitmap("car_detour_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 80, 190, kLayerAnimation);
            break;
        }
    }
}
```

---

### Task 8: Wire stable state into main loop

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`

- [ ] **Step 1: Remove old test bitmap cycle**

Delete `osdInfo` static test array and the initial draw/cycling code:

```cpp
    static osdInfo osds[3] = {
        {"si.ssbmp", 10, 10},
        {"te.ssbmp", 90, 10},
        {"wei.ssbmp", 170, 10}
    };
```

Delete:

```cpp
    visualizer.DrawBitmap(osds[0].filename, "shared_colorLUT.sscl", osds[0].x, osds[0].y, 2);
```

Delete:

```cpp
        osd_index = (num_frames / 10) % 3;
        visualizer.DrawBitmap(osds[osd_index].filename, "shared_colorLUT.sscl", osds[osd_index].x, osds[osd_index].y, 2);
```

- [ ] **Step 2: Add runtime state objects**

After `RuntimeState runtime;` add:

```cpp
    SemanticStabilizer semantic_stabilizer;
    VisionUiState ui_state;
    SemanticLabel last_osd_label = SemanticLabel::NoTarget;
```

- [ ] **Step 3: Replace action decision path**

Replace existing `next_action` block:

```cpp
        const ActionState next_action = decide_action(
            *det_result,
            &runtime,
            static_cast<float>(crop_shape[0]),
            static_cast<float>(crop_shape[1]));
        runtime.action = next_action;
        runtime.frame_index += 1;
```

With:

```cpp
        const ActionState raw_action = decide_action(
            *det_result,
            &runtime,
            static_cast<float>(crop_shape[0]),
            static_cast<float>(crop_shape[1]));
        ui_state = update_semantic_state(&semantic_stabilizer, *det_result, raw_action);
        runtime.action = ui_state.action_hint;
        runtime.frame_index += 1;
        render_semantic_osd(&visualizer, ui_state, runtime.frame_index, &last_osd_label);
```

- [ ] **Step 4: Update status log**

Replace log format with semantic fields:

```cpp
            printf("[A1] frame=%llu label=%s conf=%.3f locked=%d action=%s safe=%d det=%d obstacle=%.3f center=%.2f bottom=%.2f vx=%d vz=%d tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   semantic_label_name(ui_state.label),
                   ui_state.confidence,
                   ui_state.target_locked ? 1 : 0,
                   action_name(runtime.action),
                   ui_state.safe_to_move ? 1 : 0,
                   runtime.det_count,
                   runtime.obstacle.area_ratio,
                   runtime.obstacle.center_x_ratio,
                   runtime.obstacle.bottom_ratio,
                   vx,
                   vz,
                   chassis_state.vx,
                   chassis_state.volt);
```

---

### Task 9: Build and inspect changes

**Files:**
- Verify all modified files.

- [ ] **Step 1: Run incremental build**

Run:

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
```

Expected: build completes without C++ compile errors.

- [ ] **Step 2: If incremental build lacks SDK cache**

Run:

```bash
bash scripts/build_docker.sh --skip-ros
```

Expected: build completes, or reports missing Docker/container setup. If Docker is unavailable, record exact error.

- [ ] **Step 3: Check git diff**

Run:

```bash
git diff -- data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo docs/superpowers/specs docs/superpowers/plans
```

Expected: diff shows only OSD assets, OSD layer changes, semantic stabilization, design spec, and this plan.

---

## Self-Review

Spec coverage:

- Asset copy covered by Task 1.
- 5-layer OSD covered by Task 2 and Task 3.
- Semantic state covered by Task 4 through Task 6.
- 5-frame averaging and lock behavior covered by Task 5 and Task 6.
- Product OSD behavior covered by Task 7 and Task 8.
- Chassis stable-control integration covered by Task 8.
- Verification covered by Task 9.

Placeholder scan: no TBD/TODO/fill-later placeholders.

Type consistency: `SemanticLabel`, `FrameScores`, `VisionUiState`, `SemanticStabilizer`, `ClearLayer`, `render_semantic_osd`, and `update_semantic_state` signatures match across tasks.
