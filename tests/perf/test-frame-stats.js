/**
 * Perf gate: YUI.perf must report sane frame stats under a soft budget.
 * Soft budgets avoid flaky CI on slow machines; tighten later if needed.
 */
var PERF_MAX_FRAME_MS = 250;
var PERF_MAX_RENDER_MS = 200;

function hasPerfApi() {
  return typeof YUI !== "undefined" &&
    YUI.perf &&
    typeof YUI.perf.enable === "function" &&
    typeof YUI.perf.getFrameStats === "function" &&
    typeof YUI.perf.getLayerStats === "function";
}

function onPerfLoad() {
  if (!hasPerfApi()) {
    YTest.describe("YUI.perf", function () {
      YTest.it("API is registered", function () {
        YTest.expect(hasPerfApi()).toBeTruthy();
      });
    });
    YTest.run();
    YTest.exit();
    return;
  }

  YUI.perf.enable();
  YUI.perf.setOverlay(false);
  YUI.perf.setTopN(8);
  YUI.perf.setLogInterval(0);
  YUI.perf.watch("perf_body");
  YUI.perf.watch("perf_title");

  /* Wait for several present cycles so frameIndex / layer stats populate. */
  setTimeout(function () {
    YTest.describe("YUI.perf frame stats", function () {
      YTest.it("reports a rendered frame", function () {
        var frame = YUI.perf.getFrameStats();
        YTest.expect(frame).toBeTruthy();
        YTest.expect(frame.layerCount).toBeGreaterThan(0);
        YTest.expect(frame.frameIndex).toBeGreaterThan(0);
        YTest.expect(frame.renderMs).toBeGreaterThanOrEqual(0);
        YTest.expect(frame.frameMs).toBeGreaterThanOrEqual(0);
      });

      YTest.it("stays under soft frame budgets", function () {
        var frame = YUI.perf.getFrameStats();
        YTest.expect(frame.frameMs).toBeLessThan(PERF_MAX_FRAME_MS);
        YTest.expect(frame.renderMs).toBeLessThan(PERF_MAX_RENDER_MS);
      });

      YTest.it("returns layer stats", function () {
        var byTime = YUI.perf.getLayerStats("time");
        YTest.expect(byTime).toBeTruthy();
        YTest.expect(byTime[0] || byTime["0"]).toBeTruthy();
      });
    });

    var ok = YTest.run();
    if (typeof YUI.update === "function") {
      YUI.update({
        target: "perf_status",
        change: { text: ok ? "PASS" : "FAIL", color: ok ? "#a6e3a1" : "#f38ba8" }
      });
    }
    YTest.exit();
  }, 350);
}
