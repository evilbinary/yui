/**
 * E2E: synthesize pointer clicks via YUI.click(id), assert UI state.
 */
var g_count = 0;

function setCounter(n) {
  g_count = n;
  YUI.update({
    target: "counterLabel",
    change: { text: "count: " + g_count }
  });
}

function setStatus(text) {
  YUI.update({
    target: "statusLabel",
    change: { text: text }
  });
}

function onIncClick() {
  setCounter(g_count + 1);
  setStatus("incremented");
}

function onResetClick() {
  setCounter(0);
  setStatus("reset");
}

function onE2ELoad() {
  YTest.describe("e2e button click", function () {
    YTest.it("YUI.click is available", function () {
      YTest.expect(typeof YUI.click).toBe("function");
    });

    YTest.it("starts at count 0", function () {
      YTest.expect(YUI.getText("counterLabel")).toBe("count: 0");
      YTest.expect(YUI.getText("statusLabel")).toBe("idle");
    });

    YTest.it("Increment updates counter via pointer path", function () {
      var ok = YUI.click("incBtn");
      YTest.expect(ok).toBeTruthy();
      YTest.expect(YUI.getText("counterLabel")).toBe("count: 1");
      YTest.expect(YUI.getText("statusLabel")).toBe("incremented");
    });

    YTest.it("second click increments again", function () {
      YTest.expect(YUI.click("incBtn")).toBeTruthy();
      YTest.expect(YUI.getText("counterLabel")).toBe("count: 2");
    });

    YTest.it("Reset restores zero", function () {
      YTest.expect(YUI.click("resetBtn")).toBeTruthy();
      YTest.expect(YUI.getText("counterLabel")).toBe("count: 0");
      YTest.expect(YUI.getText("statusLabel")).toBe("reset");
    });
  });

  /* Wait one frame so layout positions buttons before hit-testing. */
  setTimeout(function () {
    YTest.run();
    YTest.exit();
  }, 200);
}
