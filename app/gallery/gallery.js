/**
 * Component Gallery — 组件平铺 + 主题切换
 * 运行: make gallery
 *   或: ya -r playground -- app/gallery/gallery.json
 */

var GALLERY_THEMES = {
    mocha: { label: "Mocha", path: "app/lib/themes/mocha.json" },
    dark: { label: "Dark", path: "app/lib/themes/dark.json" },
    latte: { label: "Latte", path: "app/lib/themes/latte.json" },
    "element-plus": { label: "Element Plus", path: "app/lib/themes/element-plus.json" },
    "element-plus-dark": { label: "EP Dark", path: "app/lib/themes/element-plus-dark.json" },
    "soft-ui": { label: "Soft UI", path: "app/lib/themes/soft-ui.json" },
    "soft-ui-dark": { label: "Soft UI Dark", path: "app/lib/themes/soft-ui-dark.json" }
};

var currentThemeId = "soft-ui";

function setStatus(msg) {
    var el = yui.find("galleryStatus");
    if (el) el.text = msg;
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
