function setStatus(text) {
    YUI.update({ target: "dump_status", change: { text: text } });
}

function assertDump(jsonStr, expectId) {
    if (!jsonStr || typeof jsonStr !== "string") {
        setStatus("FAIL: dump returned empty");
        YUI.log("FAIL: dump returned empty");
        return false;
    }
    var obj = JSON.parse(jsonStr);
    if (!obj || obj.id !== expectId) {
        setStatus("FAIL: expected id=" + expectId + " got " + (obj && obj.id));
        YUI.log("FAIL: bad id");
        return false;
    }
    if (!obj.rect || typeof obj.rect.w !== "number") {
        setStatus("FAIL: missing rect");
        return false;
    }
    if (!Array.isArray(obj.children)) {
        setStatus("FAIL: children not array");
        return false;
    }
    return true;
}

function onDumpRoot() {
    var s = YUI.dump();
    if (assertDump(s, "dump_root")) {
        setStatus("PASS: root dump ok, len=" + s.length);
        YUI.log("PASS root dump");
    }
}

function onDumpTitle() {
    var s = YUI.dump("dump_title");
    if (assertDump(s, "dump_title")) {
        if (s.indexOf('"style"') >= 0) {
            setStatus("FAIL: style should be absent by default");
            return;
        }
        setStatus("PASS: title dump ok");
        YUI.log("PASS title dump");
    }
}

function onDumpFull() {
    var s = YUI.dump("dump_root", { style: true, events: true });
    if (!assertDump(s, "dump_root")) return;
    var obj = JSON.parse(s);
    if (!obj.style || !obj.events) {
        setStatus("FAIL: missing style/events");
        YUI.log("FAIL style/events");
        return;
    }
    if (!obj.events.onLoad) {
        setStatus("FAIL: events.onLoad missing");
        return;
    }
    setStatus("PASS: style+events dump ok");
    YUI.log("PASS full dump");
}

function onDumpPageLoad() {
    YUI.log("test-layer-dump loaded");
    setStatus("loaded — click buttons or auto-run");
    // auto-run once so headless/manual check can see stdout JSON
    onDumpRoot();
    onDumpTitle();
    onDumpFull();
}
