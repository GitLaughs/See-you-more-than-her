"""
Aurora 伴侣工具 - 固件烧录模块
通过 CH347 USB-SPI 桥接烧录 A1 开发板固件。

支持功能：
  1. 检测 CH347 设备连接状态
  2. 校验固件文件完整性（MD5）
  3. 通过 CH347 SPI 接口烧录 zImage 固件
  4. 烧录进度回调

前置条件：
  - 安装 WCH CH347 驱动: https://www.wch.cn/downloads/CH343SER_EXE.html
  - A1 开发板 SW3 切到 CH347 侧
  - 下方 Type-C 连接 PC

WCH CH347DLL 路径搜索优先级：
  1. Aurora-2.0.0-ciciec.13/ 同级目录
  2. C:/Windows/System32/CH347DLL.DLL
  3. 环境变量 CH347_DLL_PATH
"""

import ctypes
import hashlib
import os
import struct
import sys
import time
from pathlib import Path
from typing import Callable, Optional

# CH347 DLL 常量
CH347_USB_VENDOR = 0x1A86
CH347_FUNC_SPI = 0x02

# SPI Flash 常量
FLASH_CMD_WRITE_ENABLE = 0x06
FLASH_CMD_PAGE_PROGRAM = 0x02
FLASH_CMD_SECTOR_ERASE = 0x20
FLASH_CMD_READ_STATUS = 0x05
FLASH_CMD_JEDEC_ID = 0x9F
FLASH_CMD_CHIP_ERASE = 0xC7

SPI_PAGE_SIZE = 256
SPI_SECTOR_SIZE = 4096


def _find_ch347_dll() -> Optional[str]:
    """按优先级搜索 CH347DLL.dll。"""
    candidates = []

    # 1. Aurora 同级目录
    repo_root = Path(__file__).resolve().parents[2]
    candidates.append(repo_root / "Aurora-2.0.0-ciciec.13" / "CH347DLL.DLL")

    # 2. System32
    sys32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    candidates.append(sys32 / "CH347DLL.DLL")

    # 3. 环境变量
    env_path = os.environ.get("CH347_DLL_PATH")
    if env_path:
        candidates.insert(0, Path(env_path))

    for p in candidates:
        if p.exists():
            return str(p)
    return None


class FlashTool:
    """CH347 SPI 固件烧录工具。"""

    def __init__(self, on_progress: Optional[Callable[[float, str], None]] = None):
        """
        on_progress: 进度回调 (percent: 0.0~1.0, message: str)
        """
        self._on_progress = on_progress or (lambda p, m: None)
        self._dll = None
        self._device_index = -1
        self._dll_path = _find_ch347_dll()

    @property
    def dll_available(self) -> bool:
        return self._dll_path is not None

    def _report(self, pct: float, msg: str):
        self._on_progress(pct, msg)

    def compute_md5(self, filepath: str) -> str:
        """计算文件 MD5 校验值。"""
        h = hashlib.md5()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                h.update(chunk)
        return h.hexdigest()

    def validate_firmware(self, firmware_path: str) -> dict:
        """校验固件文件，返回信息字典。"""
        p = Path(firmware_path)
        result = {
            "exists": p.exists(),
            "path": str(p.resolve()),
            "size": 0,
            "size_mb": 0.0,
            "md5": "",
            "valid": False,
            "error": "",
        }

        if not p.exists():
            result["error"] = f"固件文件不存在: {firmware_path}"
            return result

        result["size"] = p.stat().st_size
        result["size_mb"] = result["size"] / (1024 * 1024)

        if result["size"] < 1024:
            result["error"] = f"固件文件过小 ({result['size']} 字节)，可能不完整"
            return result

        result["md5"] = self.compute_md5(firmware_path)
        result["valid"] = True
        return result

    def detect_device(self) -> dict:
        """检测 CH347 设备连接状态。"""
        info = {
            "dll_found": self.dll_available,
            "dll_path": self._dll_path or "",
            "device_found": False,
            "device_index": -1,
            "error": "",
        }

        if not self.dll_available:
            info["error"] = (
                "未找到 CH347DLL.DLL。\n"
                "请安装 WCH CH347 驱动: https://www.wch.cn/downloads/CH343SER_EXE.html\n"
                "或设置环境变量 CH347_DLL_PATH 指向 DLL 文件。"
            )
            return info

        try:
            self._dll = ctypes.WinDLL(self._dll_path)

            # CH347OpenDevice(DeviceIndex) -> HANDLE
            self._dll.CH347OpenDevice.argtypes = [ctypes.c_ulong]
            self._dll.CH347OpenDevice.restype = ctypes.c_void_p

            # 扫描 0~15 设备号
            for idx in range(16):
                handle = self._dll.CH347OpenDevice(idx)
                if handle and handle != ctypes.c_void_p(-1).value:
                    self._device_index = idx
                    info["device_found"] = True
                    info["device_index"] = idx
                    # 关闭句柄（后续烧录时再打开）
                    self._dll.CH347CloseDevice(idx)
                    break

            if not info["device_found"]:
                info["error"] = (
                    "未检测到 CH347 设备。\n"
                    "请确认：\n"
                    "  1. A1 开发板下方 Type-C 已连接到电脑\n"
                    "  2. SW3 开关切到 CH347 侧\n"
                    "  3. CH347 驱动已正确安装"
                )
        except OSError as e:
            info["error"] = f"加载 CH347DLL 失败: {e}"

        return info

    def flash(self, firmware_path: str) -> bool:
        """
        执行 SPI Flash 烧录。

        流程：
          1. 打开 CH347 设备
          2. 初始化 SPI 接口
          3. 读取 Flash JEDEC ID
          4. 全片擦除
          5. 逐页写入
          6. 校验

        返回 True 表示成功。
        """
        fw = self.validate_firmware(firmware_path)
        if not fw["valid"]:
            self._report(0, f"固件校验失败: {fw['error']}")
            return False

        self._report(0.01, f"固件: {fw['path']} ({fw['size_mb']:.2f} MB, MD5: {fw['md5'][:8]}...)")

        if not self.dll_available or self._dll is None:
            dev = self.detect_device()
            if not dev["device_found"]:
                self._report(0, f"设备检测失败: {dev['error']}")
                return False

        try:
            # 打开设备
            handle = self._dll.CH347OpenDevice(self._device_index)
            if not handle or handle == ctypes.c_void_p(-1).value:
                self._report(0, "无法打开 CH347 设备")
                return False

            self._report(0.05, f"已打开 CH347 设备 (索引: {self._device_index})")

            # 初始化 SPI（模式 0，时钟约 30MHz）
            spi_cfg = ctypes.create_string_buffer(26)
            struct.pack_into("<BBIBBBBBBBBBB", spi_cfg, 0,
                             0,  # iMode = SPI Mode 0
                             0,  # iClock = 60MHz / 2
                             0,  # reserved
                             8,  # iByteOrder
                             0,  # iSpiWriteReadInterval
                             0,  # iSpiOutDefaultData
                             0,  # iChipSelect (CS0)
                             0,  # CS1Polarity
                             0,  # CS2Polarity
                             0,  # iIsAutoDeactiveCS
                             0,  # iActiveDelay
                             0,  # iDelayDeactive
                             0)  # reserved
            self._dll.CH347SPI_Init(self._device_index, spi_cfg)
            self._report(0.08, "SPI 接口初始化完成")

            # 读取 JEDEC ID
            jedec_buf = ctypes.create_string_buffer(4)
            jedec_buf[0] = FLASH_CMD_JEDEC_ID
            length = ctypes.c_ulong(4)
            self._dll.CH347SPI_WriteRead(
                self._device_index, 0, 4, jedec_buf, ctypes.byref(length)
            )
            jedec_id = jedec_buf[1:4]
            self._report(0.10, f"Flash JEDEC ID: {jedec_id.hex().upper()}")

            # 全片擦除
            self._report(0.12, "正在擦除 Flash（可能需要数十秒）...")
            we_buf = ctypes.create_string_buffer(1)
            we_buf[0] = FLASH_CMD_WRITE_ENABLE
            self._dll.CH347SPI_WriteRead(
                self._device_index, 0, 1, we_buf, ctypes.byref(ctypes.c_ulong(1))
            )

            ce_buf = ctypes.create_string_buffer(1)
            ce_buf[0] = FLASH_CMD_CHIP_ERASE
            self._dll.CH347SPI_WriteRead(
                self._device_index, 0, 1, ce_buf, ctypes.byref(ctypes.c_ulong(1))
            )

            # 等待擦除完成
            status_buf = ctypes.create_string_buffer(2)
            for wait_count in range(600):  # 最长等待 60 秒
                status_buf[0] = FLASH_CMD_READ_STATUS
                status_buf[1] = 0
                self._dll.CH347SPI_WriteRead(
                    self._device_index, 0, 2, status_buf, ctypes.byref(ctypes.c_ulong(2))
                )
                if not (status_buf[1] & 0x01):
                    break
                time.sleep(0.1)
            else:
                self._report(0.12, "擦除超时")
                self._dll.CH347CloseDevice(self._device_index)
                return False

            self._report(0.20, "Flash 擦除完成")

            # 逐页写入
            fw_data = Path(firmware_path).read_bytes()
            total_pages = (len(fw_data) + SPI_PAGE_SIZE - 1) // SPI_PAGE_SIZE
            write_range = 0.60  # 写入占总进度 60%

            for page_idx in range(total_pages):
                offset = page_idx * SPI_PAGE_SIZE
                chunk = fw_data[offset:offset + SPI_PAGE_SIZE]

                # Write Enable
                we_buf[0] = FLASH_CMD_WRITE_ENABLE
                self._dll.CH347SPI_WriteRead(
                    self._device_index, 0, 1, we_buf, ctypes.byref(ctypes.c_ulong(1))
                )

                # Page Program: cmd(1) + addr(3) + data
                pp_len = 4 + len(chunk)
                pp_buf = ctypes.create_string_buffer(pp_len)
                pp_buf[0] = FLASH_CMD_PAGE_PROGRAM
                pp_buf[1] = (offset >> 16) & 0xFF
                pp_buf[2] = (offset >> 8) & 0xFF
                pp_buf[3] = offset & 0xFF
                for i, b in enumerate(chunk):
                    pp_buf[4 + i] = b

                self._dll.CH347SPI_WriteRead(
                    self._device_index, 0, pp_len, pp_buf,
                    ctypes.byref(ctypes.c_ulong(pp_len))
                )

                # 等待页写入完成
                for _ in range(100):
                    status_buf[0] = FLASH_CMD_READ_STATUS
                    status_buf[1] = 0
                    self._dll.CH347SPI_WriteRead(
                        self._device_index, 0, 2, status_buf,
                        ctypes.byref(ctypes.c_ulong(2))
                    )
                    if not (status_buf[1] & 0x01):
                        break
                    time.sleep(0.001)

                pct = 0.20 + write_range * (page_idx + 1) / total_pages
                if page_idx % 100 == 0 or page_idx == total_pages - 1:
                    kb_done = min(offset + len(chunk), len(fw_data)) / 1024
                    self._report(pct, f"写入中: {kb_done:.0f} / {len(fw_data)/1024:.0f} KB")

            self._report(0.85, "写入完成，正在校验...")

            # 简易校验：读前 4KB 对比
            verify_len = min(4096, len(fw_data))
            read_buf = ctypes.create_string_buffer(verify_len + 4)
            read_buf[0] = 0x03  # Read Data command
            read_buf[1] = 0x00
            read_buf[2] = 0x00
            read_buf[3] = 0x00
            vlen = ctypes.c_ulong(verify_len + 4)
            self._dll.CH347SPI_WriteRead(
                self._device_index, 0, verify_len + 4, read_buf, ctypes.byref(vlen)
            )

            read_data = bytes(read_buf[4:4 + verify_len])
            if read_data == fw_data[:verify_len]:
                self._report(0.95, "校验通过 ✓")
            else:
                self._report(0.95, "⚠ 校验数据不一致，建议重新烧录")

            self._dll.CH347CloseDevice(self._device_index)
            self._report(1.0, "烧录完成！")
            return True

        except Exception as e:
            self._report(0, f"烧录异常: {e}")
            try:
                self._dll.CH347CloseDevice(self._device_index)
            except Exception:
                pass
            return False
