/**
 * 通讯录 App
 */

function onContactsLoad() {
    refreshContactsUI();
}

function onContactsShow() {
    applyWatchTheme();
    refreshContactsUI();
}

function refreshContactsUI() {
    YUI.setText("contacts_count", Watch.contacts.length + " 位联系人");
    for (var i = 0; i < 8; i++) {
        if (i < Watch.contacts.length) {
            var c = Watch.contacts[i];
            YUI.setText("contact_" + i + "_emoji", c.emoji);
            YUI.setText("contact_" + i + "_name", c.name);
        }
    }
}

function dialContactByIndex(index) {
    var c = Watch.contacts[index];
    if (!c) return;
    YUI.setText("contacts_status", "正在呼叫 " + c.name + "…");
}

function callContact0() { dialContactByIndex(0); }
function callContact1() { dialContactByIndex(1); }
function callContact2() { dialContactByIndex(2); }
function callContact3() { dialContactByIndex(3); }
function callContact4() { dialContactByIndex(4); }
function callContact5() { dialContactByIndex(5); }
function callContact6() { dialContactByIndex(6); }
function callContact7() { dialContactByIndex(7); }
