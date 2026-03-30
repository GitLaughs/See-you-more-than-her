#!/bin/bash
# run.sh — A1 ↔ STM32 连接测试板端执行脚本
#
# 用法: 将 ssne_connection_test 和本脚本拷贝到开发板, 执行:
#   chmod +x run.sh && ./run.sh

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]-$0}")" && pwd)

echo "=== 加载内核驱动模块 ==="
insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko 2>/dev/null || true
insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko 2>/dev/null || true

echo "=== 启动连接测试 ==="
chmod +x "${SCRIPT_DIR}/ssne_connection_test"
"${SCRIPT_DIR}/ssne_connection_test"
