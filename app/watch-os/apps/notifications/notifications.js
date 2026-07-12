/**
 * 通知中心 App
 */

function onNotificationsLoad() {
    refreshNotificationsUI();
}

function onNotificationsShow() {
    applyWatchTheme();
    refreshNotificationsUI();
}

function refreshNotificationsUI() {
    var list = Watch.notifications;
    var unread = getUnreadNotificationCount();

    YUI.setText("notif_summary", unread > 0 ? unread + " 条未读" : "没有未读通知");

    for (var i = 0; i < 4; i++) {
        if (i < list.length) {
            var n = list[i];
            var prefix = n.unread ? "● " : "";
            YUI.setText("notif_" + i + "_header", prefix + n.from + " · " + n.app);
            YUI.setText("notif_" + i + "_body", n.text);
            YUI.setText("notif_" + i + "_time", n.time);
        }
    }
    updateWatchChrome();
}

function clearAllNotifications() {
    for (var i = 0; i < Watch.notifications.length; i++) {
        Watch.notifications[i].unread = false;
    }
    refreshNotificationsUI();
}
