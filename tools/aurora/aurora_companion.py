#!/usr/bin/env python3
"""
Aurora Companion — A1 开发板摄像头可视化采集伴侣

增强版拍照工具，在 aurora_capture 基础上提供：
  - 精心设计的现代暗色玻璃态 UI
  - 摄像头断联自动检测 + 一键刷新恢复
  - 实时 FPS 及连接状态显示
  - 最近拍摄缩略图画廊 (最多 8 张)
  - 双分辨率拍摄：1280×720 (原始) 和 640×360 (训练集, 16:9)
  - 键盘快捷键：1/2/R

用法:
  python aurora_companion.py [--device 0] [--output ../../data/yolov8_dataset/raw/images] [--port 5001]
"""

import argparse
import base64
import os
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template_string, request

# ─── 摄像头参数 ───────────────────────────────────────────────────────────────
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

CAPTURE_FORMATS = {
    "1280x720": (1280, 720),   # 原始灰度图（全分辨率）
    "640x360":  (640,  360),   # YOLOv8 训练集（16:9 中心裁剪）
}

app = Flask(__name__)

# ─── 全局状态 ─────────────────────────────────────────────────────────────────
camera: cv2.VideoCapture | None = None
camera_lock = threading.Lock()
device_id_global = 0
output_dir: str = ""
capture_count = 0
recent_captures: deque = deque(maxlen=8)

_frame_count = 0
_fps_ts = time.time()
current_fps = 0.0
camera_connected = False
_fail_streak = 0
_last_reconnect = 0.0


# ─── 摄像头操作 ───────────────────────────────────────────────────────────────

def _try_open(device_id: int) -> cv2.VideoCapture | None:
    apis = [cv2.CAP_DSHOW] if sys.platform == "win32" else [cv2.CAP_V4L2]
    for api in apis + [cv2.CAP_ANY]:
        cap = cv2.VideoCapture(device_id, api)
        if cap.isOpened():
            break
    else:
        return None

    for s in ("Y800", "GREY", "Y8  "):
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*s))

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  CAMERA_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS,          CAMERA_FPS)
    cap.set(cv2.CAP_PROP_CONVERT_RGB,  0)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"[INFO] 摄像头已连接: {w}×{h} (设备 {device_id})")
    return cap


def _read_gray(cap: cv2.VideoCapture) -> np.ndarray | None:
    ret, frame = cap.read()
    if not ret:
        return None
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    if frame.shape != (CAMERA_HEIGHT, CAMERA_WIDTH):
        n = CAMERA_WIDTH * CAMERA_HEIGHT
        flat = frame.flatten()
        frame = (flat[:n].reshape(CAMERA_HEIGHT, CAMERA_WIDTH)
                 if flat.size >= n
                 else cv2.resize(frame, (CAMERA_WIDTH, CAMERA_HEIGHT)))
    return frame


def _save_capture(frame: np.ndarray, fmt: str) -> dict:
    global capture_count
    tw, th = CAPTURE_FORMATS[fmt]
    h, w = frame.shape[:2]
    if fmt == "640x360":
        x = max(0, (w - tw) // 2)
        y = max(0, (h - th) // 2)
        out = frame[y:y + th, x:x + tw]
    else:
        out = frame.copy()
    if out.shape != (th, tw):
        out = cv2.resize(out, (tw, th))

    capture_count += 1
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    name = f"capture_{ts}_{capture_count:04d}_{fmt}.png"
    path = os.path.join(output_dir, name)
    cv2.imwrite(path, out)

    thumb = cv2.resize(out, (160, 90 if fmt == "640x360" else 80))
    _, buf = cv2.imencode(".jpg", thumb, [cv2.IMWRITE_JPEG_QUALITY, 72])
    thumb_b64 = base64.b64encode(buf).decode()

    info = {
        "filename": name,
        "format": fmt,
        "size": f"{tw}×{th}",
        "thumb": thumb_b64,
        "time": datetime.now().strftime("%H:%M:%S"),
        "index": capture_count,
    }
    recent_captures.appendleft(info)
    print(f"[CAPTURE] {path}  ({tw}×{th})")
    return info


# ─── 视频流 ───────────────────────────────────────────────────────────────────

def generate_frames():
    global camera, current_fps, _frame_count, _fps_ts
    global camera_connected, _fail_streak, _last_reconnect

    FAIL_THRESH      = 10
    RECONNECT_GAP    = 3.0

    while True:
        with camera_lock:
            cap = camera

        frame = _read_gray(cap) if cap else None

        if frame is None:
            camera_connected = False
            _fail_streak += 1
            now = time.time()
            if (_fail_streak >= FAIL_THRESH
                    and now - _last_reconnect > RECONNECT_GAP):
                _last_reconnect = now
                _fail_streak = 0
                print("[INFO] 自动重连摄像头...")
                with camera_lock:
                    if camera:
                        camera.release()
                    camera = _try_open(device_id_global)
                if camera:
                    print("[INFO] 摄像头自动重连成功")
                    camera_connected = True

            # 占位黑帧
            blk = np.zeros((CAMERA_HEIGHT, CAMERA_WIDTH), dtype=np.uint8)
            cv2.putText(blk, "No Signal  —  Reconnecting...",
                        (CAMERA_WIDTH // 2 - 200, CAMERA_HEIGHT // 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.9, (60, 60, 60), 2)
            disp = cv2.cvtColor(blk, cv2.COLOR_GRAY2BGR)
            _, buf = cv2.imencode(".jpg", disp, [cv2.IMWRITE_JPEG_QUALITY, 50])
            yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                   + buf.tobytes() + b"\r\n")
            time.sleep(0.35)
            continue

        camera_connected = True
        _fail_streak = 0

        # FPS 计算
        _frame_count += 1
        now = time.time()
        if now - _fps_ts >= 1.0:
            current_fps = _frame_count / (now - _fps_ts)
            _frame_count = 0
            _fps_ts = now

        # 绘制叠加层
        disp = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
        h, w = disp.shape[:2]
        cx, cy = w // 2, h // 2

        # 640×360 训练裁剪框
        x1, y1, x2, y2 = cx - 320, cy - 180, cx + 320, cy + 180
        cv2.rectangle(disp, (x1, y1), (x2, y2), (45, 210, 100), 1)
        cv2.putText(disp, "640x360 train", (x1 + 6, y1 + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.52, (45, 210, 100), 1)

        # FPS 水印
        cv2.putText(disp, f"FPS {current_fps:.1f}", (w - 100, 24),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180, 180, 180), 1)

        _, buf = cv2.imencode(".jpg", disp, [cv2.IMWRITE_JPEG_QUALITY, 82])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
               + buf.tobytes() + b"\r\n")


# ─── HTML 模板 ────────────────────────────────────────────────────────────────

HTML = r"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Aurora Companion</title>
<style>
:root {
  --bg:        #0d1117;
  --surface:   #161b22;
  --border:    #30363d;
  --muted:     #8b949e;
  --text:      #e6edf3;
  --blue:      #58a6ff;
  --green:     #3fb950;
  --purple:    #a371f7;
  --red:       #f85149;
  --amber:     #d29922;
  --radius:    10px;
}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;min-height:100vh;display:flex;flex-direction:column;}

/* ── Topbar ─────────────────────────────────── */
.topbar{
  background:linear-gradient(90deg,#161b22,#1c2128);
  border-bottom:1px solid var(--border);
  padding:10px 24px;
  display:flex;align-items:center;justify-content:space-between;
  box-shadow:0 2px 16px rgba(0,0,0,.5);
  position:sticky;top:0;z-index:100;
}
.logo{display:flex;align-items:center;gap:10px;}
.logo-icon{
  width:32px;height:32px;border-radius:8px;
  background:linear-gradient(135deg,var(--blue),var(--purple));
  display:flex;align-items:center;justify-content:center;font-size:16px;
}
.logo-text{font-size:1.15em;font-weight:700;
  background:linear-gradient(90deg,var(--blue),var(--purple));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;}
.logo-sub{font-size:.72em;color:var(--muted);margin-left:2px;border-left:1px solid var(--border);padding-left:10px;}
.status-badge{
  display:flex;align-items:center;gap:7px;
  padding:5px 14px;border-radius:20px;
  border:1px solid var(--border);background:var(--bg);
  font-size:.83em;
}
.dot{width:8px;height:8px;border-radius:50%;background:var(--green);animation:blink 2s ease-in-out infinite;}
.dot.off{background:var(--red);animation:none;}
@keyframes blink{0%,100%{opacity:1;}50%{opacity:.3;}}

/* ── Layout ─────────────────────────────────── */
.workspace{display:grid;grid-template-columns:1fr 320px;gap:14px;padding:14px;flex:1;}

/* ── Cards ──────────────────────────────────── */
.card{
  background:var(--surface);border:1px solid var(--border);
  border-radius:var(--radius);overflow:hidden;
}
.card-head{
  padding:9px 16px;border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;
  font-size:.78em;font-weight:600;color:var(--muted);
  text-transform:uppercase;letter-spacing:.06em;
}
.card-body{padding:14px;}

/* ── Preview ─────────────────────────────────── */
.preview-wrap{position:relative;background:#000;line-height:0;}
.preview-wrap img{width:100%;display:block;}
.preview-chip{
  position:absolute;top:8px;left:8px;
  background:rgba(13,17,23,.75);backdrop-filter:blur(4px);
  border:1px solid var(--border);
  border-radius:6px;padding:3px 10px;font-size:.72em;font-family:monospace;
  color:var(--green);
}
.preview-fps{
  position:absolute;top:8px;right:8px;
  background:rgba(13,17,23,.75);backdrop-filter:blur(4px);
  border:1px solid var(--border);
  border-radius:6px;padding:3px 10px;font-size:.72em;font-family:monospace;
  color:var(--muted);
}

/* ── Sidebar ─────────────────────────────────── */
.sidebar{display:flex;flex-direction:column;gap:12px;}

/* counter */
.counter-box{text-align:center;padding:18px 0 10px;}
.counter-num{
  font-size:3.4em;font-weight:800;line-height:1;
  background:linear-gradient(135deg,var(--blue),var(--purple));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;
}
.counter-lbl{font-size:.72em;color:var(--muted);margin-top:4px;}

/* buttons */
.btn{
  width:100%;padding:11px 14px;margin-bottom:6px;
  border:1px solid transparent;border-radius:8px;
  font-size:.92em;font-weight:600;cursor:pointer;
  display:flex;align-items:center;gap:10px;
  transition:transform .12s,box-shadow .12s,background .12s;
}
.btn:active{transform:scale(.98);}
.btn-label{display:flex;flex-direction:column;align-items:flex-start;}
.btn small{font-weight:400;font-size:.76em;color:rgba(255,255,255,.55);margin-top:1px;}
.btn-icon{font-size:1.3em;flex-shrink:0;}

.btn-blue{background:linear-gradient(135deg,#1f6feb,#388bfd);color:#fff;border-color:#1f6feb;}
.btn-blue:hover{background:linear-gradient(135deg,#388bfd,var(--blue));box-shadow:0 4px 14px rgba(56,139,253,.4);}

.btn-green{background:linear-gradient(135deg,#238636,#2ea043);color:#fff;border-color:#238636;}
.btn-green:hover{background:linear-gradient(135deg,#2ea043,var(--green));box-shadow:0 4px 14px rgba(46,160,67,.4);}

.btn-ghost{background:transparent;color:var(--muted);border-color:var(--border);}
.btn-ghost:hover{background:#21262d;color:var(--text);border-color:var(--blue);}
.btn-ghost:disabled{opacity:.5;cursor:not-allowed;}

.spin-icon{display:inline-block;}
.spinning .spin-icon{animation:spin .8s linear infinite;}
@keyframes spin{to{transform:rotate(360deg);}}

/* divider */
.divider{height:1px;background:var(--border);margin:8px 0;}

/* info table */
.info-tbl{width:100%;font-size:.8em;border-collapse:collapse;}
.info-tbl td{padding:4px 0;vertical-align:top;}
.info-tbl td:first-child{color:var(--muted);width:80px;white-space:nowrap;}
.info-tbl td:last-child{color:var(--text);word-break:break-all;}

/* kbd shortcuts */
.shortcut-row{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px;align-items:center;}
.kbd{
  background:#21262d;border:1px solid var(--border);
  border-radius:4px;padding:2px 7px;
  font-size:.73em;font-family:monospace;color:var(--muted);
}
.shortcut-row span{font-size:.73em;color:var(--muted);}

/* gallery */
.gallery-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;}
.gallery-item{
  position:relative;border-radius:5px;overflow:hidden;
  background:#000;cursor:pointer;
  border:1px solid var(--border);
  transition:border-color .15s;
}
.gallery-item:hover{border-color:var(--blue);}
.gallery-item img{width:100%;display:block;}
.gallery-item .gallery-lbl{
  position:absolute;bottom:0;left:0;right:0;
  background:rgba(0,0,0,.65);
  font-size:.58em;color:var(--muted);
  padding:2px 4px;text-align:center;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis;
}
.gallery-empty{
  grid-column:1/-1;text-align:center;
  color:var(--muted);font-size:.82em;padding:18px 0;
}

/* toast */
.toast{
  position:fixed;bottom:22px;right:22px;
  padding:10px 18px;border-radius:8px;
  font-size:.88em;font-weight:600;
  opacity:0;transform:translateY(6px);
  transition:all .22s;z-index:999;
  box-shadow:0 4px 20px rgba(0,0,0,.5);
  max-width:360px;pointer-events:none;
}
.toast.show{opacity:1;transform:translateY(0);}
.toast-ok{background:#238636;color:#fff;}
.toast-err{background:#da3633;color:#fff;}
.toast-info{background:#1f6feb;color:#fff;}
</style>
</head>
<body>

<!-- Topbar -->
<div class="topbar">
  <div class="logo">
    <div class="logo-icon">⚡</div>
    <span class="logo-text">Aurora Companion</span>
    <span class="logo-sub">SC132GS · 1280×720</span>
  </div>
  <div class="status-badge">
    <div class="dot" id="dot"></div>
    <span id="statusTxt">连接中...</span>
  </div>
</div>

<!-- Main workspace -->
<div class="workspace">

  <!-- Left: Preview -->
  <div style="display:flex;flex-direction:column;gap:12px;">
    <div class="card">
      <div class="card-head">
        <span>实时预览</span>
        <span style="color:var(--green);font-size:.9em" id="resTag">1280 × 720</span>
      </div>
      <div class="preview-wrap">
        <img id="stream" src="/video_feed" alt="camera preview"
             onload="onStreamOk()" onerror="onStreamErr()">
        <div class="preview-chip">SC132GS · Gray</div>
        <div class="preview-fps" id="fpsChip">— fps</div>
      </div>
    </div>

    <!-- Gallery -->
    <div class="card">
      <div class="card-head">
        <span>最近拍摄</span>
        <span id="galleryCount" style="color:var(--muted)">0 / 8</span>
      </div>
      <div class="card-body">
        <div class="gallery-grid" id="gallery">
          <div class="gallery-empty">暂无拍摄记录</div>
        </div>
      </div>
    </div>
  </div>

  <!-- Right: Sidebar controls -->
  <div class="sidebar">

    <div class="card">
      <div class="card-head">拍照控制</div>
      <div class="card-body">
        <div class="counter-box">
          <div class="counter-num" id="counter">0</div>
          <div class="counter-lbl">已拍摄张数</div>
        </div>
        <button class="btn btn-blue" onclick="capture('1280x720')">
          <span class="btn-icon">📷</span>
          <span class="btn-label">
            拍照 1280×720
            <small>原始全分辨率灰度图</small>
          </span>
        </button>
        <button class="btn btn-green" onclick="capture('640x360')">
          <span class="btn-icon">🏹</span>
          <span class="btn-label">
            拍照 640×360
            <small>YOLOv8 训练集 · 16:9 中心裁剪</small>
          </span>
        </button>
        <div class="divider"></div>
        <button class="btn btn-ghost" id="refreshBtn" onclick="refreshCam()">
          <span class="btn-icon spin-icon" id="refreshIcon">🔄</span>
          <span class="btn-label">
            刷新摄像头
            <small>断联后点此恢复连接</small>
          </span>
        </button>
      </div>
    </div>

    <div class="card">
      <div class="card-head">设备信息</div>
      <div class="card-body">
        <table class="info-tbl">
          <tr><td>传感器</td><td>SC132GS (USB-C)</td></tr>
          <tr><td>原始分辨率</td><td>1280 × 720 灰度</td></tr>
          <tr><td>训练裁剪</td><td>640 × 360（16:9 中心）</td></tr>
          <tr><td>输出格式</td><td>PNG 8-bit 灰度</td></tr>
          <tr><td>输出目录</td><td>{{ output_dir }}</td></tr>
        </table>
        <div class="divider"></div>
        <div class="shortcut-row">
          <kbd class="kbd">1</kbd><span>→ 1280×720</span>
          &nbsp;
          <kbd class="kbd">2</kbd><span>→ 640×360</span>
          &nbsp;
          <kbd class="kbd">R</kbd><span>→ 刷新</span>
        </div>
      </div>
    </div>

  </div>
</div>

<div class="toast" id="toast"></div>

<script>
/* ── state ────────────────────────────────────── */
let captureCount = 0;
let toastTimer   = null;

/* ── stream events ───────────────────────────── */
function onStreamOk() { setConnected(true); }
function onStreamErr() { setConnected(false); }

function setConnected(ok) {
  document.getElementById('dot').className = 'dot' + (ok ? '' : ' off');
  document.getElementById('statusTxt').textContent = ok ? '实时预览中' : '摄像头未连接';
}

/* ── capture ─────────────────────────────────── */
function capture(fmt) {
  fetch('/capture', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({format: fmt})
  })
  .then(r => r.json())
  .then(d => {
    if (d.success) {
      captureCount++;
      document.getElementById('counter').textContent = captureCount;
      toast('✓ 已保存: ' + d.filename, 'ok');
      fetchGallery();
    } else {
      toast('✗ 拍照失败: ' + d.error, 'err');
    }
  })
  .catch(e => toast('✗ 请求失败: ' + e, 'err'));
}

/* ── refresh camera ──────────────────────────── */
function refreshCam() {
  const btn  = document.getElementById('refreshBtn');
  const icon = document.getElementById('refreshIcon');
  btn.disabled = true;
  btn.classList.add('spinning');
  fetch('/refresh_camera', {method: 'POST'})
  .then(r => r.json())
  .then(d => {
    if (d.success) {
      toast('✓ 摄像头已刷新', 'ok');
      const img = document.getElementById('stream');
      img.src = '/video_feed?' + Date.now();
      setConnected(true);
    } else {
      toast('✗ ' + d.error, 'err');
    }
  })
  .catch(e => toast('✗ 刷新失败: ' + e, 'err'))
  .finally(() => {
    btn.disabled = false;
    btn.classList.remove('spinning');
  });
}

/* ── gallery ─────────────────────────────────── */
function fetchGallery() {
  fetch('/recent_captures')
  .then(r => r.json())
  .then(items => {
    const g = document.getElementById('gallery');
    document.getElementById('galleryCount').textContent =
      items.length + ' / 8';
    if (!items.length) {
      g.innerHTML = '<div class="gallery-empty">暂无拍摄记录</div>';
      return;
    }
    g.innerHTML = items.map(it => `
      <div class="gallery-item" title="${it.filename}">
        <img src="data:image/jpeg;base64,${it.thumb}" alt="${it.filename}">
        <div class="gallery-lbl">${it.time} · ${it.size}</div>
      </div>`).join('');
  }).catch(() => {});
}

/* ── status poll ─────────────────────────────── */
setInterval(() => {
  fetch('/status').then(r => r.json()).then(d => {
    setConnected(d.connected);
    document.getElementById('fpsChip').textContent =
      d.fps != null ? d.fps.toFixed(1) + ' fps' : '— fps';
  }).catch(() => {});
}, 2000);

/* ── keyboard ────────────────────────────────── */
document.addEventListener('keydown', e => {
  const tag = document.activeElement.tagName.toLowerCase();
  if (tag === 'input' || tag === 'textarea') return;
  if (e.key === '1') capture('1280x720');
  if (e.key === '2') capture('640x360');
  if (e.key === 'r' || e.key === 'R') refreshCam();
});

/* ── toast ───────────────────────────────────── */
function toast(msg, type = 'ok') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast show toast-' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 3200);
}
</script>
</body>
</html>
"""


# ─── Flask 路由 ───────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template_string(HTML, output_dir=output_dir)


@app.route("/video_feed")
def video_feed():
    return Response(generate_frames(),
                    mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/capture", methods=["POST"])
def do_capture():
    data = request.get_json(silent=True) or {}
    fmt = data.get("format", "1280x720")
    if fmt not in CAPTURE_FORMATS:
        return jsonify({"success": False, "error": f"不支持的格式: {fmt}"})
    with camera_lock:
        cap = camera
    frame = _read_gray(cap) if cap else None
    if frame is None:
        return jsonify({"success": False, "error": "无法获取摄像头画面"})
    info = _save_capture(frame, fmt)
    return jsonify({"success": True, **info})


@app.route("/refresh_camera", methods=["POST"])
def refresh_camera():
    global camera, _fail_streak
    with camera_lock:
        if camera:
            camera.release()
        camera = _try_open(device_id_global)
        ok = camera is not None and camera.isOpened()
    _fail_streak = 0
    if ok:
        print("[INFO] 摄像头手动刷新成功")
        return jsonify({"success": True, "message": "摄像头已重新连接"})
    print("[WARN] 摄像头刷新失败")
    return jsonify({"success": False, "error": "无法连接到摄像头设备"})


@app.route("/recent_captures")
def recent_captures_api():
    return jsonify(list(recent_captures))


@app.route("/status")
def status():
    with camera_lock:
        connected = camera is not None and camera.isOpened()
    return jsonify({
        "connected": connected,
        "capture_count": capture_count,
        "fps": round(current_fps, 1),
        "output_dir": output_dir,
        "camera_resolution": f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
    })


# ─── 入口 ─────────────────────────────────────────────────────────────────────

def main():
    global camera, output_dir, device_id_global

    parser = argparse.ArgumentParser(description="Aurora Companion — A1 摄像头可视化采集伴侣")
    parser.add_argument("--device", type=int, default=0,    help="摄像头设备 ID (默认: 0)")
    parser.add_argument("--output", type=str,
                        default="../../data/yolov8_dataset/raw/images",
                        help="拍照保存目录")
    parser.add_argument("--port",   type=int, default=5001, help="Web 服务端口 (默认: 5001)")
    parser.add_argument("--host",   type=str, default="0.0.0.0", help="监听地址")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    output_dir = str((script_dir / args.output).resolve())
    os.makedirs(output_dir, exist_ok=True)
    print(f"[INFO] 输出目录: {output_dir}")

    device_id_global = args.device
    print(f"[INFO] 正在连接 A1 摄像头 (设备 {args.device})...")
    camera = _try_open(args.device)
    if camera is None:
        print("[WARN] 摄像头未连接，工具仍可启动，请连接后点击「刷新摄像头」")

    print(f"[INFO] Aurora Companion 已启动: http://localhost:{args.port}")
    print(f"[INFO] 快捷键: 1=1280×720  2=640×360  R=刷新摄像头")
    app.run(host=args.host, port=args.port, debug=False, threaded=True)


if __name__ == "__main__":
    main()
