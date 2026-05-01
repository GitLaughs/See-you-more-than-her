# Video ROI YOLO Dataset Tool

本工具从视频中抽帧，统一输出 640x480 图片，并根据前端输入的固定 ROI 坐标自动生成 YOLO 标签。

## 启动

```powershell
cd <repo-root>
.\tools\video\launch.ps1
```

打开：`http://127.0.0.1:6210`

## 默认输入输出

- 默认视频：`video.mp4`
- 输出图片：`tools/yolo/raw/images/`
- 输出标签：`tools/yolo/raw/labels/`
- 输出尺寸：`640x480`

## 坐标规则

前端输入的 `x1 y1 x2 y2` 是原始视频帧坐标。脚本保存图片时会 resize 到 640x480，并自动把 ROI 等比例换算成 YOLO 归一化标签。

## 标签格式

每张图生成一个同名 `.txt`：

```text
class_id x_center y_center width height
```
