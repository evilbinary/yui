// Calculator Application JavaScript Logic
// 计算器应用 JavaScript 逻辑

// 计算器状态
var calcState = {
    currentValue: "0",
    previousValue: "",
    operation: "",
    expression: "",
    memoryValue: 0,
    resetOnNextInput: false
};

// 初始化计算器
function initCalculator() {
    YUI.log("initCalculator: Initializing calculator...");
    updateDisplay();
}

// 更新显示
function updateDisplay() {
    // 更新表达式显示
    YUI.setText("expressionDisplay", calcState.expression);
    
    // 更新结果显示
    var displayValue = calcState.currentValue;
    
    // 限制显示长度
    if (displayValue.length > 12) {
        var num = parseFloat(displayValue);
        if (!isNaN(num)) {
            displayValue = num.toExponential(6);
        }
    }
    
    YUI.setText("resultDisplay", displayValue);
}

// 追加数字
function appendNumber(layerId) {
    // 从按钮的 text 属性获取数字
    var num = YUI.getText(layerId);
    YUI.log("appendNumber: " + num + " from " + layerId);
    
    // 验证是否为数字
    if (num < "0" || num > "9") {
        YUI.log("appendNumber: invalid number, skipping");
        return;
    }
    
    if (calcState.resetOnNextInput) {
        calcState.currentValue = num;
        calcState.resetOnNextInput = false;
    } else {
        // 防止以 0 开头的多位数
        if (calcState.currentValue === "0" && num !== "0") {
            calcState.currentValue = num;
        } else if (calcState.currentValue !== "0") {
            calcState.currentValue += num;
        }
    }
    
    updateDisplay();
}

// 追加小数点
function appendDecimal() {
    YUI.log("appendDecimal");
    
    if (calcState.resetOnNextInput) {
        calcState.currentValue = "0.";
        calcState.resetOnNextInput = false;
    } else {
        // 检查是否已有小数点
        if (calcState.currentValue.indexOf(".") === -1) {
            calcState.currentValue += ".";
        }
    }
    
    updateDisplay();
}

// 清除显示
function clearDisplay() {
    YUI.log("clearDisplay");
    
    calcState.currentValue = "0";
    calcState.previousValue = "";
    calcState.operation = "";
    calcState.expression = "";
    calcState.resetOnNextInput = false;
    
    updateDisplay();
}

// 退格
function backspace() {
    YUI.log("backspace");
    
    if (calcState.resetOnNextInput) {
        calcState.currentValue = "0";
        calcState.resetOnNextInput = false;
    } else {
        var len = calcState.currentValue.length;
        if (len > 1) {
            calcState.currentValue = calcState.currentValue.substring(0, len - 1);
        } else {
            calcState.currentValue = "0";
        }
    }
    
    updateDisplay();
}

// 操作符映射（从符号到操作名）
var symbolToOp = {
    "+": "add",
    "-": "subtract",
    "×": "multiply",
    "÷": "divide"
};

// 操作符处理
function operation(layerId) {
    // 从按钮的 text 属性获取操作符符号
    var symbol = YUI.getText(layerId);
    var op = symbolToOp[symbol] || symbol;
    YUI.log("operation: " + op + " (symbol: " + symbol + ")");
    
    // 如果有待计算的操作，先计算
    if (calcState.operation !== "" && !calcState.resetOnNextInput) {
        performCalculation();
    }
    
    calcState.previousValue = calcState.currentValue;
    calcState.operation = op;
    calcState.expression = calcState.previousValue + " " + symbol;
    calcState.resetOnNextInput = true;
    
    updateDisplay();
}

// 执行计算
function performCalculation() {
    var prev = parseFloat(calcState.previousValue);
    var curr = parseFloat(calcState.currentValue);
    var result = 0;
    
    YUI.log("performCalculation: " + prev + " " + calcState.operation + " " + curr);
    
    switch (calcState.operation) {
        case "add":
            result = prev + curr;
            break;
        case "subtract":
            result = prev - curr;
            break;
        case "multiply":
            result = prev * curr;
            break;
        case "divide":
            if (curr === 0) {
                calcState.currentValue = "Error";
                calcState.expression = "";
                calcState.operation = "";
                calcState.resetOnNextInput = true;
                updateDisplay();
                return;
            }
            result = prev / curr;
            break;
        default:
            return;
    }
    
    // 处理浮点数精度问题
    result = Math.round(result * 1000000000000) / 1000000000000;
    
    calcState.currentValue = String(result);
    calcState.expression = "";
    calcState.operation = "";
}

// 计算结果
function calculate() {
    YUI.log("calculate");
    
    if (calcState.operation !== "") {
        // 根据操作类型获取符号
        var opSymbol = "";
        for (var key in symbolToOp) {
            if (symbolToOp[key] === calcState.operation) {
                opSymbol = key;
                break;
            }
        }
        
        calcState.expression = calcState.previousValue + " " + opSymbol + " " + calcState.currentValue + " =";
        performCalculation();
        calcState.resetOnNextInput = true;
        updateDisplay();
    }
}

// 百分比
function percent() {
    YUI.log("percent");
    
    var value = parseFloat(calcState.currentValue);
    value = value / 100;
    
    // 处理浮点数精度
    value = Math.round(value * 1000000000000) / 1000000000000;
    
    calcState.currentValue = String(value);
    calcState.resetOnNextInput = true;
    
    updateDisplay();
}

// 切换正负号
function toggleSign() {
    YUI.log("toggleSign");
    
    if (calcState.currentValue !== "0") {
        if (calcState.currentValue.charAt(0) === "-") {
            calcState.currentValue = calcState.currentValue.substring(1);
        } else {
            calcState.currentValue = "-" + calcState.currentValue;
        }
    }
    
    updateDisplay();
}

// 清除内存
function memoryClear() {
    YUI.log("memoryClear");
    calcState.memoryValue = 0;
}

// 读取内存
function memoryRecall() {
    YUI.log("memoryRecall");
    calcState.currentValue = String(calcState.memoryValue);
    calcState.resetOnNextInput = true;
    updateDisplay();
}

// 加到内存
function memoryAdd() {
    YUI.log("memoryAdd");
    var value = parseFloat(calcState.currentValue);
    if (!isNaN(value)) {
        calcState.memoryValue += value;
    }
}

// 从内存减
function memorySubtract() {
    YUI.log("memorySubtract");
    var value = parseFloat(calcState.currentValue);
    if (!isNaN(value)) {
        calcState.memoryValue -= value;
    }
}

// 存储到内存
function memoryStore() {
    YUI.log("memoryStore");
    var value = parseFloat(calcState.currentValue);
    if (!isNaN(value)) {
        calcState.memoryValue = value;
    }
}
