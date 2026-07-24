// DB Editor - Modern UI Theme (Catppuccin Mocha)
// Uses YUI.call() bridge to invoke MySQL C handlers

var MAX_TABS = 5;
var currentDb = null;
var tabs = [];
var activeTab = 0;
var tabCounter = 1;

// ====================== 连接管理 ======================

var dbConfig = {
    host: "localhost",
    user: "root",
    password: "root",
    database: "",
    port: 3306
};

var appPreferences = {
    autoConnect: true,
    pageSize: 500,
    fontSize: 13
};

var currentTheme = "mocha";
var pendingPrefTheme = null;
var themeBeforePrefs = null;

var THEME_OPTIONS = {
    mocha: { label: "Catppuccin Mocha", path: "app/lib/themes/mocha.json" },
    dark: { label: "Dark", path: "app/lib/themes/dark.json" },
    latte: { label: "Catppuccin Latte", path: "app/lib/themes/latte.json" },
    "element-plus": { label: "Element Plus", path: "app/lib/themes/element-plus.json" },
    "element-plus-dark": { label: "Element Plus Dark", path: "app/lib/themes/element-plus-dark.json" },
    "soft-ui": { label: "Soft UI", path: "app/lib/themes/soft-ui.json" },
    "soft-ui-dark": { label: "Soft UI Dark", path: "app/lib/themes/soft-ui-dark.json" }
};

function loadDbConfig() {
    var cfg = readAppConfig();
    if (cfg.connection) {
        if (cfg.connection.host) dbConfig.host = cfg.connection.host;
        if (cfg.connection.user) dbConfig.user = cfg.connection.user;
        if (cfg.connection.password !== undefined) dbConfig.password = cfg.connection.password;
        if (cfg.connection.database !== undefined) dbConfig.database = cfg.connection.database;
        if (cfg.connection.port) dbConfig.port = cfg.connection.port;
    }
    if (cfg.preferences) {
        if (cfg.preferences.autoConnect !== undefined) appPreferences.autoConnect = cfg.preferences.autoConnect;
        if (cfg.preferences.pageSize) appPreferences.pageSize = cfg.preferences.pageSize;
        if (cfg.preferences.fontSize) appPreferences.fontSize = cfg.preferences.fontSize;
    }
    if (cfg.theme) currentTheme = cfg.theme;
    RESULT_PAGE_SIZE = appPreferences.pageSize || 500;
    applyPreferencesToUi();
    applyTheme(currentTheme || "mocha", true);
}

function saveDbConfig() {
    var cfg = readAppConfig();
    cfg.connection = {
        host: dbConfig.host,
        user: dbConfig.user,
        password: dbConfig.password,
        database: dbConfig.database || "",
        port: dbConfig.port
    };
    cfg.preferences = {
        autoConnect: appPreferences.autoConnect,
        pageSize: appPreferences.pageSize,
        fontSize: appPreferences.fontSize
    };
    cfg.theme = currentTheme;
    writeAppConfig(cfg);
}

function applyPreferencesToUi() {
    var pager = yui.find("resultPager");
    if (pager) pager.pageSize = RESULT_PAGE_SIZE;
    var editor = yui.find("sqlEditor");
    if (editor) {
        editor.style = { fontSize: appPreferences.fontSize };
    }
}

function showOverlay(id) {
    var el = yui.find(id);
    if (el) el.visible = true;
}

function hideOverlay(id) {
    var el = yui.find(id);
    if (el) el.visible = false;
}

function getInputText(id) {
    var input = yui.find(id);
    return input && input.text ? String(input.text) : "";
}

function setInputText(id, value) {
    var input = yui.find(id);
    if (input) input.text = value != null ? String(value) : "";
}

function fillConnectionForm() {
    setInputText("connHostInput", dbConfig.host);
    setInputText("connPortInput", dbConfig.port);
    setInputText("connUserInput", dbConfig.user);
    setInputText("connPasswordInput", dbConfig.password);
    setInputText("connDatabaseInput", dbConfig.database || "");
}

function readConnectionForm() {
    dbConfig.host = getInputText("connHostInput") || "localhost";
    dbConfig.port = parseInt(getInputText("connPortInput"), 10) || 3306;
    dbConfig.user = getInputText("connUserInput") || "root";
    dbConfig.password = getInputText("connPasswordInput");
    dbConfig.database = getInputText("connDatabaseInput");
}

function connectDb() {
    print("Connecting to MySQL...");
    updateStatus("连接中...", "#F9E2AF");

    var result = YUI.call("mysql_connect", JSON.stringify(dbConfig));
    if (!result) {
        updateStatus("连接失败: 无返回", "#F38BA8");
        return;
    }
    var info = JSON.parse(result);
    if (info.success) {
        updateStatus("已连接", "#A6E3A1");
        var statusText = yui.find("statusText");
        var statusIcon = yui.find("statusIcon");
        if (statusText) {
            statusText.text = "已连接 " + dbConfig.host + ":" + dbConfig.port;
            statusText.style = { color: "#A6E3A1" };
        }
        if (statusIcon) statusIcon.style = { color: "#A6E3A1" };
        loadDatabases();
    } else {
        updateStatus("连接失败: " + (info.error || "未知错误"), "#F38BA8");
    }
}

function disconnectDb() {
    YUI.call("mysql_close");
    var statusText = yui.find("statusText");
    if (statusText) {
        statusText.text = "未连接";
        statusText.style = { color: "#F38BA8" };
    }
    var statusIcon = yui.find("statusIcon");
    if (statusIcon) statusIcon.style = { color: "#F38BA8" };
    updateStatus("已断开连接", "#F38BA8");
    currentDb = null;

    var tree = yui.find("dbTree");
    if (tree) tree.data = [];
    fullDbData = [];
}


// ====================== Tab Management ======================

function initTabs() {
    tabs = [{ id: 0, name: "Query 1", sql: "" }];
    activeTab = 0;
    tabCounter = 1;
    renderTabs();
}

function renderTabs() {
    for (var i = 0; i < MAX_TABS; i++) {
        var el = yui.find("tab_" + i);
        if (!el) continue;
        if (i < tabs.length) {
            el.visible = true;
            el.size = [70, 24];
            el.text = tabs[i].name;
            el.variant = (i === activeTab) ? "tab active" : "tab";
        } else {
            el.visible = false;
            el.size = [0, 0];
        }
    }
}

function saveCurrentText() {
    var editor = yui.find("sqlEditor");
    if (editor && activeTab >= 0 && activeTab < tabs.length) {
        tabs[activeTab].sql = editor.text;
    }
}

function loadTabText(index) {
    var editor = yui.find("sqlEditor");
    if (editor && index >= 0 && index < tabs.length) {
        editor.text = tabs[index].sql;
    }
}

function switchTab(index) {
    if (index === activeTab || index < 0 || index >= tabs.length) return;
    saveCurrentText();
    activeTab = index;
    loadTabText(index);
    renderTabs();
}

function onTabClick(layerId) {
    print("onTabClick: clicked tab " + layerId);
    if (!layerId) return;
    var idx = parseInt(layerId.replace("tab_", ""));
    for (var i = 0; i < tabs.length; i++) {
        if (tabs[i].id === idx) { switchTab(i); return; }
    }
}

function onNewTab() {
    if (tabs.length >= MAX_TABS) {
        updateStatus("已达到最大标签数", "#F38BA8");
        return;
    }
    saveCurrentText();
    var id = tabCounter++;
    tabs.push({ id: id, name: "Query " + (tabs.length + 1), sql: "" });
    activeTab = tabs.length - 1;
    loadTabText(activeTab);
    renderTabs();
}

function onSqlChange(text) {
    // text 参数是 layer ID，不是文本内容，需要从 editor 读取
    var editor = yui.find("sqlEditor");
    if (editor && activeTab >= 0 && activeTab < tabs.length) {
        tabs[activeTab].sql = editor.text;
    }
}


// ====================== Menu Handlers ======================

function handleMenuClick(menuId) {
    if (!menuId) return;
    var layer = yui.find(menuId);
    var itemText = layer ? layer.text : "";
    if (itemText === "新建查询" || itemText === "打开文件" || itemText === "保存文件" ||
        itemText === "导出 CSV" || itemText === "退出") {
        onFileMenuClick(itemText);
    } else if (itemText === "新建连接" || itemText === "连接数据库" || itemText === "断开连接" || itemText === "导入数据库") {
        onDbMenuClick(itemText);
    } else {
        onSettingsMenuClick(itemText);
    }
}

function onFileMenuClick(itemText) {
    switch (itemText) {
        case "新建查询": onNewTab(); break;
        case "打开文件": onOpen(); break;
        case "保存文件": onSave(); break;
        case "导出 CSV": onExportCsv(); break;
        case "退出": onExit(); break;
    }
}

function onDbMenuClick(itemText) {
    switch (itemText) {
        case "新建连接": onNewConnection(); break;
        case "连接数据库": connectDb(); break;
        case "断开连接": disconnectDb(); break;
        case "导入数据库": onImportDb(); break;
    }
}

function onSettingsMenuClick(itemText) {
    switch (itemText) {
        case "偏好设置": onOpenSettings(); break;
        case "主题设置": onOpenSettings(); break;
        case "关于": onAbout(); break;
    }
}

function onNewConnection() {
    fillConnectionForm();
    showOverlay("connectionDialogOverlay");
    YUI.focus("connHostInput");
}

function onConnectionTest() {
    readConnectionForm();
    updateStatus("正在测试连接...", "#F9E2AF");
    var result = YUI.call("mysql_connect", JSON.stringify(dbConfig));
    if (!result) {
        updateStatus("测试失败: 无返回", "#F38BA8");
        return;
    }
    var info = JSON.parse(result);
    if (info.success) {
        updateStatus("连接测试成功", "#A6E3A1");
    } else {
        updateStatus("连接测试失败: " + (info.error || "未知错误"), "#F38BA8");
    }
}

function onConnectionOk() {
    readConnectionForm();
    saveDbConfig();
    hideOverlay("connectionDialogOverlay");
    disconnectDb();
    connectDb();
}

function onConnectionCancel() {
    hideOverlay("connectionDialogOverlay");
}

function setPrefNav(section) {
    var items = [
        { key: "appearance", nav: "prefNavAppearance", panel: "prefSectionAppearance" },
        { key: "results", nav: "prefNavResults", panel: "prefSectionResults" },
        { key: "connection", nav: "prefNavConnection", panel: "prefSectionConnection" }
    ];
    for (var i = 0; i < items.length; i++) {
        var item = items[i];
        var active = item.key === section;
        var nav = yui.find(item.nav);
        var panel = yui.find(item.panel);
        if (nav) nav.variant = active ? "navItem active" : "navItem";
        if (panel) panel.visible = active;
    }
}

function onPrefNavAppearance() { setPrefNav("appearance"); }
function onPrefNavResults() { setPrefNav("results"); }
function onPrefNavConnection() { setPrefNav("connection"); }

function onOpenSettings() {
    themeBeforePrefs = currentTheme;
    pendingPrefTheme = currentTheme;
    var auto = yui.find("prefAutoConnect");
    if (auto) auto.data = appPreferences.autoConnect !== false;
    setInputText("prefPageSizeInput", appPreferences.pageSize);
    setInputText("prefFontSizeInput", appPreferences.fontSize);
    updateThemeLabel(pendingPrefTheme);
    setPrefNav("appearance");
    showOverlay("preferencesDialogOverlay");
}

function onPreferencesOk() {
    var auto = yui.find("prefAutoConnect");
    appPreferences.autoConnect = auto ? !!auto.data : true;
    appPreferences.pageSize = parseInt(getInputText("prefPageSizeInput"), 10) || 500;
    appPreferences.fontSize = parseInt(getInputText("prefFontSizeInput"), 10) || 13;
    if (appPreferences.pageSize < 10) appPreferences.pageSize = 10;
    if (appPreferences.pageSize > 5000) appPreferences.pageSize = 5000;
    if (appPreferences.fontSize < 10) appPreferences.fontSize = 10;
    if (appPreferences.fontSize > 24) appPreferences.fontSize = 24;
    RESULT_PAGE_SIZE = appPreferences.pageSize;
    applyPreferencesToUi();
    var themeToApply = pendingPrefTheme || currentTheme;
    if (themeToApply !== currentTheme) {
        applyTheme(themeToApply, true);
    } else {
        saveDbConfig();
    }
    pendingPrefTheme = null;
    themeBeforePrefs = null;
    hideOverlay("preferencesDialogOverlay");
    updateStatus("偏好设置已保存", "#A6E3A1");
}

function onPreferencesCancel() {
    if (themeBeforePrefs && themeBeforePrefs !== currentTheme) {
        applyTheme(themeBeforePrefs, true);
    }
    pendingPrefTheme = null;
    themeBeforePrefs = null;
    hideOverlay("preferencesDialogOverlay");
}

function updateThemeLabel(themeId) {
    var label = yui.find("themeCurrentLabel");
    var id = themeId || pendingPrefTheme || currentTheme;
    var theme = THEME_OPTIONS[id];
    if (label) {
        label.text = "当前主题：" + (theme ? theme.label : id);
    }
}

function applyTheme(themeId, silent) {
    var theme = THEME_OPTIONS[themeId];
    if (!theme) return;
    if (typeof YUI.themeLoad !== "function") {
        if (!silent) updateStatus("主题 API 不可用", "#F38BA8");
        return;
    }
    var loaded = YUI.themeLoad(theme.path);
    if (!loaded || loaded.success === false) {
        if (!silent) updateStatus("主题加载失败: " + theme.path, "#F38BA8");
        return;
    }
    var themeName = loaded.name || themeId;
    currentTheme = themeId;
    YUI.themeSetCurrent(themeName);
    YUI.themeApplyToTree();
    renderTabs();
    updateThemeLabel(currentTheme);
    saveDbConfig();
    if (!silent) updateStatus("已切换主题: " + theme.label, "#A6E3A1");
}

function pickPrefTheme(themeId) {
    pendingPrefTheme = themeId;
    updateThemeLabel(themeId);
    /* 点选即预览，取消时再回退到打开偏好前的主题 */
    if (themeId !== currentTheme) {
        applyTheme(themeId, true);
    }
}

function onPrefThemeMocha() { pickPrefTheme("mocha"); }
function onPrefThemeDark() { pickPrefTheme("dark"); }
function onPrefThemeLatte() { pickPrefTheme("latte"); }
function onPrefThemeElementPlus() { pickPrefTheme("element-plus"); }
function onPrefThemeElementPlusDark() { pickPrefTheme("element-plus-dark"); }
function onPrefThemeSoftUi() { pickPrefTheme("soft-ui"); }
function onPrefThemeSoftUiDark() { pickPrefTheme("soft-ui-dark"); }

function onThemeSettings() {
    onOpenSettings();
}

function onExit() {
    updateStatus("退出应用", "#F38BA8");
}

function onImportDb() {
    updateStatus("导入功能开发中", "#F9E2AF");
}

function onAbout() {
    YUI.show("aboutDialog");
}


// ====================== Toolbar Actions ======================

function executeQuery() {
    var editor = yui.find("sqlEditor");
    var sql = editor ? editor.text : "";
    if (!sql || sql.trim() === "") {
        return;
    }
    if (activeTab >= 0 && activeTab < tabs.length) {
        tabs[activeTab].sql = sql;
    }

    updateStatus("执行中...", "#F9E2AF");

    var result = YUI.call("mysql_query", JSON.stringify({ sql: sql }));

    if (!result || result === "[]") {
        var affectedInfo = YUI.call("mysql_exec", JSON.stringify({ sql: sql }));
        if (affectedInfo) {
            var info = JSON.parse(affectedInfo);
            updateStatus("影响 " + (info.affected || 0) + " 行", "#A6E3A1");
            showResults([], 0);
        }
        return;
    }

    var rows = JSON.parse(result);
    showResults(rows, rows.length);
    updateStatus("查询完成, " + rows.length + " 行", "#A6E3A1");
}

function onExplain() {
    updateStatus("执行计划: 未实现", "#F9E2AF");
}

function onFormat() {
    var editor = yui.find("sqlEditor");
    if (!editor || !editor.text) return;
    var sql = editor.text;
    var formatted = sql.replace(/\s+/g, " ").trim();
    formatted = formatted.replace(/\b(SELECT|FROM|WHERE|AND|OR|ORDER BY|GROUP BY|HAVING|LIMIT|JOIN|LEFT JOIN|RIGHT JOIN|INNER JOIN|ON|INSERT INTO|VALUES|UPDATE|SET|DELETE|CREATE|DROP|ALTER|TABLE|INDEX|VIEW)\b/gi,
        function(m) { return "\n" + m.toUpperCase(); });
    editor.text = formatted.trim();
    if (activeTab >= 0 && activeTab < tabs.length) {
        tabs[activeTab].sql = editor.text;
    }
    updateStatus("格式化完成", "#89B4FA");
}

// ====================== File Browser ======================

var fileDialogMode = "open";  // "open" | "save" | "exportCsv"
var fileDialogPath = "./";
var fileDialogSelected = "";

function formatExportTimestamp() {
    var d = new Date();
    function pad(n) { return n < 10 ? "0" + n : "" + n; }
    return d.getFullYear() +
        pad(d.getMonth() + 1) +
        pad(d.getDate()) + "_" +
        pad(d.getHours()) +
        pad(d.getMinutes()) +
        pad(d.getSeconds());
}

function collectResultColumns(rows) {
    if (!rows || rows.length === 0) return [];
    var seen = {};
    var columns = [];
    for (var r = 0; r < rows.length; r++) {
        var row = rows[r];
        if (!row || typeof row !== "object") continue;
        for (var key in row) {
            if (Object.prototype.hasOwnProperty.call(row, key) && !seen[key]) {
                seen[key] = true;
                columns.push(key);
            }
        }
    }
    return columns;
}

function csvEscapeField(value) {
    if (value === null || value === undefined) return "";
    var s = String(value);
    if (/[",\r\n]/.test(s)) {
        return '"' + s.replace(/"/g, '""') + '"';
    }
    return s;
}

function rowsToCsv(rows) {
    var columns = collectResultColumns(rows);
    if (columns.length === 0) return "";

    var lines = [];
    var header = [];
    for (var i = 0; i < columns.length; i++) {
        header.push(csvEscapeField(columns[i]));
    }
    lines.push(header.join(","));

    for (var r = 0; r < rows.length; r++) {
        var row = rows[r] || {};
        var fields = [];
        for (var c = 0; c < columns.length; c++) {
            var val = row[columns[c]];
            if (val === null || val === undefined) {
                fields.push("");
            } else if (typeof val === "object") {
                fields.push(csvEscapeField(JSON.stringify(val)));
            } else {
                fields.push(csvEscapeField(val));
            }
        }
        lines.push(fields.join(","));
    }
    return lines.join("\r\n");
}

function showFileDialog(mode, defaultName) {
    fileDialogMode = mode;
    fileDialogSelected = "";
    var title = yui.find("fileDialogTitle");
    if (title) {
        if (mode === "save") title.text = "保存文件";
        else if (mode === "exportCsv") title.text = "导出 CSV";
        else title.text = "打开文件";
    }
    var input = yui.find("fileDialogInput");
    if (input) input.text = defaultName || "";
    var overlay = yui.find("fileDialogOverlay");
    if (overlay) overlay.visible = true;
    refreshFileList(fileDialogPath || "./");
    // 自动聚焦输入框
    YUI.focus("fileDialogInput");
}

function hideFileDialog() {
    var overlay = yui.find("fileDialogOverlay");
    if (overlay) overlay.visible = false;
}

function refreshFileList(path) {
    fileDialogPath = path;
    var pathLabel = yui.find("fileDialogPath");
    if (pathLabel) pathLabel.text = path;

    var entries = YUI.listDir(path);
    var treeData = [];
    if (entries) {
        // Directories first, then files
        var dirs = [];
        var files = [];
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].isDir) {
                dirs.push({ text: entries[i].name, icon: "folder", icon_text: "", expandable: false, _isDir: true });
            } else {
                files.push({ text: entries[i].name, icon: "file", icon_text: "", _isDir: false });
            }
        }
        treeData = dirs.concat(files);
    }
    var tree = yui.find("fileDialogList");
    if (tree) tree.data = treeData;
}

function onFileBrowserUp() {
    var path = fileDialogPath || "./";
    // Strip trailing slash and go up
    if (path.endsWith("/")) path = path.substring(0, path.length - 1);
    if (path === "" || path === ".") {
        fileDialogPath = "./";
    } else {
        var lastSlash = path.lastIndexOf("/");
        fileDialogPath = lastSlash >= 0 ? path.substring(0, lastSlash + 1) : "./";
    }
    refreshFileList(fileDialogPath);
}

function onFileBrowserSelect(layerId) {
    if (!layerId) return;
    var layer = yui.find(layerId);
    if (!layer) return;
    var nodeText = layer.text;
    if (!nodeText) return;
    var node = JSON.parse(nodeText);
    var name = node.text;

    var newPath = fileDialogPath;
    if (!newPath.endsWith("/")) newPath += "/";

    if (node._isDir) {
        newPath += name + "/";
        refreshFileList(newPath);
    } else {
        fileDialogSelected = name;
        var input = yui.find("fileDialogInput");
        if (input) input.text = name;
    }
}

function onFileBrowserOk() {
    var input = yui.find("fileDialogInput");
    var filename = input ? input.text.trim() : fileDialogSelected;
    if (!filename) {
        updateStatus("请输入文件名", "#F38BA8");
        return;
    }

    var path = fileDialogPath;
    if (!path.endsWith("/")) path += "/";
    var fullPath = path + filename;

    hideFileDialog();

    if (fileDialogMode === "save") {
        doSaveFile(fullPath);
    } else if (fileDialogMode === "exportCsv") {
        doExportCsv(fullPath);
    } else {
        doOpenFile(fullPath);
    }
}

function onFileBrowserCancel() {
    hideFileDialog();
}

function doSaveFile(path) {
    var editor = yui.find("sqlEditor");
    var sql = editor ? editor.text : "";
    if (!sql || sql.trim() === "") {
        updateStatus("没有内容可保存", "#F38BA8");
        return;
    }
    if (activeTab >= 0 && activeTab < tabs.length) {
        tabs[activeTab].sql = sql;
    }
    var ok = YUI.writeFile(path, sql);
    if (ok) {
        updateStatus("已保存到 " + path, "#A6E3A1");
    } else {
        updateStatus("保存失败: " + path, "#F38BA8");
    }
}

function doOpenFile(path) {
    var content = YUI.readFile(path);
    if (content) {
        if (activeTab >= 0 && activeTab < tabs.length) {
            tabs[activeTab].sql = content;
            tabs[activeTab].name = path.replace(/^.*[\\\/]/, "").replace(/\.sql$/, "") || tabs[activeTab].name;
        }
        var editor = yui.find("sqlEditor");
        if (editor) editor.text = content;
        renderTabs();
        updateStatus("已打开 " + path, "#A6E3A1");
    } else {
        updateStatus("打开失败: " + path, "#F38BA8");
    }
}

function onSave() {
    showFileDialog("save");
}

function onExportCsv() {
    if (!resultRowsAll || resultRowsAll.length === 0) {
        updateStatus("没有可导出的查询结果", "#F38BA8");
        return;
    }
    showFileDialog("exportCsv", "query_result_" + formatExportTimestamp() + ".csv");
}

function doExportCsv(path) {
    if (!resultRowsAll || resultRowsAll.length === 0) {
        updateStatus("没有可导出的查询结果", "#F38BA8");
        return;
    }
    if (!path) {
        updateStatus("导出失败: 无效路径", "#F38BA8");
        return;
    }
    if (path.toLowerCase().indexOf(".csv") !== path.length - 4) {
        path += ".csv";
    }

    var csv = rowsToCsv(resultRowsAll);
    var content = "\uFEFF" + csv;
    var ok = YUI.writeFile(path, content);
    if (ok) {
        updateStatus("已导出 " + resultRowsAll.length + " 行到 " + path, "#A6E3A1");
    } else {
        updateStatus("导出失败: " + path, "#F38BA8");
    }
}

function onOpen() {
    showFileDialog("open");
}

// ====================== Database Tree (Multi-DB with Categories) ======================

var fullDbData = [];

function loadDatabases() {
    var result = YUI.call("mysql_databases");
    fullDbData = [];
    if (result) {
        try {
            var dbs = JSON.parse(result);
            for (var i = 0; i < dbs.length; i++) {
                fullDbData.push({
                    id: "db_" + dbs[i],
                    text: dbs[i],
                    icon: "app/assets/icons/db-db.svg",
                    icon_text: "",
                    expanded: false,
                    expandable: true,
                    children: []
                });
            }
        } catch (e) {
            print("loadDatabases parse error: " + e);
        }
    }
    applyDbFilter("");
}

function loadCategories(dbName) {
    for (var i = 0; i < fullDbData.length; i++) {
        if (fullDbData[i].text === dbName) {
            if (!fullDbData[i].children || fullDbData[i].children.length === 0) {
                fullDbData[i].children = [
                    { text: "表", icon: "app/assets/icons/db-table.svg", icon_text: "", children: [], _db: dbName, expandable: true },
                    { text: "视图", icon: "app/assets/icons/db-view.svg", icon_text: "", children: [], _db: dbName, expandable: true },
                    { text: "存储过程", icon: "app/assets/icons/db-procedure.svg", icon_text: "", children: [], _db: dbName, expandable: true },
                    { text: "函数", icon: "app/assets/icons/db-function.svg", icon_text: "", children: [], _db: dbName, expandable: true }
                ];
                fullDbData[i].expanded = true;
            }
            break;
        }
    }
    applyDbFilter("");
}

function loadCategoryItems(dbName, category, handler) {
    var result = YUI.call(handler, JSON.stringify({ database: dbName }));
    if (!result) return;
    try {
        var items = JSON.parse(result);
        for (var i = 0; i < fullDbData.length; i++) {
            if (fullDbData[i].text === dbName) {
                var cats = fullDbData[i].children;
                for (var j = 0; j < cats.length; j++) {
                    if (cats[j].text === category) {
                        var categoryIcons = { "表": "app/assets/icons/db-table.svg", "视图": "app/assets/icons/db-view.svg", "存储过程": "app/assets/icons/db-procedure.svg", "函数": "app/assets/icons/db-function.svg" };
                        var children = [];
                        for (var k = 0; k < items.length; k++) {
                            children.push({
                                id: "item_" + dbName + "_" + category + "_" + items[k],
                                text: items[k],
                                icon: categoryIcons[category] || "item",
                                icon_text: cats[j].icon_text,
                                _db: dbName
                            });
                        }
                        cats[j].children = children;
                        cats[j].expanded = true;
                        break;
                    }
                }
                break;
            }
        }
        applyDbFilter("");
    } catch (e) {
        print("loadCategoryItems parse error: " + e);
    }
}

function onSearchDb(layerId) {
    var input = yui.find(layerId || "dbListSearch");
    applyDbFilter(input && input.text ? input.text : "");
}

function applyDbFilter(filter) {
    var tree = yui.find("dbTree");
    if (!tree) return;
    if (!filter || filter === "") {
        tree.data = fullDbData;
        return;
    }
    var f = filter.toLowerCase();
    function filterNodes(nodes) {
        var result = [];
        for (var i = 0; i < nodes.length; i++) {
            var n = nodes[i];
            var match = n.text.toLowerCase().indexOf(f) !== -1;
            var filteredChildren = n.children ? filterNodes(n.children) : [];
            if (match || filteredChildren.length > 0) {
                result.push({
                    id: n.id, text: n.text, icon: n.icon, icon_text: n.icon_text,
                    expanded: true,
                    children: filteredChildren.length > 0 ? filteredChildren : (n.children ? [] : undefined)
                });
            }
        }
        return result;
    }
    tree.data = filterNodes(fullDbData);
}


// ====================== Query Results ======================

var RESULT_PAGE_SIZE = 500;
var resultRowsAll = [];

function showResults(rows, rowCount) {
    resultRowsAll = rows || [];
    var total = rowCount != null ? rowCount : resultRowsAll.length;
    var pager = yui.find("resultPager");
    if (pager) {
        pager.pageSize = RESULT_PAGE_SIZE;
        pager.total = total;
        pager.page = 1;
    }
    renderResultPage(1, RESULT_PAGE_SIZE);
}

function renderResultPage(page, pageSize) {
    page = page || 1;
    pageSize = pageSize || RESULT_PAGE_SIZE;
    var start = (page - 1) * pageSize;
    var end = Math.min(start + pageSize, resultRowsAll.length);

    var table = yui.find("resultTable");
    if (table) table.data = resultRowsAll.slice(start, end);

    var resultInfo = yui.find("resultInfo");
    if (resultInfo) {
        if (resultRowsAll.length === 0) {
            resultInfo.text = "(0 行)";
        } else if (resultRowsAll.length <= pageSize) {
            resultInfo.text = "(" + resultRowsAll.length + " 行)";
        } else {
            resultInfo.text = "共 " + resultRowsAll.length + " 行";
        }
    }
}

function onResultPageChange(layerId) {
    var pager = yui.find(layerId);
    if (!pager || !pager.text) return;
    try {
        var state = JSON.parse(pager.text);
        renderResultPage(state.page, state.pageSize);
    } catch (e) {}
}

function onResultRowClick(layerId) {
    var layer = yui.find(layerId);
    if (!layer || !layer.text) return;
    try {
        var row = JSON.parse(layer.text);
        if (row) {
            updateStatus("选中行: " + JSON.stringify(row), "#89B4FA");
        }
    } catch (e) {}
}

function clearEditor() {
    var editor = yui.find("sqlEditor");
    if (editor) editor.text = "";
    if (activeTab >= 0 && activeTab < tabs.length) tabs[activeTab].sql = "";
    showResults([], 0);
    updateStatus("就绪", "#F38BA8");
}


// ====================== Status ======================

function updateStatus(text, color) {
    var statusText = yui.find("statusText");
    if (statusText) {
        statusText.text = text;
        statusText.style = { color: color ? color : "#CDD6F4" };
    }
    if (color) {
        var statusIcon = yui.find("statusIcon");
        if (statusIcon) statusIcon.style = { color: color };
    }
}


// ====================== Events ======================

function onDbSelect(layerId) {
    if (!layerId) return;
    var layer = yui.find(layerId);
    if (!layer) return;
    var nodeText = layer.text;
    if (!nodeText) return;
    var node = JSON.parse(nodeText);
    currentDb = node;

    if (node.icon && node.icon.indexOf("db-db") >= 0) {
        if (!node.children || node.children.length === 0) {
            loadCategories(node.text);
        }
        var statusText = yui.find("statusText");
        if (statusText) statusText.text = "数据库: " + node.text;
    } else if (node.expandable) {
        var dbName = node._db;
        if (!dbName) {
            for (var i = 0; i < fullDbData.length && !dbName; i++) {
                var cats = fullDbData[i].children || [];
                for (var j = 0; j < cats.length; j++) {
                    if (cats[j].text === node.text) {
                        dbName = cats[j]._db || fullDbData[i].text;
                        break;
                    }
                }
            }
        }
        var handlers = { "表": "mysql_db_tables", "视图": "mysql_db_views", "存储过程": "mysql_db_procedures", "函数": "mysql_db_functions" };
        var handler = handlers[node.text];
        if (handler && dbName) {
            loadCategoryItems(dbName, node.text, handler);
        }
        var statusText = yui.find("statusText");
        if (statusText) statusText.text = "加载: " + (dbName || "?") + "." + node.text;
    } else if (node.text) {
        // Leaf item - prefer _db from node, fall back to search
        var dbName = node._db;
        if (!dbName) {
            for (var i = 0; i < fullDbData.length && !dbName; i++) {
                var cats = fullDbData[i].children || [];
                for (var j = 0; j < cats.length; j++) {
                    var items = cats[j].children || [];
                    for (var k = 0; k < items.length; k++) {
                        if (items[k].text === node.text) {
                            dbName = fullDbData[i].text;
                            break;
                        }
                    }
                }
            }
        }
        var statusText = yui.find("statusText");
        if (statusText) statusText.text = "选中: " + (dbName || "?") + "." + node.text;

        var editor = yui.find("sqlEditor");
        if (editor && dbName) {
            editor.text = "SELECT * FROM `" + dbName + "`.`" + node.text + "` LIMIT 100;";
        }
        if (activeTab >= 0 && activeTab < tabs.length) tabs[activeTab].sql = editor.text;
    }
}


// ====================== Treeview Events ======================

function onDbExpand(layerId) {
    if (!layerId) return;
    var layer = yui.find(layerId);
    if (!layer) return;
    var nodeText = layer.text;
    if (!nodeText) return;
    var node = JSON.parse(nodeText);

    // 同步展开状态到 fullDbData，避免重建时丢失折叠状态
    for (var i = 0; i < fullDbData.length; i++) {
        if (fullDbData[i].text === node.text) {
            fullDbData[i].expanded = node.expanded;
            break;
        }
        // 也检查分类节点
        var cats = fullDbData[i].children || [];
        for (var j = 0; j < cats.length; j++) {
            if (cats[j].text === node.text) {
                cats[j].expanded = node.expanded;
                break;
            }
        }
    }

    // 展开数据库节点时加载分类
    if (node.icon && node.icon.indexOf("db-db") >= 0 && node.expanded) {
        if (!node.children || node.children.length === 0) {
            loadCategories(node.text);
        }
    }
    // 展开分类节点时加载其子项
    if (node.expandable && node.expanded) {
        var dbName = node._db;
        if (!dbName) {
            for (var i = 0; i < fullDbData.length; i++) {
                var cats = fullDbData[i].children || [];
                for (var j = 0; j < cats.length; j++) {
                    if (cats[j].text === node.text) {
                        dbName = cats[j]._db || fullDbData[i].text;
                        break;
                    }
                }
                if (dbName) break;
            }
        }
        if (dbName) {
            var handlers = { "表": "mysql_db_tables", "视图": "mysql_db_views", "存储过程": "mysql_db_procedures", "函数": "mysql_db_functions" };
            var handler = handlers[node.text];
            if (handler) {
                loadCategoryItems(dbName, node.text, handler);
            }
        }
    }
}

// ====================== App Layout Config ======================

var APP_CONFIG_PATH = "app/db/db-config.json";
var EDITOR_SASH_HEIGHT = 6;
var _resizeSaveTimer = null;
var _pendingAppLayout = null;

function readAppConfig() {
    if (typeof YUI.readFile !== "function") return {};
    var raw = YUI.readFile(APP_CONFIG_PATH);
    if (!raw) return {};
    try {
        return JSON.parse(raw);
    } catch (e) {
        return {};
    }
}

function writeAppConfig(cfg) {
    if (typeof YUI.writeFile !== "function" || !cfg) return;
    YUI.writeFile(APP_CONFIG_PATH, JSON.stringify(cfg, null, 2));
}

function applyEditorSplit(editorHeight) {
    var editorContainer = yui.find("sqlEditorContainer");
    var resultContainer = yui.find("resultContainer");
    var editorPanel = yui.find("editorPanel");
    if (!editorContainer || !resultContainer) return;

    var panelH = editorPanel && editorPanel.size ? editorPanel.size[1] : 534;
    var resultHeight = panelH - EDITOR_SASH_HEIGHT - editorHeight;
    if (resultHeight < 20) return;

    // 只调整高度，宽度 0 表示跟随布局拉伸
    editorContainer.size = [0, editorHeight];
    resultContainer.size = [0, resultHeight];
}

function collectAppLayout(extra) {
    var cfg = readAppConfig();
    var root = yui.find("dbEditor");
    if (root && root.size) {
        cfg.windowWidth = root.size[0];
        cfg.windowHeight = root.size[1];
    }
    var editorContainer = yui.find("sqlEditorContainer");
    if (editorContainer && editorContainer.size) {
        cfg.sqlEditorContainer = editorContainer.size[1];
    }
    if (extra) {
        for (var key in extra) {
            if (extra.hasOwnProperty(key)) {
                cfg[key] = extra[key];
            }
        }
    }
    return cfg;
}

function saveAppLayout(extra) {
    writeAppConfig(collectAppLayout(extra));
}

function loadAppLayout() {
    _pendingAppLayout = readAppConfig();
}

function applyAppLayout(layerId) {
    var cfg = _pendingAppLayout || readAppConfig();
    if (!cfg) return;

    if (cfg.windowWidth > 0 && cfg.windowHeight > 0 && typeof YUI.resizeRoot === "function") {
        YUI.resizeRoot(cfg.windowWidth, cfg.windowHeight);
    }
    if (cfg.sqlEditorContainer > 0) {
        applyEditorSplit(cfg.sqlEditorContainer);
    }
}

function onEditorResultSashChange(layerId) {
    var sash = yui.find(layerId);
    if (!sash || !sash.text) return;
    try {
        var state = JSON.parse(sash.text);
        if (state.targetHeight > 0) {
            saveAppLayout({ sqlEditorContainer: state.targetHeight });
        }
    } catch (e) {}
}

function onWindowResize(layerId) {
    var cfg = readAppConfig();
    if (cfg.sqlEditorContainer > 0) {
        applyEditorSplit(cfg.sqlEditorContainer);
    }
    if (_resizeSaveTimer) clearTimeout(_resizeSaveTimer);
    _resizeSaveTimer = setTimeout(function() {
        _resizeSaveTimer = null;
        saveAppLayout();
    }, 300);
}

// ====================== Init ======================

function onLoad() {
  YUI.log('onLoad');
  loadDbConfig();
  loadAppLayout();
  if (appPreferences.autoConnect !== false) {
    connectDb();
  } else {
    updateStatus("未连接", "#F38BA8");
  }
}

// ====================== Init ======================
initTabs();
