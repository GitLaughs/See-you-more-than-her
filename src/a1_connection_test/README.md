# A1 ↔ STM32 连接测试

最小化的上下位机通信验证工具，用于检测 A1 开发板与 STM32 底盘控制板的连接状态。

## 测试流程

| 步骤 | 测试项 | 内容 |
|------|--------|------|
| 1 | UART 初始化 | 验证 GPIO/UART 驱动加载和硬件初始化 |
| 2 | Hello 通信 | 发送停车帧，读取 STM32 回传状态，验证双向通信 |
| 3 | 底盘前进 | 以 100 mm/s 前进 1 秒，验证底盘驱动链路 |

## 板端执行

```bash
# 方法 1: 使用 run.sh
chmod +x scripts/run.sh
./scripts/run.sh

# 方法 2: 手动执行
insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko
insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko
./ssne_connection_test
```

## 编译

```bash
# 在 Docker 容器内编译
docker exec A1_Builder bash /app/scripts/build_connection_test.sh
```

## 硬件连接

```
A1 GPIO_PIN_0 (UART TX0) ──→ STM32 UART3 RX (PB11)
A1 GPIO_PIN_2 (UART RX0) ←── STM32 UART3 TX (PB10)
A1 GND ────────────────────── STM32 GND
```

故障排查请参考 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)。
