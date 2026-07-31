function onRingDebugLoad() {
    setTimeout(function () {
        YUI.screenshot("ring-debug.png");
        setTimeout(function () {
            YUI.exit(0);
        }, 200);
    }, 3000);
}
