扩展后的属性处理器 - 完整支持列表
当前支持的属性（共 23 个）

1. 基础属性
id - 图层标识符
2. 文本属性
text - 文本内容
label - 标签文本

3. 颜色属性
color - 前景色
bgColor - 背景色
opacity - 不透明度 (0-255)

4. 字体属性 ⭐ 新增
font - 字体路径
fontSize - 字体大小
fontWeight - 字体粗细 (normal, bold, light)

5. 样式属性
borderRadius - 圆角半径
source - 图片源路径 ⭐ 新增

6. 尺寸和位置属性
size - 尺寸数组 [width, height]
position - 位置数组 [x, y]
width - 宽度 ⭐ 新增
height - 高度 ⭐ 新增
padding - 内边距数组

7. 布局属性 ⭐ 新增
flex - 弹性比例
rotation - 旋转角度

8. 状态属性
visible - 可见性
enabled - 启用状态
focusable - 可聚焦
scrollable - 可滚动 (0/1/2/3)


使用示例
```json
{
  "id": "myButton",
  "text": "Click Me",
  "font": "Roboto-Bold.ttf",
  "fontSize": 18,
  "fontWeight": "bold",
  "color": "#ffffff",
  "bgColor": "#4caf50",
  "opacity": 255,
  "borderRadius": 8,
  "width": 200,
  "height": 50,
  "position": [100, 100],
  "flex": 1.5,
  "rotation": 15,
  "visible": true,
  "focusable": true,
  "scrollable": 1
}
```