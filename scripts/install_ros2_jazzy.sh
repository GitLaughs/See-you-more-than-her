#!/bin/bash
set -e
LOG=/tmp/ros2_install.log
exec >> $LOG 2>&1
echo "=== ROS2 Install START $(date) ==="

# 清理 apt 锁
pkill -9 apt-get dpkg 2>/dev/null || true
sleep 2
rm -f /var/lib/apt/lists/lock* /var/cache/apt/archives/lock /var/lib/dpkg/lock*
dpkg --configure -a 2>/dev/null || true

# 更新 apt 并安装基础工具
apt-get update
apt-get install -y --no-install-recommends \
  curl \
  gpg \
  lsb-release \
  ca-certificates \
  software-properties-common

# 添加 ROS2 GPG 密钥
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.asc \
  | gpg --dearmor -o /usr/share/keyrings/ros-archive-keyring.gpg

# 添加 ROS2 软件源
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" \
  > /etc/apt/sources.list.d/ros2.list

# 安装 ROS2 Jazzy
apt-get update
apt-get install -y --no-install-recommends \
  ros-jazzy-ros-base \
  python3-colcon-common-extensions \
  python3-rosdep

# 初始化 rosdep
rosdep init || true
rosdep update

# 设置环境变量
echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc

echo "=== ROS2 Install DONE $(date) ==="
