"""
Aurora 伴侣工具 - 三维点云显示模块
接收 A1 板端 RPLidar 扫描数据，在 Windows 本地渲染三维点云。

数据格式（来自 A1 TCP 9090）:
  {"type":"pointcloud","points":[{"a":角度度,"d":距离米,"q":信号质量}, ...]}

显示模式:
  1. 2D 极坐标雷达图（俯视图）
  2. 3D 散点图（带高度模拟）
"""

import math
import threading
from collections import deque

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.patches import Wedge
import numpy as np


class PointcloudViewer:
    """三维点云查看器，基于 matplotlib。"""

    def __init__(self, radar_range: float = 5.0, history_frames: int = 5):
        self.radar_range = radar_range
        self.history_frames = history_frames
        self._points_history: deque = deque(maxlen=history_frames)
        self._latest_points: list = []
        self._obstacle_zones: list = []
        self._lock = threading.Lock()
        self._fig = None
        self._ax_2d = None
        self._ax_3d = None

    def update_points(self, msg: dict):
        """TCP 回调：更新点云数据。"""
        points = msg.get("points", [])
        with self._lock:
            self._latest_points = points
            self._points_history.append(points)

    def update_obstacles(self, msg: dict):
        """TCP 回调：更新障碍区域。"""
        zones = msg.get("zones", [])
        with self._lock:
            self._obstacle_zones = zones

    def update_frame(self, msg: dict):
        """TCP 回调：更新综合帧数据。"""
        with self._lock:
            pc = msg.get("pointcloud", [])
            if pc:
                self._latest_points = pc
                self._points_history.append(pc)
            oz = msg.get("obstacle_zones", [])
            if oz:
                self._obstacle_zones = oz

    def _draw(self, frame_num):
        """刷新绘图。"""
        with self._lock:
            points = list(self._latest_points)
            obstacles = list(self._obstacle_zones)
            history = list(self._points_history)

        # --- 2D 雷达图 ---
        self._ax_2d.clear()
        self._ax_2d.set_title("雷达俯视图", fontsize=12)
        self._ax_2d.set_ylim(0, self.radar_range)

        # 绘制障碍扇区
        for zone in obstacles:
            a_start = zone.get("angle_start", 0)
            a_end = zone.get("angle_end", 0)
            blocked = zone.get("blocked", False)
            min_dist = zone.get("min_dist", self.radar_range)
            color = "red" if blocked else "green"
            alpha = 0.3 if blocked else 0.1
            wedge = Wedge(
                (0, 0), min(min_dist, self.radar_range),
                a_start - 90, a_end - 90,
                alpha=alpha, color=color
            )
            self._ax_2d.add_patch(wedge)

        # 绘制历史点（淡色）
        for i, old_pts in enumerate(history[:-1] if len(history) > 1 else []):
            if not old_pts:
                continue
            angles = [math.radians(p["a"]) for p in old_pts if p.get("d", 0) > 0.01]
            dists = [p["d"] for p in old_pts if p.get("d", 0) > 0.01]
            fade = 0.1 + 0.15 * (i / max(len(history) - 1, 1))
            self._ax_2d.scatter(angles, dists, s=1, c="gray", alpha=fade)

        # 绘制当前帧点
        if points:
            angles = [math.radians(p["a"]) for p in points if p.get("d", 0) > 0.01]
            dists = [p["d"] for p in points if p.get("d", 0) > 0.01]
            qualities = [p.get("q", 8) for p in points if p.get("d", 0) > 0.01]
            colors = np.array(qualities) / 15.0
            self._ax_2d.scatter(angles, dists, s=3, c=colors, cmap="viridis", alpha=0.8)

        # 绘制距离标尺
        for r in range(1, int(self.radar_range) + 1):
            self._ax_2d.plot(np.linspace(0, 2 * math.pi, 100),
                             [r] * 100, ":", color="gray", linewidth=0.3, alpha=0.3)

        # --- 3D 散点图 ---
        self._ax_3d.clear()
        self._ax_3d.set_title("三维点云", fontsize=12)
        self._ax_3d.set_xlabel("X (m)")
        self._ax_3d.set_ylabel("Y (m)")
        self._ax_3d.set_zlabel("Z (m)")
        self._ax_3d.set_xlim(-self.radar_range, self.radar_range)
        self._ax_3d.set_ylim(-self.radar_range, self.radar_range)
        self._ax_3d.set_zlim(-0.5, 1.0)

        if points:
            xs, ys, zs, cs = [], [], [], []
            for p in points:
                d = p.get("d", 0)
                if d < 0.01:
                    continue
                a_rad = math.radians(p["a"])
                xs.append(d * math.cos(a_rad))
                ys.append(d * math.sin(a_rad))
                # 2D 雷达无高度信息，用质量模拟 Z 轴
                zs.append(p.get("q", 8) * 0.02)
                cs.append(d)

            if xs:
                self._ax_3d.scatter(xs, ys, zs, s=2, c=cs, cmap="plasma", alpha=0.7)

        # 绘制坐标原点标记（机器人位置）
        self._ax_3d.scatter([0], [0], [0], s=50, c="red", marker="^")

        return []

    def show(self):
        """启动交互式显示窗口（阻塞主线程）。"""
        self._fig = plt.figure("Aurora 点云查看器", figsize=(14, 6))
        self._ax_2d = self._fig.add_subplot(121, projection="polar")
        self._ax_3d = self._fig.add_subplot(122, projection="3d")

        ani = matplotlib.animation.FuncAnimation(
            self._fig, self._draw, interval=100, blit=False, cache_frame_data=False
        )
        plt.tight_layout()
        plt.show()
