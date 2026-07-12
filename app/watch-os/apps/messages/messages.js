/**
 * 信息 App
 */

var activeMessageIndex = 0;

function onMessagesLoad() {
    refreshMessagesUI();
}

function onMessagesShow() {
    applyWatchTheme();
    refreshMessagesUI();
}

function refreshMessagesUI() {
    var list = Watch.messages;
    for (var i = 0; i < 4; i++) {
        if (i < list.length) {
            var m = list[i];
            YUI.setText("msg_" + i + "_name", m.unread > 0 ? m.name + " (" + m.unread + ")" : m.name);
            YUI.setText("msg_" + i + "_preview", m.preview);
            YUI.setText("msg_" + i + "_time", m.time);
        }
    }
    showMessageThread(activeMessageIndex);
}

function showMessageThread(index) {
    activeMessageIndex = index;
    var m = Watch.messages[index];
    if (!m) return;

    YUI.setText("msg_thread_title", m.name);
    YUI.setText("msg_thread_body", m.preview);
    YUI.setText("msg_thread_reply", m.reply ? "我：" + m.reply : "点击回复发送快捷消息");
    YUI.setText("msg_status", "与 " + m.name + " 的对话");
}

function openMessage0() { showMessageThread(0); }
function openMessage1() { showMessageThread(1); }

function replyMessage() {
    var m = Watch.messages[activeMessageIndex];
    if (!m) return;
    m.reply = "好的 👌";
    m.unread = 0;
    showMessageThread(activeMessageIndex);
    YUI.setText("msg_status", "已发送给 " + m.name);
}
