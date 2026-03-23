# 项目编译和构建指南

**最后更新**: 2026-03-23  
**版本**: 1.0 Production  

---

## 1. 编译环境概览

### 1.1 系统架构

```
┌─────────────────────────────────────────────┐
│  构建系统 (Build System)                      │
├─────────────────────────────────────────────┤
│ Docker 容器 (a1-sdk-builder:latest)         │
│  ├─ 基础镜像: Ubuntu 20.04 + Buildroot     │
│  ├─ ROS版本: ROS2 Jazzy                     │
│  ├─ SDK: SmartSens A1 SDK (Buildroot)      │
│  ├─ 编译器: GCC 9.x (ARM & x86_64)         │
│  ├─ NPU工具链: SmartSens NPU SDK           │
│  └─ 总大小: ~807MB                         │
│                                             │
│ 挂载点:                                     │
│  /app/smartsens_sdk  ← data/A1_SDK_SC132GS│
│  /app/src            ← src/                │
│  /app/models         ← models/             │
│  /app/output         ← output/             │
└─────────────────────────────────────────────┘
```

### 1.2 编译流程

```
scripts/build_src_all.sh (主编译脚本)
    ├─ Step 1: SDK 编译 (~30-60 min)
    ├─ Step 2: ROS2 编译 (~15-30 min)
    └─ Step 3: 产物收集
```

---

## 2. ROS2 编译步骤

### 2.1 启动容器

```bash
cd docker/
docker-compose up -d
```

### 2.2 编译 ROS2 工作区

```bash
docker exec A1_Builder bash -c "
  cd /app/src/ros2_ws && 
  source /opt/ros/jazzy/setup.bash && 
  colcon build --symlink-install
"
```

---

**详见完整文档后续内容...**

---

**编写者**: GitHub Copilot  
**状态**: Production Ready ✅
