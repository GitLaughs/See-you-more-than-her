"""
Aurora 伴侣工具 - 障碍/避障信息显示模块
以 2D 雷达视图展示 6 个扇区的障碍状态 + 最近障碍距离。

数据格式（来自 A1 TCP 9090）:
  {"type":"obstacle_zones","zones":[
    {"angle_start":0,"angle_end":60,"min_dist":0.35,"blocked":true}, ...
  ]}

显示：
  - 扇区着色：红色=阻塞，绿色=畅通
  - 中心机器人图标
  - 实时最小距离标注
"""

import math
import threading

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.patches import Wedge
import numpy as np


class ObstacleViewer:
    """障碍区域可视化查看器。"""

    def __init__(self, radar_range: float = 5.0, warn_dist: float = 0.5):
        self.radar_range = radar_range
        self.warn_dist = warn_dist
        self._zones: list = []
        self._detections: list = []
        self._lock = threading.Lock()
        self._fig = None
        self._ax = None

    def update_obstacles(self, msg: dict):
        """TCP 回调：更新障碍区域。"""
        zones = msg.get("zones", [])
        with self._lock:
            self._zones = zones

    def update_detections(self, msg: dict):
        """TCP 回调：更新检测结果（用于标注）。"""
        data = msg.get("data", [])
        with self._lock:
            self._detections = data

    def update_frame(self, msg: dict):
        """TCP 回调：更新综合帧数据。"""
        with self._lock:
            oz = msg.get("obstacle_zones", [])
            if oz:
                self._zones = oz
            det = msg.get("detections", [])
            if det:
                self._detections = det

    def _draw(self, frame_num):
        """刷新绘图。"""
        with self._lock:
            zones = list(self._zones)
            detections = list(self._detections)

        self._ax.clear()
        self._ax.set_title("障碍区域\u3000", fontsize=14, fontweight="bold")
        self._ax.set_ylim(0, self.radar_range)
        self._ax.set_yticklabels([])

        # 绘制距离环
        for r in np.arange(0.5, self.radar_range + 0.1, 0.5):
            theta = np.linspace(0, 2 * math.pi, 200)
            self._ax.plot(theta, [r] * len(theta), "--", color="gray",
                          linewidth=0.4, alpha=0.4)

        # 警告距离环（高亮）
        theta = np.linspace(0, 2 * math.pi, 200)
        self._ax.plot(theta, [self.warn_dist] * len(theta), "-",
                      color="orange", linewidth=1.5, alpha=0.6)

        if not zones:
            self._ax.text(0, self.radar_range * 0.5, "等待数据...",
                          ha="center", va="center", fontsize=12, color="gray")
            return []

        # 绘制每个扇区
        blocked_count = 0
        for zone in zones:
            a_start = zone.get("angle_start", 0)
            a_end = zone.get("angle_end", 0)
            blocked = zone.get("blocked", False)
            min_dist = zone.get("min_dist", self.radar_range)

            if blocked:
                blocked_count += 1

            # 扇区底色
            color = "#FF4444" if blocked else "#44DD44"
            alpha = 0.35 if blocked else 0.15

            # 画满扇区背景
            theta_range = np.linspace(math.radians(a_start), math.radians(a_end), 50)
            self._ax.fill_between(theta_range, 0, self.radar_range,
                                  color=color, alpha=alpha)

            # 最近障碍物弧线
            if min_dist < self.radar_range:
                self._ax.plot(theta_range, [min_dist] * len(theta_range),
                              color=color, linewidth=2.5, alpha=0.8)

            # 标注距离
            mid_angle = math.radians((a_start + a_end) / 2)
            label_r = min(min_dist, self.radar_range) * 0.6
            dist_text = f"{min_dist:.2f}m"
            text_color = "red" if blocked else "green"
            self._ax.text(mid_angle, max(label_r, 0.3), dist_text,
                          ha="center", va="center", fontsize=8,
                          fontweight="bold", color=text_color,
                          bbox=dict(boxstyle="round,pad=0.15",
                                    facecolor="white", alpha=0.8, edgecolor=text_color))

        # 中心状态
        status = "⚠ 阻塞" if blocked_count > 0 else "✓ 畅通"
        status_color = "red" if blocked_count > 0 else "green"
        self._ax.text(0, 0.15, status, ha="center", va="center",
                      fontsize=10, fontweight="bold", color=status_color)

        # 检测结果摘要（右上角）
        if detections:
            names = [f"{d.get('class', '?')}({d.get('score', 0):.0%})"
                     for d in detections[:5]]
            det_text = "检测: " + ", ".join(names)
            self._ax.text(math.pi / 4, self.radar_range * 0.92, det_text,
                          fontsize=7, color="blue", ha="center",
                          bbox=dict(boxstyle="round", facecolor="lightyellow", alpha=0.8))

        return []

    def show(self):
        """启动交互式显示窗口（阻塞主线程）。"""
        self._fig = plt.figure("Aurora 障碍区域查看器", figsize=(7, 7))
        self._ax = self._fig.add_subplot(111, projection="polar")

        ani = matplotlib.animation.FuncAnimation(
            self._fig, self._draw, interval=150, blit=False, cache_frame_data=False
        )
        plt.tight_layout()
        plt.show()
