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

# 切换 Ubuntu 源为清华镜像
cat > /etc/apt/sources.list.d/ubuntu.sources << 'SOURCES'
Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SOURCES

# 更新 apt 并安装基础工具
apt-get update
apt-get install -y --no-install-recommends \
  curl \
  gpg \
  lsb-release \
  ca-certificates \
  software-properties-common

# 添加 ROS2 GPG 密钥（使用国内可访问的源）
curl -sSL http://packages.ros.org/ros.key \
  | gpg --dearmor -o /usr/share/keyrings/ros-archive-keyring.gpg

# 添加 ROS2 软件源（清华镜像）
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  https://mirrors.tuna.tsinghua.edu.cn/ros2/ubuntu $(lsb_release -cs) main" \
  > /etc/apt/sources.list.d/ros2.list

# 安装 ROS2 Jazzy
apt-get update
apt-get install -y --no-install-recommends \
  ros-jazzy-ros-base \
  python3-colcon-common-extensions \
  python3-rosdep

# 初始化 rosdep
rosdep init || true
rosdep update --rosdistro jazzy

# 设置环境变量
grep -qF 'source /opt/ros/jazzy/setup.bash' /etc/bash.bashrc \
  || echo "source /opt/ros/jazzy/setup.bash" >> /etc/bash.bashrc

echo "=== ROS2 Install DONE $(date) ==="
