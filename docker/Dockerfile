# docker/Dockerfile
###############################################################################
# A1_Builder — SmartSens SDK + ROS2 Jazzy 一体化构建镜像
#
# 基础镜像：思特威官方 A1 SDK 镜像（Ubuntu 24.04 Noble）
# 额外集成：ROS2 Jazzy Desktop + colcon + rosdep
#
# 前置条件: docker load -i a1-sdk-builder-latest.tar
# 构建方法: docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
###############################################################################

FROM a1-sdk-builder:latest

WORKDIR /app

# --- 基础构建工具 ---
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    curl \
    gnupg2 \
    lsb-release \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# --- 安装 ROS2 Jazzy (Ubuntu 24.04 Noble) ---
RUN curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.asc \
      | gpg --dearmor -o /usr/share/keyrings/ros-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
      http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" \
      > /etc/apt/sources.list.d/ros2.list && \
    apt-get update && apt-get install -y --no-install-recommends \
      ros-jazzy-ros-base \
      python3-colcon-common-extensions \
      python3-rosdep \
    && rosdep init \
    && rosdep update \
    && rm -rf /var/lib/apt/lists/*

# --- 自动 source ROS2 环境（供 bash -lc 命令使用）---
RUN echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc

WORKDIR /app/src

# 环境变量
ENV PYTHONPATH=/app/src
ENV A1_SDK_PATH=/app/smartsens_sdk
ENV PYTHONUNBUFFERED=1

CMD ["/bin/bash", "-lc", "sleep infinity"]
