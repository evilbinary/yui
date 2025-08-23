分层设计

JSON解析层： parse_layer()  递归构建图层树

资源管理层： load_textures()  异步加载纹理资源

渲染管线层： render_layer()  实现深度优先绘制

事件处理层： handle_event()  支持事件冒泡机制

关键优化点

纹理缓存：重复资源仅加载一次

脏矩形渲染：仅重绘变更区域（示例未展示）

空间分割：优化图层点击检测效率


quickjs 支持


使用c实现一套叫yui的json解析，直接使用json库，根据上面的json ui 编写一个ui backend 渲染指出
如何设计JSON资源文件的结构，才能更好地支持动态UI更新？
1、​​树状层级结构​​


2、模块化拆分


二、组件化声明：属性与样式分离

三、事件驱动机制：动态交互核心



将复杂UI拆分为独立模块，通过"source"字段引用外部JSON文件：
{
  "id": "userForm",
  "type": "Form",
  "source": "components/user_form.json" // 外部模块化设计
}



```json
{
  "type": "Button",
  "position": [300, 250],
  "size": [200, 100],
  "events": {
    "onClick": "handleButtonClick"
  }
}


{
  "id": "main_panel",
  "type": "Panel",
  "position": [0, 0],
  "size": [800, 600],
  "style": {
    "color": "#2C3E50"
  },
  "children": [
    {
      "id": "header",
      "type": "Header",
      "position": [50, 20],
      "size": [700, 60],
      "style": {
        "color": "#3498DB"
      }
    },
    {
      "id": "submit_btn",
      "type": "Button",
      "position": [350, 500],
      "size": [100, 50],
      "style": {
        "color": "#27AE60"
      },
      "events": {
        "onClick": "@handleSubmit"
      }
    },
    {
      "id": "logo",
      "type": "Image",
      "position": [300, 100],
      "size": [200, 200],
      "source": "assets/logo.bmp"
    }
  ]
}


{
  "id": "userFormPanel",
  "type": "Panel",
  "theme": "default",
  "eventHandlers": {
    "@submitForm": "userService.saveData",
    "@validateEmail": "utils.checkEmailFormat"
  },
  "children": [
    {
      "id": "emailInput",
      "type": "Input",
      "label": "邮箱",
      "placeholder": "输入邮箱",
      "events": {
        "onChange": "@validateEmail"
      },
      "dataBinding": {
        "path": "user.email"
      }
    },
    {
      "id": "submitBtn",
      "type": "Button",
      "text": "提交",
      "events": {
        "onClick": "@submitForm"
      }
    }
  ],
  "metadata": {
    "version": "1.0.2",
    "dependencies": ["userService", "utils"]
  }
}




       
```



xattr -dr com.apple.quarantine ../libs/avif.framework
xattr -dr com.apple.quarantine ../libs/webp.framework
xattr -dr com.apple.quarantine ../libs/SDL2.framework
xattr -dr com.apple.quarantine ../libs/SDL2_ttf.framework/
xattr -dr com.apple.quarantine ../libs/SDL2_image.framework/


export DYLD_FRAMEWORK_PATH=../libs
export DYLD_LIBRARY_PATH="../libs"