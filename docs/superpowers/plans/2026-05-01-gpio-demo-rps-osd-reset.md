# GPIO + Demo-RPS OSD Reset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the board app to the known-good demo-rps video/OSD pipeline, make the full background render correctly, ensure GPIO is loaded before app startup, and keep only `P/R/S -> forward/stop/backward` chassis control.

**Architecture:** Treat demo-rps as the display/pipeline source of truth: same `1920x1080` video path, same OSD layer setup, same background assets, same classifier model path. Then replace only the game-phase logic in `demo_rps_game.cpp` with a minimal stabilized gesture-to-chassis mapping loop, while loading `gpio_kmod.ko` before the app starts so `ChassisController` can open `/dev/gpiodev`.

**Tech Stack:** SmartSens A1 SDK C++, SSNE, demo-rps `RPS_CLASSIFIER`, OSD bitmap layers, GPIO/UART chassis transport, Buildroot rootfs overlay, Docker EVB build.

---

## File Structure

**Modify:**
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/board/m1pro/rootfs_overlay/usr/smartsoc/smartsoc_start.sh`
  - Load `gpio_kmod.ko` before launching `app_demo`.
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt`
  - Build `demo_rps_game.cpp` and `rps_classifier.cpp`, keep `chassis_controller.cpp`, UART/GPIO libs.
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
  - Replace guess-game state machine with minimal `P/R/S -> forward/stop/backward` loop.

**Replace from demo-rps source of truth:**
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/log.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/rps_classifier.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/background.ssbmp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/shared_colorLUT.sscl`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/1.ssbmp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/models/model_rps.m1model`

**Preserve from current app_demo:**
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp`

**Delete from app_demo:**
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/scrfd_gray.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_gray.cpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp`
- old app-only assets no longer used by demo-rps baseline

---

### Task 1: Load GPIO module before app startup

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/board/m1pro/rootfs_overlay/usr/smartsoc/smartsoc_start.sh`

- [ ] **Step 1: Add `gpio_kmod.ko` load next to the other kernel modules**

Replace this section:

```sh
insmod ${ko_dir}/uart_kmod.ko

cd app_demo
./scripts/run.sh
```

with:

```sh
insmod ${ko_dir}/uart_kmod.ko
insmod ${ko_dir}/gpio_kmod.ko

cd app_demo
./scripts/run.sh
```

- [ ] **Step 2: Verify startup script contains the module load**

Run:

```bash
grep -n "gpio_kmod\.ko\|uart_kmod\.ko\|run.sh" data/A1_SDK_SC132GS/smartsens_sdk/smart_software/board/m1pro/rootfs_overlay/usr/smartsoc/smartsoc_start.sh
```

Expected output includes:

```text
insmod ${ko_dir}/uart_kmod.ko
insmod ${ko_dir}/gpio_kmod.ko
./scripts/run.sh
```

- [ ] **Step 3: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/board/m1pro/rootfs_overlay/usr/smartsoc/smartsoc_start.sh
git commit -m "fix: load gpio module before app demo"
```

---

### Task 2: Reset app_demo display stack to demo-rps source of truth

**Files:**
- Modify by replacement:
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/log.hpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/rps_classifier.cpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp`
- Preserve after copy:
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp`
  - `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp`
- Replace assets:
  - `.../app_assets/background.ssbmp`
  - `.../app_assets/shared_colorLUT.sscl`
  - `.../app_assets/1.ssbmp`
  - `.../app_assets/models/model_rps.m1model`

- [ ] **Step 1: Back up chassis files before copying demo-rps tree**

Run:

```bash
cp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp /tmp/chassis_controller.hpp
cp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp /tmp/chassis_controller.cpp
```

Expected: both files copied successfully.

- [ ] **Step 2: Replace display/pipeline files from demo-rps**

Run:

```bash
cp demo-rps/ssne_ai_demo/include/common.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp
cp demo-rps/ssne_ai_demo/include/log.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/log.hpp
cp demo-rps/ssne_ai_demo/include/osd-device.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp
cp demo-rps/ssne_ai_demo/include/utils.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp
cp demo-rps/ssne_ai_demo/src/osd-device.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp
cp demo-rps/ssne_ai_demo/src/pipeline_image.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp
cp demo-rps/ssne_ai_demo/src/rps_classifier.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/rps_classifier.cpp
cp demo-rps/ssne_ai_demo/src/utils.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp
```

Expected: files replaced without error.

- [ ] **Step 3: Restore chassis files after copying demo-rps files**

Run:

```bash
cp /tmp/chassis_controller.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/chassis_controller.hpp
cp /tmp/chassis_controller.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/chassis_controller.cpp
```

Expected: chassis files restored over demo-rps copy.

- [ ] **Step 4: Replace required assets with demo-rps versions**

Run:

```bash
cp demo-rps/ssne_ai_demo/app_assets/background.ssbmp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/background.ssbmp
cp demo-rps/ssne_ai_demo/app_assets/shared_colorLUT.sscl data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/shared_colorLUT.sscl
cp demo-rps/ssne_ai_demo/app_assets/1.ssbmp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/1.ssbmp
cp demo-rps/ssne_ai_demo/app_assets/models/model_rps.m1model data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/models/model_rps.m1model
```

Expected: all asset copies succeed.

- [ ] **Step 5: Verify pipeline and OSD baseline match demo-rps**

Run:

```bash
grep -n "SSNE_YUV422_16\|OnlineSetOutputImage\|TYPE_IMAGE\|OSD_LAYER_SIZE 5" \
  data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp \
  data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp \
  data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp
```

Expected output includes:

```text
format_online = SSNE_YUV422_16;
OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
#define OSD_LAYER_SIZE 5
TYPE_IMAGE
```

- [ ] **Step 6: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/common.hpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/log.hpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/osd-device.hpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/utils.hpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/osd-device.cpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/pipeline_image.cpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/rps_classifier.cpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/utils.cpp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/background.ssbmp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/shared_colorLUT.sscl
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/1.ssbmp
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/app_assets/models/model_rps.m1model
git commit -m "refactor: restore demo-rps display baseline"
```

---

### Task 3: Replace game state machine with stabilized gesture-to-chassis control

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/scrfd_gray.cpp`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_gray.cpp`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp`

- [ ] **Step 1: Point CMake at the demo-rps entry and source set**

Replace this block in `CMakeLists.txt`:

```cmake
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_face.cpp)

set(SSNE_AI_DEMO_SOURCES
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/chassis_controller.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/osd-device.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/pipeline_image.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/scrfd_gray.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/utils.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/yolov8_gray.cpp"
)
```

with:

```cmake
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_rps_game.cpp)

set(SSNE_AI_DEMO_SOURCES
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/chassis_controller.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/osd-device.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/pipeline_image.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/rps_classifier.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/utils.cpp"
)
```

Leave GPIO/UART libs in `target_link_libraries(...)`.

- [ ] **Step 2: Replace `demo_rps_game.cpp` with minimal gesture-to-chassis loop**

Replace the entire file with:

```cpp
/*
 * @Filename: demo_rps_game.cpp
 * @Description: Demo-rps video baseline with gesture-to-chassis control
 */
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

using namespace std;

bool g_exit_flag = false;
std::mutex g_mtx;

namespace {

constexpr int16_t kForwardVx = 100;
constexpr int16_t kStopVx = 0;
constexpr int16_t kBackwardVx = -100;
constexpr int kConfirmFrames = 3;

struct RuntimeState {
    uint64_t frame_index = 0;
    std::string candidate = "NoTarget";
    std::string locked = "NoTarget";
    int candidate_frames = 0;
    bool chassis_ready = false;
};

void keyboard_listener() {
    std::string input;
    std::cout << "键盘监听线程已启动，输入 'q' 退出程序..." << std::endl;

    while (true) {
        std::cin >> input;
        std::lock_guard<std::mutex> lock(g_mtx);
        if (input == "q" || input == "Q") {
            g_exit_flag = true;
            std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
            break;
        }
        std::cout << "输入无效（仅 'q' 有效），请重新输入：" << std::endl;
    }
}

bool check_exit_flag() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_exit_flag;
}

std::string stabilize_label(RuntimeState* state, const std::string& label) {
    if (label == state->candidate) {
        state->candidate_frames += 1;
    } else {
        state->candidate = label;
        state->candidate_frames = 1;
    }

    if (state->candidate_frames >= kConfirmFrames) {
        state->locked = state->candidate;
    }

    return state->locked;
}

void select_velocity(const std::string& label, int16_t* vx, int16_t* vy, int16_t* vz) {
    *vx = kStopVx;
    *vy = 0;
    *vz = 0;

    if (label == "P") {
        *vx = kForwardVx;
    } else if (label == "R") {
        *vx = kStopVx;
    } else if (label == "S") {
        *vx = kBackwardVx;
    }
}

}  // namespace

int main() {
    array<int, 2> img_shape = {1920, 1080};
    array<int, 2> cls_shape = {320, 320};
    string path_cls = "/app_demo/app_assets/models/model_rps.m1model";

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    RPS_CLASSIFIER classifier;
    classifier.Initialize(path_cls, &img_shape, &cls_shape);

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape, "shared_colorLUT.sscl");

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();

    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);
    visualizer.DrawBitmap("background.ssbmp", "shared_colorLUT.sscl", 0, 0, 2);

    ssne_tensor_t img_sensor;
    RuntimeState runtime;
    runtime.chassis_ready = chassis_ready;

    std::thread listener_thread(keyboard_listener);
    auto last_status_log = std::chrono::steady_clock::now();

    while (!check_exit_flag()) {
        processor.GetImage(&img_sensor);

        std::string label;
        float score = 0.0f;
        classifier.Predict(&img_sensor, label, score);
        const std::string locked_label = stabilize_label(&runtime, label);

        int16_t vx = 0;
        int16_t vy = 0;
        int16_t vz = 0;
        select_velocity(locked_label, &vx, &vy, &vz);
        if (runtime.chassis_ready) {
            chassis.SendVelocity(vx, vy, vz);
        }

        ChassisState chassis_state;
        chassis.ReadTelemetry(chassis_state);

        runtime.frame_index += 1;
        const auto now = std::chrono::steady_clock::now();
        if (now - last_status_log >= std::chrono::seconds(2)) {
            printf("[A1] frame=%llu label=%s score=%.3f locked=%s vx=%d vz=%d tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   label.c_str(),
                   score,
                   locked_label.c_str(),
                   vx,
                   vz,
                   chassis_state.vx,
                   chassis_state.volt);
            last_status_log = now;
        }
    }

    if (runtime.chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
        chassis.Release();
    }

    if (listener_thread.joinable()) {
        listener_thread.join();
    }

    classifier.Release();
    processor.Release();
    visualizer.Release();

    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }

    return 0;
}
```

- [ ] **Step 3: Delete obsolete app files from the old app_demo path**

Run:

```bash
rm -f data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp
rm -f data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/scrfd_gray.cpp
rm -f data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/yolov8_gray.cpp
rm -f data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp
rm -f data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp
```

Expected: files removed from working tree.

- [ ] **Step 4: Verify entrypoint and mapping**

Run:

```bash
grep -n "add_executable\|rps_classifier.cpp\|kForwardVx\|kBackwardVx\|ChassisController\|model_rps" \
  data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt \
  data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
```

Expected output includes:

```text
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_rps_game.cpp)
rps_classifier.cpp
kForwardVx = 100
kBackwardVx = -100
ChassisController
/app_demo/app_assets/models/model_rps.m1model
```

- [ ] **Step 5: Commit**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_rps_game.cpp
git add -u data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo
git commit -m "feat: map RPS gestures to chassis control"
```

---

### Task 4: Build and verify the board package

**Files:**
- Build verification only.

- [ ] **Step 1: Run app-only EVB build**

Run:

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

Expected output includes:

```text
[EVB构建] ✓ ssne_ai_demo 编译成功
[EVB构建] ✓ zImage 重新打包成功
```

and exits with code `0`.

- [ ] **Step 2: Run whitespace/sanity diff check**

Run:

```bash
git diff --check -- data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo
```

Expected: no output except possible harmless LF/CRLF warnings.

- [ ] **Step 3: Verify board startup script and PR-relevant status**

Run:

```bash
git status --short
gh pr view --json url,title,headRefName,baseRefName
```

Expected:
- changed files only under intended app demo / startup paths plus any user-kept docs ignores
- PR metadata still points at current branch

- [ ] **Step 4: Commit build-backed integration changes**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/board/m1pro/rootfs_overlay/usr/smartsoc/smartsoc_start.sh
git commit -m "fix: restore GPIO-backed demo-rps runtime"
```

---

### Task 5: Push and confirm PR state

**Files:**
- Remote verification only.

- [ ] **Step 1: Push current branch**

Run:

```bash
git push
```

Expected: remote branch updates successfully.

- [ ] **Step 2: Confirm PR URL and branch pairing**

Run:

```bash
gh pr view --json url,title,headRefName,baseRefName
```

Expected JSON includes the current branch as `headRefName` and a valid `url`.

- [ ] **Step 3: Manual board verification checklist**

After flashing the produced image, run:

```bash
ssh root@<A1_IP>
/app_demo/scripts/run.sh
```

Expected board-side behavior:
- no `Failed to open /dev/gpiodev`
- no `[chassis] gpio_init 失败`
- full demo-rps background visible, not half-clipped
- `P/R/S` classifier logs continue every ~2 seconds
- hand gestures map to forward/stop/backward velocity updates

---

## Self-Review

- Spec coverage: GPIO load, demo-rps full OSD reset, stable gesture mapping, compile verification, PR update all covered.
- Placeholder scan: no `TBD`, `TODO`, or implied future fill-ins.
- Type consistency: `demo_rps_game.cpp` uses `RPS_CLASSIFIER`, `ChassisController`, and `VISUALIZER` consistently with the copied demo-rps headers.
- Scope check: single subsystem only — board-side `ssne_ai_demo` runtime reset and control mapping.
