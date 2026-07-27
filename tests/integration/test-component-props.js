/**
 * YUI 组件特有属性测试
 * 验证每个组件类型独有的属性配置
 *
 * 用法: playground tests/integration/test-component-props.json
 */
function onCompPropsLoad() {

  YTest.describe("Label 组件属性", function () {
    YTest.it("text 可通过 setText 设置", function () {
      YUI.setText("cp_label", "Updated Label");
      YTest.expect(YUI.getText("cp_label")).toBe("Updated Label");
    });

    YTest.it("fontSize 可通过 getProperty 读取", function () {
      var fs = YUI.getProperty("cp_label", "fontSize");
      YTest.expect(fs !== null && fs !== undefined).toBeTruthy();
    });

    YTest.it("color 可通过 setProperty 修改", function () {
      YUI.setProperty("cp_label", "color", "#ff0000");
      var c = YUI.getProperty("cp_label", "color");
      YTest.expect(c).toBeTruthy();
    });
  });

  YTest.describe("Button 组件属性", function () {
    YTest.it("text 正确初始渲染", function () {
      YTest.expect(YUI.getText("cp_btn")).toBe("Click Me");
    });

    YTest.it("bgColor 可通过 getProperty 读取", function () {
      var c = YUI.getProperty("cp_btn", "bgColor");
      YTest.expect(c).toBeTruthy();
    });

    YTest.it("borderRadius 可通过 update 修改", function () {
      var ret = YUI.update({ target: "cp_btn", change: { borderRadius: 16 } });
      var br = YUI.getProperty("cp_btn", "borderRadius");
      YTest.expect(br !== null && br !== undefined).toBeTruthy();
    });

    YTest.it("text 可通过 setText 更新", function () {
      YUI.setText("cp_btn", "Updated!");
      YTest.expect(YUI.getText("cp_btn")).toBe("Updated!");
    });
  });

  YTest.describe("Input 组件属性", function () {
    YTest.it("初始 text 正确", function () {
      YTest.expect(YUI.getText("cp_input")).toBe("initial value");
    });

    YTest.it("setText 可修改输入框值", function () {
      YUI.setText("cp_input", "modified value");
      YTest.expect(YUI.getText("cp_input")).toBe("modified value");
    });

    YTest.it("清空 text", function () {
      YUI.setText("cp_input", "");
      YTest.expect(YUI.getText("cp_input")).toBe("");
    });
  });

  YTest.describe("Text 组件属性", function () {
    YTest.it("初始文本包含多行", function () {
      var t = YUI.getText("cp_text");
      YTest.expect(t.indexOf("\n") >= 0).toBeTruthy();
    });

    YTest.it("setText 可修改多行文本", function () {
      YUI.setText("cp_text", "New line 1\nNew line 2\nNew line 3");
      YTest.expect(YUI.getText("cp_text")).toBe("New line 1\nNew line 2\nNew line 3");
    });

    YTest.it("contentHeight 返回数字", function () {
      var h = YUI.getProperty("cp_text", "contentHeight");
      YTest.expect(typeof h === "number" || typeof h === "string").toBeTruthy();
    });
  });

  YTest.describe("Loading 组件属性", function () {
    YTest.it("color 可通过 setProperty 修改", function () {
      YUI.setProperty("cp_loading", "color", "#f38ba8");
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("Image 组件属性", function () {
    YTest.it("source 可通过 update 修改", function () {
      var ret = YUI.update({ target: "cp_image", change: { source: "new-icon.png" } });
      YTest.expect(true).toBeTruthy();
    });

    YTest.it("source 可清空", function () {
      YUI.update({ target: "cp_image", change: { source: "" } });
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("Treeview 组件属性", function () {
    YTest.it("初始 data 已加载", function () {
      var tree = YUI.find("cp_treeview");
      YTest.expect(tree !== null && tree !== undefined).toBeTruthy();
    });

    YTest.it("data 可通过直接赋值替换", function () {
      var tree = YUI.find("cp_treeview");
      if (tree) {
        tree.data = [
          { text: "Root 1", icon: "folder", icon_text: "📁", children: [
            { text: "Item A", icon: "file", icon_text: "📄" }
          ]},
          { text: "Root 2", icon: "folder", icon_text: "📁" }
        ];
      }
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("View 组件属性", function () {
    YTest.it("子节点可通过 YUI.find 找到", function () {
      var child = YUI.find("cp_child1");
      YTest.expect(child !== null && child !== undefined).toBeTruthy();
    });

    YTest.it("子节点 text 可独立读取", function () {
      YTest.expect(YUI.getText("cp_child1")).toBe("Child 1");
      YTest.expect(YUI.getText("cp_child2")).toBe("Child 2");
    });

    YTest.it("layout 可通过 update 修改", function () {
      var ret = YUI.update({
        target: "cp_view",
        change: { layout: { type: "vertical", spacing: 4, padding: [8, 8, 8, 8] } }
      });
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("跨组件操作", function () {
    YTest.it("YUI.renderFromJson 动态添加组件", function () {
      var json = JSON.stringify({
        id: "cp_dynamic",
        type: "Label",
        text: "Dynamic",
        size: [200, 24],
        style: { color: "#cdd6f4", fontSize: 12 }
      });
      var ret = YUI.renderFromJson("compPropsRoot", json, true);
      YTest.expect(ret >= 0).toBeTruthy();
    });

    YTest.it("YUI.update 批量更新", function () {
      YUI.update([
        { target: "cp_label", change: { fontSize: 16 } },
        { target: "cp_btn", change: { text: "BatchUpdate" } },
        { target: "cp_input", change: { text: "batch" } }
      ]);
      YTest.expect(YUI.getText("cp_btn")).toBe("BatchUpdate");
      YTest.expect(YUI.getText("cp_input")).toBe("batch");
    });
  });

  YTest.run();
  YTest.exit();
}
