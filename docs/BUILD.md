# 编译手册（A1 平台）

本手册记录在 A1_Builder 容器中完整构建 SmartSens SDK、示例程序以及 ROS2 工作区，并生成可写入主板的 EVB 固件的步骤与说明。

前提（在 CI / 容器 内执行）
- 使用仓库根目录（包含 `smartsens_sdk`、`src/ros2_ws`、`docker/`）
- 已安装 Docker（本地）或在 CI 中可使用 Docker Runner

主要脚本
- `data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh`：SDK 原有的构建脚本（由供应商提供）。
- `scripts/build_src_all.sh`：仓库新增封装脚本，会调用 SDK 构建并构建 ROS2 工作区，最后收集 EVB 工件。
- `scripts/build_ros2_ws.sh`：ROS2 工作区构建脚本，支持全量构建和按包增量构建。
- `scripts/collect_evb_artifacts.sh`：收集 SDK 和 ROS2 产物到 `output/evb/`。
- `scripts/build_incremental.sh`：只编译改动部分的入口脚本，适合跳过长时间的全量编译。

在本地使用 Docker 构建（推荐）
1. 在仓库根目录构建镜像：

```bash
docker build -f docker/Dockerfile -t a1_builder .
```

2. 运行容器进行全量构建（会打印日志到控制台并在完成后把 EVB 放到 `data/A1_SDK_SC132GS/smartsens_sdk/output/images/` 和仓库级 `output/evb/`）：

```bash
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder \
  bash -lc "chmod +x scripts/build_src_all.sh && scripts/build_src_all.sh"
```

3. 构建完成后，检查这两个位置：
- `data/A1_SDK_SC132GS/smartsens_sdk/output/images/`
- `output/evb/`

如果你只改了局部代码，不需要每次都跑完整链路，可以用增量脚本：

```bash
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder bash -lc "bash scripts/build_incremental.sh sdk ssne_ai_demo"
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder bash -lc "bash scripts/build_incremental.sh ros --clean robot_navigation_ros2 ncnn_ros2"
```

说明：
- `sdk ssne_ai_demo` 只重编 demo 这一层，适合改 C++ 业务代码
- `ros ...` 只重编指定 ROS2 包及其依赖，适合改局部 ROS 节点
- 如果你想回到完整链路，再执行 `scripts/build_src_all.sh`

当前项目里真正可写入主板的镜像文件是：

```text
data/A1_SDK_SC132GS/smartsens_sdk/output/images/zImage.smartsens-m1-evb
```

注意这里的 `-evb` 是文件名后缀，不是 `.evb` 扩展名。它就是我们当前需要的板端镜像名。

在 CI（GitHub Actions）中的运行
- 仓库的 CI 增加了 `build-in-container` 作业：在构建 Docker 镜像后，会运行 `scripts/build_src_all.sh` 并上传 `evb-files` 工件到 Actions 页面。

如何判断生成的文件是否可写入主板
- 一般供应商会给出固件文件名/格式要求（你的主板需要 `-evb` 文件名后缀）。当前 SDK 构建产物就是 `zImage.smartsens-m1-evb`，它属于你要的板端可写入镜像。
- `scripts/build_src_all.sh` 会把这个镜像复制到 `output/evb/`，便于后续刷板或上传到 CI 工件。

如果没有生成 `.evb`
- 请检查 `data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh` 的日志，确认 SDK 构建步骤是否会产出镜像并放置到 `output/images` 或其它目录。
- 若 SDK 默认不生成 EVB，则需要在 SDK 构建后增加镜像打包或转换步骤（可以在 `scripts/build_src_all.sh` 中补充）。

日志与调试
- 构建日志会直接输出到控制台；在 CI 中，Actions 会保存日志供诊断。
- 本地首次运行时建议在交互式容器里执行构建以便实时查看错误：

```bash
docker run --rm -it -v $(pwd):/workspace -w /workspace a1_builder bash
# 进入后手动执行：
# chmod +x scripts/build_src_all.sh
# scripts/build_src_all.sh
```

常见问题
- 如果在主机（Windows）上直接运行 apt/apt-get，会失败：必须在容器（Ubuntu）内运行依赖安装命令。
- 若 CI 没有找到 `docker`，请在 runner 上启用 Docker 支持或使用 self-hosted runner。

联系方式
- 有任何构建问题，可把构建日志片段贴上来，我会基于日志继续排查。

---
该文档由自动化脚本更新，首次提交随功能改动一起加入仓库。
