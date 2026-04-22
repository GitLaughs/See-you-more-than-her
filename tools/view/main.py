import time
import threading
import json
import base64
from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO, emit
import cv2
import numpy as np

app = Flask(__name__)
app.config['SECRET_KEY'] = 'a1_secret!'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# 虚拟小车状态
car_state = {
    "forward": False,
    "back": False,
    "left": False,
    "right": False
}
car_state_lock = threading.Lock()

# 视频流状态
camera = None
camera_lock = threading.Lock()
stream_active = False

def update_car_state(key, value):
    with car_state_lock:
        car_state[key] = value

def get_car_state():
    with car_state_lock:
        return car_state.copy()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/car_state')
def api_car_state():
    return jsonify(get_car_state())

@socketio.on('connect')
def handle_connect():
    print('[WS] Client connected')

@socketio.on('disconnect')
def handle_disconnect():
    print('[WS] Client disconnected')
    # 重置小车状态
    with car_state_lock:
        for k in car_state:
            car_state[k] = False

@socketio.on('message')
def handle_message(msg):
    # 处理 { "type": "cmd"/"preset", "data": {} }
    try:
        if isinstance(msg, str):
            data = json.loads(msg)
        else:
            data = msg
            
        msg_type = data.get('type')
        msg_data = data.get('data', {})
        
        if msg_type == 'cmd':
            # WASD 控制
            with car_state_lock:
                for k in ['forward', 'back', 'left', 'right']:
                    if k in msg_data:
                        car_state[k] = bool(msg_data[k])
            
        elif msg_type == 'preset':
            # 预设动作 (模拟执行)
            action = msg_data.get('action')
            print(f'[WS] Executing preset: {action}')
            # 简单模拟延时
            time.sleep(0.5)
            
        # 返回 echo 校验
        emit('echo', {'status': 'ok', 'type': msg_type, 'timestamp': time.time()})
        
    except Exception as e:
        print(f'[WS] Error parsing message: {e}')
        emit('echo', {'status': 'error', 'msg': str(e)})

def video_stream_thread():
    global camera, stream_active
    print('[VIDEO] Stream thread started')
    
    # 模拟视频流 (这里可以接入真实的 OpenCV 摄像头或 Yolo 推理)
    # 为了演示，我们生成一个带 OSD 的测试画面
    cap = cv2.VideoCapture(0)  # 尝试打开默认摄像头
    if not cap.isOpened():
        print('[VIDEO] Warning: Could not open camera 0. Using synthetic frames.')
    
    fps = 25
    delay = 1.0 / fps
    
    while stream_active:
        start_time = time.time()
        
        frame = None
        if cap and cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                frame = None
                
        if frame is None:
            # Synthetic frame
            frame = np.zeros((480, 640, 3), dtype=np.uint8)
            cv2.putText(frame, "A1 Camera Preview (Simulated)", (50, 240), 
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            
        # 添加 OSD (模拟 YOLO 检测框)
        cv2.rectangle(frame, (100, 100), (300, 300), (0, 0, 255), 2)
        cv2.putText(frame, "PERSON 0.95", (100, 90), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
                    
        # 编码并发送
        _, buffer = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        frame_data = base64.b64encode(buffer).decode('utf-8')
        
        socketio.emit('video_frame', {'image': frame_data})
        
        elapsed = time.time() - start_time
        sleep_time = max(0, delay - elapsed)
        time.sleep(sleep_time)
        
    if cap and cap.isOpened():
        cap.release()
    print('[VIDEO] Stream thread stopped')

@app.route('/api/stream/start')
def start_stream():
    global stream_active
    if not stream_active:
        stream_active = True
        threading.Thread(target=video_stream_thread, daemon=True).start()
    return jsonify({"status": "started"})

@app.route('/api/stream/stop')
def stop_stream():
    global stream_active
    stream_active = False
    return jsonify({"status": "stopped"})

if __name__ == '__main__':
    print("[INFO] Starting A1 Camera Preview Tool on port 8000...")
    stream_active = True
    threading.Thread(target=video_stream_thread, daemon=True).start()
    socketio.run(app, host='0.0.0.0', port=8000, debug=False, allow_unsafe_werkzeug=True)
