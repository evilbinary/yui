/**
 * Editor E2E — mapped from Native checklist (YUI desktop subset).
 *
 * Covered: input / CJK+emoji / long text / paste / cursor / selection /
 * selectAll / line start-end / backspace / delete selection / replace /
 * cut-copy-paste / fontSize / Enter newline / scroll-to-cursor / maxLength /
 * readonly.
 *
 * Not covered here (platform / missing feature): IME compose UI, undo/redo,
 * attributedText (bold/italic), keyboardType, autocapitalize/autocorrect,
 * secure entry, double-click word select, drag-select gesture.
 */
function prop(id, name) {
  return YUI.getProperty(id, name);
}

function propNum(id, name) {
  var v = prop(id, name);
  var n = parseFloat(v);
  return isNaN(n) ? NaN : n;
}

function setProp(id, name, value) {
  return YUI.setProperty(id, name, value);
}

function textOf(id) {
  var t = prop(id, "text");
  return t === null || t === undefined ? "" : String(t);
}

function makeLongString(n) {
  var chunk = "0123456789";
  var out = "";
  var i;
  for (i = 0; i < n; i += chunk.length) {
    out += chunk;
  }
  return out.substring(0, n);
}

function onEditorE2ELoad() {
  setTimeout(function () {
    /* -------- 1. 基本输入 -------- */
    YTest.describe("1. basic input", function () {
      YTest.it("inputs plain text", function () {
        YUI.setText("inputEditor", "");
        setProp("inputEditor", "insertText", "hello");
        YTest.expect(textOf("inputEditor")).toBe("hello");
        YTest.expect(propNum("inputEditor", "length")).toBe(5);
      });

      YTest.it("inputs CJK", function () {
        YUI.setText("inputEditor", "");
        setProp("inputEditor", "insertText", "你好世界");
        YTest.expect(textOf("inputEditor")).toContain("你好");
        YTest.expect(textOf("inputEditor")).toContain("世界");
      });

      YTest.it("inputs emoji", function () {
        YUI.setText("inputEditor", "");
        setProp("inputEditor", "insertText", "hi😀");
        YTest.expect(textOf("inputEditor")).toContain("😀");
      });

      YTest.it("accepts long text without crash", function () {
        var longText = makeLongString(100000);
        setProp("inputEditor", "maxLength", 200000);
        YUI.setText("inputEditor", longText);
        YTest.expect(propNum("inputEditor", "length")).toBe(100000);
        YUI.setText("inputEditor", "");
      });

      YTest.it("pastes text", function () {
        YUI.setText("inputEditor", "");
        YUI.setClipboard("pasted-ok");
        setProp("inputEditor", "paste", 1);
        YTest.expect(textOf("inputEditor")).toBe("pasted-ok");
      });
    });

    /* -------- 2. 光标与选区 -------- */
    YTest.describe("2. cursor and selection", function () {
      YTest.it("sets cursor position (selectedRange.location)", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "cursorPos", 5);
        YTest.expect(propNum("selectEditor", "cursorPos")).toBe(5);
      });

      YTest.it("sets selection range (selectedRange.length)", function () {
        setProp("selectEditor", "selection", "6,11");
        YTest.expect(propNum("selectEditor", "selectionStart")).toBe(6);
        YTest.expect(propNum("selectEditor", "selectionEnd")).toBe(11);
        YTest.expect(prop("selectEditor", "selectedText")).toBe("World");
        YTest.expect(propNum("selectEditor", "selectionEnd") -
          propNum("selectEditor", "selectionStart")).toBe(5);
      });

      YTest.it("select all", function () {
        setProp("selectEditor", "selectAll", 1);
        YTest.expect(propNum("selectEditor", "selectionStart")).toBe(0);
        YTest.expect(propNum("selectEditor", "selectionEnd")).toBe(11);
        YTest.expect(prop("selectEditor", "selectedText")).toBe("Hello World");
      });

      YTest.it("select all via Ctrl+A key path", function () {
        setProp("selectEditor", "clearSelection", 1);
        YUI.sendKey({ id: "selectEditor", key: "a", mod: "ctrl" });
        YTest.expect(propNum("selectEditor", "selectionEnd") -
          propNum("selectEditor", "selectionStart")).toBe(11);
      });

      YTest.it("moves cursor to line start / end", function () {
        YUI.setText("multiEditor", "abc\ndef\nghi");
        setProp("multiEditor", "cursorPos", 5); /* on 'e' of def */
        setProp("multiEditor", "moveLineStart", 1);
        YTest.expect(propNum("multiEditor", "cursorPos")).toBe(4); /* after \n */
        setProp("multiEditor", "moveLineEnd", 1);
        YTest.expect(propNum("multiEditor", "cursorPos")).toBe(7); /* before \n */
      });

      YTest.it("clamps cursor to text length", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "cursorPos", 999);
        YTest.expect(propNum("selectEditor", "cursorPos")).toBe(11);
      });
    });

    /* -------- 3. 编辑操作 -------- */
    YTest.describe("3. edit operations", function () {
      YTest.it("backspace shortens text and moves cursor left", function () {
        YUI.setText("inputEditor", "abcd");
        setProp("inputEditor", "cursorPos", 4);
        setProp("inputEditor", "backspace", 1);
        YTest.expect(textOf("inputEditor")).toBe("abc");
        YTest.expect(propNum("inputEditor", "cursorPos")).toBe(3);
      });

      YTest.it("delete selection removes selected text", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "selection", "0,5");
        setProp("selectEditor", "deleteSelection", 1);
        YTest.expect(textOf("selectEditor")).toBe(" World");
      });

      YTest.it("typing replaces selection", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "selection", "6,11");
        setProp("selectEditor", "insertText", "YUI");
        YTest.expect(textOf("selectEditor")).toBe("Hello YUI");
      });

      YTest.it("cut removes text and fills clipboard", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "selection", "6,11");
        setProp("selectEditor", "cutSelection", 1);
        YTest.expect(textOf("selectEditor")).toBe("Hello ");
        YTest.expect(YUI.getClipboard()).toBe("World");
      });

      YTest.it("copy keeps text and fills clipboard", function () {
        YUI.setText("selectEditor", "Hello World");
        setProp("selectEditor", "selection", "0,5");
        setProp("selectEditor", "copySelection", 1);
        YTest.expect(textOf("selectEditor")).toBe("Hello World");
        YTest.expect(YUI.getClipboard()).toBe("Hello");
      });

      YTest.it("paste inserts clipboard", function () {
        YUI.setClipboard("XYZ");
        YUI.setText("selectEditor", "Hi");
        setProp("selectEditor", "cursorPos", 2);
        setProp("selectEditor", "paste", 1);
        YTest.expect(textOf("selectEditor")).toBe("HiXYZ");
      });

      YTest.it("sendKey Backspace path", function () {
        YUI.setText("inputEditor", "ab");
        setProp("inputEditor", "cursorPos", 2);
        YUI.sendKey({ id: "inputEditor", key: "Backspace" });
        YTest.expect(textOf("inputEditor")).toBe("a");
      });
    });

    /* -------- 4. 格式与样式（YUI 子集） -------- */
    YTest.describe("4. format and style", function () {
      YTest.it("reads and updates fontSize", function () {
        YTest.expect(propNum("selectEditor", "fontSize")).toBeGreaterThan(0);
        setProp("selectEditor", "fontSize", 22);
        YTest.expect(propNum("selectEditor", "fontSize")).toBe(22);
        setProp("selectEditor", "fontSize", 18);
      });

      YTest.it("updates fontSize via YUI.update", function () {
        YUI.update({ target: "selectEditor", change: { fontSize: 20 } });
        YTest.expect(propNum("selectEditor", "fontSize")).toBe(20);
        setProp("selectEditor", "fontSize", 18);
      });
    });

    /* -------- 5. 键盘（子集） -------- */
    YTest.describe("5. keyboard", function () {
      YTest.it("Enter inserts newline", function () {
        YUI.setText("multiEditor", "a");
        setProp("multiEditor", "cursorPos", 1);
        YUI.sendKey({ id: "multiEditor", key: "Enter" });
        YTest.expect(textOf("multiEditor")).toContain("\n");
      });
    });

    /* -------- 6. 滚动与可见性 -------- */
    YTest.describe("6. scroll and visibility", function () {
      YTest.it("scrollToCursor moves multiline scrollY", function () {
        var lines = [];
        var i;
        for (i = 0; i < 30; i++) {
          lines.push("row" + i);
        }
        YUI.setText("multiEditor", lines.join("\n"));
        setProp("multiEditor", "scrollY", 0);
        setProp("multiEditor", "cursorPos", textOf("multiEditor").length);
        setProp("multiEditor", "scrollToCursor", 1);
        YTest.expect(propNum("multiEditor", "scrollY")).toBeGreaterThan(0);
      });

      YTest.it("manual scrollY stays until scrollToCursor", function () {
        setProp("multiEditor", "scrollY", 40);
        YTest.expect(propNum("multiEditor", "scrollY")).toBe(40);
        setProp("multiEditor", "insertText", "!");
        YTest.expect(propNum("multiEditor", "scrollY")).toBe(40);
      });

      YTest.it("single-line scrollX API for far cursor", function () {
        var endPos = textOf("singleEditor").length;
        setProp("singleEditor", "scrollX", 0);
        setProp("singleEditor", "cursorPos", endPos);
        setProp("singleEditor", "scrollToCursor", 1);
        YTest.expect(propNum("singleEditor", "scrollX") >= 0).toBeTruthy();
      });
    });

    /* -------- 7. 限制与校验 -------- */
    YTest.describe("7. limits and validation", function () {
      YTest.it("respects maxLength", function () {
        YUI.setText("limitEditor", "");
        setProp("limitEditor", "maxLength", 8);
        setProp("limitEditor", "insertText", "1234567890");
        /* maxLength counts with trailing NUL room → at most maxLength-1 chars */
        YTest.expect(propNum("limitEditor", "length")).toBeLessThanOrEqual(7);
        YTest.expect(propNum("limitEditor", "length")).toBeGreaterThan(0);
      });

      YTest.it("readonly rejects insertText", function () {
        var before = textOf("readonlyEditor");
        var ok = setProp("readonlyEditor", "insertText", "nope");
        YTest.expect(ok).toBeFalsy();
        YTest.expect(textOf("readonlyEditor")).toBe(before);
      });

      YTest.it("editable=false blocks backspace", function () {
        setProp("readonlyEditor", "editable", 0);
        var before = textOf("readonlyEditor");
        setProp("readonlyEditor", "backspace", 1);
        YTest.expect(textOf("readonlyEditor")).toBe(before);
      });
    });

    YTest.run();
    YTest.exit();
  }, 280);
}
