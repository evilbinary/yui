// DB Editor - Modern UI Theme (Catppuccin Mocha)

var currentDb = null;

function connectDb() {
  print("Connecting to database...");
  
  var statusText = yui.find("statusText");
  var statusIcon = yui.find("statusIcon");
  var dbInfo = yui.find("dbInfo");
  
  if (statusText) {
    statusText.text = "已连接";
    statusText.style = { color: "#A6E3A1" };
  }
  if (statusIcon) {
    statusIcon.text = "●";
    statusIcon.style = { color: "#A6E3A1" };
  }
  if (dbInfo) {
    dbInfo.text = "my_database";
  }
  
  loadDbList();
}

function loadDbList() {
  var tree = yui.find("dbTree");
  if (!tree) return;
  
  tree.data = [
    { 
      id: "db1", 
      text: "my_database", 
      icon: "database",
      expanded: true, 
      children: [
        { id: "t1", text: "users", icon: "table" },
        { id: "t2", text: "orders", icon: "table" },
        { id: "t3", text: "products", icon: "table" },
        { id: "v1", text: "user_view", icon: "view" }
      ] 
    },
    { 
      id: "db2", 
      text: "test_db", 
      icon: "database",
      children: [
        { id: "t4", text: "logs", icon: "table" },
        { id: "t5", text: "events", icon: "table" }
      ]
    },
    { 
      id: "db3", 
      text: "production", 
      icon: "database",
      children: [
        { id: "t6", text: "customers", icon: "table" }
      ]
    }
  ];
}

function executeQuery() {
  var editor = yui.find("sqlEditor");
  var sql = editor ? editor.text : "";
  
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
  if (resultInfo) {
    resultInfo.text = "(" + mockData.length + " 行)";
  }
  
  updateStatus("查询完成", "#A6E3A1");
}

function showResults(rows) {
  var grid = yui.find("resultGrid");
  if (!grid) return;
  grid.data = rows;
}

function clearEditor() {
  var editor = yui.find("sqlEditor");
  if (editor) {
    editor.text = "";
  }
  var resultGrid = yui.find("resultGrid");
  if (resultGrid) {
    resultGrid.data = [];
  }
  var resultInfo = yui.find("resultInfo");
  if (resultInfo) {
    resultInfo.text = "(0 行)";
  }
  updateStatus("就绪", "#F38BA8");
}

function updateStatus(text, color) {
  var statusText = yui.find("statusText");
  if (statusText) {
    statusText.text = text;
    statusText.style = { color: color ? color : "#CDD6F4" };
  }
  if (color) {
    var statusIcon = yui.find("statusIcon");
    if (statusIcon) {
      statusIcon.style = { color: color };
    }
  }
}

function onSqlClick() {
  print("SQL editor clicked");
}

function onSqlChange(text) {
  print("SQL changed: " + text);
}

function onDbSelect(node) {
  if (!node) return;
  
  currentDb = node;
  
  var statusText = yui.find("statusText");
  if (statusText) {
    statusText.text = "选中: " + node.text;
  }
  
  if (node.icon === "table") {
    var editor = yui.find("sqlEditor");
    if (editor) {
      editor.text = "SELECT * FROM " + node.text + " LIMIT 100;";
    }
  }
}

// 初始化 - 在脚本加载时自动执行
updateStatus("未连接", "#F38BA8");
loadDbList();
