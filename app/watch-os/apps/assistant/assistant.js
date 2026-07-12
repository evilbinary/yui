function onAssistantLoad() {}

function onAssistantShow() {
    applyWatchTheme();
}

function askAiWeather() {
    var w = Watch.complications.weather;
    YUI.setText("ai_msg_user", "你：今天天气怎么样？");
    YUI.setText("ai_msg_bot", "YUI：" + w.temp + "°C，" + w.cond + "。");
}

function askAiSteps() {
    YUI.setText("ai_msg_user", "你：今天走了多少步？");
    YUI.setText("ai_msg_bot", "YUI：" + formatWatchNumber(Watch.complications.steps.value) + " 步，目标 " + formatWatchNumber(Watch.complications.steps.goal) + "。");
}

function askAiSchedule() {
    YUI.setText("ai_msg_user", "你：今天有什么安排？");
    YUI.setText("ai_msg_bot", "YUI：15:00 项目例会，22:00 就寝闹钟。");
}
