"""
Aurora 伴侣工具 - 目标检测结果显示模块
以图表形式展示 YOLOv8/SCRFD 检测结果，包含置信度柱状图和边界框列表。

数据格式（来自 A1 TCP 9090）:
  {"type":"detections","data":[
    {"class":"person","score":0.95,"box":[x1,y1,x2,y2]}, ...
  ]}
"""

import threading
from collections import deque

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import numpy as np


# 类别颜色映射（与 A1 OSD 层一致）
_CLASS_COLORS = {
    "person": "#FF6B6B",
    "face": "#4ECDC4",
    "car": "#45B7D1",
    "bicycle": "#96CEB4",
    "dog": "#FFEAA7",
    "cat": "#DDA0DD",
}
_DEFAULT_COLOR = "#A0A0A0"


class DetectionViewer:
    """目标检测结果查看器。"""

    def __init__(self, max_history: int = 60):
        self.max_history = max_history
        self._detections: list = []
        self._count_history: deque = deque(maxlen=max_history)
        self._class_counts: dict = {}
        self._lock = threading.Lock()
        self._fig = None
        self._ax_bar = None
        self._ax_trend = None
        self._ax_table = None

    def update_detections(self, msg: dict):
        """TCP 回调：更新检测数据。"""
        data = msg.get("data", [])
        with self._lock:
            self._detections = data
            self._count_history.append(len(data))
            counts = {}
            for d in data:
                cls = d.get("class", "unknown")
                counts[cls] = counts.get(cls, 0) + 1
            self._class_counts = counts

    def update_frame(self, msg: dict):
        """TCP 回调：更新综合帧数据。"""
        det = msg.get("detections", [])
        if det:
            with self._lock:
                self._detections = det
                self._count_history.append(len(det))
                counts = {}
                for d in det:
                    cls = d.get("class", "unknown")
                    counts[cls] = counts.get(cls, 0) + 1
                self._class_counts = counts

    def _draw(self, frame_num):
        """刷新绘图。"""
        with self._lock:
            detections = list(self._detections)
            class_counts = dict(self._class_counts)
            count_history = list(self._count_history)

        # --- 置信度柱状图 ---
        self._ax_bar.clear()
        self._ax_bar.set_title("检测置信度", fontsize=11)
        self._ax_bar.set_xlim(0, 1.0)
        self._ax_bar.set_xlabel("置信度")

        if detections:
            labels = []
            scores = []
            colors = []
            for i, d in enumerate(detections[:12]):  # 最多显示 12 个
                cls = d.get("class", "?")
                score = d.get("score", 0)
                labels.append(f"#{i} {cls}")
                scores.append(score)
                colors.append(_CLASS_COLORS.get(cls, _DEFAULT_COLOR))

            y_pos = np.arange(len(labels))
            self._ax_bar.barh(y_pos, scores, color=colors, height=0.6, alpha=0.8)
            self._ax_bar.set_yticks(y_pos)
            self._ax_bar.set_yticklabels(labels, fontsize=8)
            self._ax_bar.axvline(x=0.5, color="orange", linestyle="--",
                                 linewidth=1, alpha=0.5, label="阈值 0.5")
            self._ax_bar.invert_yaxis()

            # 标注数值
            for i, (s, c) in enumerate(zip(scores, colors)):
                self._ax_bar.text(s + 0.02, i, f"{s:.2f}", va="center",
                                  fontsize=7, color=c)
        else:
            self._ax_bar.text(0.5, 0.5, "无检测结果", transform=self._ax_bar.transAxes,
                              ha="center", va="center", fontsize=12, color="gray")

        # --- 检测数量趋势 ---
        self._ax_trend.clear()
        self._ax_trend.set_title("检测数量趋势", fontsize=11)
        self._ax_trend.set_ylabel("目标数")
        self._ax_trend.set_xlabel("帧")

        if count_history:
            x = np.arange(len(count_history))
            self._ax_trend.fill_between(x, count_history, alpha=0.3, color="steelblue")
            self._ax_trend.plot(x, count_history, "-", color="steelblue", linewidth=1.5)
            self._ax_trend.set_ylim(0, max(max(count_history) + 1, 5))
            # 最新值标记
            self._ax_trend.text(len(count_history) - 1, count_history[-1],
                                f" {count_history[-1]}", fontsize=9, color="steelblue",
                                fontweight="bold", va="bottom")
        else:
            self._ax_trend.set_ylim(0, 5)

        # --- 类别统计饼图/文本 ---
        self._ax_table.clear()
        self._ax_table.set_title("类别分布", fontsize=11)
        self._ax_table.axis("off")

        if class_counts:
            classes = list(class_counts.keys())
            counts_list = list(class_counts.values())
            colors = [_CLASS_COLORS.get(c, _DEFAULT_COLOR) for c in classes]
            self._ax_table.pie(counts_list, labels=classes, colors=colors,
                               autopct="%1.0f%%", startangle=90, textprops={"fontsize": 9})
        else:
            self._ax_table.text(0.5, 0.5, "等待数据...", ha="center", va="center",
                                fontsize=12, color="gray")

        return []

    def show(self):
        """启动交互式显示窗口（阻塞主线程）。"""
        self._fig = plt.figure("Aurora 检测结果查看器", figsize=(14, 7))
        self._ax_bar = self._fig.add_subplot(131)
        self._ax_trend = self._fig.add_subplot(132)
        self._ax_table = self._fig.add_subplot(133)

        ani = matplotlib.animation.FuncAnimation(
            self._fig, self._draw, interval=200, blit=False, cache_frame_data=False
        )
        plt.tight_layout()
        plt.show()
