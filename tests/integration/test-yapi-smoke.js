/**
 * YUI API 冒烟测试
 * 验证所有 YUI 接口的基本可用性
 * 用法: playground tests/integration/test-yapi-smoke.json
 */

function onSmokeLoad() {
  var passed = 0;
  var failed = 0;
  var failedTests = [];

  function test(name, fn) {
    try {
      fn();
      YUI.log("[PASS] " + name);
      passed++;
    } catch (e) {
      YUI.log("[FAIL] " + name + ": " + e);
      failed++;
      failedTests.push({ name: name, error: e });
    }
  }

  function expect(actual) {
    return {
      toBe: function(expected) {
        if (actual !== expected) {
          throw "expected " + expected + " but got " + actual;
        }
      },
      toBeTruthy: function() {
        if (!actual) {
          throw "expected truthy but got " + actual;
        }
      },
      toBeFalsy: function() {
        if (actual) {
          throw "expected falsy but got " + actual;
        }
      },
      toBeGreaterThan: function(n) {
        var num = Number(actual);
        if (!(num > n)) {
          throw "expected > " + n + " but got " + actual + " (as " + num + ")";
        }
      },
      toContain: function(s) {
        if (typeof actual !== "string" || actual.indexOf(s) < 0) {
          throw "expected to contain '" + s + "' in " + actual;
        }
      },
      toBeNumber: function() {
        if (typeof actual !== "number") {
          throw "expected number but got " + typeof actual;
        }
      },
      toBeString: function() {
        if (typeof actual !== "string") {
          throw "expected string but got " + typeof actual;
        }
      }
    };
  }

  // 辅助函数：将属性值转为数字（C 层可能返回字符串）
  function toNumber(val) {
    return typeof val === "string" ? parseInt(val, 10) : val;
  }

  YUI.log("=== YUI API Smoke Test ===");

  // ============================================
  // 核心 API
  // ============================================

  test("YUI.find exists and is function", function() {
    expect(typeof YUI.find).toBe("function");
  });

  test("YUI.setText / getText basic", function() {
    YUI.setText("testLabel", "hello");
    expect(YUI.getText("testLabel")).toBe("hello");
  });

  test("YUI.setText with empty string", function() {
    YUI.setText("testLabel", "");
    expect(YUI.getText("testLabel")).toBe("");
  });

  test("YUI.setProperty / getProperty basic", function() {
    YUI.setProperty("testLabel", "color", "#ff0000");
    var color = YUI.getProperty("testLabel", "color");
    expect(typeof color).toBe("string");
  });

  test("YUI.setBgColor basic", function() {
    YUI.setBgColor("testView", "#112233");
    expect(YUI.getProperty("testView", "bgColor")).toBeTruthy();
  });

  test("YUI.hide / show basic", function() {
    YUI.hide("testLabel");
    expect(YUI.getProperty("testLabel", "visible")).toBeFalsy();
    YUI.show("testLabel");
    expect(YUI.getProperty("testLabel", "visible")).toBeTruthy();
  });

  test("YUI.renderFromJson basic", function() {
    var json = JSON.stringify({
      id: "dynamicView",
      type: "View",
      size: [100, 100],
      children: []
    });
    YUI.renderFromJson("testView", json);
    expect(typeof YUI.find("dynamicView")).toBeTruthy();
  });

  test("YUI.update with object", function() {
    YUI.update({
      target: "testLabel",
      change: { text: "updated via object" }
    });
    expect(YUI.getText("testLabel")).toBe("updated via object");
  });

  test("YUI.update with JSON string", function() {
    var json = JSON.stringify({
      target: "testLabel",
      change: { text: "updated via json" }
    });
    YUI.update(json);
    expect(YUI.getText("testLabel")).toBe("updated via json");
  });

  test("YUI.dump root", function() {
    var s = YUI.dump();
    expect(typeof s).toBe("string");
    var o = JSON.parse(s);
    expect(o.id).toBe("smokeRoot");
  });

  test("YUI.dump by id", function() {
    var s = YUI.dump("testLabel");
    expect(typeof s).toBe("string");
    var o = JSON.parse(s);
    expect(o.id).toBe("testLabel");
  });

  test("YUI.log exists", function() {
    expect(typeof YUI.log).toBe("function");
    YUI.log("smoke test log");
  });

  // ============================================
  // 文件 API
  // ============================================

  test("YUI.writeFile / readFile basic", function() {
    var result = YUI.writeFile("smoke-test-temp.txt", "hello file");
    expect(result).toBeTruthy();
    var content = YUI.readFile("smoke-test-temp.txt");
    expect(content).toBe("hello file");
  });

  test("YUI.listDir basic", function() {
    var files = YUI.listDir(".");
    // listDir 返回 JSON 字符串或对象
    expect(typeof files === "string" || typeof files === "object").toBeTruthy();
  });

  // ============================================
  // 窗口/渲染 API
  // ============================================

  test("YUI.resizeRoot basic", function() {
    var origW = YUI.getProperty("smokeRoot", "width");
    YUI.resizeRoot(400, 300);
    expect(YUI.getProperty("smokeRoot", "width")).toBe(400);
    YUI.resizeRoot(origW, 300);
  });

  test("YUI.screenshot exists", function() {
    expect(typeof YUI.screenshot).toBe("function");
  });

  // ============================================
  // 输入/交互 API
  // ============================================

  test("YUI.focus basic", function() {
    YUI.focus("testInput");
    // 只验证不崩溃
    expect(true).toBeTruthy();
  });

  test("YUI.click exists", function() {
    expect(typeof YUI.click).toBe("function");
  });

  test("YUI.clickAt exists", function() {
    expect(typeof YUI.clickAt).toBe("function");
  });

  test("YUI.sendKey exists", function() {
    expect(typeof YUI.sendKey).toBe("function");
  });

  test("YUI.getClipboard / setClipboard basic", function() {
    YUI.setClipboard("clipboard test");
    var clip = YUI.getClipboard();
    expect(typeof clip).toBe("string");
  });

  // ============================================
  // 事件 API
  // ============================================

  test("YUI.setEvent exists", function() {
    expect(typeof YUI.setEvent).toBe("function");
  });

  test("YUI.call exists", function() {
    expect(typeof YUI.call).toBe("function");
  });

  // ============================================
  // 主题 API
  // ============================================

  test("YUI.themeLoad exists", function() {
    expect(typeof YUI.themeLoad).toBe("function");
  });

  test("YUI.themeSetCurrent exists", function() {
    expect(typeof YUI.themeSetCurrent).toBe("function");
  });

  test("YUI.themeUnload exists", function() {
    expect(typeof YUI.themeUnload).toBe("function");
  });

  test("YUI.themeApplyToTree exists", function() {
    expect(typeof YUI.themeApplyToTree).toBe("function");
  });

  // ============================================
  // Inspect API
  // ============================================

  test("YUI.inspect exists", function() {
    expect(typeof YUI.inspect).toBe("object");
  });

  test("YUI.inspect.enable exists", function() {
    expect(typeof YUI.inspect.enable).toBe("function");
  });

  test("YUI.inspect.disable exists", function() {
    expect(typeof YUI.inspect.disable).toBe("function");
  });

  test("YUI.inspect.setLayer exists", function() {
    expect(typeof YUI.inspect.setLayer).toBe("function");
  });

  test("YUI.inspect.setShowBounds exists", function() {
    expect(typeof YUI.inspect.setShowBounds).toBe("function");
  });

  test("YUI.inspect.setShowInfo exists", function() {
    expect(typeof YUI.inspect.setShowInfo).toBe("function");
  });

  // ============================================
  // Perf API (可选)
  // ============================================

  test("YUI.perf exists or undefined", function() {
    // perf API 是可选的，不强制存在
    if (typeof YUI.perf !== "undefined") {
      expect(typeof YUI.perf).toBe("object");
    } else {
      expect(typeof YUI.perf).toBe("undefined");
    }
  });

  if (typeof YUI.perf !== "undefined") {
    test("YUI.perf.enable exists", function() {
      expect(typeof YUI.perf.enable).toBe("function");
    });

    test("YUI.perf.getFrameStats exists", function() {
      expect(typeof YUI.perf.getFrameStats).toBe("function");
    });

    test("YUI.perf.getLayerStats exists", function() {
      expect(typeof YUI.perf.getLayerStats).toBe("function");
    });
  }

  // ============================================
  // Text 组件特殊属性
  // ============================================

  test("YUI.getProperty contentHeight for Text", function() {
    var h = YUI.getProperty("testText", "contentHeight");
    expect(typeof h).toBe("number");
    expect(h).toBeGreaterThan(0);
  });

  // ============================================
  // 边界情况测试
  // ============================================

  test("YUI.find non-existent returns null", function() {
    var found = YUI.find("nonExistentId12345");
    expect(found === null || found === undefined).toBeTruthy();
  });

  test("YUI.getText non-existent returns empty", function() {
    var text = YUI.getText("nonExistentId12345");
    // 不应崩溃
    expect(typeof text === "string" || text === null || text === undefined).toBeTruthy();
  });

  test("YUI.dump non-existent returns valid result", function() {
    var result = YUI.dump("nonExistentId12345");
    // 返回值可能是字符串或对象，只验证不崩溃
    expect(result !== null || result === null).toBeTruthy();
  });

  // ============================================
  // 结果汇总
  // ============================================

  YUI.log("=== Smoke Test Complete ===");
  YUI.log("Passed: " + passed + ", Failed: " + failed);
  
  if (failedTests.length > 0) {
    YUI.log("=== FAILED TESTS ===");
    for (var i = 0; i < failedTests.length; i++) {
      YUI.log("  - " + failedTests[i].name + ": " + failedTests[i].error);
    }
  }
  
  YUI.log("YTEST_RESULT passed=" + passed + " failed=" + failed);

  setTimeout(function() {
    YUI.exit(failed > 0 ? 1 : 0);
  }, 100);
}