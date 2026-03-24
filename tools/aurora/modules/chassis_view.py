"""
底盘状态视图 — 显示 STM32 底盘连接状态、速度、电池电压等信息
"""
import time
import numpy as np


class ChassisViewer:
    """底盘状态与人脸检测信息面板。"""

    # 渐变色
    _BAR_COLORS = {"normal": "#4CAF50", "warn": "#FF9800", "error": "#F44336"}

    def __init__(self, max_speed: float = 500.0, voltage_range=(10.0, 12.6)):
        self.max_speed = max_speed
        self.voltage_range = voltage_range
        self._ax = None

        # 状态数据
        self._chassis_connected = False
        self._vx = 0.0
        self._vy = 0.0
        self._vz = 0.0
        self._voltage = 0.0
        self._face_count = 0
        self._obstacle = False
        self._drive_state = "停止"  # 停止 / 直行 / 避障停车
        self._fps = 0.0
        self._last_update = time.time()
        self._frame_count = 0
        self._speed_history = []

    def update_chassis(self, msg: dict):
        """更新底盘状态。"""
        data = msg.get("data", msg)
        self._chassis_connected = True
        self._vx = data.get("vx", 0.0)
        self._vy = data.get("vy", 0.0)
        self._vz = data.get("vz", 0.0)
        self._voltage = data.get("voltage", 0.0)
        self._speed_history.append(abs(self._vx))
        if len(self._speed_history) > 100:
            self._speed_history = self._speed_history[-100:]

    def update_face_drive(self, msg: dict):
        """更新人脸驱动状态。"""
        data = msg.get("data", msg)
        self._face_count = data.get("face_count", 0)
        self._obstacle = data.get("obstacle", False)
        self._drive_state = data.get("state", "停止")

    def update_detections(self, msg: dict):
        """从检测结果中提取人脸数量。"""
        data = msg.get("data", [])
        self._face_count = sum(1 for d in data if d.get("class") == "face")
        self._frame_count += 1
        now = time.time()
        dt = now - self._last_update
        if dt >= 1.0:
            self._fps = self._frame_count / dt
            self._frame_count = 0
            self._last_update = now

    def update_frame(self, msg: dict):
        """处理综合帧。"""
        if "chassis" in msg:
            self.update_chassis({"data": msg["chassis"]})
        if "face_drive" in msg:
            self.update_face_drive({"data": msg["face_drive"]})

    def _draw(self, frame_num):
        if self._ax is None:
            return

        ax = self._ax
        ax.clear()
        ax.set_xlim(0, 10)
        ax.set_ylim(0, 10)
        ax.set_aspect("equal")
        ax.axis("off")

        bg = "#1E1E2E"
        ax.set_facecolor(bg)

        # 标题
        ax.text(5, 9.5, "底盘控制 & 人脸驱动", fontsize=12, fontweight="bold",
                ha="center", va="top", color="#CDD6F4",
                fontfamily="Microsoft YaHei")

        # 连接状态
        conn_color = "#A6E3A1" if self._chassis_connected else "#F38BA8"
        conn_text = "● UART 已连接" if self._chassis_connected else "○ UART 未连接"
        ax.text(5, 8.6, conn_text, fontsize=10, ha="center", va="top",
                color=conn_color, fontfamily="Microsoft YaHei")

        # 速度仪表
        y_gauge = 7.0
        speed_abs = abs(self._vx)
        speed_ratio = min(speed_abs / self.max_speed, 1.0)

        # 速度条背景
        ax.barh(y_gauge, 8, height=0.5, left=1, color="#313244", zorder=1)
        # 速度条填充
        bar_color = self._BAR_COLORS["normal"] if speed_abs < 300 else \
                    self._BAR_COLORS["warn"] if speed_abs < 400 else \
                    self._BAR_COLORS["error"]
        ax.barh(y_gauge, 8 * speed_ratio, height=0.5, left=1,
                color=bar_color, alpha=0.8, zorder=2)

        ax.text(0.5, y_gauge, "速度", fontsize=9, ha="right", va="center",
                color="#A6ADC8", fontfamily="Microsoft YaHei")
        ax.text(5, y_gauge, f"{self._vx:.0f} mm/s", fontsize=11,
                ha="center", va="center", color="white",
                fontweight="bold", zorder=3, fontfamily="Microsoft YaHei")

        # 电压条
        y_volt = 5.8
        v_min, v_max = self.voltage_range
        v_ratio = max(0, min((self._voltage / 1000 - v_min) / (v_max - v_min), 1.0))
        ax.barh(y_volt, 8, height=0.4, left=1, color="#313244")
        volt_color = "#A6E3A1" if v_ratio > 0.3 else "#F9E2AF" if v_ratio > 0.15 else "#F38BA8"
        ax.barh(y_volt, 8 * v_ratio, height=0.4, left=1, color=volt_color, alpha=0.8)

        ax.text(0.5, y_volt, "电压", fontsize=9, ha="right", va="center",
                color="#A6ADC8", fontfamily="Microsoft YaHei")
        ax.text(5, y_volt, f"{self._voltage / 1000:.1f} V", fontsize=10,
                ha="center", va="center", color="white", fontweight="bold",
                fontfamily="Microsoft YaHei")

        # 人脸检测状态
        y_face = 4.4
        if self._face_count > 0:
            face_icon = "😀"
            face_text = f"检测到 {self._face_count} 张人脸"
            face_color = "#A6E3A1"
        else:
            face_icon = "😶"
            face_text = "未检测到人脸"
            face_color = "#6C7086"
        ax.text(2, y_face, face_icon, fontsize=20, ha="center", va="center")
        ax.text(5.5, y_face, face_text, fontsize=11, ha="center", va="center",
                color=face_color, fontfamily="Microsoft YaHei")

        # 驱动状态
        y_state = 3.2
        state_map = {
            "直行": ("▶ 直行中", "#A6E3A1"),
            "停止": ("■ 已停止", "#6C7086"),
            "避障停车": ("⚠ 避障停车", "#F9E2AF"),
        }
        s_text, s_color = state_map.get(self._drive_state, ("? 未知", "#6C7086"))
        ax.text(5, y_state, s_text, fontsize=14, ha="center", va="center",
                color=s_color, fontweight="bold",
                fontfamily="Microsoft YaHei",
                bbox=dict(boxstyle="round,pad=0.4", facecolor="#313244",
                          edgecolor=s_color, alpha=0.8))

        # 速度历史趋势 (小图)
        if len(self._speed_history) > 2:
            y_base, h = 1.0, 1.5
            x_range = np.linspace(1, 9, len(self._speed_history))
            y_vals = np.array(self._speed_history)
            y_scaled = y_base + (y_vals / max(self.max_speed, 1)) * h
            ax.plot(x_range, y_scaled, color="#89B4FA", linewidth=1.2, alpha=0.7)
            ax.fill_between(x_range, y_base, y_scaled, color="#89B4FA", alpha=0.15)
            ax.text(0.5, y_base + h / 2, "历史", fontsize=8, ha="right",
                    va="center", color="#585B70", fontfamily="Microsoft YaHei")

        # FPS
        ax.text(9.5, 0.3, f"{self._fps:.0f} FPS", fontsize=8, ha="right",
                va="bottom", color="#585B70", fontfamily="Microsoft YaHei")
