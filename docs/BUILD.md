# 编译手册（A1 平台）

本手册记录在 A1_Builder 容器中完整构建 SmartSens SDK、示例程序以及 ROS2 工作区，并生成可写入主板的 EVB 固件的步骤与说明。

前提（在 CI / 容器 内执行）
- 使用仓库根目录（包含 `smartsens_sdk`、`src/ros2_ws`、`docker/`）
- 已安装 Docker（本地）或在 CI 中可使用 Docker Runner

主要脚本
- `smartsens_sdk/scripts/a1_sc132gs_build.sh`：SDK 原有的构建脚本（由供应商提供）。
- `smartsens_sdk/scripts/build_evb_from_src.sh`：仓库新增封装脚本，会调用 SDK 构建并构建 ROS2 工作区，最后收集 *.evb 工件。

在本地使用 Docker 构建（推荐）
1. 在仓库根目录构建镜像：

```bash
docker build -f docker/Dockerfile -t a1_builder .
```

2. 运行容器进行全量构建（会打印日志到控制台并在完成后把 EVB 放到 `smartsens_sdk/output/images/`）：

```bash
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder \
  bash -lc "chmod +x smartsens_sdk/scripts/build_evb_from_src.sh && smartsens_sdk/scripts/build_evb_from_src.sh"
```

3. 构建完成后，检查 `smartsens_sdk/output/images/` 目录下的文件：
- 如果出现以 `.evb` 结尾的文件（或供应商指定的镜像文件），这就是可写入主板的固件（一般为 EVB/board image）。

在 CI（GitHub Actions）中的运行
- 仓库的 CI 增加了 `build-in-container` 作业：在构建 Docker 镜像后，会运行 `smartsens_sdk/scripts/build_evb_from_src.sh` 并上传 `evb-files` 工件到 Actions 页面。

如何判断生成的文件是否可写入主板
- 一般供应商会给出固件文件名/格式要求（你的主板需要 `-evb` 格式）。本脚本会尝试收集 `smartsens_sdk/output/images/` 下的文件以及仓库内所有 `*.evb` 文件。
- 若 `*.evb` 文件存在且供应商说明一致，则可以直接使用厂商提供的刷写工具/脚本将其写入主板。

如果没有生成 `.evb`
- 请检查 `smartsens_sdk/scripts/a1_sc132gs_build.sh` 的日志，确认 SDK 构建步骤是否会产出镜像并放置到 `output/images` 或其它目录。
- 若 SDK 默认不生成 EVB，则需要在 SDK 构建后增加镜像打包或转换步骤（可以在 `build_evb_from_src.sh` 中补充）。

日志与调试
- 构建日志会直接输出到控制台；在 CI 中，Actions 会保存日志供诊断。
- 本地首次运行时建议在交互式容器里执行构建以便实时查看错误：

```bash
docker run --rm -it -v $(pwd):/workspace -w /workspace a1_builder bash
# 进入后手动执行：
# chmod +x smartsens_sdk/scripts/build_evb_from_src.sh
# smartsens_sdk/scripts/build_evb_from_src.sh
```

常见问题
- 如果在主机（Windows）上直接运行 apt/apt-get，会失败：必须在容器（Ubuntu）内运行依赖安装命令。
- 若 CI 没有找到 `docker`，请在 runner 上启用 Docker 支持或使用 self-hosted runner。

联系方式
- 有任何构建问题，可把构建日志片段贴上来，我会基于日志继续排查。

---
该文档由自动化脚本更新，首次提交随功能改动一起加入仓库。
