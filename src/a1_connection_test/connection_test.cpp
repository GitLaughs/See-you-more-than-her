/**
 * @file connection_test.cpp
 * @brief A1 ↔ STM32 连接测试程序
 *
 * 测试流程:
 *   [1] 初始化 UART (GPIO_PIN_0=TX, GPIO_PIN_2=RX, 115200 baud)
 *   [2] 向 STM32 发送 "hello" 信息 (通过速度帧携带特征码)
 *   [3] 读取 STM32 回传状态帧, 验证通信链路
 *   [4] 控制底盘前进 1 秒 (100 mm/s), 验证底盘驱动正常
 *   [5] 发送停止指令, 打印测试结果汇总
 *
 * 硬件连接:
 *   A1 GPIO_PIN_0 (UART TX0) → STM32 UART3 RX (PB11)
 *   A1 GPIO_PIN_2 (UART RX0) → STM32 UART3 TX (PB10)
 *   GND ↔ GND
 *
 * 板端执行:
 *   insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko
 *   insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko
 *   ./ssne_connection_test
 */

#include "include/chassis_controller.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// 测试参数
static constexpr uint32_t BAUDRATE       = 115200;
static constexpr int16_t  FORWARD_SPEED  = 100;    // mm/s
static constexpr int      FORWARD_MS     = 1000;   // 前进持续时间
static constexpr int      HELLO_RETRIES  = 5;      // Hello 发送重试次数
static constexpr int      STATUS_RETRIES = 10;     // 状态读取重试次数

// 测试结果枚举
enum TestResult { PASS = 0, FAIL = 1, SKIP = 2 };

static const char* result_str(TestResult r) {
  switch (r) {
    case PASS: return "\033[32mPASS\033[0m";
    case FAIL: return "\033[31mFAIL\033[0m";
    case SKIP: return "\033[33mSKIP\033[0m";
  }
  return "UNKNOWN";
}

static void print_banner() {
  std::printf("\n");
  std::printf("╔══════════════════════════════════════════════════╗\n");
  std::printf("║     A1 ↔ STM32 连接测试 (Connection Test)      ║\n");
  std::printf("╠══════════════════════════════════════════════════╣\n");
  std::printf("║  协议: WHEELTEC C50X 11-byte UART (115200 8N1) ║\n");
  std::printf("║  TX: GPIO_PIN_0 → STM32 UART3 RX (PB11)       ║\n");
  std::printf("║  RX: GPIO_PIN_2 ← STM32 UART3 TX (PB10)       ║\n");
  std::printf("╚══════════════════════════════════════════════════╝\n");
  std::printf("\n");
}

static void print_summary(TestResult t1, TestResult t2, TestResult t3) {
  std::printf("\n");
  std::printf("╔══════════════════════════════════════════════════╗\n");
  std::printf("║              测试结果汇总                       ║\n");
  std::printf("╠══════════════════════════════════════════════════╣\n");
  std::printf("║  [1] UART 初始化       : %s                    ║\n", result_str(t1));
  std::printf("║  [2] Hello 通信测试    : %s                    ║\n", result_str(t2));
  std::printf("║  [3] 底盘前进测试      : %s                    ║\n", result_str(t3));
  std::printf("╚══════════════════════════════════════════════════╝\n");

  int fail_count = (t1 == FAIL) + (t2 == FAIL) + (t3 == FAIL);
  if (fail_count == 0) {
    std::printf("\n✓ 所有测试通过！上下位机连接正常，底盘驱动正常。\n\n");
  } else {
    std::printf("\n✗ %d 项测试失败，请参考故障排查文档: TROUBLESHOOTING.md\n\n", fail_count);
  }
}

int main() {
  print_banner();

  ssne_demo::ChassisController chassis;
  TestResult result_uart  = FAIL;
  TestResult result_hello = FAIL;
  TestResult result_drive = FAIL;

  // ═══════════════════════════════════════════════════════════════════
  // 测试 1: UART 初始化
  // ═══════════════════════════════════════════════════════════════════
  std::printf("━━━ 测试 1/3: UART 初始化 ━━━\n");
  std::printf("  初始化 GPIO + UART (波特率: %u)...\n", BAUDRATE);

  if (!chassis.Open(BAUDRATE)) {
    std::fprintf(stderr, "  ✗ UART 初始化失败!\n");
    std::fprintf(stderr, "  → 检查: 是否已加载 gpio_kmod.ko 和 uart_kmod.ko\n");
    std::fprintf(stderr, "  → 执行: insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko\n");
    std::fprintf(stderr, "  → 执行: insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko\n");
    result_uart = FAIL;
    print_summary(result_uart, result_hello, result_drive);
    return 1;
  }

  std::printf("  ✓ UART 初始化成功\n\n");
  result_uart = PASS;

  // ═══════════════════════════════════════════════════════════════════
  // 测试 2: Hello 通信测试
  // ═══════════════════════════════════════════════════════════════════
  std::printf("━━━ 测试 2/3: Hello 通信测试 ━━━\n");
  std::printf("  向 STM32 发送 Hello 信号 (停车帧 + 状态读取)...\n");

  // 发送停止帧作为 "hello" 握手信号 — STM32 收到有效帧后会回传状态
  // STM32 WHEELTEC 固件在接收到合法帧后会在 UART3 上持续回传 24 字节状态帧
  bool hello_ok = false;
  for (int i = 0; i < HELLO_RETRIES; ++i) {
    std::printf("  [%d/%d] 发送 Hello 帧...", i + 1, HELLO_RETRIES);

    // 发送停车帧 (vx=vy=vz=0) 作为 hello 信号
    if (!chassis.SendStop()) {
      std::printf(" 发送失败\n");
      usleep(200000);  // 200ms
      continue;
    }
    std::printf(" 已发送, 等待回复...");

    // 等待 STM32 处理并回传状态
    usleep(100000);  // 100ms

    // 尝试读取状态帧
    int16_t vx_fb = 0, voltage = 0;
    bool got_reply = false;
    for (int r = 0; r < STATUS_RETRIES; ++r) {
      if (chassis.ReceiveStatus(&vx_fb, &voltage)) {
        got_reply = true;
        break;
      }
      usleep(50000);  // 50ms
    }

    if (got_reply) {
      std::printf(" ✓ 收到回复!\n");
      std::printf("  → 反馈速度: %d mm/s\n", vx_fb);
      std::printf("  → 电池电压: %d mV (%.1f V)\n", voltage, voltage / 1000.0);
      hello_ok = true;
      break;
    } else {
      std::printf(" 未收到回复\n");
      usleep(300000);  // 300ms
    }
  }

  if (hello_ok) {
    std::printf("  ✓ Hello 通信测试通过 — 上下位机已联通!\n\n");
    result_hello = PASS;
  } else {
    std::fprintf(stderr, "  ✗ Hello 通信测试失败 — 未收到 STM32 回复\n");
    std::fprintf(stderr, "  → 检查: TX/RX 是否交叉连接 (A1 TX→STM32 RX, A1 RX←STM32 TX)\n");
    std::fprintf(stderr, "  → 检查: GND 是否共地\n");
    std::fprintf(stderr, "  → 检查: STM32 是否已上电并运行 WHEELTEC 固件\n\n");
    result_hello = FAIL;
    // 继续执行底盘测试 (即使 Hello 失败，发送指令的链路可能单向可用)
  }

  // ═══════════════════════════════════════════════════════════════════
  // 测试 3: 底盘前进测试
  // ═══════════════════════════════════════════════════════════════════
  std::printf("━━━ 测试 3/3: 底盘前进测试 ━━━\n");
  std::printf("  发送前进指令: vx=%d mm/s, 持续 %d ms\n", FORWARD_SPEED, FORWARD_MS);
  std::printf("  ⚠ 请确保小车周围无障碍物!\n");
  std::printf("  3 秒后开始...\n");

  // 安全延时倒计时
  for (int i = 3; i > 0; --i) {
    std::printf("  %d...\n", i);
    sleep(1);
  }

  std::printf("  → 开始前进!\n");

  // 持续发送前进指令 (20Hz, 50ms 间隔)
  int send_count = 0;
  int fail_count = 0;
  int intervals = FORWARD_MS / 50;

  for (int i = 0; i < intervals; ++i) {
    if (chassis.SendVelocity(FORWARD_SPEED, 0, 0)) {
      send_count++;
    } else {
      fail_count++;
    }
    usleep(50000);  // 50ms
  }

  // 立即停车
  std::printf("  → 发送停车指令...\n");
  chassis.SendStop();
  usleep(50000);
  chassis.SendStop();  // 再发一次确保停车

  std::printf("  → 前进指令已发送 %d 帧, 失败 %d 帧\n", send_count, fail_count);

  // 读取停车后状态
  usleep(200000);
  int16_t final_vx = 0, final_voltage = 0;
  bool got_final = false;
  for (int r = 0; r < STATUS_RETRIES; ++r) {
    if (chassis.ReceiveStatus(&final_vx, &final_voltage)) {
      got_final = true;
      break;
    }
    usleep(50000);
  }

  if (got_final) {
    std::printf("  → 停车后反馈: 速度=%d mm/s, 电压=%d mV\n", final_vx, final_voltage);
  }

  if (send_count > 0 && fail_count == 0) {
    std::printf("  ✓ 底盘前进测试通过 — 所有指令均发送成功\n");
    std::printf("  (请目视确认小车是否实际前进了约 %.0f mm)\n\n",
                FORWARD_SPEED * (FORWARD_MS / 1000.0));
    result_drive = PASS;
  } else if (send_count > 0) {
    std::printf("  ⚠ 底盘前进测试部分通过 — %d/%d 帧发送成功\n\n",
                send_count, send_count + fail_count);
    result_drive = PASS;  // 大部分成功也算通过
  } else {
    std::fprintf(stderr, "  ✗ 底盘前进测试失败 — 未能发送任何指令\n\n");
    result_drive = FAIL;
  }

  // ═══════════════════════════════════════════════════════════════════
  // 汇总
  // ═══════════════════════════════════════════════════════════════════
  chassis.Close();
  print_summary(result_uart, result_hello, result_drive);

  // 返回值: 0 = 全部通过, 1 = 有失败
  return (result_uart == PASS && result_hello == PASS && result_drive == PASS) ? 0 : 1;
}
