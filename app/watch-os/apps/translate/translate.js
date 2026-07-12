var translateLang = "zh-en";

var translateSamples = {
    "zh-en": { source: "你好，很高兴认识你", target: "Hello, nice to meet you" },
    "en-zh": { source: "Hello, nice to meet you", target: "你好，很高兴认识你" }
};

function onTranslateLoad() {
    refreshTranslateUI();
}

function onTranslateShow() {
    applyWatchTheme();
    refreshTranslateUI();
}

function refreshTranslateUI() {
    var sample = translateSamples[translateLang];
    YUI.setText("tr_source", sample.source);
    YUI.setText("tr_target", sample.target);
    YUI.setText("tr_lang", translateLang === "zh-en" ? "中文 ⇄ English" : "English ⇄ 中文");
}

function swapTranslateLang() {
    translateLang = translateLang === "zh-en" ? "en-zh" : "zh-en";
    refreshTranslateUI();
}

function listenTranslate() {
    YUI.setText("tr_status", "聆听中…");
    setTimeout(function() {
        refreshTranslateUI();
        YUI.setText("tr_status", "翻译完成");
    }, 800);
}
