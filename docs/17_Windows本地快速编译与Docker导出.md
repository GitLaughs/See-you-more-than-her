# 17 Windows 本地快速编译与 Docker 导出

本文记录一次在 Windows 上把 EVB 快速构建跑通的实战流程，并给出可直接复用的一键脚本。

## 为什么要用本地容器

当 SDK、源码和输出目录直接挂载在 Windows 工作区时，`build_complete_evb.sh --app-only` 很容易被 9p/共享目录 I/O 拖慢，甚至在 `make` 阶段卡死。

这次的做法是：

1. 保留一个带 Windows 挂载的源容器，只用于复制需要的目录。
2. 在 Docker 内部创建一个不挂载 Windows 共享目录的本地容器 `A1_Builder_local`。
3. 把 SDK、脚本和必要工具链流式复制进本地容器。
4. 在本地容器中执行快速构建。
5. 把产物从 Docker 自动导出回 Windows 工作区。

## 已验证的快速路径

1. 在本地容器中执行 `scripts/build_complete_evb.sh --app-only`。
2. 先重编 `ssne_ai_demo`，再重新打包 `zImage`。
3. 成功后从容器里导出 `output/evb/<时间戳>/`。

本次实践中，最重要的收益是把耗时和卡顿都从 Windows 挂载盘转移到了容器本地文件系统，构建过程稳定了很多。

## 一键脚本

推荐直接运行 [tools/evb/build_complete_evb_local.ps1](../tools/evb/build_complete_evb_local.ps1)。

默认行为：

1. 在 `A1_Builder_local` 容器里执行快速构建。
2. 自动导出最新的 `zImage.smartsens-m1-evb` 和 `ssne_ai_demo` 到本地 `output/evb/<时间戳>/`。
3. 在本地写入 `output/evb/latest.txt`，方便快速找到最近一次产物。

## 使用方式

在仓库根目录打开 PowerShell，执行：

```powershell
.\tools\evb\build_complete_evb_local.ps1
```

如果你想跑完整构建而不是快速模式，可以加 `-FullBuild`：

```powershell
.\tools\evb\build_complete_evb_local.ps1 -FullBuild
```

## 产物位置

构建完成后，本地会得到：

```text
output/evb/
    latest.txt
    <YYYYMMDD_HHMMSS>/
        zImage.smartsens-m1-evb
        ssne_ai_demo
```

## 备注

如果容器中的 SDK 目录还是旧的，建议先把源容器里的 `data/A1_SDK_SC132GS/smartsens_sdk` 重新同步到 `A1_Builder_local`，再运行脚本。