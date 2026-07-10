function onLoginLoad() {}

function onLoginShow() {
    var target = Shop.redirectAfterLogin || "/";
    var targetLabel = target === "/cart" ? "购物袋" : "首页";
    YUI.setText("login_redirect", "登录后将跳转到：" + targetLabel);
    updateStatusBar();
}

function loginAsUser() {
    Shop.login("小明", "user");
    loginFinish();
}

function loginAsAdmin() {
    Shop.login("小红", "admin");
    loginFinish();
}

function loginCancel() {
    Shop.redirectAfterLogin = null;
    if (YUI.canBack()) {
        YUI.back();
    } else {
        goHome();
    }
    updateStatusBar();
}

function loginFinish() {
    updateAuthButton();
    var target = Shop.redirectAfterLogin || "/";
    Shop.redirectAfterLogin = null;
    YUI.navigate(target);
    updateStatusBar();
}
