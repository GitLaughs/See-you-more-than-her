# ISP Debug

## 使用说明
1. 更新`m1-bsp`的`isp_debug`分支
2. `ssne_ai_demo`放入`m1-bsp/smart_software/src`中，编译成镜像
3. `m1_test.py`中，`imgld3_en`改为`1`，`sv_width`和`sv_height`分别改为Debug输出的宽高，`sv_type`根据图像类型改为`raw8`/`yuv8`/`yuv10`等
4. RGB图像使用`raw8`类型，`sv_width`改为原始宽度的3倍，可以在Demo软件中显示
5. 运行`m1_test.py`烧录镜像，自动打开Demo软件后，手动停止脚本，再到m1上运行Demo即可出图
