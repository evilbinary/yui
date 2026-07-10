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
            var bg = i === activeTab ? "#1E1E2E" : "transparent";
            var color = i === activeTab ? "#CDD6F4" : "#6C7086";
            el.visible = true;
            el.size = [70, 24];
            el.text = tabs[i].name;
            el.style = { color: color, bgColor: bg, fontSize: 11, padding: [4, 10, 4, 10], borderRadius: 4 };
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
    if (itemText === "新建查询" || itemText === "打开文件" || itemText === "保存文件" || itemText === "退出") {
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
        case "主题设置": onThemeSettings(); break;
        case "关于": onAbout(); break;
    }
}

function onNewConnection() {
    // 打开连接配置面板 — 先用默认值直接连接
    connectDb();
}

function onExit() {
    updateStatus("退出应用", "#F38BA8");
}

function onImportDb() {
    updateStatus("导入功能开发中", "#F9E2AF");
}

function onOpenSettings() {
    updateStatus("设置功能开发中", "#F9E2AF");
}

function onThemeSettings() {
    updateStatus("主题设置开发中", "#F9E2AF");
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
    var treeData = rowsToTreeData(rows);
    showResults(treeData, rows.length);
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

var fileDialogMode = "open";  // "open" or "save"
var fileDialogPath = "./";
var fileDialogSelected = "";

function showFileDialog(mode) {
    fileDialogMode = mode;
    fileDialogSelected = "";
    var title = yui.find("fileDialogTitle");
    if (title) title.text = mode === "save" ? "保存文件" : "打开文件";
    var input = yui.find("fileDialogInput");
    if (input) input.text = "";
    var overlay = yui.find("fileDialogOverlay");
    if (overlay) overlay.visible = true;
    refreshFileList(fileDialogPath || "./");
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

function onSearchDb(text) {
    applyDbFilter(text ? text : "");
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

// 将 SQL 查询结果行转换为 TreeView 兼容的格式
function rowsToTreeData(rows) {
    if (!rows || rows.length === 0) return [];
    var cols = Object.keys(rows[0]);
    // 构造表头
    var header = cols.map(function(c) { return c; }).join(" | ");
    var treeData = [];
    // 添加行数据，每行显示为文本
    for (var i = 0; i < rows.length; i++) {
        var values = [];
        for (var j = 0; j < cols.length; j++) {
            var v = rows[i][cols[j]];
            values.push(v !== null && v !== undefined ? String(v) : "NULL");
        }
        treeData.push({ text: values.join(" | ") });
    }
    return treeData;
}

function showResults(treeData, rowCount) {
    var grid = yui.find("resultGrid");
    if (grid) grid.data = treeData;
    var resultInfo = yui.find("resultInfo");
    if (resultInfo) resultInfo.text = "(" + (rowCount || 0) + " 行)";
}

function clearEditor() {
    var editor = yui.find("sqlEditor");
    if (editor) editor.text = "";
    if (activeTab >= 0 && activeTab < tabs.length) tabs[activeTab].sql = "";
    var resultGrid = yui.find("resultGrid");
    if (resultGrid) resultGrid.data = [];
    var resultInfo = yui.find("resultInfo");
    if (resultInfo) resultInfo.text = "(0 行)";
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
    YUI.log('onDbSelect');

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

// ====================== Init ======================

function onLoad() {
  YUI.log('onLoad');
  connectDb();
  //loadDatabases();
}

// ====================== Init ======================
initTabs();
updateStatus("未连接", "#F38BA8");
