/**
 * Watch OS 终端 — 模拟 shell（无系统 exec，命令在 JS 内实现）
 */

var TERM_MAX_LINES = 64;
var termLines = [];
var termHistory = [];
var termHistIdx = -1;
var termCwd = "app/watch-os";
var termBooted = false;

function onTerminalLoad() {
    if (!termBooted) {
        terminalBoot();
        termBooted = true;
    }
}

function onTerminalShow() {
    applyWatchTheme();
}

function terminalBoot() {
    termPrint("YUI Watch Terminal 1.0");
    termPrint("模拟 shell · 输入 help 查看命令");
    termPrint("");
}

function termPrint(line) {
    termLines.push(line == null ? "" : String(line));
    while (termLines.length > TERM_MAX_LINES) {
        termLines.shift();
    }
    YUI.setText("term_output", termLines.join("\n"));
}

function termPrintBlock(lines) {
    for (var i = 0; i < lines.length; i++) {
        termPrint(lines[i]);
    }
}

function termJoinArgs(args, start) {
    var i = start || 0;
    var out = "";
    for (; i < args.length; i++) {
        if (out) out += " ";
        out += args[i];
    }
    return out;
}

function termResolvePath(path) {
    var base = termCwd;
    var raw = path || ".";
    var parts;
    var stack = [];
    var i;

    if (raw.charAt(0) === "/") {
        stack = raw.substring(1).split("/");
    } else if (raw.indexOf("/") >= 0) {
        parts = (base + "/" + raw).split("/");
        stack = parts;
    } else {
        parts = base.split("/");
        for (i = 0; i < parts.length; i++) {
            if (parts[i]) stack.push(parts[i]);
        }
        stack.push(raw);
    }

    var out = [];
    for (i = 0; i < stack.length; i++) {
        var seg = stack[i];
        if (!seg || seg === ".") continue;
        if (seg === "..") {
            if (out.length > 0) out.pop();
        } else {
            out.push(seg);
        }
    }
    return out.join("/") || ".";
}

function runTerminalCommand() {
    var line = (YUI.getText("term_input") || "").replace(/^\s+|\s+$/g, "");
    if (!line) return;

    YUI.setText("term_input", "");
    termHistory.push(line);
    termHistIdx = termHistory.length;
    termPrint(termCwd + " $ " + line);
    terminalExec(line);
}

function terminalExec(line) {
    var parts = line.split(/\s+/);
    var cmd = parts[0];
    var args = parts.slice(1);

    if (cmd === "help" || cmd === "?") {
        termPrintBlock([
            "help          本帮助",
            "clear         清屏",
            "echo <text>   输出文本",
            "date          日期时间",
            "pwd           当前目录",
            "cd <path>     切换目录",
            "ls [path]     列目录",
            "cat <file>    读文件",
            "apps          已安装应用",
            "open <id>     打开应用",
            "theme <mode>  dark | light",
            "bat           电量",
            "steps         今日步数",
            "calc <expr>   简单运算 (+ - * /)",
            "history       命令历史",
            "version       版本信息"
        ]);
        return;
    }
    if (cmd === "clear") {
        clearTerminal();
        return;
    }
    if (cmd === "echo") {
        termPrint(termJoinArgs(args, 0) || "");
        return;
    }
    if (cmd === "date") {
        termPrint(new Date().toString());
        return;
    }
    if (cmd === "pwd") {
        termPrint(termCwd);
        return;
    }
    if (cmd === "cd") {
        terminalCmdCd(args[0] || "~");
        return;
    }
    if (cmd === "ls" || cmd === "dir") {
        terminalCmdLs(args[0] || ".");
        return;
    }
    if (cmd === "cat" || cmd === "type") {
        terminalCmdCat(args[0]);
        return;
    }
    if (cmd === "apps") {
        terminalCmdApps();
        return;
    }
    if (cmd === "open") {
        terminalCmdOpen(args[0]);
        return;
    }
    if (cmd === "theme") {
        terminalCmdTheme(args[0]);
        return;
    }
    if (cmd === "bat" || cmd === "battery") {
        termPrint("电量 " + Watch.battery + "%");
        return;
    }
    if (cmd === "steps") {
        var s = Watch.complications.steps;
        termPrint(formatWatchNumber(s.value) + " / " + formatWatchNumber(s.goal) + " 步");
        return;
    }
    if (cmd === "calc") {
        terminalCmdCalc(termJoinArgs(args, 0));
        return;
    }
    if (cmd === "history") {
        if (!termHistory.length) {
            termPrint("(empty)");
        } else {
            for (var h = 0; h < termHistory.length; h++) {
                termPrint("  " + (h + 1) + "  " + termHistory[h]);
            }
        }
        return;
    }
    if (cmd === "version" || cmd === "ver") {
        termPrint("YUI Watch OS · terminal 1.0");
        return;
    }
    if (cmd === "whoami") {
        termPrint("watch");
        return;
    }
    if (cmd === "uname") {
        termPrint("YUI-Watch arm64");
        return;
    }

    termPrint("sh: " + cmd + ": 未找到命令 (输入 help)");
}

function terminalCmdCd(path) {
    var target;

    if (!path || path === "~") {
        termCwd = "app/watch-os";
        return;
    }
    target = termResolvePath(path);
    if (typeof YUI.listDir !== "function") {
        termPrint("cd: listDir 不可用");
        return;
    }
    if (YUI.listDir(target)) {
        termCwd = target;
        return;
    }
    termPrint("cd: " + path + ": 无此目录");
}

function terminalCmdLs(path) {
    var dir = termResolvePath(path || ".");
    if (typeof YUI.listDir !== "function") {
        termPrint("ls: listDir 不可用");
        return;
    }
    var entries = YUI.listDir(dir);
    if (!entries) {
        termPrint("ls: 无法读取 " + dir);
        return;
    }
    if (!entries.length) {
        termPrint("(empty)");
        return;
    }
    var dirs = [];
    var files = [];
    for (var i = 0; i < entries.length; i++) {
        var e = entries[i];
        if (e.isDir) dirs.push(e.name + "/");
        else files.push(e.name);
    }
    dirs.sort();
    files.sort();
    var all = dirs.concat(files);
    var line = "";
    for (var j = 0; j < all.length; j++) {
        if (line.length > 28) {
            termPrint(line);
            line = "";
        }
        line += (line ? "  " : "") + all[j];
    }
    if (line) termPrint(line);
}

function terminalCmdCat(file) {
    var path;

    if (!file) {
        termPrint("cat: 缺少文件参数");
        return;
    }
    if (typeof YUI.readFile !== "function") {
        termPrint("cat: readFile 不可用");
        return;
    }
    if (file.indexOf("/") >= 0) {
        path = termResolvePath(file);
    } else if (termCwd === ".") {
        path = file;
    } else {
        path = termCwd + "/" + file;
    }
    var text = YUI.readFile(path);
    if (text == null) {
        termPrint("cat: " + file + ": 无法读取");
        return;
    }
    var lines = String(text).split("\n");
    for (var i = 0; i < lines.length && i < 24; i++) {
        termPrint(lines[i]);
    }
    if (lines.length > 24) {
        termPrint("... (" + lines.length + " 行，已截断)");
    }
}

function terminalCmdApps() {
    var apps = WatchAppRegistry.getLauncherApps();
    for (var i = 0; i < apps.length; i++) {
        var a = apps[i];
        termPrint(a.icon + " " + a.id + "  " + a.title);
    }
    termPrint("共 " + apps.length + " 个应用");
}

function terminalCmdOpen(id) {
    if (!id) {
        termPrint("open: 缺少应用 id");
        return;
    }
    if (!WatchAppRegistry.findById(id)) {
        termPrint("open: 未知应用 " + id);
        return;
    }
    termPrint("启动 " + id + " …");
    WatchAppRegistry.openById(id);
}

function terminalCmdTheme(mode) {
    if (!mode || (mode !== "dark" && mode !== "light")) {
        termPrint("用法: theme dark|light");
        return;
    }
    Watch.themeMode = mode;
    Theme.setCurrent(mode);
    applyWatchTheme();
    termPrint("主题 → " + mode);
}

function terminalCmdCalc(expr) {
    if (!expr) {
        termPrint("calc: 缺少表达式");
        return;
    }
    if (!/^[0-9+\-*/().%\s]+$/.test(expr)) {
        termPrint("calc: 仅支持数字与 +-*/() .");
        return;
    }
    try {
        var val = Function('"use strict"; return (' + expr + ")")();
        if (typeof val === "number" && isFinite(val)) {
            termPrint(String(val));
        } else {
            termPrint("calc: 无效结果");
        }
    } catch (e) {
        termPrint("calc: " + e.message);
    }
}

function clearTerminal() {
    termLines = [];
    YUI.setText("term_output", "");
}

function terminalHistUp() {
    if (!termHistory.length) return;
    if (termHistIdx <= 0) termHistIdx = 0;
    else termHistIdx--;
    YUI.setText("term_input", termHistory[termHistIdx]);
}

function terminalHistDown() {
    if (!termHistory.length) return;
    if (termHistIdx >= termHistory.length - 1) {
        termHistIdx = termHistory.length;
        YUI.setText("term_input", "");
        return;
    }
    termHistIdx++;
    YUI.setText("term_input", termHistory[termHistIdx]);
}

function terminalQuickHelp() {
    YUI.setText("term_input", "help");
    runTerminalCommand();
}

function terminalQuickLs() {
    YUI.setText("term_input", "ls");
    runTerminalCommand();
}

function terminalQuickPwd() {
    YUI.setText("term_input", "pwd");
    runTerminalCommand();
}

function terminalQuickApps() {
    YUI.setText("term_input", "apps");
    runTerminalCommand();
}
