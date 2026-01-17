# 事件系统

设计背景与架构

1. 解耦架构
组件层（TextComponent）只负责文本编辑逻辑，不直接执行 JavaScript
事件系统层负责解析 @ 前缀、查找并执行对应的 JavaScript 函数
通过 user_data 作为桥梁，两层保持独立，符合单一职责原则
2. 延迟绑定策略
解析时：JSON 加载阶段只存储事件名称字符串，不立即绑定函数
触发时：当文本变化时，调用 text_component_trigger_on_change()，事件系统再查找并执行对应函数
优势：支持热更新、动态事件注册，避免循环依赖

# 事件处理流程

JSON配置: "onChange": "@onTextChange"
    ↓ (解析)
cJSON 对象 → event_name = "@onTextChange"
    ↓ (存储)
layer->event->change_name = "@onTextChange" (堆副本)
    ↓ (文本变化时)
text_component_trigger_on_change() 被调用
    ↓ (事件系统处理)
查找 "@onTextChange" 对应的 JS 函数并执行