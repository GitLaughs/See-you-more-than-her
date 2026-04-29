# Remove A1 GPIO Test Program Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Back up board-side GPIO test entry and host-side `tools/testgpio` panel into `output/GPIO_old/`, remove related runtime/build logic, and verify full EVB build still succeeds.

**Architecture:** Keep normal `ssne_ai_demo` face-demo path intact while deleting only temporary GPIO test program pieces. Copy source snapshots into `output/GPIO_old/` first, then remove host panel files, remove early `--gpio-test` dispatch and runner sources from board app, trim user-facing mentions of test program, and verify no active code path still references GPIO test entry.

**Tech Stack:** Bash, C++, CMake, Python/Flask, PowerShell, repo build script `scripts/build_complete_evb.sh`

---

## File map

- Backup destination: `output/GPIO_old/`
  - Stores copies of removed host-panel files and board-side GPIO test sources for local archival.
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
  - Remove `--gpio-test` argument branch and now-unused helpers/includes.
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt`
  - Remove `gpio_test_runner.cpp` from source list. Keep existing GPIO library link unchanged.
- Delete after backup: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp`
  - Temporary GPIO test interface header.
- Delete after backup: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp`
  - Temporary GPIO test implementation.
- Delete after backup: `tools/testgpio/`
  - Flask panel, launcher, template, and local logs for GPIO test program.
- Delete after backup: `tools/testgpio_tests.py`
  - Host-panel tests specific to removed program.
- Modify: `README.md`
  - Remove user-facing test program entry points if present.
- Modify: `CLAUDE.md`
  - Remove repo guidance that describes `tools/testgpio` and `--gpio-test` workflow so future work does not target deleted program.

### Task 1: Back up GPIO test program sources

**Files:**
- Create: `output/GPIO_old/tools/testgpio/`
- Create: `output/GPIO_old/tools/testgpio_tests.py`
- Create: `output/GPIO_old/ssne_ai_demo/demo_face.cpp`
- Create: `output/GPIO_old/ssne_ai_demo/include/gpio_test_runner.hpp`
- Create: `output/GPIO_old/ssne_ai_demo/src/gpio_test_runner.cpp`
- Test: backup file existence under `output/GPIO_old/`

- [ ] **Step 1: Verify parent output directory exists before writing backup**

Run:
```bash
ls "E:/See-you-more-than-her/output"
```
Expected: directory listing includes existing build/artifact folders; command exits 0.

- [ ] **Step 2: Create backup directory layout**

Run:
```bash
mkdir -p "E:/See-you-more-than-her/output/GPIO_old/tools" "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/include" "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/src"
```
Expected: directories created with no error output.

- [ ] **Step 3: Copy host-side GPIO panel into backup**

Run:
```bash
cp -a "E:/See-you-more-than-her/tools/testgpio" "E:/See-you-more-than-her/output/GPIO_old/tools/testgpio"
```
Expected: `output/GPIO_old/tools/testgpio` contains `app.py`, `runner.py`, `server.py`, `templates/index.html`, and `launch.ps1`.

- [ ] **Step 4: Copy host-side GPIO tests into backup**

Run:
```bash
cp -a "E:/See-you-more-than-her/tools/testgpio_tests.py" "E:/See-you-more-than-her/output/GPIO_old/tools/testgpio_tests.py"
```
Expected: backup test file exists at `output/GPIO_old/tools/testgpio_tests.py`.

- [ ] **Step 5: Copy board-side GPIO test sources into backup**

Run:
```bash
cp -a "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp" "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/demo_face.cpp" && cp -a "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp" "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/include/gpio_test_runner.hpp" && cp -a "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp" "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/src/gpio_test_runner.cpp"
```
Expected: three backup files exist under `output/GPIO_old/ssne_ai_demo/...`.

- [ ] **Step 6: Verify backup contents before deletion**

Run:
```bash
ls "E:/See-you-more-than-her/output/GPIO_old/tools/testgpio" && ls "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/include" && ls "E:/See-you-more-than-her/output/GPIO_old/ssne_ai_demo/src"
```
Expected: host files and both runner files are listed.

- [ ] **Step 7: Commit backup snapshot**

```bash
git add output/GPIO_old/tools/testgpio output/GPIO_old/tools/testgpio_tests.py output/GPIO_old/ssne_ai_demo/demo_face.cpp output/GPIO_old/ssne_ai_demo/include/gpio_test_runner.hpp output/GPIO_old/ssne_ai_demo/src/gpio_test_runner.cpp
git commit -m "chore: back up gpio test program"
```

### Task 2: Remove board-side GPIO test entry

**Files:**
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp:7-89`
- Modify: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt:27-36`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp`
- Delete: `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp`
- Test: grep for `--gpio-test` and `gpio_test_runner`

- [ ] **Step 1: Write failing removal assertions**

Create `E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_board_checks.py` with:

```python
from pathlib import Path

repo = Path(r"E:/See-you-more-than-her")
demo = (repo / "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp").read_text(encoding="utf-8")
cmake = (repo / "data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt").read_text(encoding="utf-8")

assert "--gpio-test" not in demo
assert "gpio_test_runner.hpp" not in demo
assert "collect_args(" not in demo
assert "gpio_test_runner.cpp" not in cmake
```

- [ ] **Step 2: Run assertions to verify they fail before code removal**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_board_checks.py"
```
Expected: FAIL with assertion because current files still contain `--gpio-test`, include, helper, and CMake source entry.

- [ ] **Step 3: Remove GPIO test include, helper, and early dispatch from `demo_face.cpp`**

Replace file header/body section so top of file becomes:

```cpp
/*
 * @Filename: demo_face.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include <fstream>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <regex>
#include <dirent.h>
#include <unistd.h>
#include "include/utils.hpp"

using namespace std;

// 全局退出标志（线程安全）
bool g_exit_flag = false;
// 保护退出标志的互斥锁
std::mutex g_mtx;

// OSD 贴图结构体
struct osdInfo {
    std::string filename; // OSD 文件名
    uint16_t x;           // 起始坐标 x
    uint16_t y;           // 起始坐标 y
};

/**
 * @brief 键盘监听程序，用于结束demo
 */
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
        } else {
            std::cout << "输入无效（仅 'q' 有效），请重新输入：" << std::endl;
        }
    }
}

/**
 * @brief 检查退出标志的辅助函数（线程安全）
 * @return 是否需要退出
 */
bool check_exit_flag() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_exit_flag;
}

/**
 * @brief 人脸检测演示程序主函数
 * @return 执行结果，0表示成功
 */
int main(int argc, char** argv) {
```

This removes `#include "include/gpio_test_runner.hpp"`, `collect_args`, and early `--gpio-test` branch while leaving normal demo flow untouched.

- [ ] **Step 4: Remove board runner source from CMake**

Edit `E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt` so source list becomes:

```cmake
set(SSNE_AI_DEMO_SOURCES
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/chassis_controller.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/osd-device.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/pipeline_image.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/scrfd_gray.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/utils.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/yolov8_gray.cpp"
)
```

- [ ] **Step 5: Delete board-side GPIO runner files**

Run:
```bash
rm "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp" "E:/See-you-more-than-her/data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp"
```
Expected: files removed with exit code 0.

- [ ] **Step 6: Re-run removal assertions**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_board_checks.py"
```
Expected: PASS with no output.

- [ ] **Step 7: Verify search results only hit backups/docs**

Run:
```bash
grep -R --line-number --exclude-dir=.git --exclude-dir=.claude --exclude-dir=output "--gpio-test\|gpio_test_runner" "E:/See-you-more-than-her"
```
Expected: no matches in active source tree; only plan/spec docs may still mention removed entry if not cleaned yet.

- [ ] **Step 8: Commit board-side removal**

```bash
git add data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt
git rm data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp
git commit -m "refactor: remove board gpio test entry"
```

### Task 3: Remove host-side GPIO test panel

**Files:**
- Delete: `tools/testgpio/__init__.py`
- Delete: `tools/testgpio/app.py`
- Delete: `tools/testgpio/runner.py`
- Delete: `tools/testgpio/server.py`
- Delete: `tools/testgpio/templates/index.html`
- Delete: `tools/testgpio/launch.ps1`
- Delete: `tools/testgpio_tests.py`
- Test: glob/list under `tools/testgpio`

- [ ] **Step 1: Write failing host-removal assertions**

Create `E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_host_checks.py` with:

```python
from pathlib import Path

repo = Path(r"E:/See-you-more-than-her")
assert not (repo / "tools/testgpio").exists()
assert not (repo / "tools/testgpio_tests.py").exists()
```

- [ ] **Step 2: Run assertions to verify they fail before deletion**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_host_checks.py"
```
Expected: FAIL because `tools/testgpio` and `tools/testgpio_tests.py` still exist.

- [ ] **Step 3: Remove host-side GPIO panel and tests**

Run:
```bash
rm -rf "E:/See-you-more-than-her/tools/testgpio" && rm "E:/See-you-more-than-her/tools/testgpio_tests.py"
```
Expected: directory and test file removed.

- [ ] **Step 4: Re-run host-removal assertions**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_host_checks.py"
```
Expected: PASS with no output.

- [ ] **Step 5: Verify `tools/` listing no longer contains `testgpio`**

Run:
```bash
ls "E:/See-you-more-than-her/tools"
```
Expected: `aurora` remains; `testgpio` absent.

- [ ] **Step 6: Commit host-side removal**

```bash
git rm -r tools/testgpio tools/testgpio_tests.py
git commit -m "refactor: remove gpio test host panel"
```

### Task 4: Remove references to deleted program

**Files:**
- Modify: `README.md:79-83`
- Modify: `CLAUDE.md:18-20,112-130,187-196`
- Test: grep for `testgpio|--gpio-test` in active docs and code

- [ ] **Step 1: Write failing reference-removal assertions**

Create `E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_reference_checks.py` with:

```python
from pathlib import Path

repo = Path(r"E:/See-you-more-than-her")
readme = (repo / "README.md").read_text(encoding="utf-8")
claude_md = (repo / "CLAUDE.md").read_text(encoding="utf-8")

for text in (readme, claude_md):
    assert "tools/testgpio" not in text
    assert "--gpio-test" not in text
    assert "python tools/testgpio_tests.py" not in text
    assert "python -m testgpio.server" not in text
```

- [ ] **Step 2: Run assertions to verify they fail before doc cleanup**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_reference_checks.py"
```
Expected: FAIL because current docs still mention deleted program.

- [ ] **Step 3: Remove GPIO test program section from `README.md`**

Edit `README.md` so command list section becomes:

```md
常用脚本：
- 完整镜像：`scripts/build_complete_evb.sh`
- ROS2 工作区：`scripts/build_ros2_ws.sh`
- 定向增量：`scripts/build_incremental.sh`
- 初始化环境：`scripts/bootstrap.sh`
```

No `tools/testgpio` launcher or test-program references should remain in `README.md`.

- [ ] **Step 4: Remove deleted-program guidance from `CLAUDE.md`**

Edit these parts of `CLAUDE.md`:

1. Repository shape list: remove ``tools/testgpio/`` from Windows host tools bullet and remove testgpio description.
2. Common commands: delete GPIO host panel command block:

```md
### GPIO host panel

```bash
python tools/testgpio_tests.py
```

Single-file test entry:

```bash
python -m unittest tools.testgpio_tests
```

From repo root on Windows:

```powershell
.\tools\testgpio\launch.ps1
```

Server entry if needed:

```bash
python -m testgpio.server
```
```

3. Build/runtime architecture section: remove subsection `### 5. GPIO test path` including bullets about `testgpio.app`, `testgpio.runner.GpioRunner`, SSH command shape, and logs.

After edit, `CLAUDE.md` should still describe Aurora and board-side runtime, but no longer instruct future work toward deleted GPIO test program.

- [ ] **Step 5: Re-run reference-removal assertions**

Run:
```bash
python "E:/See-you-more-than-her/output/GPIO_old/checks/remove_gpio_reference_checks.py"
```
Expected: PASS with no output.

- [ ] **Step 6: Search active tree for stale references**

Run:
```bash
python - <<'PY'
from pathlib import Path
repo = Path(r"E:/See-you-more-than-her")
needles = ("tools/testgpio", "--gpio-test", "python tools/testgpio_tests.py", "python -m testgpio.server")
for path in repo.rglob("*"):
    if any(part in {'.git', '.claude', 'output'} for part in path.parts):
        continue
    if path.is_file() and path.suffix in {'.md', '.py', '.cpp', '.hpp', '.txt', '.ps1', '.sh', '.cmake'}:
        text = path.read_text(encoding='utf-8', errors='ignore')
        hits = [needle for needle in needles if needle in text]
        if hits:
            print(path)
            for hit in hits:
                print('  ', hit)
PY
```
Expected: no output from active source/docs. Plan/spec docs may still contain historical mentions if they are not excluded; if so, leave them because they document removed work.

- [ ] **Step 7: Commit reference cleanup**

```bash
git add README.md CLAUDE.md
git commit -m "docs: remove gpio test program references"
```

### Task 5: Verify build and repo state

**Files:**
- Test: `scripts/build_complete_evb.sh`
- Test: git status and build logs under `output/logs`

- [ ] **Step 1: Run final active-tree search before build**

Run:
```bash
python - <<'PY'
from pathlib import Path
repo = Path(r"E:/See-you-more-than-her")
needles = ("tools/testgpio", "--gpio-test", "gpio_test_runner")
for path in repo.rglob("*"):
    if any(part in {'.git', '.claude', 'output', 'docs/superpowers'} for part in path.parts):
        continue
    if path.is_file():
        text = path.read_text(encoding='utf-8', errors='ignore')
        if any(needle in text for needle in needles):
            print(path)
PY
```
Expected: no output.

- [ ] **Step 2: Run full EVB build**

Run:
```bash
bash "E:/See-you-more-than-her/scripts/build_complete_evb.sh"
```
Expected: script completes with `✓ 完整 EVB 构建完成！` and writes fresh artifacts under `output/evb/<timestamp>/`.

- [ ] **Step 3: Confirm build artifacts exist**

Run:
```bash
ls "E:/See-you-more-than-her/output/evb/latest"
```
Expected: listing includes `ssne_ai_demo` and `zImage.smartsens-m1-evb`.

- [ ] **Step 4: Check working tree after verification**

Run:
```bash
git status --short
```
Expected: only intended deletions, backups, docs edits, and build-log changes remain.

- [ ] **Step 5: Commit final verified removal**

```bash
git add README.md CLAUDE.md output/GPIO_old data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/CMakeLists.txt
git add -u tools/testgpio tools/testgpio_tests.py data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/include/gpio_test_runner.hpp data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/src/gpio_test_runner.cpp
git commit -m "refactor: remove gpio test program"
```

## Self-review

- Spec coverage: backup, board removal, host-panel removal, reference cleanup, and full-build verification all mapped to Tasks 1-5.
- Placeholder scan: no `TODO`/`TBD`; each task has exact files, commands, and expected results.
- Type consistency: all filenames and needles use current repo names `tools/testgpio`, `gpio_test_runner`, and `--gpio-test` consistently.
