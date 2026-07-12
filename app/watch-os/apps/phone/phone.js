/**
 * 电话 App
 */

var phoneFavorites = ["妈妈", "爸爸", "姐姐", "弟弟"];

function onPhoneLoad() {
    YUI.setText("phone_status", "点击头像快速拨号");
}

function onPhoneShow() {
    applyWatchTheme();
}

function dialContact(name) {
    YUI.setText("phone_status", "正在呼叫 " + name + "…");
}

function callFav0() { dialContact(phoneFavorites[0]); }
function callFav1() { dialContact(phoneFavorites[1]); }
function callFav2() { dialContact(phoneFavorites[2]); }
function callFav3() { dialContact(phoneFavorites[3]); }
