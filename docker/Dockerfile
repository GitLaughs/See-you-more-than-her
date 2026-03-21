# E:\it-gets-you-better-than-her\docker\Dockerfile.fixed

###############################################################################
# 基于思特威官方 A1 SDK 镜像，添加项目所需的 Python 依赖，并修复脚本文件的换行符问题
#
# 前置条件: 先执行 docker load -i a1-sdk-builder-latest.tar
#
# 构建方法: docker build -t a1-sdk-builder-fixed -f Dockerfile.fixed .
###############################################################################

# 以官方导入的镜像为基础
FROM a1-sdk-builder:latest

WORKDIR /app

# 安装必要的依赖
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    && rm -rf /var/lib/apt/lists/*


# 注意：requirements.txt和src目录会通过docker-compose.yml挂载

WORKDIR /app/src

# 环境变量
ENV PYTHONPATH=/app/src
ENV A1_SDK_PATH=/app/smartsens_sdk
ENV PYTHONUNBUFFERED=1

# 使用常驻前台进程，保证容器不会因无交互输入而自动退出。
CMD ["/bin/bash", "-lc", "sleep infinity"]
