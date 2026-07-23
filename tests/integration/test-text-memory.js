/**
 * YUI.setText 内存压力测试
 * 验证高频 setText 调用是否存在内存泄漏
 * 用法: playground tests/integration/test-text-memory.json
 * 观察: 任务管理器中 playground.exe 内存变化
 * 
 * 自动测试模式: 设置 AUTO_TEST=1 环境变量或在 JSON 中设置 "autoTest": true
 */

var g_autoTest = false;
var g_autoTestPassed = 0;
var g_autoTestFailed = 0;

var g_state = {
  running: false,
  timer: null,
  iteration: 0,
  maxIterations: 500,
  charCount: 0,
  maxChars: 10000,
  text: "",
  startTime: 0,
  interval: 20  // 20ms，模拟 SSE 流式输出
};

function setStatus(text) {
  YUI.setText("statusLabel", text);
}

function refreshStats() {
  var elapsed = Date.now() - g_state.startTime;
  var line = "迭代: " + g_state.iteration + "/" + g_state.maxIterations +
    " | 字符: " + g_state.charCount + "/" + g_state.maxChars +
    " | 耗时: " + elapsed + "ms";
  YUI.setText("statsLabel", line);
}

function generateChunk(iteration) {
  var phrases = [
    "正在分析图片构图：主体居中，背景简洁。",
    "光影关系良好，侧逆光增强立体感。",
    "色彩饱和度适中，建议微调暖色调。",
    "主题表达清晰，视觉焦点集中在人物。",
    "景深控制得当，背景虚化自然。"
  ];
  return "[" + iteration + "] " + phrases[iteration % phrases.length] + "\n";
}

function tick() {
  if (!g_state.running) {
    return;
  }

  if (g_state.iteration >= g_state.maxIterations || g_state.charCount >= g_state.maxChars) {
    stop();
    setStatus("测试完成，请观察任务管理器内存变化");
    return;
  }

  // 生成新文本块
  var chunk = generateChunk(g_state.iteration);
  g_state.text += chunk;
  g_state.charCount += chunk.length;
  g_state.iteration++;

  // 截断到最大字符数
  if (g_state.text.length > g_state.maxChars) {
    g_state.text = g_state.text.substring(0, g_state.maxChars);
  }

  // 高频 setText
  YUI.setText("streamText", g_state.text);

  refreshStats();
  g_state.timer = setTimeout(tick, g_state.interval);
}

function start() {
  if (g_state.running) return;

  g_state.running = true;
  g_state.iteration = 0;
  g_state.charCount = 0;
  g_state.text = "";
  g_state.startTime = Date.now();

  setStatus("运行中...每 " + g_state.interval + "ms setText 一次");
  tick();
}

function stop() {
  g_state.running = false;
  if (g_state.timer) {
    clearTimeout(g_state.timer);
    g_state.timer = null;
  }
  refreshStats();
  setStatus("已停止");
}

function reset() {
  stop();
  g_state.text = "";
  g_state.iteration = 0;
  g_state.charCount = 0;
  YUI.setText("streamText", "");
  YUI.setText("statsLabel", "就绪");
  setStatus("已重置");
}

// ============================================
// 内存稳定性测试（脏检查优化验证）
// ============================================

var g_dirtyCheck = {
  running: false,
  timer: null,
  count: 0,
  maxCount: 100,
  lastText: "",
  sameTextCount: 0
};

function startDirtyCheckTest() {
  if (g_dirtyCheck.running) return;

  g_dirtyCheck.running = true;
  g_dirtyCheck.count = 0;
  g_dirtyCheck.sameTextCount = 0;

  setStatus("脏检查测试: 相同文本不应触发原生更新");

  function tick() {
    if (!g_dirtyCheck.running || g_dirtyCheck.count >= g_dirtyCheck.maxCount) {
      g_dirtyCheck.running = false;
      setStatus("脏检查测试完成");
      return;
    }

    // 每次设置相同的文本（验证脏检查是否生效）
    var text = "重复文本测试 - " + Math.floor(g_dirtyCheck.count / 10);
    YUI.setText("streamText", text);

    if (text === g_dirtyCheck.lastText) {
      g_dirtyCheck.sameTextCount++;
    }
    g_dirtyCheck.lastText = text;
    g_dirtyCheck.count++;

    YUI.setText("statsLabel", "脏检查: " + g_dirtyCheck.count + "/" + g_dirtyCheck.maxCount);
    g_dirtyCheck.timer = setTimeout(tick, 20);
  }

  tick();
}

function stopDirtyCheckTest() {
  g_dirtyCheck.running = false;
  if (g_dirtyCheck.timer) {
    clearTimeout(g_dirtyCheck.timer);
    g_dirtyCheck.timer = null;
  }
}

// ============================================
// 增量刷新测试
// ============================================

var g_batched = {
  running: false,
  timer: null,
  pending: "",
  text: "",
  chars: 0,
  maxChars: 10000,
  lastRender: 0,
  interval: 20,
  batchInterval: 100  // 100ms 批量刷新
};

function startBatchedTest() {
  if (g_batched.running) return;

  g_batched.running = true;
  g_batched.pending = "";
  g_batched.text = "";
  g_batched.chars = 0;
  g_batched.lastRender = Date.now();

  setStatus("批量测试: 每 " + g_batched.batchInterval + "ms 刷新一次");

  function tick() {
    if (!g_batched.running) return;

    // 生成字符
    var chunk = "[" + g_batched.chars + "] 测试文本块 ";
    g_batched.pending += chunk;
    g_batched.chars += chunk.length;

    // 批量刷新
    var now = Date.now();
    if (now - g_batched.lastRender >= g_batched.batchInterval) {
      g_batched.text += g_batched.pending;
      if (g_batched.text.length > g_batched.maxChars) {
        g_batched.text = g_batched.text.substring(0, g_batched.maxChars);
      }
      YUI.setText("streamText", g_batched.text);
      g_batched.pending = "";
      g_batched.lastRender = now;

      YUI.setText("statsLabel", "批量: " + g_batched.chars + "/" + g_batched.maxChars);
    }

    if (g_batched.chars >= g_batched.maxChars) {
      g_batched.running = false;
      setStatus("批量测试完成");
      return;
    }

    setTimeout(tick, g_batched.interval);
  }

  tick();
}

function onLoad() {
  YUI.log("Text Memory Test loaded");

  // 自动测试模式：在 JSON 中设置 "autoTest": true
  // 直接运行测试并退出
  YUI.log("Running automated tests...");
  runAutoTests();
}

// ============================================
// 自动测试模式
// ============================================

function autoTest(name, fn) {
  try {
    fn();
    g_autoTestPassed++;
    YUI.log("[PASS] " + name);
  } catch (e) {
    g_autoTestFailed++;
    YUI.log("[FAIL] " + name + ": " + e);
  }
}

function runAutoTests() {
  YUI.log("=== Auto Test: setText Memory ===");

  // 测试 1: 基本 setText/getText
  autoTest("setText/getText basic", function() {
    YUI.setText("streamText", "hello");
    var text = YUI.getText("streamText");
    if (text !== "hello") {
      throw "expected 'hello' but got '" + text + "'";
    }
  });

  // 测试 2: 高频 setText（100次）
  autoTest("high frequency setText (100 iterations)", function() {
    var text = "";
    for (var i = 0; i < 100; i++) {
      text += "Line " + i + "\n";
      YUI.setText("streamText", text);
    }
    // 验证最终文本正确
    var finalText = YUI.getText("streamText");
    if (finalText.indexOf("Line 99") < 0) {
      throw "final text missing 'Line 99'";
    }
  });

  // 测试 3: 空 setText
  autoTest("setText empty string", function() {
    YUI.setText("streamText", "");
    var text = YUI.getText("streamText");
    if (text !== "") {
      throw "expected empty string but got '" + text + "'";
    }
  });

  // 测试 4: 脏检查（相同文本不应泄漏）
  autoTest("dirty check - same text repeated", function() {
    var sameText = "same text repeated";
    for (var i = 0; i < 50; i++) {
      YUI.setText("streamText", sameText);
    }
    // 不应崩溃
    if (YUI.getText("streamText") !== sameText) {
      throw "text mismatch after repeated setText";
    }
  });

  // 测试 5: 长文本
  autoTest("long text (5000 chars)", function() {
    var longText = "";
    for (var i = 0; i < 500; i++) {
      longText += "ABCDEFGHIJ";
    }
    YUI.setText("streamText", longText);
    var result = YUI.getText("streamText");
    if (result.length !== 5000) {
      throw "expected 5000 chars but got " + result.length;
    }
  });

  // 汇总并退出
  YUI.log("=== Auto Test Complete ===");
  YUI.log("Passed: " + g_autoTestPassed + ", Failed: " + g_autoTestFailed);
  YUI.log("YTEST_RESULT passed=" + g_autoTestPassed + " failed=" + g_autoTestFailed);

  setTimeout(function() {
    YUI.exit(g_autoTestFailed > 0 ? 1 : 0);
  }, 100);
}