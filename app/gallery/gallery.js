/**
 * Component Gallery — 组件平铺 + 主题切换
 * 运行: make gallery
 *   或: ya -r playground -- app/gallery/app.json
 */

var GALLERY_THEMES = {
    mocha: { label: "Mocha", path: "app/lib/themes/mocha.json", buttonId: "btnThemeMocha" },
    dark: { label: "Dark", path: "app/lib/themes/dark.json", buttonId: "btnThemeDark" },
    latte: { label: "Latte", path: "app/lib/themes/latte.json", buttonId: "btnThemeLatte" },
    "element-plus": { label: "Element Plus", path: "app/lib/themes/element-plus.json", buttonId: "btnThemeEp" },
    "element-plus-dark": { label: "EP Dark", path: "app/lib/themes/element-plus-dark.json", buttonId: "btnThemeEpDark" },
    "soft-ui": { label: "Soft UI", path: "app/lib/themes/soft-ui.json", buttonId: "btnThemeSoft" },
    "soft-ui-dark": { label: "Soft UI Dark", path: "app/lib/themes/soft-ui-dark.json", buttonId: "btnThemeSoftDark" }
};

var currentThemeId = "soft-ui";

function setStatus(msg) {
    var el = yui.find("galleryStatus");
    if (el) el.text = msg;
}

function updateThemeButtonSelection(themeId) {
    var id;
    for (id in GALLERY_THEMES) {
        if (!GALLERY_THEMES.hasOwnProperty(id)) continue;
        var btn = yui.find(GALLERY_THEMES[id].buttonId);
        if (!btn) continue;
        btn.variant = (id === themeId) ? "primary" : "compact";
    }
}

function applyGalleryTheme(themeId) {
    var theme = GALLERY_THEMES[themeId];
    if (!theme) {
        setStatus("未知主题: " + themeId);
        return;
    }
    if (typeof YUI.themeLoad !== "function") {
        setStatus("主题 API 不可用");
        return;
    }
    var loaded = YUI.themeLoad(theme.path);
    if (!loaded || loaded.success === false) {
        setStatus("加载失败: " + theme.path);
        return;
    }
    var name = loaded.name || themeId;
    currentThemeId = themeId;
    YUI.themeSetCurrent(name);
    /* 先改选中态 variant，再 apply，primary 才会落在当前主题按钮上 */
    updateThemeButtonSelection(themeId);
    YUI.themeApplyToTree();
    setStatus("当前主题：" + theme.label + " (" + name + ")");
}

function onThemeMocha() { applyGalleryTheme("mocha"); }
function onThemeDark() { applyGalleryTheme("dark"); }
function onThemeLatte() { applyGalleryTheme("latte"); }
function onThemeElementPlus() { applyGalleryTheme("element-plus"); }
function onThemeElementPlusDark() { applyGalleryTheme("element-plus-dark"); }
function onThemeSoftUi() { applyGalleryTheme("soft-ui"); }
function onThemeSoftUiDark() { applyGalleryTheme("soft-ui-dark"); }

function onGalleryLog(msg) {
    setStatus(String(msg || "clicked"));
}

function onShowInfoDialog() {
    YUI.show("galleryInfoDialog");
}

function onGalleryPagerChange() {
    setStatus("分页已切换");
}

function onGallerySelect() {
    setStatus("Table / Tree 选中");
}

function onGalleryReady() {
    applyGalleryTheme(currentThemeId);
}

onGalleryReady();
