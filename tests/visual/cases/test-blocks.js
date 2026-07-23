/**
 * Visual capture: solid-color blocks (no text) → PNG for baseline diff.
 * Runner compares tests/visual/output/<name>.png vs baselines/<name>.png
 */
var VISUAL_OUT = "tests/visual/output/blocks.png";

function onVisualLoad() {
  if (typeof YUI.screenshot !== "function") {
    YTest.describe("visual", function () {
      YTest.it("YUI.screenshot exists", function () {
        YTest.expect(typeof YUI.screenshot).toBe("function");
      });
    });
    YTest.run();
    YTest.exit();
    return;
  }

  if (YUI.perf && typeof YUI.perf.setOverlay === "function") {
    YUI.perf.setOverlay(false);
  }

  setTimeout(function () {
    var rc = YUI.screenshot(VISUAL_OUT);
    YTest.describe("visual capture", function () {
      YTest.it("screenshot returns 0", function () {
        YTest.expect(rc).toBe(0);
      });
    });
    YTest.run();
    YTest.exit(rc === 0 ? 0 : 1);
  }, 250);
}
