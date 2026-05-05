# CLAUDE.md

你需要使用中文思考和回答用户的问题。

这是本仓库中 Claude Code（claude.ai/code）的工作指引。

## 仓库结构

本仓库是 A1 视觉机器人整套工程，不是单一应用。当前主要由四个一方层组成：

- **板端 AI Demo** — `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
  - 运行在 SmartSens A1 开发板上。
  - 负责推理流水线、OSD、`A1_TEST` CLI/调试路径，以及 UART/底盘集成。
- **SDK / 固件打包层** — `data/A1_SDK_SC132GS/smartsens_sdk/` 以及仓库根目录下的 `scripts/`
  - 包含厂商 SDK 树和仓库内的构建包装脚本。
  - 产出最终 EVB 镜像，而不只是单个应用二进制。
- **Windows 主机工具** — `tools/aurora/`、`tools/PC/`、`tools/A1/`
  - Aurora：摄像头预览、拍照、COM13 终端、手动 `A1_TEST` 检查。
  - PC：PC 直连 STM32 的串口调试。
  - A1：通过 COM13 → `A1_TEST` → STM32 的中继控制。
- **STM32 集成参考** — `src/stm32_akm_driver/`
  - 保存 STM32 侧集成说明、参考协议和配套文档。
  - 主要作为对接参考，不是当前 Windows 联调工具的主入口。

优先在 `scripts/`、`tools/`、`docs/` 和 `.../ssne_ai_demo/` 中修改。其余 `smartsens_sdk/`、`third_party/ultralytics/` 以及大量 vendor 代码应视为外部导入或重 vendor 区域。

## 常用命令

除非另有说明，否则都在仓库根目录执行。

在修改构建或集成行为前，请先阅读 `README.md`、`tools/aurora/README.md`、`docs/03_编译与烧录.md`、`docs/06_程序概览.md` 和 `docs/07_架构设计.md`。

### 初始化 / 容器

```bash
bash scripts/bootstrap.sh
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
bash scripts/bootstrap.sh --sdk-only
bash scripts/bootstrap.sh --docker-only --skip-build
```

```bash
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
docker compose -f docker/docker-compose.yml up -d
```

### 完整固件 / EVB 构建

在 `A1_Builder` 容器内执行：

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

### Docker 包装构建

```bash
bash scripts/build_docker.sh
bash scripts/build_docker.sh --clean
```

### 增量构建

```bash
bash scripts/build_incremental.sh sdk ssne_ai_demo
bash scripts/build_incremental.sh sdk m1_sdk_lib
bash scripts/build_incremental.sh sdk linux
```

### 板端运行检查

```bash
ssh root@<A1_IP>
cd /app_demo
./scripts/run.sh
```

### Windows 主机工具

Aurora 视频 / COM13 终端：

```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
.\launch.ps1 -Port 6201
.\launch.ps1 -ListenHost 0.0.0.0
.\launch.ps1 -Device 0
```

PC 直连 STM32 工具：

```powershell
cd tools/PC
.\launch.ps1
```

A1 中继工具：

```powershell
cd tools/A1
.\launch.ps1
```

默认端口：Aurora `6201`，PC `6202`，A1 `6203`。

### Windows 工具校验

```bash
python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py
```

注意：文档中的脚本和容器名称可能不一致。`README`/文档中的完整镜像构建使用 `docker exec A1_Builder ...`；而 `scripts/build_docker.sh` 使用的 service 名为 `dev`。

## 构建与运行架构

### 1. 固件构建流程

权威的镜像构建入口是 `scripts/build_complete_evb.sh`。

流程如下：
1. 视情况重建 SDK 基础层
2. 重建 `ssne_ai_demo`
3. 重新执行 SDK 打包，让最新应用进入最终 `zImage`
4. 将产物收集到 `output/evb/<timestamp>/`

结论：`ssne_ai_demo` 单独构建后并不是可部署产物。最终可刷写的输出是 `zImage.smartsens-m1-evb`。

### 2. 板端应用与 SDK 打包

`ssne_ai_demo` 是当前板端运行路径。实际生效的构建根目录是 `data/A1_SDK_SC132GS/smartsens_sdk/`。`scripts/build_complete_evb.sh` 会重建应用、重新执行 SDK 打包，并输出可刷写的 `zImage.smartsens-m1-evb`。

### 3. Windows 工具结构

`tools/aurora/aurora_companion.py` 是视频 / COM13 入口。它负责串联：

- `qt_camera_bridge.py`：QtMultimedia 摄像头路径
- `serial_terminal.py`：共享的 `A1_TEST` 串口终端
- `templates/companion_ui.html`：Aurora Web UI

`tools/PC/pc_tool.py` 是 STM32 直连入口，仅注册 `pc_chassis.py`。默认通信端口为 `COM17`。

`tools/A1/a1_tool.py` 是 COM13 中继入口，注册 A1 relay 与 serial-terminal 路由，用于 COM13 → `A1_TEST` → STM32。

当前 Aurora 默认启动流程是：启动脚本会先尝试自动拉起 `Aurora.exe` 做相机初始化，再启动 Companion；如果 `Aurora.exe` 启动失败，Companion 仍会继续启动，必要时可显式使用 `-SkipAurora`。

### 4. 硬件 / 控制路径

必须区分以下两条调试 / 控制路径：

- **直连 STM32 路径**：PC 串口 → STM32 UART
- **经 A1 中继路径**：PC COM13 → A1 `A1_TEST` CLI → A1 UART0 → STM32 UART3

很多 Windows 工具问题，本质上都是把这两条路径混淆了。

## 需要始终记住的仓库指导

- 在改动构建或集成逻辑前，先读 `README.md` 和 `tools/aurora/README.md`。
- `docs/03_编译与烧录.md` 是最完整的端到端构建 / 烧录参考。
- `docs/06_程序概览.md` 和 `docs/07_架构设计.md` 是最好的架构摘要。
- `docs/13_贡献指南.md` 对仓库结构和工作流相关修改很有帮助。
- 不要把 `output/` 当作真实来源。
- `build_complete_evb.sh --app-only` 默认假设之前已经做过一次完整构建，SDK 缓存已存在。
- 当前 `output/evb/latest/` 只是最近一次构建的便捷路径，不能替代真实构建来源与日志。

## 工具链与工作流陷阱

- 使用 `Read` 工具读取源码 / 文本文件时，不要传 `pages`。`pages` 仅适用于 PDF，空值也会导致读取失败。
- 容器内的修改默认都是临时的，除非已经同步回本仓库。优先先改仓库，再通过 `docker exec A1_Builder ...` 构建；如果在 `/app` 内临时排查或打补丁，提交前必须同步回对应仓库路径。
- 排查板端 OSD 问题时，在 `VISUALIZER::Initialize`、`DrawBitmap`、`osd_add_texture_layer`、`osd_flush_texture_layer` 周围补 stdout 证据后再下结论。仅靠截图无法区分“应用未运行”“刷入镜像过旧”“OSD API 调用失败”还是“Aurora 预览路径问题”。
- `data/A1_SDK_SC132GS/smartsens_sdk/` 是 SDK 构建根（已扁平化，不再嵌套）。Docker 内存在符号链接 `smartsens_sdk/smartsens_sdk/smart_software -> ../smart_software` 用于兼容 toolchain-wrapper 硬编码路径。
- 从 `git.smartsenstech.ai` clone/fetch 时，如果 Git 实际走到 `127.0.0.1` 并断开，需要在命令级关闭代理环境变量和代理配置。
- 在 Windows 上替换官方 SDK 内容后，首次容器构建前要把 SDK 的 Buildroot 控制文件和可执行脚本统一规范为 LF，不只是 `scripts/*.sh`。
- Docker 应将宿主机的 `data/A1_SDK_SC132GS` bind mount 到 `/app/data/A1_SDK_SC132GS`；构建脚本面向的是挂载后的嵌套 SDK 根目录。
- 本地 `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/` 是 SDK 外部树，不是镜像。
- `scripts/build_complete_evb.sh --app-only` 在找不到嵌套 SDK 基线产物时应快速失败；先完整构建一次以建立缓存。
- `tools/aurora/launch.ps1` 当前会在默认端口不可用时自动向后寻找可用端口，不再是“固定端口不可用就直接失败”的旧行为。
- `tools/aurora/aurora_companion.py` 内仍保留少量历史端口常量，但实际默认启动端口以启动脚本和命令行参数为准，即 `6201`。
- 使用 `demo-rps/dataprocess_modeltrain/export_onnx.py` 把 `data/rps_dataset/training_output_balanced/best.pt` 导出为 `data/rps_dataset/training_output_balanced/a1_convert/best.onnx`。
- 使用 `tools/convert/generate_a1_datasets.py` 打包 A1 校准/评估数据时，默认数据集根是 `data/rps_dataset/processed_dataset_regularized`。
- `datasets.zip` 必须保留 `calibrate_datasets/` 和 `evaluate_datasets/` 两级目录，不能把 `.npy` 平铺到 zip 根目录。
- 当前 5 类 MobileNet 分类 ONNX 直接输出 `logits`，不是 YOLO head6；不要对它做 head6 裁剪。
- `demo-rps/dataprocess_modeltrain/prepare_video_dataset.py` 按视频数切分 train/val/test；每类视频太少会让 val 或 train 缺类，尤其 `NoTarget` 单视频会导致训练失败。
- `train_a1_5class_classifier.py` 训练前会强制检查 train/val 每类都有样本；正式训练可先用 `nvidia-smi` 确认 CUDA 进程与 GPU 利用率。
- 后台训练 stdout 可能缓冲，日志文件短时间为空不代表卡死；优先看 TaskOutput、checkpoint 更新时间和 `nvidia-smi`。
- 切换 `ssne_ai_demo` 模型时必须同时更新：`demo_rps_game.cpp` 模型路径、`scripts/run.sh` 模型存在性检查、`app_assets/models/` 实际文件；构建后用 `docker exec A1_Builder bash -lc "test -f /app/data/A1_SDK_SC132GS/smartsens_sdk/output/target/app_demo/app_assets/models/<model>.m1model"` 验证。
- `.m1model` 可能被根目录 `models/` ignore 规则误伤；如果需要把板端模型提交进 `app_assets/models/`，先用 `git check-ignore -v <path>` 确认，必要时改 `.gitignore` 或 `git add -f`。
- 5 类 A1 分类模型当前类别顺序为 `person / stop / forward / obstacle / NoTarget`；板端 `rps_classifier.cpp`、`demo_rps_game.cpp`、Aurora `rps_snapshot` 的 `scores` 长度必须同步为 5。

## 板端 OSD 交互指导

- 当前模型：demo-rps 风格的 RPS 分类模型（`model_rps.m1model`，320×320 输入，P/R/S/NoTarget 输出）。
- OSD 图层分布：
  - Layer 0：分类状态文本 / 语义提示（TYPE_GRAPHIC 或文本位图）
  - Layer 1：预留固定正方形（TYPE_GRAPHIC）
  - Layer 2：背景位图（TYPE_IMAGE），透明开窗 720×1280 摄像头区域
  - Layer 3/4：RPS 结果贴图 / 预留（TYPE_IMAGE）
- 坐标管线：demo-rps 评委主线不再走 YOLO 检测框还原；当前主线是 `model_rps.m1model` 分类输出 + `P -> forward`、其他 -> stop 映射。
- 底盘优先级：forward > stop；无单独 obstacle/person 检测优先级。
- 仅使用 `background.ssbmp`（透明开窗）和 `background_colorLUT.sscl` 两个 OSD 资源
- 训练/推理语义与 OSD 像素必须分离：当前主线不再依赖 YOLO 输入 640×640 或检测框坐标还原。
