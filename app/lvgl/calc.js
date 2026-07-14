/**
 * LVGL Calculator — LvLabel display + LvBtnMatrix keypad
 */

var calcState = {
    currentValue: "0",
    previousValue: "",
    operation: "",
    expression: "",
    memoryValue: 0,
    resetOnNextInput: false
};

var opSymbols = {
    add: "+",
    subtract: "-",
    multiply: "x",
    divide: "/"
};

var symbolToOp = {
    "+": "add",
    "-": "subtract",
    "x": "multiply",
    "/": "divide"
};

function initCalculator() {
    YUI.log("LVGL calculator ready");
    // YUI.inspect.enable();
    // YUI.inspect.setShowBounds(true);
    // YUI.inspect.setShowInfo(true);
    updateDisplay();
}

function updateDisplay() {
    var displayValue = calcState.currentValue;

    if (displayValue.length > 12) {
        var num = parseFloat(displayValue);
        if (!isNaN(num)) {
            displayValue = num.toExponential(6);
        }
    }

    YUI.setText("calc_expression", calcState.expression);
    YUI.setText("calc_result", displayValue);
}

function appendNumber(num) {
    if (calcState.resetOnNextInput) {
        calcState.currentValue = num;
        calcState.resetOnNextInput = false;
    } else if (calcState.currentValue === "0" && num !== "0") {
        calcState.currentValue = num;
    } else if (calcState.currentValue !== "0") {
        calcState.currentValue += num;
    }
    updateDisplay();
}

function appendDecimal() {
    if (calcState.resetOnNextInput) {
        calcState.currentValue = "0.";
        calcState.resetOnNextInput = false;
    } else if (calcState.currentValue.indexOf(".") === -1) {
        calcState.currentValue += ".";
    }
    updateDisplay();
}

function clearDisplay() {
    calcState.currentValue = "0";
    calcState.previousValue = "";
    calcState.operation = "";
    calcState.expression = "";
    calcState.resetOnNextInput = false;
    updateDisplay();
}

function backspace() {
    if (calcState.resetOnNextInput) {
        calcState.currentValue = "0";
        calcState.resetOnNextInput = false;
    } else if (calcState.currentValue.length > 1) {
        calcState.currentValue = calcState.currentValue.substring(0, calcState.currentValue.length - 1);
    } else {
        calcState.currentValue = "0";
    }
    updateDisplay();
}

function performCalculation() {
    var prev = parseFloat(calcState.previousValue);
    var curr = parseFloat(calcState.currentValue);
    var result = 0;

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

    result = Math.round(result * 1000000000000) / 1000000000000;
    calcState.currentValue = String(result);
    calcState.expression = "";
    calcState.operation = "";
}

function setOperation(op) {
    if (calcState.operation !== "" && !calcState.resetOnNextInput) {
        performCalculation();
    }

    calcState.previousValue = calcState.currentValue;
    calcState.operation = op;
    calcState.expression = calcState.previousValue + " " + opSymbols[op];
    calcState.resetOnNextInput = true;
    updateDisplay();
}

function calculate() {
    if (calcState.operation === "") {
        return;
    }

    calcState.expression = calcState.previousValue + " " +
        opSymbols[calcState.operation] + " " + calcState.currentValue + " =";
    performCalculation();
    calcState.resetOnNextInput = true;
    updateDisplay();
}

function percent() {
    var value = parseFloat(calcState.currentValue) / 100;
    value = Math.round(value * 1000000000000) / 1000000000000;
    calcState.currentValue = String(value);
    calcState.resetOnNextInput = true;
    updateDisplay();
}

function toggleSign() {
    if (calcState.currentValue !== "0" && calcState.currentValue !== "Error") {
        if (calcState.currentValue.charAt(0) === "-") {
            calcState.currentValue = calcState.currentValue.substring(1);
        } else {
            calcState.currentValue = "-" + calcState.currentValue;
        }
        updateDisplay();
    }
}

function memoryClear() {
    calcState.memoryValue = 0;
}

function memoryRecall() {
    calcState.currentValue = String(calcState.memoryValue);
    calcState.resetOnNextInput = true;
    updateDisplay();
}

function memoryAdd() {
    var value = parseFloat(calcState.currentValue);
    if (!isNaN(value)) {
        calcState.memoryValue += value;
    }
}

function memorySubtract() {
    var value = parseFloat(calcState.currentValue);
    if (!isNaN(value)) {
        calcState.memoryValue -= value;
    }
}

function handleKey(key) {
    if (key >= "0" && key <= "9") {
        appendNumber(key);
        return;
    }

    switch (key) {
        case "C":
            clearDisplay();
            break;
        case "<-":
            backspace();
            break;
        case "%":
            percent();
            break;
        case "+":
        case "-":
        case "x":
        case "/":
            setOperation(symbolToOp[key]);
            break;
        case ".":
            appendDecimal();
            break;
        case "=":
            calculate();
            break;
        case "+/-":
            toggleSign();
            break;
        default:
            YUI.log("unknown key: " + key);
    }
}

function onKeypadChange() {
    var key = YUI.getText("calc_keypad");
    if (!key || key === "\n") {
        return;
    }
    handleKey(key);
}
