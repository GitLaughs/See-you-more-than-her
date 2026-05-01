# OSD 交互图片准备建议

本文记录从 RPS demo 学到的 OSD 叠图交互方案，并给当前机器人展示状态准备图片资源。

## 总体原则

- 识别输入语义统一按 `640x480` 设计，但 OSD 贴图位置使用板端显示层绝对坐标。
- 资源先做 SVG/BMP，BMP 放在 [docs/osd_assets/](osd_assets/) 供人工转换为 SmartSens `.ssbmp`。
- 背景/长期元素放 layer 2，状态和动画放 layer 3/4。
- 状态切换时清理 layer 3/4，避免旧动画残留。
- 文字尽量大于 32px，线条尽量粗，避免上板后糊。

## 图片清单

### 人物识别：发送“你好”

| 文件名 | 建议尺寸 | 用途 | 层 |
| --- | ---: | --- | --- |
| `hello_bubble.ssbmp` | `360x120` | 识别到人物后显示“你好”气泡 | 4 |
| `hello_icon.ssbmp` | `96x96` | 可选挥手/头像图标 | 4 |

建议：右上角或检测框附近短暂显示 1-2 秒。

### 前进手势：小车前进动画

| 文件名 | 建议尺寸 | 用途 | 层 |
| --- | ---: | --- | --- |
| `car_forward_0.ssbmp` | `320x180` | 小车前进动画帧 | 3 |
| `car_forward_1.ssbmp` | `320x180` | 小车前进动画帧 | 3 |
| `car_forward_2.ssbmp` | `320x180` | 小车前进动画帧 | 3 |
| `car_forward_3.ssbmp` | `320x180` | 小车前进动画帧 | 3 |

建议：小车 + 向前箭头，箭头每帧前移/闪烁。

### 停止手势：小车停止动画

| 文件名 | 建议尺寸 | 用途 | 层 |
| --- | ---: | --- | --- |
| `car_stop_0.ssbmp` | `320x180` | 小车停止状态 | 3 |
| `car_stop_1.ssbmp` | `320x180` | STOP 闪烁帧 | 3 |
| `car_stop_2.ssbmp` | `320x180` | 禁止符号闪烁帧 | 3 |

建议：红色 STOP、禁止符号、刹车线。

### 障碍物：避障提示 + 绕行动画

| 文件名 | 建议尺寸 | 用途 | 层 |
| --- | ---: | --- | --- |
| `obstacle_alert.ssbmp` | `480x160` | “检测到障碍物，正在避障”提示 | 4 |
| `car_detour_0.ssbmp` | `480x270` | 绕行动画帧 | 3 |
| `car_detour_1.ssbmp` | `480x270` | 绕行动画帧 | 3 |
| `car_detour_2.ssbmp` | `480x270` | 绕行动画帧 | 3 |
| `car_detour_3.ssbmp` | `480x270` | 绕行动画帧 | 3 |
| `car_detour_4.ssbmp` | `480x270` | 绕行动画帧 | 3 |
| `car_detour_5.ssbmp` | `480x270` | 绕行动画帧 | 3 |

建议：顶部显示避障提示，底部/右下角显示小车沿弯曲路径绕开障碍。

## 状态机建议

```text
NoTarget -> 清 layer 3/4，显示 idle
person -> hello_bubble，短暂显示
forward gesture -> car_forward_* 循环播放
stop gesture -> car_stop_* 闪烁/常驻
obstacle_box -> obstacle_alert + car_detour_* 循环播放
```

## 坐标建议

若显示画布为 `1920x1080`：

| 状态 | 建议位置 |
| --- | --- |
| hello_bubble | `(1400, 120)` |
| car_forward_* | `(800, 760)` |
| car_stop_* | `(800, 760)` |
| obstacle_alert | `(720, 80)` |
| car_detour_* | `(720, 700)` |

若显示画布为 `640x480`：

| 状态 | 建议位置 |
| --- | --- |
| hello_bubble | `(260, 40)` |
| car_forward_* | `(160, 280)` |
| car_stop_* | `(160, 280)` |
| obstacle_alert | `(80, 40)` |
| car_detour_* | `(80, 190)` |

## 已生成草图资源

SVG 草图和同名 BMP 均放在 [docs/osd_assets/](osd_assets/)。SVG 是可编辑源图，BMP 是当前可交给 SmartSens OSD 工具链继续手动转换的中间格式。

- `hello_bubble.svg` / `hello_bubble.bmp`
- `hello_icon.svg` / `hello_icon.bmp`
- `car_forward_0.svg` 到 `car_forward_3.svg`，以及同名 `.bmp`
- `car_stop_0.svg` 到 `car_stop_2.svg`，以及同名 `.bmp`
- `obstacle_alert.svg` / `obstacle_alert.bmp`
- `car_detour_0.svg` 到 `car_detour_5.svg`，以及同名 `.bmp`
