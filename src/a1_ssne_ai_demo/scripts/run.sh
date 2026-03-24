
#!/bin/bash
# 加载 GPIO 和 UART 内核驱动模块
insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko 2>/dev/null || true
insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko 2>/dev/null || true

chmod +x ./ssne_face_drive_demo
./ssne_face_drive_demo