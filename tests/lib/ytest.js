/**
 * YTest — lightweight Jest-like harness for YUI playground JS.
 * Load before test files: "js": ["../lib/ytest.js", "my-test.js"]
 */
(function (global) {
  var suites = [];
  var current = null;
  var passed = 0;
  var failed = 0;
  var results = [];

  function AssertionError(message) {
    this.name = "AssertionError";
    this.message = message || "assertion failed";
  }

  function Expect(actual) {
    this.actual = actual;
  }

  Expect.prototype.toBe = function (expected) {
    if (this.actual !== expected) {
      throw new AssertionError(
        "expect(" + stringify(this.actual) + ").toBe(" + stringify(expected) + ")"
      );
    }
  };

  Expect.prototype.toEqual = function (expected) {
    var a = tryStringify(this.actual);
    var b = tryStringify(expected);
    if (a !== b) {
      throw new AssertionError(
        "expect(" + a + ").toEqual(" + b + ")"
      );
    }
  };

  Expect.prototype.toBeTruthy = function () {
    if (!this.actual) {
      throw new AssertionError("expect(" + stringify(this.actual) + ").toBeTruthy()");
    }
  };

  Expect.prototype.toBeFalsy = function () {
    if (this.actual) {
      throw new AssertionError("expect(" + stringify(this.actual) + ").toBeFalsy()");
    }
  };

  Expect.prototype.toBeNull = function () {
    if (this.actual !== null) {
      throw new AssertionError("expect(" + stringify(this.actual) + ").toBeNull()");
    }
  };

  Expect.prototype.toBeUndefined = function () {
    if (typeof this.actual !== "undefined") {
      throw new AssertionError("expect(" + stringify(this.actual) + ").toBeUndefined()");
    }
  };

  Expect.prototype.toContain = function (item) {
    var ok = false;
    if (typeof this.actual === "string") {
      ok = this.actual.indexOf(item) >= 0;
    } else if (this.actual && typeof this.actual.indexOf === "function") {
      ok = this.actual.indexOf(item) >= 0;
    }
    if (!ok) {
      throw new AssertionError(
        "expect(" + stringify(this.actual) + ").toContain(" + stringify(item) + ")"
      );
    }
  };

  Expect.prototype.toBeGreaterThan = function (n) {
    if (!(this.actual > n)) {
      throw new AssertionError(
        "expect(" + this.actual + ").toBeGreaterThan(" + n + ")"
      );
    }
  };

  Expect.prototype.toBeGreaterThanOrEqual = function (n) {
    if (!(this.actual >= n)) {
      throw new AssertionError(
        "expect(" + this.actual + ").toBeGreaterThanOrEqual(" + n + ")"
      );
    }
  };

  Expect.prototype.toBeLessThan = function (n) {
    if (!(this.actual < n)) {
      throw new AssertionError(
        "expect(" + this.actual + ").toBeLessThan(" + n + ")"
      );
    }
  };

  Expect.prototype.toBeLessThanOrEqual = function (n) {
    if (!(this.actual <= n)) {
      throw new AssertionError(
        "expect(" + this.actual + ").toBeLessThanOrEqual(" + n + ")"
      );
    }
  };

  function tryStringify(v) {
    try {
      return JSON.stringify(v);
    } catch (e) {
      return String(v);
    }
  }

  function stringify(v) {
    if (typeof v === "string") return JSON.stringify(v);
    if (v === null) return "null";
    if (typeof v === "undefined") return "undefined";
    return tryStringify(v);
  }

  function log() {
    var parts = [];
    for (var i = 0; i < arguments.length; i++) parts.push(String(arguments[i]));
    var line = parts.join(" ");
    if (global.YUI && typeof YUI.log === "function") {
      YUI.log(line);
    } else if (typeof print === "function") {
      print(line);
    }
  }

  var YTest = {
    describe: function (name, fn) {
      var suite = { name: name, tests: [] };
      var prev = current;
      current = suite;
      suites.push(suite);
      try {
        fn();
      } finally {
        current = prev;
      }
    },

    it: function (name, fn) {
      if (!current) {
        YTest.describe("(root)", function () {
          YTest.it(name, fn);
        });
        return;
      }
      current.tests.push({ name: name, fn: fn });
    },

    expect: function (actual) {
      return new Expect(actual);
    },

    run: function () {
      passed = 0;
      failed = 0;
      results = [];
      for (var s = 0; s < suites.length; s++) {
        var suite = suites[s];
        log("YTest:", suite.name);
        for (var t = 0; t < suite.tests.length; t++) {
          var test = suite.tests[t];
          try {
            test.fn();
            passed++;
            results.push({ suite: suite.name, name: test.name, ok: true });
            log("  ✓", test.name);
          } catch (e) {
            failed++;
            var msg = e && e.message ? e.message : String(e);
            results.push({
              suite: suite.name,
              name: test.name,
              ok: false,
              message: msg
            });
            log("  ✗", test.name);
            log("   ", msg);
          }
        }
      }
      log("YTest:", passed + " passed,", failed + " failed,", passed + failed + " total");
      log("YTEST_RESULT passed=" + passed + " failed=" + failed);
      return failed === 0;
    },

    exit: function (code) {
      if (typeof code !== "number") {
        code = failed > 0 ? 1 : 0;
      }
      if (global.YUI && typeof YUI.exit === "function") {
        YUI.exit(code);
      } else {
        log("YTest: YUI.exit missing, code=" + code);
      }
      return code;
    },

    reset: function () {
      suites = [];
      current = null;
      passed = 0;
      failed = 0;
      results = [];
    },

    getFailed: function () {
      return failed;
    },

    getPassed: function () {
      return passed;
    }
  };

  global.YTest = YTest;
})(typeof globalThis !== "undefined" ? globalThis : this);
