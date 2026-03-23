# STM32 AKM 小车驱动集成

本目录包含WHEELTEC C50X AKM小车的STM32控制板源代码和ROS驱动集成信息。

## 硬件规格

### 通信接口

| 接口 | 规格 | 用途 |
|------|------|------|
| **UART1** | 115200 bps | 终端/ROS通信 |
| **UART3** | 115200 bps | 主控制端口（默认） |
| **UART4** | 9600/230400 bps | 蓝牙APP控制 |

### 控制系统

- **MCU**: STM32F407（Keil工程）
- **编码器**: 增量式（500线或13线）TI12模式正交编码
- **舵机**: PWM控制（10kHz）- AKM差速转向系统
- **电池监测**: ADC采样（16位）
- **加速度计/陀螺仪**: 支持扩展传感器

## 通信协议

### 接收格式（ROS→STM32）

```
[0x5a] [Mode] [Vx_H] [Vx_L] [Vy_H] [Vy_L] [Vz_H] [Vz_L] [Check] [0x5e]
```

**模式定义**:
- `0x00`: 正常运动（推荐）
- `0x01`: 自动充电模式
- `0x02`: 导航自动充电（AKM专用）
- `0x03`: 充电座对接

**数据说明**:
- **Vx/Vy**: 线性速度 (mm/s × 1000 的16位整数)
- **Vz**: 转角/角速度 (mrad/s × 1000 的16位整数)
  - AKM小车：作为前轮转角
  - 其他类型：作为Z轴角速度

### 发送格式（STM32→ROS）

```
[0x5a] [StopFlag] [Vx] [Vy] [Vz] [AccX] [AccY] [AccZ] [GyroX] [GyroY] [GyroZ] [BatVol] [Check] [0x5e]
```

长度：24字节

**内容**:
- 位置 2-7: 3轴速度反馈（16位×1000）
- 位置 8-13: 3轴加速度反馈（16位）
- 位置 14-19: 3轴角速度反馈（16位）
- 位置 20-21: 电池电压（16位×1000）

## ROS集成方案

### 与ROS驱动的关系

当前 `base_control_ros2` 已经能够直接与STM32 AKM通信，提供以下功能：

1. **底盘控制节点**
   - 订阅: `/cmd_vel` (geometry_msgs/Twist)
   - 发布: `/odom` (nav_msgs/Odometry)
   - 通过UART3与STM32通信

2. **编码器反馈**
   - 读取STM32返回的速度/加速度/电池信息
   - 用于里程计计算

3. **安全管理**
   - 心跳检测
   - 超时保护

### 推荐ROS包配置

保留以下包：
- `base_control_ros2` - 底盘驱动核心（UART驱动层）
- `hardware_driver` - 传感器驱动（雷达等）
- `a1_robot_stack` - 硬件集成配置
- `object_information_msgs_ros2` - 消息定义

可选的新增包：
- `robot_localization` - 里程计融合（如需SLAM）
- `differential_drive_controller` - 差速控制优化

## 源代码组织

```
WHEELTEC_C50X_2025.12.26/
├── USER/                   # 应用层
│   ├── main.c             # 主程序入口
│   ├── system.c           # 系统初始化（AKM特定逻辑）
│   └── ...
├── HARDWARE/              # 硬件驱动层
│   ├── uartx.c            # UART驱动（UART1/3/4）
│   ├── encoder.c          # 编码器读取
│   ├── motor.c            # 电机PWM控制
│   └── adc.c              # ADC采样（电池、舵机反馈）
├── BALANCE/               # 运动控制逻辑
│   ├── uartx_callback.c   # ROS数据接收处理
│   ├── data_task.c        # 运动学计算、数据发送
│   └── motor_task.c       # 电机控制任务
├── CORE/                  # 数据类型定义
├── FreeRTOS/              # RTOS核心
├── USB_HOST/              # USB接口（扩展）
└── Akm_Car.hex            # 预编译固件
```

## 编译和烧录

### Keil MDK-ARM 编译

1. 打开工程: `WHEELTEC_C50X_2025.12.26/WHEELTEC.uvprojx`
2. 编译: Project → Build
3. 生成固件: `OBJ/WHEELTEC.hex` 或使用 `Akm_Car.hex`

### 固件烧录

- **J-Link**: 使用 `WHEELTEC_C50X_2025.12.26/DebugConfig/Flash_JLink.jflash`
- **ST-Link**: 使用STM32 CubeProgrammer
- **UART**: 使用 bootloader（通过专有工具）

## 与base_control_ros2的集成要点

### 配置匹配

在 `base_control_ros2` 的配置中，需要：

1. **UART端口配置**:
   ```yaml
   uart_port: "/dev/ttyUSB0"  # 或 "/dev/ttyACM0"（ST-Link虚拟COM）
   baudrate: 115200
   ```

2. **小车类型选择** (system.c中的初始化):
   ```c
   // 确保CarType设为AKM
   #define CAR_TYPE AKMDZ  // 或相应定义
   ```

3. **坐标系和运动学参数**:
   - 轴距: 200mm (取决于实际小车宽度)
   - 轮半径: 根据实际配置
   - 编码器分辨率: 500或13 (取决于编码器型号)

### 通信调试

查看 `base_control_ros2/script/test_ros_control.sh` 中的测试命令：

```bash
# 测试前进
ros2 topic pub /cmd_vel geometry_msgs/Twist '{linear: {x: 0.5, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}'

# 测试旋转
ros2 topic pub /cmd_vel geometry_msgs/Twist '{linear: {x: 0.0, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 1.0}}'
```

## 常见问题

### 问题1: 串口连接失败

**原因**: USB驱动未安装或端口配置错误

**解决**:
- Windows: 检查CH340驱动或ST-Link驱动
- Linux: `ls /dev/ttyUSB*` 或 `ls /dev/ttyACM*`
- 确保用户有串口权限: `sudo usermod -a -G dialout $USER`

### 问题2: 小车无响应

**原因**: 波特率不匹配或STM32未正确初始化

**解决**:
- 确认 `UART3` 配置为 115200 bps
- 检查STM32是否执行了CarType初始化
- 验证ROS消息格式是否正确

### 问题3: 编码器数值异常

**原因**: 编码器连接松动或参数配置错误

**解决**:
- 检查编码器TI12引脚连接
- 验证 `EncoderMultiples=4` 配置
- 校准编码器零点

## 文档参考

- `WHEELTEC_C50X_2025.12.26/源码工程使用指南.pdf` - 详细的Keil工程说明
- `WHEELTEC_C50X_2025.12.26/更新记录.txt` - 版本更新日志
- `base_control_ros2/README.MD` - ROS驱动详细说明
- `hardware_connection.md` - A1开发板与STM32小车的连接方案

## 后续优化方向

1. **ROS2驱动增强**
   - 实现编码器反馈的完整里程计节点
   - 集成IMU数据到`robot_localization`

2. **固件优化**
   - 增加PID参数自适应
   - 扩展遥控功能（手柄集成）

3. **SLAM集成**
   - 轻量级SLAM方案（gmapping/Cartographer）
   - 动态地图构建和导航

## 许可证

WHEELTEC STM32代码遵循原厂许可证。ROS集成部分遵循MIT许可证。
