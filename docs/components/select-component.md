# Select 组件文档

## 概述

Select 组件是一个下拉选择器，结合了 Dropdown 和 ListBox 的优点，提供了丰富的交互功能和灵活的配置选项。

## 特性

- ✅ 占位符支持
- ✅ 禁用选项支持  
- ✅ 键盘导航 (↑↓←→, Enter, Space, Escape)
- ✅ 鼠标悬停效果
- ✅ 滚动条支持 (长列表)
- ✅ 自定义样式配置
- ✅ 回调函数支持
- ✅ JSON 配置解析
- ✅ 文本裁剪和省略
- ✅ 圆角边框支持

## JSON 配置示例

```json
{
  "type": "Select",
  "name": "mySelect",
  "position": [20, 90],
  "size": [250, 32],
  "style": {
    "bgColor": "rgba(255, 255, 255, 255)",
    "radius": 4
  },
  "selectConfig": {
    "maxVisibleItems": 8,
    "itemHeight": 28,
    "borderWidth": 1,
    "borderRadius": 4,
    "placeholder": "请选择一个选项",
    "items": [
      "选项 1",
      "选项 2",
      {
        "text": "禁用选项",
        "disabled": true
      }
    ],
    "colors": {
      "bgColor": "rgba(255, 255, 255, 255)",
      "textColor": "rgba(51, 51, 51, 255)",
      "borderColor": "rgba(204, 204, 204, 255)",
      "arrowColor": "rgba(102, 102, 102, 255)",
      "dropdownBgColor": "rgba(255, 255, 255, 255)",
      "hoverBgColor": "rgba(245, 245, 245, 255)",
      "selectedBgColor": "rgba(0, 120, 215, 255)",
      "selectedTextColor": "rgba(255, 255, 255, 255)",
      "disabledTextColor": "rgba(170, 170, 170, 255)"
    }
  }
}
```

## 配置选项

### selectConfig

| 属性 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| maxVisibleItems | number | 8 | 最大可见选项数量 |
| itemHeight | number | 28 | 每个选项的高度 |
| borderWidth | number | 1 | 边框宽度 |
| borderRadius | number | 4 | 边框圆角半径 |
| placeholder | string | - | 占位符文本 |
| items | array | - | 选项数组 |

### items 数组

每个选项可以是：
- 字符串：`"选项文本"`
- 对象：`{"text": "选项文本", "disabled": false}`

### colors

| 颜色属性 | 描述 |
|----------|------|
| bgColor | 组件背景色 |
| textColor | 文本颜色 |
| borderColor | 边框颜色 |
| arrowColor | 下拉箭头颜色 |
| dropdownBgColor | 下拉菜单背景色 |
| hoverBgColor | 悬停背景色 |
| selectedBgColor | 选中背景色 |
| selectedTextColor | 选中文本颜色 |
| disabledTextColor | 禁用文本颜色 |
| scrollbarColor | 滚动条颜色 |
| scrollbarBgColor | 滚动条背景色 |

## API 参考

### 创建和销毁

```c
SelectComponent* select_component_create(Layer* layer);
SelectComponent* select_component_create_from_json(Layer* layer, cJSON* json_obj);
void select_component_destroy(SelectComponent* component);
```

### 选项管理

```c
void select_component_add_item(SelectComponent* component, const char* text, void* user_data);
void select_component_add_placeholder(SelectComponent* component, const char* text);
void select_component_remove_item(SelectComponent* component, int index);
void select_component_clear_items(SelectComponent* component);
void select_component_insert_item(SelectComponent* component, int index, const char* text, void* user_data);
```

### 选择操作

```c
void select_component_set_selected(SelectComponent* component, int index);
int select_component_get_selected(SelectComponent* component);
const char* select_component_get_selected_text(SelectComponent* component);
void* select_component_get_selected_data(SelectComponent* component);
void select_component_clear_selection(SelectComponent* component);
```

### 选项状态

```c
void select_component_set_item_disabled(SelectComponent* component, int index, int disabled);
int select_component_is_item_disabled(SelectComponent* component, int index);
int select_component_get_item_count(SelectComponent* component);
const char* select_component_get_item_text(SelectComponent* component, int index);
```

### 样式设置

```c
void select_component_set_colors(SelectComponent* component, 
                                Color bg, Color text, Color border, Color arrow,
                                Color dropdown_bg, Color hover, Color selected_bg, 
                                Color selected_text, Color disabled_text);
void select_component_set_border_style(SelectComponent* component, int width, int radius);
void select_component_set_max_visible_items(SelectComponent* component, int max_visible);
```

### 回调设置

```c
void select_component_set_user_data(SelectComponent* component, void* data);
void select_component_set_selection_changed_callback(SelectComponent* component, void (*callback)(int, void*));
void select_component_set_dropdown_expanded_callback(SelectComponent* component, void (*callback)(int, void*));
```

### 控制操作

```c
void select_component_expand(SelectComponent* component);
void select_component_collapse(SelectComponent* component);
void select_component_toggle(SelectComponent* component);
```

## 键盘交互

| 按键 | 功能 |
|------|------|
| ↑ | 向上导航选项 |
| ↓ | 向下导航选项 |
| Enter | 选择高亮选项或切换展开状态 |
| Space | 选择高亮选项或切换展开状态 |
| Escape | 收起下拉菜单 |

## 使用示例

### 基础用法

```c
// 创建 Select 组件
SelectComponent* select = select_component_create(layer);

// 添加选项
select_component_add_item(select, "选项 1", NULL);
select_component_add_item(select, "选项 2", NULL);
select_component_add_item(select, "选项 3", NULL);

// 设置选择变化回调
select_component_set_selection_changed_callback(select, on_selection_changed, user_data);
```

### 带占位符

```c
SelectComponent* select = select_component_create(layer);
select_component_add_placeholder(select, "请选择...");
select_component_add_item(select, "苹果", NULL);
select_component_add_item(select, "香蕉", NULL);
```

### 禁用选项

```c
select_component_add_item(select, "正常选项", NULL);
select_component_add_item(select, "禁用选项", NULL);
select_component_set_item_disabled(select, 1, true);
```

## 文件结构

- `src/components/select_component.h` - 头文件
- `src/components/select_component.c` - 实现文件
- `app/test-select.json` - 测试配置文件

## 测试

运行测试：
```bash
ya -b yui
ya -r yui app/test-select.json
```

测试配置包含了多种用例：
- 基础选择器
- 带禁用选项的水果选择器
- 长列表城市选择器