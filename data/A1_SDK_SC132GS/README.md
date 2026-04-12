# A1_SDK_SC132GS

SmartSens A1 开发板 SDK，基于 SC132GS 图像传感器，包含 Buildroot 构建系统、NPU 推理库和 OSD 硬件加速库。

## 目录结构

```text
A1_SDK_SC132GS/
└── smartsens_sdk/
    ├── smart_software/          # 自定义应用层（Buildroot BR2_EXTERNAL）
    │   ├── package/
    │   │   ├── m1_sdk_lib/      # SDK 基础库构建配置
    │   │   └── ssne_ai_demo/    # AI Demo 构建配置
    │   ├── src/app_demo/        # SDK 演示程序源码
    │   ├── configs/             # Buildroot defconfig
    │   └── scripts/             # 构建脚本
    ├── output/images/           # 编译产物（zImage 固件）
    └── scripts/                 # SDK 级构建入口
```

## 构建方法

```bash
cd smartsens_sdk
bash scripts/a1_sc132gs_build.sh
```

## 产物

| 文件 | 描述 |
|------|------|
| `output/images/zImage.smartsens-m1-evb` | 可写入开发板的固件镜像 |

## 核心库

| 库 | 功能 |
|----|------|
| `libssne.so` | SSNE 神经网络推理引擎 |
| `libcmabuffer.so` | CMA 内存管理 |
| `libosd.so` | OSD 硬件叠加渲染 |
| `libemb.so` | 嵌入式硬件抽象层 |

## 相关文档

- [编译手册](../../docs/BUILD.md)
- [Demo 工程说明](../../src/a1_ssne_ai_demo/README.md)

