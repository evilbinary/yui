/**
 * Timer / Perf API 测试
 * 需 mquickjs + Game 模块（playground-mqjs），标准 playground 下自动跳过
 * 用法: playground-mqjs tests/integration/test-timer-perf.json
 */

function onLoad() {
  if (typeof Timer === "undefined" || typeof Perf === "undefined") {
    YUI.log("[SKIP] Timer/Perf not available in this build");
    YUI.log("YTEST_RESULT passed=0 failed=0");
    setTimeout(function () { YUI.exit(0); }, 100);
    return;
  }

  var passed = 0;
  var failed = 0;
  var failedTests = [];

  function test(name, fn) {
    try {
      fn();
      passed++;
      YUI.log("[PASS] " + name);
    } catch (e) {
      failed++;
      failedTests.push({ name: name, error: e });
      YUI.log("[FAIL] " + name + ": " + e);
    }
  }

  function expect(actual) {
    return {
      toBe: function(expected) {
        if (actual !== expected) {
          throw "expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual);
        }
      },
      toBeTruthy: function() {
        if (!actual) {
          throw "expected truthy but got " + JSON.stringify(actual);
        }
      },
      toBeNumber: function() {
        if (typeof actual !== "number") {
          throw "expected number but got " + typeof actual;
        }
      },
      toBeObject: function() {
        if (typeof actual !== "object" || actual === null) {
          throw "expected object but got " + typeof actual;
        }
      },
      toBeFunction: function() {
        if (typeof actual !== "function") {
          throw "expected function but got " + typeof actual;
        }
      },
      toBeGreaterThanOrEqual: function(n) {
        if (!(actual >= n)) {
          throw "expected >= " + n + " but got " + actual;
        }
      },
      toHaveProperty: function(key) {
        if (!(key in actual)) {
          throw "expected property '" + key + "' in " + JSON.stringify(Object.keys(actual));
        }
      }
    };
  }

  YUI.log("=== Timer / Perf API Test ===");

  // ============================================
  // Timer API
  // ============================================

  test("typeof Timer is object", function() {
    expect(typeof Timer).toBe("object");
  });

  test("Timer is not null", function() {
    expect(Timer).toBeTruthy();
  });

  test("Timer.dt is a function", function() {
    expect(typeof Timer.dt).toBe("function");
  });

  test("Timer.getDt is a function", function() {
    expect(typeof Timer.getDt).toBe("function");
  });

  test("Timer.dt() returns a number", function() {
    expect(typeof Timer.dt()).toBe("number");
  });

  test("Timer.getDt() returns a number", function() {
    expect(typeof Timer.getDt()).toBe("number");
  });

  test("Timer.dt() returns non-negative", function() {
    expect(Timer.dt()).toBeGreaterThanOrEqual(0);
  });

  test("Timer.getDt() returns same type as Timer.dt()", function() {
    expect(typeof Timer.getDt()).toBe(typeof Timer.dt());
  });

  test("Timer.dt() multiple calls stable types", function() {
    var a = Timer.dt();
    var b = Timer.dt();
    expect(typeof a).toBe("number");
    expect(typeof b).toBe("number");
    expect(a).toBeGreaterThanOrEqual(0);
    expect(b).toBeGreaterThanOrEqual(0);
  });

  test("Timer.getDt with extra args does not crash", function() {
    expect(typeof Timer.getDt(42, "extra")).toBe("number");
  });

  test("Timer.dt with extra args does not crash", function() {
    expect(typeof Timer.dt(42, "extra")).toBe("number");
  });

  test("assign Timer variable then call method", function() {
    var t = Timer;
    expect(typeof t.dt()).toBe("number");
  });

  // ============================================
  // Perf API
  // ============================================

  test("typeof Perf is object", function() {
    expect(typeof Perf).toBe("object");
  });

  test("Perf is not null", function() {
    expect(Perf).toBeTruthy();
  });

  test("Perf.getStats is a function", function() {
    expect(typeof Perf.getStats).toBe("function");
  });

  test("Perf.getStats() returns an object", function() {
    expect(typeof Perf.getStats()).toBe("object");
  });

  test("Perf.getStats() has entities", function() {
    expect(Perf.getStats()).toHaveProperty("entities");
  });

  test("Perf.getStats() has draws", function() {
    expect(Perf.getStats()).toHaveProperty("draws");
  });

  test("Perf.getStats() has particles", function() {
    expect(Perf.getStats()).toHaveProperty("particles");
  });

  test("Perf.getStats() has fps", function() {
    expect(Perf.getStats()).toHaveProperty("fps");
  });

  test("Perf.getStats() has updateMs", function() {
    expect(Perf.getStats()).toHaveProperty("updateMs");
  });

  test("Perf.getStats() has renderMs", function() {
    expect(Perf.getStats()).toHaveProperty("renderMs");
  });

  test("Perf.getStats() entities is number", function() {
    expect(typeof Perf.getStats().entities).toBe("number");
  });

  test("Perf.getStats() draws is number", function() {
    expect(typeof Perf.getStats().draws).toBe("number");
  });

  test("Perf.getStats() particles is number", function() {
    expect(typeof Perf.getStats().particles).toBe("number");
  });

  test("Perf.getStats() fps is number", function() {
    expect(typeof Perf.getStats().fps).toBe("number");
  });

  test("Perf.getStats() updateMs is number", function() {
    expect(typeof Perf.getStats().updateMs).toBe("number");
  });

  test("Perf.getStats() renderMs is number", function() {
    expect(typeof Perf.getStats().renderMs).toBe("number");
  });

  test("Perf.getStats() all stats non-negative", function() {
    var s = Perf.getStats();
    expect(s.entities).toBeGreaterThanOrEqual(0);
    expect(s.draws).toBeGreaterThanOrEqual(0);
    expect(s.particles).toBeGreaterThanOrEqual(0);
    expect(s.fps).toBeGreaterThanOrEqual(0);
    expect(s.updateMs).toBeGreaterThanOrEqual(0);
    expect(s.renderMs).toBeGreaterThanOrEqual(0);
  });

  test("Perf.getStats() multiple calls return objects", function() {
    expect(typeof Perf.getStats()).toBe("object");
    expect(typeof Perf.getStats()).toBe("object");
  });

  test("Perf.getStats with extra args does not crash", function() {
    expect(typeof Perf.getStats(42, "extra")).toBe("object");
  });

  test("assign Perf variable then call method", function() {
    var p = Perf;
    expect(typeof p.getStats()).toBe("object");
    expect(typeof p.getStats().fps).toBe("number");
  });

  // ============================================
  // 交叉验证
  // ============================================

  test("Timer is accessible via global scope", function() {
    expect(typeof Timer).toBe("object");
  });

  test("Perf is accessible via global scope", function() {
    expect(typeof Perf).toBe("object");
  });

  test("Timer and Perf are different objects", function() {
    expect(Timer !== Perf).toBeTruthy();
  });

  test("Timer.dt.call exists", function() {
    expect(typeof Timer.dt.call).toBe("function");
  });

  test("Perf.getStats.call exists", function() {
    expect(typeof Perf.getStats.call).toBe("function");
  });

  test("Timer methods not shared with Perf", function() {
    expect(Timer.getStats === undefined).toBeTruthy();
    expect(Perf.dt === undefined).toBeTruthy();
  });

  // ============================================
  // 结果汇总
  // ============================================

  YUI.log("=== Timer/Perf Test Complete ===");
  YUI.log("Passed: " + passed + ", Failed: " + failed);

  if (failedTests.length > 0) {
    YUI.log("=== FAILED TESTS ===");
    for (var i = 0; i < failedTests.length; i++) {
      YUI.log("  - " + failedTests[i].name + ": " + failedTests[i].error);
    }
  }

  YUI.log("YTEST_RESULT passed=" + passed + " failed=" + failed);

  setTimeout(function () {
    try { YUI.exit(failed > 0 ? 1 : 0); } catch (e) {}
  }, 100);
}
