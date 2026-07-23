function onTestLoad() {
  YTest.describe("YUI.dump", function () {
    YTest.it("returns parseable JSON for root", function () {
      var s = YUI.dump();
      YTest.expect(typeof s).toBe("string");
      var o = JSON.parse(s);
      YTest.expect(o.id).toBe("dump_root");
      YTest.expect(o.rect.w).toBeGreaterThan(0);
      YTest.expect(o.children).toBeTruthy();
    });

    YTest.it("dumps subtree by id", function () {
      var o = JSON.parse(YUI.dump("dump_title"));
      YTest.expect(o.id).toBe("dump_title");
      YTest.expect(o.children.length).toBe(0);
    });

    YTest.it("omits style/events by default", function () {
      var s = YUI.dump("dump_title");
      YTest.expect(s.indexOf('"style"') < 0).toBeTruthy();
      YTest.expect(s.indexOf('"events"') < 0).toBeTruthy();
    });

    YTest.it("includes style and events when flagged", function () {
      var o = JSON.parse(YUI.dump("dump_root", { style: true, events: true }));
      YTest.expect(o.style).toBeTruthy();
      YTest.expect(o.events).toBeTruthy();
      YTest.expect(o.events.onLoad).toBeTruthy();
    });
  });

  YTest.run();
  YTest.exit();
}
