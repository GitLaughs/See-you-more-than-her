// main.js

let socket;
let reconnectTimer;
let lastPingTime;
let currentCmd = { forward: false, back: false, left: false, right: false };

document.addEventListener('DOMContentLoaded', () => {
    initLayout();
    initSocket();
    initControls();
    initCarStatePolling();
    
    // 三维点云暴露接口
    window.showPointCloud = function(buffer) {
        console.log('[PointCloud] Received Float32Array:', buffer);
    };
});

// ====================
// 布局与分割线逻辑
// ====================
function initLayout() {
    const resizer = document.getElementById('resizer');
    const leftPanel = document.getElementById('left-panel');
    const container = document.querySelector('.app-container');

    // 恢复之前的比例
    const savedRatio = localStorage.getItem('splitRatio');
    if (savedRatio) {
        leftPanel.style.flex = `0 0 ${savedRatio}%`;
    }

    let isResizing = false;

    resizer.addEventListener('mousedown', (e) => {
        isResizing = true;
        document.body.style.cursor = 'col-resize';
        resizer.classList.add('active');
    });

    document.addEventListener('mousemove', (e) => {
        if (!isResizing) return;
        let newWidth = (e.clientX / container.offsetWidth) * 100;
        // 限制最小和最大宽度
        if (newWidth < 20) newWidth = 20;
        if (newWidth > 80) newWidth = 80;
        leftPanel.style.flex = `0 0 ${newWidth}%`;
    });

    document.addEventListener('mouseup', () => {
        if (isResizing) {
            isResizing = false;
            document.body.style.cursor = 'default';
            resizer.classList.remove('active');
            
            // 保存比例
            const ratio = (leftPanel.offsetWidth / container.offsetWidth) * 100;
            localStorage.setItem('splitRatio', ratio);
        }
    });
}

// ====================
// WebSocket 逻辑
// ====================
function initSocket() {
    socket = io({
        reconnectionDelay: 3000,
        reconnectionDelayMax: 3000,
        timeout: 5000,
        autoConnect: false
    });

    const canvas = document.getElementById('video-canvas');
    const ctx = canvas.getContext('2d');
    const fpsCounter = document.getElementById('fps-counter');
    const latencyCounter = document.getElementById('latency-counter');
    const overlay = document.getElementById('reconnect-overlay');
    
    let frameCount = 0;
    let lastFpsTime = performance.now();

    socket.on('connect', () => {
        console.log('[WS] Connected');
        overlay.classList.add('hidden');
        document.getElementById('btn-connect').classList.add('active');
        document.getElementById('btn-disconnect').classList.remove('active');
    });

    socket.on('disconnect', () => {
        console.log('[WS] Disconnected');
        overlay.classList.remove('hidden');
        document.getElementById('btn-connect').classList.remove('active');
        document.getElementById('btn-disconnect').classList.add('active');
    });

    socket.on('video_frame', (data) => {
        // 计算 FPS
        frameCount++;
        const now = performance.now();
        if (now - lastFpsTime >= 1000) {
            fpsCounter.textContent = `FPS: ${Math.round(frameCount * 1000 / (now - lastFpsTime))}`;
            frameCount = 0;
            lastFpsTime = now;
        }

        // 渲染图像
        const img = new Image();
        img.onload = () => {
            canvas.width = img.width;
            canvas.height = img.height;
            ctx.drawImage(img, 0, 0);
        };
        img.src = 'data:image/jpeg;base64,' + data.image;
    });

    socket.on('echo', (data) => {
        if (data.timestamp) {
            const latency = Math.round((Date.now() / 1000 - data.timestamp) * 1000);
            latencyCounter.textContent = `延迟: ${latency} ms`;
            if (latency > 500) {
                latencyCounter.style.color = 'var(--danger)';
                // 红色闪烁提示
                document.querySelector('.control-panel').style.backgroundColor = 'rgba(255, 0, 0, 0.1)';
                setTimeout(() => {
                    document.querySelector('.control-panel').style.backgroundColor = 'transparent';
                }, 200);
            } else {
                latencyCounter.style.color = 'var(--text-color)';
            }
        }
    });

    // 绑定连接按钮
    document.getElementById('btn-connect').addEventListener('click', () => {
        socket.connect();
    });

    document.getElementById('btn-disconnect').addEventListener('click', () => {
        socket.disconnect();
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    });

    document.getElementById('btn-close-overlay').addEventListener('click', () => {
        overlay.classList.add('hidden');
    });
}

// ====================
// 控制面板逻辑
// ====================
function initControls() {
    // 预设动作
    document.querySelectorAll('.preset-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const action = btn.getAttribute('data-action');
            if (socket && socket.connected) {
                socket.emit('message', JSON.stringify({
                    type: 'preset',
                    data: { action: action }
                }));
            }
        });
    });

    // WASD 实时控制
    const keyMap = {
        'w': 'forward', 'W': 'forward',
        's': 'back', 'S': 'back',
        'a': 'left', 'A': 'left',
        'd': 'right', 'D': 'right'
    };

    function sendCmd() {
        if (socket && socket.connected) {
            socket.emit('message', JSON.stringify({
                type: 'cmd',
                data: currentCmd
            }));
        }
    }

    function handleKeyDown(e) {
        const cmd = keyMap[e.key];
        if (cmd && !currentCmd[cmd]) {
            currentCmd[cmd] = true;
            document.querySelector(`.key-btn[data-key="${e.key.toUpperCase()}"]`).classList.add('active');
            sendCmd();
        }
    }

    function handleKeyUp(e) {
        const cmd = keyMap[e.key];
        if (cmd && currentCmd[cmd]) {
            currentCmd[cmd] = false;
            document.querySelector(`.key-btn[data-key="${e.key.toUpperCase()}"]`).classList.remove('active');
            sendCmd();
        }
    }

    document.addEventListener('keydown', handleKeyDown);
    document.addEventListener('keyup', handleKeyUp);

    // 点云面板显示切换
    const pcBtn = document.getElementById('btn-toggle-pc');
    const pcContainer = document.getElementById('point-cloud-container');
    pcBtn.addEventListener('click', () => {
        if (pcContainer.style.display === 'none') {
            pcContainer.style.display = 'flex';
            pcBtn.textContent = '隐藏点云';
        } else {
            pcContainer.style.display = 'none';
            pcBtn.textContent = '显示点云';
        }
    });
}

// ====================
// 小车状态轮询 (50ms)
// ====================
function initCarStatePolling() {
    setInterval(() => {
        fetch('/api/car_state')
            .then(res => res.json())
            .then(data => {
                updateCarUI(data);
            })
            .catch(err => console.error('[Polling] Error:', err));
    }, 50);
}

function updateCarUI(state) {
    const setArrow = (id, active) => {
        const el = document.getElementById(id);
        if (el) {
            if (active) el.classList.add('active');
            else el.classList.remove('active');
        }
    };

    setArrow('arrow-forward', state.forward);
    setArrow('arrow-back', state.back);
    setArrow('arrow-left', state.left);
    setArrow('arrow-right', state.right);
}
