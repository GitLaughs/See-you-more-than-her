#!/usr/bin/env python3
"""
Aurora 伴侣工具 - 主入口
=====================================
配合 Aurora 应用，为 A1 开发板提供完整的：
  1. 三维点云实时显示（RPLidar 360° 扫描）
  2. 障碍/避障区域可视化（6 扇区雷达图）
  3. 目标检测结果展示（YOLOv8/SCRFD 置信度+趋势）
  4. 固件烧录（CH347 SPI Flash）

运行方式：
  python aurora_companion.py                 # 默认: 全部功能
  python aurora_companion.py --view          # 仅启动可视化面板
  python aurora_companion.py --flash [PATH]  # 仅执行固件烧录
  python aurora_companion.py --host IP       # 指定 A1 IP 地址
  python aurora_companion.py --demo          # 演示模式（模拟数据）
"""

import argparse
import math
import os
import random
import sys
import threading
import time
from pathlib import Path

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Wedge
import numpy as np

# 确保 modules/ 在路径中
sys.path.insert(0, str(Path(__file__).resolve().parent))

from modules.tcp_client import TcpClient
from modules.pointcloud_view import PointcloudViewer
from modules.obstacle_view import ObstacleViewer
from modules.detection_view import DetectionViewer
from modules.flash_tool import FlashTool

# ---------------------------------------------------------------------------
# 配置加载
# ---------------------------------------------------------------------------
_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_FIRMWARE = _REPO_ROOT / "output" / "evb" / "zImage.smartsens-m1-evb"
_SETTINGS_FILE = Path(__file__).resolve().parent / "config" / "settings.toml"


def _load_settings() -> dict:
    """加载 TOML 配置，兼容无 tomllib 的环境。"""
    settings = {
        "host": "192.168.1.100",
        "port": 9090,
        "reconnect": 3.0,
        "radar_range": 5.0,
        "obstacle_warn": 0.5,
        "fps": 10,
    }
    if not _SETTINGS_FILE.exists():
        return settings
    try:
        # Python 3.11+ 内置 tomllib
        import tomllib
        with open(_SETTINGS_FILE, "rb") as f:
            cfg = tomllib.load(f)
    except ImportError:
        # 简单解析 key = value 形式
        cfg = {}
        section = ""
        for line in _SETTINGS_FILE.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("["):
                section = line.strip("[]").strip()
                cfg.setdefault(section, {})
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                k = k.strip()
                v = v.strip().strip('"').strip("'")
                try:
                    v = float(v) if "." in v else int(v)
                except ValueError:
                    pass
                if section:
                    cfg.setdefault(section, {})[k] = v
                else:
                    cfg[k] = v

    conn = cfg.get("connection", {})
    disp = cfg.get("display", {})
    settings["host"] = conn.get("host", settings["host"])
    settings["port"] = int(conn.get("port", settings["port"]))
    settings["reconnect"] = float(conn.get("reconnect_interval", settings["reconnect"]))
    settings["radar_range"] = float(disp.get("radar_range", settings["radar_range"]))
    settings["obstacle_warn"] = float(disp.get("obstacle_warn_dist", settings["obstacle_warn"]))
    settings["fps"] = int(disp.get("refresh_fps", settings["fps"]))
    return settings


# ---------------------------------------------------------------------------
# 演示数据生成器（--demo 模式）
# ---------------------------------------------------------------------------
class DemoDataGenerator:
    """模拟 A1 板端数据，用于离线演示/调试。"""

    def __init__(self, client: TcpClient):
        self._client = client
        self._running = False
        self._thread = None

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()
        # 触发"已连接"
        self._client._emit("connected", {"host": "demo", "port": 0})

    def stop(self):
        self._running = False

    def _loop(self):
        t = 0.0
        classes = ["person", "face", "car", "dog", "bicycle"]
        while self._running:
            t += 0.1

            # 模拟点云（360° 扫描，带噪声）
            points = []
            for a in range(0, 360, 2):
                base_dist = 2.0 + 1.5 * math.sin(math.radians(a * 3 + t * 30))
                d = max(0.05, base_dist + random.gauss(0, 0.1))
                q = random.randint(5, 15)
                points.append({"a": a, "d": d, "q": q})
            self._client._emit("pointcloud", {"type": "pointcloud", "points": points})

            # 模拟障碍区域
            zones = []
            for i in range(6):
                a_start = i * 60
                a_end = (i + 1) * 60
                min_dist = 1.0 + math.sin(t + i) * 0.8
                blocked = min_dist < 0.5
                zones.append({
                    "angle_start": a_start, "angle_end": a_end,
                    "min_dist": round(min_dist, 2), "blocked": blocked
                })
            self._client._emit("obstacle_zones", {"type": "obstacle_zones", "zones": zones})

            # 模拟检测结果
            n_det = random.randint(0, 4)
            data = []
            for _ in range(n_det):
                cls = random.choice(classes)
                score = round(random.uniform(0.3, 0.99), 2)
                x1, y1 = random.randint(10, 300), random.randint(10, 200)
                data.append({
                    "class": cls, "score": score,
                    "box": [x1, y1, x1 + random.randint(40, 200), y1 + random.randint(60, 250)]
                })
            self._client._emit("detections", {"type": "detections", "data": data})

            time.sleep(0.15)


# ---------------------------------------------------------------------------
# 综合面板
# ---------------------------------------------------------------------------
class CompanionPanel:
    """
    综合可视化面板：将点云、障碍、检测三个视图合并到单窗口。
    布局: [ 雷达俯视图 | 3D 点云 | 障碍区域 | 检测结果 ]
    """

    def __init__(self, tcp: TcpClient, settings: dict):
        self.tcp = tcp
        self.settings = settings
        self.pc_viewer = PointcloudViewer(
            radar_range=settings["radar_range"],
            history_frames=5,
        )
        self.obs_viewer = ObstacleViewer(
            radar_range=settings["radar_range"],
            warn_dist=settings["obstacle_warn"],
        )
        self.det_viewer = DetectionViewer(max_history=60)

        # 注册回调
        tcp.on("pointcloud", self.pc_viewer.update_points)
        tcp.on("obstacle_zones", self.pc_viewer.update_obstacles)
        tcp.on("obstacle_zones", self.obs_viewer.update_obstacles)
        tcp.on("detections", self.obs_viewer.update_detections)
        tcp.on("detections", self.det_viewer.update_detections)
        tcp.on("frame", self.pc_viewer.update_frame)
        tcp.on("frame", self.obs_viewer.update_frame)
        tcp.on("frame", self.det_viewer.update_frame)

        self._fig = None
        self._status_text = None
        self._connected = False

        tcp.on("connected", self._on_connected)
        tcp.on("disconnected", self._on_disconnected)

    def _on_connected(self, msg):
        self._connected = True

    def _on_disconnected(self, msg):
        self._connected = False

    def _draw_all(self, frame_num):
        """统一刷新所有面板。"""
        self.pc_viewer._draw(frame_num)
        self.obs_viewer._draw(frame_num)
        self.det_viewer._draw(frame_num)

        # 更新状态栏
        status = "● 已连接" if self._connected else "○ 等待连接..."
        color = "green" if self._connected else "gray"
        if self._status_text:
            self._status_text.set_text(
                f"  {status}  |  A1: {self.settings['host']}:{self.settings['port']}  |"
                f"  雷达范围: {self.settings['radar_range']}m"
            )
            self._status_text.set_color(color)
        return []

    def show(self):
        """启动综合面板。"""
        self._fig = plt.figure("Aurora 伴侣工具", figsize=(18, 9))
        self._fig.patch.set_facecolor("#F5F5F5")

        # 标题
        self._fig.suptitle(
            "Aurora 伴侣工具  —  A1 开发板实时数据可视化",
            fontsize=14, fontweight="bold", y=0.98
        )

        # 布局: 2 行, 上面 3 列 (雷达 + 3D + 障碍), 下面 3 列 (置信度 + 趋势 + 分布)
        gs = self._fig.add_gridspec(2, 3, hspace=0.35, wspace=0.3,
                                     top=0.92, bottom=0.08, left=0.05, right=0.97)

        # 上排
        self.pc_viewer._ax_2d = self._fig.add_subplot(gs[0, 0], projection="polar")
        self.pc_viewer._ax_3d = self._fig.add_subplot(gs[0, 1], projection="3d")
        self.obs_viewer._ax = self._fig.add_subplot(gs[0, 2], projection="polar")

        # 下排
        self.det_viewer._ax_bar = self._fig.add_subplot(gs[1, 0])
        self.det_viewer._ax_trend = self._fig.add_subplot(gs[1, 1])
        self.det_viewer._ax_table = self._fig.add_subplot(gs[1, 2])

        # 状态栏
        self._status_text = self._fig.text(
            0.02, 0.01, "○ 等待连接...", fontsize=9, color="gray",
            family="monospace"
        )

        interval = max(50, int(1000 / self.settings["fps"]))
        ani = animation.FuncAnimation(
            self._fig, self._draw_all, interval=interval,
            blit=False, cache_frame_data=False
        )
        plt.show()


# ---------------------------------------------------------------------------
# 烧录流程
# ---------------------------------------------------------------------------
def run_flash(firmware_path: str | None = None):
    """交互式烧录流程。"""
    if firmware_path is None:
        firmware_path = str(_DEFAULT_FIRMWARE)

    print("=" * 60)
    print("  Aurora 伴侣工具 - 固件烧录")
    print("=" * 60)

    flash = FlashTool(on_progress=lambda p, m: print(f"  [{p*100:5.1f}%] {m}"))

    # 校验固件
    print(f"\n[1/3] 校验固件: {firmware_path}")
    fw = flash.validate_firmware(firmware_path)
    if not fw["valid"]:
        print(f"  ✗ {fw['error']}")
        return False
    print(f"  ✓ 大小: {fw['size_mb']:.2f} MB, MD5: {fw['md5']}")

    # 检测设备
    print(f"\n[2/3] 检测 CH347 设备...")
    dev = flash.detect_device()
    if not dev["device_found"]:
        print(f"  ✗ {dev['error']}")
        if not dev["dll_found"]:
            print("\n  提示: 请安装 WCH CH347 驱动后重试。")
        return False
    print(f"  ✓ 设备已连接 (索引: {dev['device_index']})")

    # 确认
    print(f"\n[3/3] 准备烧录...")
    print(f"  固件: {fw['path']}")
    print(f"  设备: CH347 #{dev['device_index']}")
    confirm = input("\n  确认烧录？(y/N): ").strip().lower()
    if confirm != "y":
        print("  已取消。")
        return False

    print()
    return flash.flash(firmware_path)


# ---------------------------------------------------------------------------
# 主入口
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Aurora 伴侣工具 — A1 开发板烧录 & 实时数据可视化",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "示例:\n"
            "  python aurora_companion.py                    # 综合面板\n"
            "  python aurora_companion.py --demo             # 演示模式（模拟数据）\n"
            "  python aurora_companion.py --host 10.0.0.50   # 指定 A1 地址\n"
            "  python aurora_companion.py --flash             # 烧录默认固件\n"
            "  python aurora_companion.py --flash path/fw.bin # 烧录指定固件\n"
        ),
    )
    parser.add_argument("--view", action="store_true", help="仅启动可视化面板")
    parser.add_argument("--flash", nargs="?", const="__default__",
                        metavar="FIRMWARE", help="执行固件烧录")
    parser.add_argument("--host", type=str, help="A1 开发板 IP 地址")
    parser.add_argument("--port", type=int, help="A1 调试端口（默认 9090）")
    parser.add_argument("--demo", action="store_true", help="演示模式（模拟数据）")
    args = parser.parse_args()

    settings = _load_settings()
    if args.host:
        settings["host"] = args.host
    if args.port:
        settings["port"] = args.port

    # 烧录模式
    if args.flash is not None:
        fw_path = None if args.flash == "__default__" else args.flash
        success = run_flash(fw_path)
        sys.exit(0 if success else 1)

    # 可视化模式
    print("=" * 60)
    print("  Aurora 伴侣工具 - 实时数据可视化")
    print("=" * 60)
    print(f"  A1 地址: {settings['host']}:{settings['port']}")
    print(f"  雷达范围: {settings['radar_range']}m")
    print(f"  刷新率: {settings['fps']} FPS")
    if args.demo:
        print("  模式: 演示（模拟数据）")
    print()

    tcp = TcpClient(
        host=settings["host"],
        port=settings["port"],
        reconnect_interval=settings["reconnect"],
    )

    panel = CompanionPanel(tcp, settings)

    if args.demo:
        demo = DemoDataGenerator(tcp)
        demo.start()
    else:
        tcp.start()

    try:
        panel.show()
    except KeyboardInterrupt:
        pass
    finally:
        if args.demo:
            demo.stop()
        else:
            tcp.stop()
        print("\n已退出。")


if __name__ == "__main__":
    main()
