# Documentation Refresh and Restructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite repository entrypoint documentation so `README.md`, `tools/aurora/README.md`, and key `docs/` pages accurately reflect current code, build flow, and runtime verification paths.

**Architecture:** Treat documentation as a layered knowledge base: root `README.md` is the repository entrypoint, `tools/aurora/README.md` is the Windows tooling manual, and numbered `docs/` files are grouped into onboarding, current-state, collaboration, and specialist references. Keep filenames stable, fix links and stale facts in place, and rewrite planning-style pages into current-state summaries backed by the codebase.

**Tech Stack:** Markdown, GitHub-flavored Markdown links, Bash/PowerShell command examples, current repository scripts (`scripts/bootstrap.sh`, `scripts/build_complete_evb.sh`, `scripts/build_ros2_ws.sh`, `scripts/build_incremental.sh`), Aurora Windows tooling docs.

---

## File structure map

### Repository entrypoints
- Modify: `README.md` — top-level repository overview, build/run verification, docs index, collaboration notes.
- Modify: `tools/aurora/README.md` — Aurora manual for setup, launch, UI capabilities, dual control paths, A1_TEST terminal, troubleshooting.

### Onboarding docs
- Modify: `docs/01_快速上手.md` — concise newcomer path aligned to current repo structure and scripts.
- Modify: `docs/02_环境搭建.md` — environment setup with bootstrap-first flow and current prerequisites.
- Modify: `docs/03_编译与烧录.md` — full/incremental build and board deployment paths aligned to current scripts.
- Modify: `docs/11_常见问题.md` — FAQ aligned to current paths, current scripts, and current failure modes.

### Current-state and architecture docs
- Modify: `docs/06_程序概览.md` — current codebase tour matching actual modules and boundaries.
- Modify: `docs/07_架构设计.md` — architecture/current integration boundaries, correcting stale protocol and ROS assumptions.
- Modify: `docs/12_项目规划.md` — convert from sprint/member plan to current implemented state plus next directions.

### Collaboration docs
- Modify: `docs/13_贡献指南.md` — contribution flow, branch/PR expectations, doc update expectations, repo boundary notes.
- Modify: `docs/14_后续开发建议.md` — rewrite as grounded next-step suggestions based on current shipped capabilities.

### Specialist docs with light-touch fixes
- Modify: `docs/08_ROS底盘集成.md` — fix links and stale references only if broken during index pass.
- Modify: `docs/09_AI模型训练.md` — fix links and stale references only if broken during index pass.
- Modify: `docs/10_雷达集成.md` — fix links and stale references only if broken during index pass.
- Modify: `docs/15_AI模型转换与部署.md` — fix links and stale references only if broken during index pass.
- Modify: `docs/16_A1深度感知与点云避障方案.md` — fix links and stale references only if broken during index pass.

### Planning artifacts
- Read-only reference: `docs/superpowers/specs/2026-04-27-docs-refresh-design.md`
- Create: `docs/superpowers/plans/2026-04-27-docs-refresh.md`

---

### Task 1: Rewrite root repository README

**Files:**
- Modify: `README.md`
- Reference: `CLAUDE.md`, `tools/aurora/README.md`, `docs/01_快速上手.md`, `docs/03_编译与烧录.md`

- [ ] **Step 1: Replace the outdated README structure with current entrypoint sections**

Replace the current top-level narrative with a structure equivalent to this content skeleton:

```md
# A1 Vision Robot Stack

基于 SmartSens A1 开发板的嵌入式机器人软件栈，覆盖板端视觉推理、SDK 镜像打包、ROS2 底盘集成和 Windows Aurora 联调工具。

## 仓库由什么组成
- 板端 AI Demo：`data/A1_SDK_SC132GS/.../ssne_ai_demo/`
- SDK / 固件打包层：`data/A1_SDK_SC132GS/smartsens_sdk/`
- ROS2 工作区：`src/ros2_ws/`
- Windows Aurora 工具：`tools/aurora/`
- STM32 集成参考：`src/stm32_akm_driver/`

## 快速开始
### 1. 初始化环境
```bash
bash scripts/bootstrap.sh
```

### 2. 启动构建容器
```bash
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
docker compose -f docker/docker-compose.yml up -d
```

### 3. 生成 EVB 镜像
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

### 4. 板端运行验证
```bash
ssh root@<A1_IP>
/app_demo/scripts/run.sh
```

### 5. Windows 侧 Aurora 联调
```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
```

默认 Companion 地址：`http://127.0.0.1:5801`
```

- [ ] **Step 2: Add repository boundary and build-flow sections**

Insert sections that explain first-party vs vendor-heavy areas and the packaging consequence that final deployable output is `zImage.smartsens-m1-evb`, not only `ssne_ai_demo`:

```md
## 仓库边界
优先修改：`scripts/`、`tools/aurora/`、`src/ros2_ws/`、`docs/`、`data/.../ssne_ai_demo/`
谨慎修改：`data/A1_SDK_SC132GS/smartsens_sdk/` 其余部分、`third_party/ultralytics/`、`WHEELTEC_C50X_2025.12.26/`

## 构建与部署路径
`scripts/build_complete_evb.sh` 会依次重建 Demo、可选构建 ROS2、再重新打包 SDK 镜像，因此最终可烧录产物是 `output/evb/<timestamp>/zImage.smartsens-m1-evb`。
```

- [ ] **Step 3: Replace the broken docs index table with valid Markdown links**

Use a working docs index like this:

```md
## 文档索引

### 入门
- [快速上手](docs/01_%E5%BF%AB%E9%80%9F%E4%B8%8A%E6%89%8B.md)
- [环境搭建](docs/02_%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA.md)
- [编译与烧录](docs/03_%E7%BC%96%E8%AF%91%E4%B8%8E%E7%83%A7%E5%BD%95.md)
- [常见问题](docs/11_%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98.md)

### 模块与架构
- [程序概览](docs/06_%E7%A8%8B%E5%BA%8F%E6%A6%82%E8%A7%88.md)
- [架构设计](docs/07_%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [Aurora 工具说明](tools/aurora/README.md)
- [STM32 集成参考](src/stm32_akm_driver/README.md)

### 协作与后续
- [项目现状与后续方向](docs/12_%E9%A1%B9%E7%9B%AE%E8%A7%84%E5%88%92.md)
- [贡献指南](docs/13_%E8%B4%A1%E7%8C%AE%E6%8C%87%E5%8D%97.md)
- [后续开发建议](docs/14_%E5%90%8E%E7%BB%AD%E5%BC%80%E5%8F%91%E5%BB%BA%E8%AE%AE.md)
```

- [ ] **Step 4: Add collaboration notes grounded in current gitignored/generated boundaries**

Add a short section like:

```md
## 协作注意事项
- `output/` 是本地构建产物，不随 Git 同步。
- `third_party/ultralytics/`、`WHEELTEC_C50X_2025.12.26/` 属于外部依赖或厂商内容。
- 入口文档描述当前默认路径；专题文档只在确有对应代码时声明“已支持”。
```

- [ ] **Step 5: Review rendered Markdown locally**

Run: `git diff -- README.md`
Expected: README diff shows corrected port `5801`, valid docs links, and no malformed table rows like `| [docs/01_快速上手.md |`.

- [ ] **Step 6: Commit README rewrite**

```bash
git add README.md
git commit -m "docs: rewrite repository README"
```

### Task 2: Rewrite Aurora documentation around current tooling

**Files:**
- Modify: `tools/aurora/README.md`
- Reference: `tools/aurora/aurora_companion.py`, `tools/aurora/launch.ps1`, `tools/aurora/serial_terminal.py`, `tools/aurora/relay_comm.py`, `tools/aurora/chassis_comm.py`, `tools/aurora/ros_bridge.py`

- [ ] **Step 1: Replace the short README with a current-purpose Aurora manual**

Restructure the file around this outline:

```md
# Aurora Windows 工具

Aurora 是仓库里的 Windows 侧联调入口，用于相机预览、A1 串口调试、STM32 底盘控制和 ROS 辅助调试。

## 适用场景
- 预览 A1 / Windows 摄像头
- 通过 COM13 使用 A1_TEST 调试板端程序
- 直连 STM32 做底盘控制
- 通过 ROS bridge 查看和下发底盘相关状态
```

- [ ] **Step 2: Document current module structure and launch flow**

Add a module table using current files:

```md
## 目录结构
| 文件 | 作用 |
| --- | --- |
| `aurora_companion.py` | Flask + PySide6 主入口 |
| `qt_camera_bridge.py` | QtMultimedia 相机桥 |
| `serial_terminal.py` | A1_TEST 串口终端 |
| `relay_comm.py` | PC → A1_TEST → STM32 relay 通道 |
| `chassis_comm.py` | PC 直连 STM32 控制 |
| `ros_bridge.py` | ROS 侧状态与控制桥 |
| `templates/companion_ui.html` | 单页 Web UI |
| `launch.ps1` | Windows 启动脚本 |
```

Document launch commands with current defaults:

```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
```

State the default URL explicitly:

```md
默认地址：`http://127.0.0.1:5801`
```

- [ ] **Step 3: Rewrite control-path explanation to match current dual modes**

Add two explicit subsections:

```md
## 双控制路径
### 直连 STM32
PC 串口 → STM32 UART。主要由 `chassis_comm.py` 负责，适合直接验证底盘运动与遥测。

### 经由 A1
PC COM13 → A1_TEST → A1 UART0 → STM32 UART3。主要由 `serial_terminal.py` 与 `relay_comm.py` 负责，适合联调板端程序和底盘联动。
```

- [ ] **Step 4: Rewrite A1_TEST and troubleshooting sections based on current behavior**

Document current commands and accepted workflow:

```md
## A1_TEST 常用命令
- `help`
- `status`
- `A1_TEST test_echo <msg>`
- `A1_TEST debug_status`
- `A1_TEST debug_frame`
- `A1_TEST link_test on`
- `A1_TEST link_test off`
- `A1_TEST stop`
- `A1_TEST move <vx> <vy> <vz>`

## 常见问题
### Companion 没画面
当前已接受流程：先打开 Aurora.exe 完成相机初始化，再由 Companion 接管。

### COM13 连接失败
确认串口号、115200 波特率、A1 板端程序已运行。
```

- [ ] **Step 5: Run a cheap syntax/consistency check against referenced Python files**

Run: `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py tools/aurora/chassis_comm.py tools/aurora/ros_bridge.py`
Expected: no output.

- [ ] **Step 6: Commit Aurora doc rewrite**

```bash
git add tools/aurora/README.md
git commit -m "docs: refresh aurora guide"
```

### Task 3: Rewrite onboarding and build docs to current scripts

**Files:**
- Modify: `docs/01_快速上手.md`
- Modify: `docs/02_环境搭建.md`
- Modify: `docs/03_编译与烧录.md`
- Reference: `scripts/bootstrap.sh`, `scripts/build_complete_evb.sh`, `scripts/build_ros2_ws.sh`, `scripts/build_incremental.sh`, `CLAUDE.md`

- [ ] **Step 1: Rewrite `docs/01_快速上手.md` into a short newcomer path**

Replace old “full architecture lecture” sections with a concise onboarding structure:

```md
# 01 快速上手

## 本文适合谁
第一次接触仓库、想先把环境起起来并跑通一次完整流程的同学。

## 你会接触到的 4 块内容
- 板端 Demo：`data/.../ssne_ai_demo/`
- SDK / 固件打包：`data/A1_SDK_SC132GS/smartsens_sdk/`
- ROS2：`src/ros2_ws/`
- Aurora：`tools/aurora/`

## 最短路径
1. `bash scripts/bootstrap.sh`
2. `docker compose -f docker/docker-compose.yml up -d`
3. `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"`
4. `ssh root@<A1_IP>` 后运行 `/app_demo/scripts/run.sh`
5. Windows 侧执行 `tools/aurora/launch.ps1`
```

- [ ] **Step 2: Rewrite `docs/02_环境搭建.md` to make bootstrap the primary setup path**

Update the setup flow to lead with bootstrap and keep manual steps as fallback:

```md
## 推荐方式：使用 bootstrap 脚本
```bash
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
```

脚本会负责：
- 检查并拉取 `data/A1_SDK_SC132GS/`
- 可选加载基础 Docker 镜像
- 构建最终 `a1-sdk-builder:latest`
- 启动 `A1_Builder` 容器
```

Add fallback manual sections for cloning SDK and loading image only if needed.

- [ ] **Step 3: Rewrite `docs/03_编译与烧录.md` around current full/incremental flows**

Use current script-aligned sections:

```md
## 完整固件构建
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

```md
## ROS2 单独构建
```bash
bash scripts/build_ros2_ws.sh
bash scripts/build_ros2_ws.sh --clean
bash scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot
```

```md
## 增量构建
```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
bash scripts/build_incremental.sh sdk m1_sdk_lib
bash scripts/build_incremental.sh sdk linux
bash scripts/build_incremental.sh ros wheeltec_multi
```
```

Also make it explicit that Aurora is not the firmware flasher; official Aurora.exe handles flashing.

- [ ] **Step 4: Verify no stale file names remain in these three docs**

Run: `git diff -- docs/01_快速上手.md docs/02_环境搭建.md docs/03_编译与烧录.md`
Expected: no references to stale doc names such as `编译手册.md`, `环境搭建指南.md`, `双板架构设计.md`, or dead `17_Windows本地快速编译与Docker导出.md` unless the file actually exists and is intentionally referenced.

- [ ] **Step 5: Commit onboarding/build doc rewrites**

```bash
git add docs/01_快速上手.md docs/02_环境搭建.md docs/03_编译与烧录.md
git commit -m "docs: align onboarding and build guides"
```

### Task 4: Rewrite current-state, architecture, and planning docs

**Files:**
- Modify: `docs/06_程序概览.md`
- Modify: `docs/07_架构设计.md`
- Modify: `docs/12_项目规划.md`
- Reference: `README.md`, `CLAUDE.md`, `src/ros2_ws/README.md`, `src/stm32_akm_driver/README.md`

- [ ] **Step 1: Rewrite `docs/06_程序概览.md` as a current codebase tour**

Keep the file focused on “what lives where” instead of outdated internal details. Use a structure like:

```md
# 06 程序概览

## 本文定位
帮助第一次读代码的人快速找到板端 Demo、SDK 打包、ROS2、Aurora 分别在哪。

## 四个主要区域
- `data/.../ssne_ai_demo/`：板端推理、OSD、A1_TEST、UART 控制
- `data/A1_SDK_SC132GS/smartsens_sdk/`：镜像打包和 SDK 构建基础
- `src/ros2_ws/`：ROS2 Jazzy 工作区
- `tools/aurora/`：Windows 联调工具
```

Add a “修改任务怎么定位入口” section mapping common task types to directories.

- [ ] **Step 2: Rewrite `docs/07_架构设计.md` to current integration boundaries, not speculative full navigation stack**

Replace stale assumptions like “Nav2 already on board” and incorrect UART frame values with current documented boundaries:

```md
## 当前系统边界
- A1 板端：负责图像采集、SCRFD/YOLOv8 推理、OSD、A1_TEST 和到 STM32 的 UART 控制
- SDK 打包层：负责把最新 app 嵌入 EVB 镜像
- ROS2：提供独立工作区和后续集成路径，不等于默认板端运行栈
- Aurora：提供 Windows 侧预览、串口调试和底盘联调
```

Document the two debug/control paths and note that some ROS2 packages are intentionally disabled by `COLCON_IGNORE`.

- [ ] **Step 3: Rewrite `docs/12_项目规划.md` into current state + next directions**

Replace members/sprints/issues tables with sections like:

```md
# 项目现状与后续方向

## 当前已落地
- 板端 `ssne_ai_demo` 已具备 SCRFD/YOLOv8、OSD、A1_TEST、Link-Test、STM32 UART 控制
- `scripts/build_complete_evb.sh` 已打通完整 EVB 打包路径
- `tools/aurora/` 已具备视频预览、A1_TEST 终端、双模式底盘调试
- `src/ros2_ws/` 已存在 Jazzy 工作区和底盘相关包，但不是默认板端运行路径

## 当前边界
- RPLidar、深度感知、导航/SLAM 仍属于专题集成方向，不应在入口文档中写成默认现状

## 后续方向
- ROS2 包级联调与启用范围梳理
- Aurora 稳定性与联调体验持续改进
- 板端 app 与镜像打包链路的迭代验证
```

- [ ] **Step 4: Review these three docs for contradictions with root README**

Run: `git diff -- docs/06_程序概览.md docs/07_架构设计.md docs/12_项目规划.md`
Expected: docs describe ROS2 as workspace/integration path, not already-default on-board runtime stack; architecture descriptions use current boundaries and current A1/Aurora roles.

- [ ] **Step 5: Commit current-state doc rewrites**

```bash
git add docs/06_程序概览.md docs/07_架构设计.md docs/12_项目规划.md
git commit -m "docs: rewrite architecture and status guides"
```

### Task 5: Rewrite FAQ and collaboration docs

**Files:**
- Modify: `docs/11_常见问题.md`
- Modify: `docs/13_贡献指南.md`
- Modify: `docs/14_后续开发建议.md`
- Reference: `README.md`, `CLAUDE.md`, `git status`, current repo boundaries

- [ ] **Step 1: Rewrite `docs/11_常见问题.md` to current failure modes and paths**

Keep the FAQ, but replace stale paths and made-up commands. Include entries like:

```md
### `output/evb/` 没有新固件
确认执行的是 `build_complete_evb.sh`，而不是只做了 `build_incremental.sh`。

### Aurora 页面打不开
确认 `tools/aurora/launch.ps1` 已启动，并访问 `http://127.0.0.1:5801`。

### ROS2 包编不过
先确认包在 `src/ros2_ws/src/` 下，且没有 `COLCON_IGNORE`。
```

- [ ] **Step 2: Rewrite `docs/13_贡献指南.md` into practical repo contribution guidance**

Use sections like:

```md
# 贡献指南

## 改动前先判断改哪里
- 板端行为：先看 `data/.../ssne_ai_demo/`
- 镜像打包：先看 `scripts/` 和 SDK 打包路径
- Aurora：先看 `tools/aurora/`
- ROS2：先看 `src/ros2_ws/`

## 提交前最小验证
- 文档：检查链接和命令
- Aurora Python：`python -m py_compile ...`
- ROS2：优先包级构建
- 板端：优先增量构建或完整 EVB 构建
```

Also mention that vendor-heavy areas should be edited cautiously.

- [ ] **Step 3: Rewrite `docs/14_后续开发建议.md` into grounded suggestions, not pseudo-roadmap code dumps**

Replace speculative large code blocks with short sections:

```md
## 值得继续推进的方向
### ROS2 集成收敛
先明确哪些包属于默认路径，哪些继续保持 `COLCON_IGNORE`。

### Aurora 联调体验
继续优化相机初始化、串口反馈和 UI 可观测性。

### 板端运行验证
围绕 `ssne_ai_demo`、A1_TEST 和 UART 控制建立更稳定的回归检查路径。
```

- [ ] **Step 4: Review for factual tone and remove “already done” claims without code backing**

Run: `git diff -- docs/11_常见问题.md docs/13_贡献指南.md docs/14_后续开发建议.md`
Expected: no fake issue lists, no speculative feature-complete claims, no giant pasted code sketches presented as existing implementation.

- [ ] **Step 5: Commit FAQ/collaboration doc rewrites**

```bash
git add docs/11_常见问题.md docs/13_贡献指南.md docs/14_后续开发建议.md
git commit -m "docs: refresh faq and contribution guides"
```

### Task 6: Light-touch cleanup for specialist docs and cross-links

**Files:**
- Modify as needed: `docs/08_ROS底盘集成.md`, `docs/09_AI模型训练.md`, `docs/10_雷达集成.md`, `docs/15_AI模型转换与部署.md`, `docs/16_A1深度感知与点云避障方案.md`
- Reference: rewritten `README.md` and rewritten numbered docs

- [ ] **Step 1: Search for stale references introduced by old doc naming/indexing**

Run:

```bash
git grep -nE "编译手册.md|环境搭建指南.md|双板架构设计.md|项目规划与分工|localhost:5001|17_Windows本地快速编译与Docker导出" -- README.md docs tools/aurora/README.md
```

Expected: matches only where intentionally preserved with updated explanation; otherwise use the results as a cleanup list.

- [ ] **Step 2: Fix broken links and stale references in specialist docs only where needed**

When a stale reference is found, replace it with the new numbered filename and current wording, for example:

```md
参见 [03_编译与烧录](03_编译与烧录.md)
参见 [07_架构设计](07_架构设计.md)
默认 Aurora 地址为 `http://127.0.0.1:5801`
```

Do not expand these specialist docs beyond link/fact cleanup unless a section is impossible to understand without a one-paragraph correction.

- [ ] **Step 3: Review cleanup diff scope**

Run: `git diff -- docs/08_ROS底盘集成.md docs/09_AI模型训练.md docs/10_雷达集成.md docs/15_AI模型转换与部署.md docs/16_A1深度感知与点云避障方案.md`
Expected: small, targeted edits only.

- [ ] **Step 4: Commit specialist doc cleanup**

```bash
git add docs/08_ROS底盘集成.md docs/09_AI模型训练.md docs/10_雷达集成.md docs/15_AI模型转换与部署.md docs/16_A1深度感知与点云避障方案.md
git commit -m "docs: fix specialist doc references"
```

### Task 7: Final verification, PR prep, and GitHub PR creation

**Files:**
- Review all modified doc files
- Use GitHub CLI for PR creation

- [ ] **Step 1: Review final doc diff only**

Run:

```bash
git diff --stat
git diff -- README.md tools/aurora/README.md docs/
```

Expected: diff contains documentation files only for this work; unrelated repo changes are not staged into the doc PR.

- [ ] **Step 2: Run final consistency searches**

Run:

```bash
git grep -n "127.0.0.1:5801" README.md tools/aurora/README.md docs
git grep -n "build_complete_evb.sh" README.md docs
git grep -n "COLCON_IGNORE" README.md docs
git grep -n "A1_TEST" README.md tools/aurora/README.md docs
```

Expected: entry docs consistently reference the current port, current build scripts, current ROS2 package boundary language, and current A1_TEST naming.

- [ ] **Step 3: Check branch state before PR**

Run:

```bash
git status
git log --oneline --decorate -5
```

Expected: working tree clean except intended doc commits; latest commits show doc-focused commit messages.

- [ ] **Step 4: Push branch and open PR with a doc-focused summary**

Use:

```bash
git push -u origin HEAD
gh pr create --title "docs: refresh repo and aurora documentation" --body "$(cat <<'EOF'
## Summary
- rewrite repository entrypoint docs around current code and build paths
- refresh Aurora guide and key numbered docs to match current tooling behavior
- convert stale planning-style docs into current-state and next-step references

## Test plan
- [x] Reviewed Markdown diffs for root README, Aurora README, and numbered docs
- [x] Ran targeted consistency searches for stale links, old ports, and current script names
- [x] Ran Aurora Python syntax check for files referenced by documentation

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 5: Verify PR URL and summarize scope**

Run: `gh pr view --json url,title,body --jq '{url: .url, title: .title}'`
Expected: returns PR URL and title.

- [ ] **Step 6: Commit any final doc-only fixes if verification exposed issues**

If needed, use:

```bash
git add README.md tools/aurora/README.md docs/
git commit -m "docs: fix final documentation inconsistencies"
```

Only do this if a verification step found a real documentation issue.
