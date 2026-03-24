# EVB 烧录和部署指南

## 快速开始

### 1. 生成完整 EVB 镜像

在容器内执行一键编译和打包：

```powershell
# 最简单方式（编译 SDK + Demo + ROS2）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"

# 跳过 ROS2 编译（仅 SDK + Demo，更快）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 清除旧编译，重新完整构建（时间最长）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

编译完成后，产物位于 `output/evb/`：
- **`zImage.smartsens-m1-evb`** ← 这就是完整的 EVB 镜像文件，可以直接烧录！
- `zImage.smartsens-m1-evb.v-complete-YYYYMMDD-HHMMSS` — 版本备份

### 2. 烧录到主板

#### 方式 A：使用 Aurora 伴侣工具（推荐）

```powershell
cd tools/aurora
.\launch.ps1 --flash ..\output\evb\zImage.smartsens-m1-evb
```

烧录前确认：
- A1 开发板 Type-C (SPI) 已连接 PC
- SW3 开关已切到 CH347 侧
- WCH CH347 驱动已安装

#### 方式 B：使用命令行工具

```bash
# 在容器内，使用 Smart Tools SDK 工具
docker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && \
  ./tools/burn_tool/x86_linux/burn_tool -f /app/output/evb/zImage.smartsens-m1-evb"
```

#### 方式 C：手工复制（调试）

如果开发板已启动，可以通过 SSH 复制：

```bash
# 从 PC 上传到开发板
scp output/evb/zImage.smartsens-m1-evb root@<A1_IP>:/tmp/

# 登入开发板
ssh root@<A1_IP>

# 烧录（需要 root 权限）
dd if=/tmp/zImage.smartsens-m1-evb of=/dev/mmcblk0p2 bs=4M
sync
```

### 3. 板端验证

烧录完成并重启后：

```bash
# 登入开发板
ssh root@<A1_IP>

# 运行人脸检测 + 底盘控制 Demo
/app_demo/scripts/run.sh

# 预期输出：
# [INFO] FaceDriveApp 初始化完成
# [INFO] 检测到人脸 → 直行 100 mm/s
# [DRIVE] 人脸 (3), 直行 100 mm/s
# [STOP] 未检测到人脸
```

## 详细说明

### EVB 镜像构成

`zImage.smartsens-m1-evb` 包含：
- **内核** (`zImage`) — 5.15.0 内核 + DTB
- **Rootfs** (嵌入 initramfs) — 包含：
  - SmartSens 库 + NPU 驱动
  - 应用程序 (`/app_demo/ssne_face_drive_demo`)
  - 启动脚本 (`/app_demo/scripts/run.sh`)
  - 模型文件 (`/app_demo/app_assets/models/`)
  - ROS2 环境（如包含）

### 编译时间估计

| 脚本 | 耗时 | 说明 |
|------|------|------|
| `build_complete_evb.sh` | ~30-40 分钟 | 完整编译（--skip-ros 可减少到 15-20 分钟） |
| `build_incremental.sh sdk demo` | ~2 分钟 | 只编译 Demo（不更新 zImage） |
| `build_incremental.sh collect` | <1 分钟 | 仅收集产物 |

### 常见问题

**Q1: 为什么 `zImage` 这么大（5.7 MB）？**  
A: 内核 + 完整的 rootfs (initramfs) 都嵌入了。这是为了支持无 eMMC 启动。

**Q2: 更新了 Demo 代码，但 zImage 中还是旧版？**  
A: 需要执行 `build_complete_evb.sh` 来重新打包 zImage。`build_incremental.sh sdk demo` 只编译 Demo，不更新 EVB 镜像。

**Q3: 能否只更新 /app_demo 而不重新烧录？**  
A: 可以！通过 SSH 上传新的二进制：
```bash
scp output/evb/ssne_face_drive_demo root@<A1_IP>:/app_demo/
```
然后重启应用或开发板。

**Q4: 烧录失败，怎样回滚到上一个版本？**  
A: 使用版本备份烧录：
```powershell
.\launch.ps1 --flash ..\output\evb\zImage.smartsens-m1-evb.v-baseline-...
```

## 开发工作流

### 快速迭代（仅修改 Demo）

```bash
# 1. 修改 Demo 代码（src/a1_ssne_ai_demo/src/*.cpp）
# 2. 增量编译 Demo
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_face_drive_demo"

# 3. 上传到板端测试
scp output/evb/ssne_face_drive_demo root@<A1_IP>:/app_demo/

# 4. 板端重启应用
ssh root@<A1_IP> "/app_demo/scripts/run.sh"
```

### 完整发版（含内核/驱动变更）

```bash
# 1. 修改代码（Demo + 内核 + Buildroot 配置）
# 2. 完整编译 EVB
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"

# 3. 烧录新固件
.\tools\aurora\launch.ps1 --flash output\evb\zImage.smartsens-m1-evb

# 4. 开发板重启后验证
ssh root@<A1_IP> "/app_demo/scripts/run.sh"
```

## 故障排查

### 烧录失败

- **错误：`Unable to find CH347 device`**  
  → 检查 USB 连接 + 驱动安装 + SW3 开关位置

- **错误：`Burned successfully but device not responding`**  
  → 尝试强制重启开发板（物理按钮或断电重新上电）

### 启动后无输出

- 检查 UART 连接（调试串口）
- 使用 `minicom` 或 `PuTTY` 连接串口观察启动日志
- 确认 SD 卡/eMMC 已正确烧录

### Demo 运行报错

查看日志：
```bash
ssh root@<A1_IP>
cd /app_demo
./scripts/run.sh 2>&1 | tee run.log
```

常见错误：
- `GPIO device not found` → 内核驱动未加载：运行 `insmod /lib/modules/.../gpio_kmod.ko`
- `Model file not found` → 确认 `/app_demo/app_assets/models/face_640x480.m1model` 存在
- `UART init failed` → 检查 STM32 小车连接和波特率设置

## 脚本参考

### build_complete_evb.sh

**用途**：一键编译完整 EVB 镜像

**用法**：
```bash
bash build_complete_evb.sh [OPTIONS]

OPTIONS:
  --clean      清除 Buildroot 缓存，重新完整编译（时间最长）
  --skip-ros   跳过 ROS2 编译（加快速度）
  -v,--verbose 详细日志输出
  -h,--help    显示本帮助
```

**示例**：
```bash
# 快速编译（跳过 ROS2）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 完整清洁编译
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --clean"
```

### build_incremental.sh

**用途**：增量编译（仅更新指定组件）

**用法**：
```bash
bash build_incremental.sh sdk [ssne_face_drive_demo|m1_sdk_lib|linux|full]
bash build_incremental.sh ros [--clean] [PACKAGE ...]
bash build_incremental.sh collect
```

**示例**：
```bash
# 仅编译 Demo，不更新 EVB 镜像
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_face_drive_demo"

# 编译指定 ROS2 包
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh ros base_control_ros2"

# 收集产物到 output/evb/
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh collect"
```

## 下一步

- [快速上手指南](快速上手指南.md) — 环境搭建全流程
- [常见问题](常见问题.md) — 编译、烧录、运行常见错误
- [硬件连接说明](硬件连接说明.md) — A1 <-> STM32 小车接线
