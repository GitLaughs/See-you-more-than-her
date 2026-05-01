# demo-rps 亮点记忆

用途：之后需要复用 demo-rps 思路时，先读这份，再看代码。

## 位置

- 数据处理与训练：[demo-rps/dataprocess_modeltrain/](../demo-rps/dataprocess_modeltrain/)
- 板端游戏 demo：[demo-rps/ssne_ai_demo/](../demo-rps/ssne_ai_demo/)
- 主循环：[demo_rps_game.cpp](../demo-rps/ssne_ai_demo/demo_rps_game.cpp)
- 分类器：[rps_classifier.cpp](../demo-rps/ssne_ai_demo/src/rps_classifier.cpp)
- OSD：[utils.cpp](../demo-rps/ssne_ai_demo/src/utils.cpp)

## 核心价值

RPS demo 不是单点分类模型。它展示完整闭环：

1. 用视频快速采集类别数据。
2. 固定 ROI 抽帧生成训练图片。
3. 从同源视频避开 ROI 生成负样本。
4. 训练轻量分类模型。
5. 导出 ONNX / m1model。
6. 板端用同一 ROI 做 NPU 推理。
7. 推理结果经过多帧平均稳定。
8. OSD 直接输出有交互意义的画面。

## 数据集制作亮点

### 视频优先

目录按类别放视频：

```text
datasets/
├── R/
├── P/
└── S/
```

[prepare_video_dataset.py](../demo-rps/dataprocess_modeltrain/prepare_video_dataset.py) 每隔 N 帧保存一张，并按固定 ROI 裁剪。相比手动截图，视频采集速度快，天然覆盖轻微姿态、光照、距离变化。

### 固定 ROI 贯穿训练和部署

训练裁剪默认 `210 270 750 810`，板端推理也在 [rps_classifier.cpp](../demo-rps/ssne_ai_demo/src/rps_classifier.cpp) 使用同一 ROI。这个设计降低模型任务难度，也减少板端算力消耗。

### 负样本来自同源环境

[generate_negative_dataset.py](../demo-rps/dataprocess_modeltrain/generate_negative_dataset.py) 从同一批视频里随机裁剪非目标区域，生成 `N` 类。这样负样本光照、背景、相机噪声都贴近真实运行环境。

## 训练亮点

模型使用 MobileNetV1 + 小分类头，输入 `320x320`。输出不是 softmax 四分类，而是三路 sigmoid：

```text
[P_score, R_score, S_score]
```

负样本目标是 `[0, 0, 0]`。推理时三路分数都低，就得到 `NoTarget`。这个设计让“无目标”变成自然状态，而不是硬塞一个互斥类别。

验证指标也围绕部署目标：

- `positive_top1`：正样本分类准不准。
- `negative_recall`：空场景是否不误报。
- 最佳模型按两者平均分保存。

## 板端推理亮点

板端链路：

```text
sensor YUV422_16 → fixed crop → resize 320x320 → RGB → NPU → scores → label/NoTarget
```

重点不是每帧立刻执行动作，而是先取比赛阶段前 5 帧平均，再锁定最终标签。这个方法适合机器人视觉控制，能减少单帧抖动。

## OSD/前端意义亮点

OSD 不只画 debug 框。RPS demo 使用 `.ssbmp` 资源做完整交互画面：

- `background.ssbmp`
- `ready.ssbmp`
- `1/2/3.ssbmp`
- `r/p/s.ssbmp`
- `win.ssbmp`

这说明板端可以直接输出“用户能理解的状态画面”。对本项目，OSD/Aurora 应显示语义状态，而不是只显示原始识别框。

## 可迁移到本项目的做法

训练/转换/推理模板已抽到 [tools/yolo/](../tools/yolo/)；官方数据集抽帧工具继续负责视频截帧，这里只保留 MobileNetV1 sigmoid 分类训练和 A1 部署参考。

### 数据采集

- 在 Aurora 增加按类别采集视频/图片能力。
- 每个目标/动作/场景一个目录。
- 用脚本自动抽帧、裁剪、生成负样本。
- 保留 ROI 参数，确保训练和板端推理一致。

### 模型输出

不要只输出类别 ID。优先输出能被 UI 和控制直接使用的状态：

```text
label
confidence
NoTarget
target_locked
action_hint
safe_to_move
```

### 控制稳定

视觉结果进入底盘控制前先稳定：

- 多帧平均置信度。
- 连续 N 帧同类才确认。
- 低置信度进入 `NoTarget`。
- 状态锁定短时间，避免来回抖。

### OSD/Aurora

把 OSD 当产品画面：

- 等待采集
- 正在识别
- 已锁定目标
- 无目标
- 准备移动
- 停止/错误

## 最值得复制的一句话

采集即训练数据，推理即 UI 状态，OSD 即产品画面。
