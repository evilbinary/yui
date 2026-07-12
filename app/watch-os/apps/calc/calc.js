/**
 * 计算器 App（Watch 入口）
 */

function onCalcLoad() {
    if (typeof initCalculator === "function") {
        initCalculator();
    }
}

function onCalcShow() {
    if (typeof initCalculator === "function") {
        initCalculator();
    }
    applyWatchTheme();
}
