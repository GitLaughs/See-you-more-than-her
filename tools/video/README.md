# Video ROI YOLO Dataset Tool

本工具从视频中抽帧，统一输出 640x480 图片，并根据前端输入或鼠标拖框得到的固定 ROI 坐标自动生成 YOLO 标签。

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

## 标注流程

1. 输入视频路径，默认使用仓库根目录的 `video.mp4`。
2. 点击“加载第一帧”，页面会显示视频第一帧预览。
3. 点击“启用鼠标拖框”，在预览图上拖动生成 ROI。
4. 用 `x1 y1 x2 y2` 输入框做精细调整。
5. 输入类别名、类别 ID、抽帧间隔和输出前缀。
6. 点击“生成训练图片和 YOLO 标签”。

## 坐标规则

预览固定显示为 640x480。前端回填的 `x1 y1 x2 y2` 是原始视频帧坐标。脚本保存图片时会 resize 到 640x480，并自动把 ROI 等比例换算成 YOLO 归一化标签。

## 标签格式

每张图生成一个同名 `.txt`：

```text
class_id x_center y_center width height
```
