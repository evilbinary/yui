// DB Editor - Modern UI Theme (Catppuccin Mocha)

var MAX_TABS = 5;
var currentDb = null;
var tabs = [];
var activeTab = 0;
var tabCounter = 1;


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
      el.text = tabs[i].name;
      el.style = { color: color, bgColor: bg, fontSize: 11, padding: [4, 10, 4, 10], borderRadius: 4 };
    } else {
      el.visible = false;
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
  if (activeTab >= 0 && activeTab < tabs.length) {
    tabs[activeTab].sql = text;
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

// 文件菜单点击处理
function onFileMenuClick(itemText) {
  switch (itemText) {
    case "新建查询": onNewTab(); break;
    case "打开文件": onOpen(); break;
    case "保存文件": onSave(); break;
    case "退出": onExit(); break;
  }
}

// 数据库菜单点击处理
function onDbMenuClick(itemText) {
  switch (itemText) {
    case "新建连接": onNewConnection(); break;
    case "连接数据库": connectDb(); break;
    case "断开连接": disconnectDb(); break;
    case "导入数据库": onImportDb(); break;
  }
}

// 设置菜单点击处理
function onSettingsMenuClick(itemText) {
  switch (itemText) {
    case "偏好设置": onOpenSettings(); break;
    case "主题设置": onThemeSettings(); break;
    case "关于": onAbout(); break;
  }
}

function onNewConnection() {
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
  updateStatus("YUI DB Editor v0.1", "#89B4FA");
}


// ====================== Toolbar Actions ======================

function executeQuery() {
  var sql = "";
  if (activeTab >= 0 && activeTab < tabs.length) {
    sql = tabs[activeTab].sql;
  }
  if (!sql || sql.trim() === "") {
    updateStatus("请输入 SQL 语句", "#F38BA8");
    return;
  }
  updateStatus("执行中...", "#F9E2AF");
  var mockData = [
    { id: 1, name: "Alice", email: "alice@example.com", created_at: "2024-01-15" },
    { id: 2, name: "Bob", email: "bob@example.com", created_at: "2024-01-16" },
    { id: 3, name: "Charlie", email: "charlie@example.com", created_at: "2024-01-17" },
    { id: 4, name: "David", email: "david@example.com", created_at: "2024-01-18" },
    { id: 5, name: "Eve", email: "eve@example.com", created_at: "2024-01-19" }
  ];
  showResults(mockData);
  var resultInfo = yui.find("resultInfo");
  if (resultInfo) resultInfo.text = "(" + mockData.length + " 行)";
  updateStatus("查询完成", "#A6E3A1");
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
  updateStatus("格式化完成", "#89B4FA");
}

function onSave() {
  updateStatus("保存: 未实现", "#F9E2AF");
}

function onOpen() {
  updateStatus("打开: 未实现", "#F9E2AF");
}


// ====================== Database Connection ======================

function connectDb() {
  print("Connecting to database...");
  var statusText = yui.find("statusText");
  var statusIcon = yui.find("statusIcon");
  if (statusText) {
    statusText.text = "已连接";
    statusText.style = { color: "#A6E3A1" };
  }
  if (statusIcon) {
    statusIcon.text = "●";
    statusIcon.style = { color: "#A6E3A1" };
  }
  loadDbList();
}

function disconnectDb() {
  var btn = yui.find("btnConnect");
  if (btn) btn.text = "连接";
  var statusText = yui.find("statusText");
  if (statusText) { statusText.text = "未连接"; statusText.style = { color: "#F38BA8" }; }
  var statusIcon = yui.find("statusIcon");
  if (statusIcon) { statusIcon.text = "●"; statusIcon.style = { color: "#F38BA8" }; }
  updateStatus("已断开连接", "#F38BA8");
  currentDb = null;
}


// ====================== Database List ======================

var fullDbData = [];

function loadDbList() {
  fullDbData = [
    { id: "db1", text: "my_database", icon: "database", expanded: true,
      children: [
        { id: "t1", text: "users", icon: "table" },
        { id: "t2", text: "orders", icon: "table" },
        { id: "t3", text: "products", icon: "table" },
        { id: "v1", text: "user_view", icon: "view" }
      ]
    },
    { id: "db2", text: "test_db", icon: "database",
      children: [
        { id: "t4", text: "logs", icon: "table" },
        { id: "t5", text: "events", icon: "table" }
      ]
    },
    { id: "db3", text: "production", icon: "database",
      children: [
        { id: "t6", text: "customers", icon: "table" }
      ]
    }
  ];
  applyDbFilter("");
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
          id: n.id, text: n.text, icon: n.icon,
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

function showResults(rows) {
  var grid = yui.find("resultGrid");
  if (!grid) return;
  grid.data = rows;
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

function onDbSelect(node) {
  if (!node) return;
  currentDb = node;
  var statusText = yui.find("statusText");
  if (statusText) statusText.text = "选中: " + node.text;
  if (node.icon === "table") {
    var editor = yui.find("sqlEditor");
    if (editor) editor.text = "SELECT * FROM " + node.text + " LIMIT 100;";
    if (activeTab >= 0 && activeTab < tabs.length) tabs[activeTab].sql = editor.text;
  }
}


// ====================== Init ======================

initTabs();
updateStatus("未连接", "#F38BA8");
loadDbList();
