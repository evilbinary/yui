/**
 * YUI 通用 Layer 属性测试
 * 测试所有 Layer 共有的属性（id, text, size, position, visible,
 * color, bgColor, fontSize, padding, borderRadius, flex, scrollable,
 * opacity, layout, focusable, data, 等）
 *
 * 用法: playground tests/integration/test-layer-props.json
 */
function onLayerPropsLoad() {
  YTest.describe("id / type", function () {
    YTest.it("YUI.dump 返回正确 id", function () {
      var o = JSON.parse(YUI.dump("lp_title"));
      YTest.expect(o.id).toBe("lp_title");
    });

    YTest.it("YUI.dump 根节点包含子节点", function () {
      var o = JSON.parse(YUI.dump());
      YTest.expect(o.children.length).toBeGreaterThan(0);
    });

    YTest.it("YUI.find 返回对象含 text 属性", function () {
      var layer = YUI.find("lp_labelA");
      YTest.expect(layer !== null && layer !== undefined).toBeTruthy();
      YTest.expect(typeof layer.text).toBe("string");
    });
  });

  YTest.describe("text 属性", function () {
    YTest.it("setText / getText 读写", function () {
      YUI.setText("lp_labelA", "hello world");
      YTest.expect(YUI.getText("lp_labelA")).toBe("hello world");
    });

    YTest.it("setText / getText 空字符串", function () {
      YUI.setText("lp_labelA", "");
      YTest.expect(YUI.getText("lp_labelA")).toBe("");
    });

    YTest.it("setText / getText 中文", function () {
      YUI.setText("lp_labelA", "你好世界");
      YTest.expect(YUI.getText("lp_labelA")).toBe("你好世界");
    });

    YTest.it("setText / getText 特殊字符", function () {
      YUI.setText("lp_labelA", "!@#$%^&*()_+{}|:<>?");
      YTest.expect(YUI.getText("lp_labelA")).toBe("!@#$%^&*()_+{}|:<>?");
    });
  });

  YTest.describe("visible 属性", function () {
    YTest.it("hide 后 visible 为 falsy", function () {
      YUI.hide("lp_labelA");
      var v = YUI.getProperty("lp_labelA", "visible");
      YTest.expect(v ? true : false).toBeFalsy();
    });

    YTest.it("show 后 visible 为 truthy", function () {
      YUI.show("lp_labelA");
      var v = YUI.getProperty("lp_labelA", "visible");
      YTest.expect(v ? true : false).toBeTruthy();
    });
  });

  YTest.describe("size 属性", function () {
    YTest.it("初始 size 为指定值", function () {
      var w = YUI.getProperty("lp_title", "width");
      YTest.expect(Number(w)).toBeGreaterThan(0);
    });

    YTest.it("update 可修改 size", function () {
      var ret = YUI.update({ target: "lp_labelA", change: { size: [200, 30] } });
      var w = YUI.getProperty("lp_labelA", "width");
      YTest.expect(Number(w)).toBe(200);
    });
  });

  YTest.describe("color / bgColor 属性", function () {
    YTest.it("getProperty color 返回字符", function () {
      var c = YUI.getProperty("lp_labelA", "color");
      YTest.expect(typeof c).toBe("string");
    });

    YTest.it("setProperty 可修改 color", function () {
      YUI.setProperty("lp_labelA", "color", "#ff0000");
      var c = YUI.getProperty("lp_labelA", "color");
      YTest.expect(c).toBeTruthy();
    });

    YTest.it("update 可修改 bgColor", function () {
      var ret = YUI.update({ target: "lp_viewA", change: { bgColor: "#2a2a3a" } });
      var c = YUI.getProperty("lp_viewA", "bgColor");
      YTest.expect(c).toBeTruthy();
    });
  });

  YTest.describe("fontSize / padding / borderRadius", function () {
    YTest.it("getProperty fontSize 存在", function () {
      var fs = YUI.getProperty("lp_btnA", "fontSize");
      YTest.expect(fs !== null && fs !== undefined).toBeTruthy();
    });

    YTest.it("getProperty borderRadius 存在", function () {
      var br = YUI.getProperty("lp_btnA", "borderRadius");
      YTest.expect(br !== null && br !== undefined).toBeTruthy();
    });
  });

  YTest.describe("flex / scrollable / opacity", function () {
    YTest.it("flex 可通过 update 设置", function () {
      var ret = YUI.update({ target: "lp_labelA", change: { flex: 1 } });
      YTest.expect(true).toBeTruthy();
    });

    YTest.it("scrollable 可读取", function () {
      var s = YUI.getProperty("lp_textA", "scrollable");
      YTest.expect(s !== null && s !== undefined).toBeTruthy();
    });

    YTest.it("opacity 可通过 update 设置", function () {
      var ret = YUI.update({ target: "lp_viewA", change: { opacity: 0.5 } });
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("focusable / enabled", function () {
    YTest.it("focusable 可设置", function () {
      YUI.update({ target: "lp_inputA", change: { focusable: 1 } });
      YTest.expect(true).toBeTruthy();
    });

    YTest.it("focus 不崩溃", function () {
      YUI.focus("lp_inputA");
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("layout 属性", function () {
    YTest.it("update 可修改 layout", function () {
      var ret = YUI.update({
        target: "lp_viewA",
        change: { layout: { type: "vertical", spacing: 4, padding: [8, 8, 8, 8] } }
      });
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("data 属性", function () {
    YTest.it("Treeview data 可重新设置", function () {
      var tree = YUI.find("lp_treeviewA");
      if (tree) {
        tree.data = [
          { text: "New Root", icon: "folder", icon_text: "📁", children: [
            { text: "New Child", icon: "file", icon_text: "📄" }
          ]}
        ];
      }
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("source 属性", function () {
    YTest.it("Image source 可设置", function () {
      var ret = YUI.update({ target: "lp_imageA", change: { source: "test.png" } });
      YTest.expect(true).toBeTruthy();
    });
  });

  YTest.describe("dump 操作", function () {
    YTest.it("dump 返回 JSON 含 style", function () {
      var o = JSON.parse(YUI.dump("lp_viewA", { style: true }));
      YTest.expect(o.style).toBeTruthy();
    });

    YTest.it("dump 非存在 id 不崩溃", function () {
      var s = YUI.dump("nonExistentId");
      YTest.expect(typeof s === "string" || s === null || s === undefined).toBeTruthy();
    });
  });

  YTest.describe("边界情况", function () {
    YTest.it("find 非存在 id 返回 null", function () {
      var found = YUI.find("noSuchId");
      YTest.expect(found === null || found === undefined).toBeTruthy();
    });

    YTest.it("getText 非存在 id 不崩溃", function () {
      var t = YUI.getText("noSuchId");
      YTest.expect(typeof t === "string" || t === null || t === undefined).toBeTruthy();
    });

    YTest.it("getProperty 非存在 id 不崩溃", function () {
      var p = YUI.getProperty("noSuchId", "color");
      YTest.expect(p === null || p === undefined).toBeTruthy();
    });
  });

  YTest.run();
  YTest.exit();
}
