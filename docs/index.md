# YUI Documentation Index

欢迎来到 YUI 文档中心！这里包含了所有关于 YUI 框架的学习资料和参考文档。

## 📚 文档分类

### 🚀 快速入门
- [README](../README.md) - 项目概览和快速开始指南
- [开发者指南](developer-guide.md) - 环境搭建、开发流程和调试技巧

### 🏗️ 架构设计
- [架构设计文档](architecture.md) - 系统架构、模块关系和设计原理
- [功能特性说明](feature.md) - 框架核心功能详细介绍

### 🎨 UI 开发
- [JSON 格式规范](json-format-spec.md) - UI 配置文件完整语法说明
- [组件使用指南](#组件文档) - 各类 UI 组件的使用方法
- [布局系统](layout.md) - 灵活的布局管理机制
- [主题系统](theme.md) - 动态主题切换和样式管理

### 💻 编程接口
- [JavaScript API 参考](yui-js-api.md) - 完整的 JS 接口文档
- [C API 参考](#c-api文档) - 核心 C 函数接口说明

### 🔧 高级主题
- [性能优化指南](blur-performance-optimization.md) - 渲染性能调优技巧
- [网络编程](net.md) - Socket 和 HTTP 客户端使用
- [动画系统](#动画文档) - 丰富的动画效果实现
- [视觉效果](glass-effect-guide.md) - 玻璃态等高级视觉效果

### 📋 组件文档

| 组件 | 文档 | 状态 | 说明 |
|------|------|------|------|
| Dialog | [dialog-component.md](dialog-component.md) | ✅ 完整 | 对话框组件 |
| Select | [select-component.md](select-component.md) | ✅ 完整 | 下拉选择组件 |
| Button | [待补充]() | ⏳ 计划中 | 按钮组件 |
| Input | [待补充]() | ⏳ 计划中 | 输入框组件 |
| List | [待补充]() | ⏳ 计划中 | 列表组件 |
| Grid | [待补充]() | ⏳ 计划中 | 网格组件 |

### 🎬 动画文档

| 主题 | 文档 | 状态 | 说明 |
|------|------|------|------|
| 基础动画 | [animate.h](../src/animate.h) | ✅ 完整 | 动画系统核心 |
| 缓动函数 | [待补充]() | ⏳ 计划中 | 各种缓动效果 |
| 关键帧动画 | [待补充]() | ⏳ 计划中 | 复杂动画序列 |

### 📞 C API 文档

| 模块 | 头文件 | 状态 | 说明 |
|------|--------|------|------|
| 图层系统 | [layer.h](../src/layer.h) | ✅ 完整 | 核心图层管理 |
| 渲染系统 | [render.h](../src/render.h) | ✅ 完整 | 图形渲染接口 |
| 事件系统 | [event.h](../src/event.h) | ✅ 完整 | 事件处理机制 |
| 布局系统 | [layout.h](../src/layout.h) | ✅ 完整 | 自动布局计算 |
| 动画系统 | [animate.h](../src/animate.h) | ✅ 完整 | 动画效果实现 |
| 主题系统 | [theme.h](../src/theme.h) | ✅ 完整 | 主题管理接口 |

## 🎯 学习路径推荐

### 🌱 新手入门路线

1. **了解基础概念**
   - 阅读 [README](../README.md)
   - 浏览 [功能特性说明](feature.md)

2. **环境搭建**
   - 按照 [开发者指南](developer-guide.md) 配置开发环境
   - 运行第一个示例程序

3. **UI 开发入门**
   - 学习 [JSON 格式规范](json-format-spec.md)
   - 尝试创建简单界面

4. **实践练习**
   - 参考 `app/tests/` 目录下的示例
   - 完成几个基础组件的使用

### 🚀 进阶开发路线

1. **深入理解架构**
   - 详细阅读 [架构设计文档](architecture.md)
   - 理解各模块间的交互关系

2. **掌握高级特性**
   - 学习 [主题系统](theme.md)
   - 掌握 [布局系统](layout.md)
   - 使用 [动画系统](animate.h)

3. **性能优化**
   - 阅读 [性能优化指南](blur-performance-optimization.md)
   - 学习调试技巧

4. **扩展开发**
   - 创建自定义组件
   - 贡献代码到项目

### 👨‍💻 开发者路线

1. **源码阅读**
   - 从 `main.c` 开始理解程序入口
   - 阅读核心模块实现

2. **贡献准备**
   - 阅读 [贡献指南](../CONTRIBUTING.md)
   - 了解代码规范和提交流程

3. **实际贡献**
   - 修复 bug
   - 添加新功能
   - 完善文档

## 🔍 快速查找

### 按关键词查找

**基础概念**
- [什么是 YUI？](../README.md#概述)
- [核心特性](../README.md#核心特性)
- [系统架构](architecture.md#系统概述)

**开发环境**
- [Windows 环境配置](developer-guide.md#windows-环境配置)
- [macOS 环境配置](developer-guide.md#macos-环境配置)
- [Linux 环境配置](developer-guide.md#linux-环境配置)

**UI 开发**
- [JSON 配置语法](json-format-spec.md)
- [组件类型](json-format-spec.md#核心组件类型)
- [样式设置](json-format-spec.md#通用属性)

**编程接口**
- [JavaScript API](yui-js-api.md)
- [核心函数](yui-js-api.md#yui-core-api-所有引擎通用)
- [Socket API](yui-js-api.md#socket-api-mquickjs--quickjs)

**调试排错**
- [日志系统](developer-guide.md#日志系统)
- [调试工具](developer-guide.md#调试工具)
- [常见问题](developer-guide.md#常见问题解答)

### 按任务查找

**我要创建一个简单的界面**
1. [JSON 格式规范](json-format-spec.md) - 学习配置语法
2. [基础组件](json-format-spec.md#核心组件类型) - 了解可用组件
3. 参考 `app/test-simple.json` - 查看示例

**我要添加动画效果**
1. [动画系统](animate.h) - 了解动画 API
2. [缓动函数](待补充) - 选择合适的动画曲线
3. 参考 `app/tests/test-animation.json` - 查看动画示例

**我要自定义主题**
1. [主题系统](theme.md) - 学习主题机制
2. [样式属性](theme.md#支持的样式属性) - 了解可定制项
3. 参考 `app/dark-theme.json` - 查看主题示例

**我要调试性能问题**
1. [性能优化指南](blur-performance-optimization.md)
2. [调试技巧](developer-guide.md#调试技巧)
3. [内存管理](architecture.md#内存管理)

## 📊 文档状态

### ✅ 已完成文档
- [x] README.md
- [x] developer-guide.md
- [x] architecture.md
- [x] CONTRIBUTING.md
- [x] feature.md
- [x] json-format-spec.md
- [x] yui-js-api.md
- [x] theme.md
- [x] dialog-component.md
- [x] select-component.md

### ⏳ 待完善文档
- [ ] 组件详细文档 (Button, Input, List 等)
- [ ] 动画系统详细指南
- [ ] 网络编程完整教程
- [ ] C API 详细参考
- [ ] 最佳实践集锦
- [ ] 迁移指南

### 🆕 计划中文档
- [ ] 视频教程系列
- [ ] 交互式示例演示
- [ ] API 自动生成文档
- [ ] 中文/英文双语支持

## 🤝 参与文档建设

我们欢迎任何人参与到文档的完善工作中来！

### 如何贡献文档

1. **发现文档问题**
   - 内容不准确或过时
   - 格式不统一
   - 缺少示例代码

2. **提出改进建议**
   - 在 GitHub Issues 中报告
   - 或直接提交 Pull Request

3. **贡献新文档**
   - 遵循现有文档格式
   - 提供实用的示例
   - 确保内容准确性

### 文档质量标准

- **准确性**: 技术内容必须正确无误
- **完整性**: 覆盖相关主题的所有重要方面
- **实用性**: 提供可操作的指导和示例
- **一致性**: 遵循统一的格式和风格
- **时效性**: 及时更新以反映最新变化

## 📞 获取帮助

### 社区支持

- **GitHub Issues**: 报告 bug 或请求功能
- **GitHub Discussions**: 技术讨论和问答
- **邮件列表**: [待建立]()

### 学习资源

- **示例代码**: `app/tests/` 目录
- **API 参考**: [JavaScript API](yui-js-api.md)
- **视频教程**: [计划中]()

---

<p align="center">
  <em>文档最后更新: February 2026</em><br>
  <a href="https://github.com/evilbinary/YUI">访问 GitHub 仓库</a> · 
  <a href="../CONTRIBUTING.md">贡献指南</a>
</p>