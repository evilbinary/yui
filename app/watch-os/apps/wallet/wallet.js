/**
 * 钱包 App
 */

function onWalletLoad() {
    YUI.setText("wallet_status", "双击侧边按钮唤起钱包");
}

function onWalletShow() {
    applyWatchTheme();
}

function walletPay() {
    YUI.setText("wallet_status", "请将手表靠近读卡器…");
}
