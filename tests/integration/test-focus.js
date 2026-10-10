/**
 * 焦点导航集成测试：YUI.focus / YUI.focusMove / YUI.focusGet / onFocus / onBlur
 *
 * 用法: playground tests/integration/test-focus.json
 */
var g_focusA = 0;
var g_blurA = 0;
var g_clickB = 0;

function onFocusA(layerId) { g_focusA++; }
function onBlurA(layerId) { g_blurA++; }
function onClickB(layerId) { g_clickB++; }

function onFocusTestLoad() {
  YTest.describe("focus API", function () {
    YTest.it("focus 设置焦点并触发 onFocus", function () {
      YUI.focus("fa");
      YTest.expect(YUI.focusGet()).toBe("fa");
      YTest.expect(g_focusA).toBe(1);
    });

    YTest.it("focusMove right 移到下一个可聚焦层并触发 onBlur", function () {
      YUI.focusMove("right");
      YTest.expect(YUI.focusGet()).toBe("fb");
      YTest.expect(g_blurA).toBe(1);
    });

    YTest.it("focusMove left 移回", function () {
      YUI.focusMove("left");
      YTest.expect(YUI.focusGet()).toBe("fa");
    });

    YTest.it("focusClear 清空焦点", function () {
      YUI.focusClear();
      YTest.expect(YUI.focusGet()).toBe("");
    });
  });

  YTest.run();
  YTest.exit();
}
