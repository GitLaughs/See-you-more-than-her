# YOLOv8 数据集模板

这个目录只保留一份轻量、可版本化的训练数据模板，方便队友统一数据结构和训练入口。

## 目录结构

- raw/images：切分前的原始图片
- raw/labels：与 raw/images 对应的 YOLO txt 标注
- images/train|val|test：切分后的图片集
- labels/train|val|test：切分后的标注集

## 使用说明

- 尽量不要把大体积原始数据直接提交到 git
- 用 [tools/yolov8/split_dataset.py](../../tools/yolov8/split_dataset.py) 做确定性切分
- 训练前先检查 [data/yolov8_dataset/dataset.yaml](dataset.yaml) 里的类别名是否已经更新
