# A1 Camera Preview Tool

这是一个基于 Python (Flask + Socket.IO) + HTML5 + PowerShell 的 A1 摄像头实时预览与小车控制工具。

## 目录结构
- `main.py`: 后端 Flask 服务器，处理视频流、WebSocket 通信和小车状态管理。
- `templates/index.html`: 前端主页面，包含左右分栏、视频画布、虚拟小车和控制面板。
- `static/css/style.css`: 样式文件，遵循白色主题规范。
- `static/js/main.js`: 前端交互逻辑，包括拖拽分栏、WebSocket 收发、WASD 监听和 50ms 状态刷新。
- `start.ps1`: 启动脚本，自动检查环境依赖并启动服务。
- `requirements.txt`: Python 依赖包列表。

## 运行步骤
1. 确保已安装 Python >= 3.8。
2. 在 PowerShell 中运行 `.\start.ps1`。
3. 脚本会自动检查端口、安装依赖、启动 `main.py` 并自动在默认浏览器中打开 `http://localhost:8000`。

## 接口说明

### HTTP 接口
- `GET /`: 返回前端主页面。
- `GET /api/car_state`: 返回当前小车运动状态 JSON (50ms 轮询)。
- `GET /api/stream/start`: 启动后端视频流线程。
- `GET /api/stream/stop`: 停止后端视频流线程。

### WebSocket 接口
- `connect` / `disconnect`: 处理客户端连接。
- `video_frame`: 后端推送带 OSD 的视频帧数据 (Base64 编码的 JPEG)。
- `message`: 客户端下发控制指令。
  - 格式: `{ "type": "cmd", "data": { "forward": true, "left": false } }`
  - 格式: `{ "type": "preset", "data": { "action": "forward_500" } }`
- `echo`: 后端返回指令接收确认，包含时间戳用于延迟计算。

### 前端预留接口
- `window.showPointCloud(buffer)`: 接收 Float32Array(xyzrgb) 的 3D 点云数据供 Three.js 渲染。当前在 Console 输出占位。
