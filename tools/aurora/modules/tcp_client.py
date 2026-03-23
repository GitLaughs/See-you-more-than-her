"""
Aurora 伴侣工具 - TCP 客户端模块
连接 A1 开发板调试数据接口（端口 9090），解析 JSON 数据流。

A1 板端通过 debug_data_interface 发送换行符分隔的 JSON 消息，格式:
  {"type":"pointcloud","points":[{"a":角度,"d":距离,"q":质量},...]}\n
  {"type":"detections","data":[{"class":"类别","score":置信度,"box":[x1,y1,x2,y2]},...]}\n
  {"type":"obstacle_zones","zones":[{"angle_start":0,"angle_end":60,"min_dist":0.35,"blocked":true},...]}\n
  {"type":"frame","timestamp_ms":..., "pointcloud":[...],"detections":[...],"obstacle_zones":[...]}\n
"""

import json
import socket
import threading
import time
from typing import Callable, Optional


class TcpClient:
    """非阻塞 TCP 客户端，自动重连，解析换行分隔 JSON。"""

    def __init__(self, host: str = "192.168.1.100", port: int = 9090,
                 reconnect_interval: float = 3.0, buffer_size: int = 65536):
        self.host = host
        self.port = port
        self.reconnect_interval = reconnect_interval
        self.buffer_size = buffer_size

        self._sock: Optional[socket.socket] = None
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._callbacks: dict[str, list[Callable]] = {}
        self._lock = threading.Lock()

    def on(self, message_type: str, callback: Callable):
        """注册消息回调。message_type: pointcloud/detections/obstacle_zones/frame/connected/disconnected"""
        self._callbacks.setdefault(message_type, []).append(callback)

    def _emit(self, message_type: str, data):
        for cb in self._callbacks.get(message_type, []):
            try:
                cb(data)
            except Exception as e:
                print(f"[TcpClient] 回调异常 ({message_type}): {e}")

    def start(self):
        """启动后台接收线程。"""
        self._running = True
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()

    def stop(self):
        """停止客户端。"""
        self._running = False
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
        if self._thread:
            self._thread.join(timeout=5)

    def _connect(self) -> bool:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            sock.connect((self.host, self.port))
            sock.settimeout(1.0)
            self._sock = sock
            print(f"[TcpClient] 已连接 {self.host}:{self.port}")
            self._emit("connected", {"host": self.host, "port": self.port})
            return True
        except (ConnectionRefusedError, OSError, TimeoutError) as e:
            print(f"[TcpClient] 连接失败: {e}")
            return False

    def _run_loop(self):
        buf = ""
        while self._running:
            if self._sock is None:
                if not self._connect():
                    time.sleep(self.reconnect_interval)
                    continue
                buf = ""

            try:
                chunk = self._sock.recv(self.buffer_size)
                if not chunk:
                    raise ConnectionResetError("连接关闭")
                buf += chunk.decode("utf-8", errors="replace")

                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        msg = json.loads(line)
                        msg_type = msg.get("type", "unknown")
                        self._emit(msg_type, msg)
                    except json.JSONDecodeError:
                        pass

            except (socket.timeout,):
                continue
            except (ConnectionResetError, BrokenPipeError, OSError):
                print(f"[TcpClient] 连接断开，{self.reconnect_interval}s 后重连...")
                self._emit("disconnected", {})
                try:
                    self._sock.close()
                except OSError:
                    pass
                self._sock = None
                time.sleep(self.reconnect_interval)
