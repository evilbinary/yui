var categoryProductIds = [];

function onCategoryLoad() {
    categoryProductIds = [];
}

function onCategoryShow() {
    var route = YUI.currentRoute();
    var name = route && route.params ? route.params.name : "unknown";
    var label = Shop.getCategoryLabel(name);
    var list = Shop.catalog[name] || [];

    categoryProductIds = [];
    for (var i = 0; i < list.length; i++) {
        categoryProductIds.push(list[i].id);
    }

    YUI.setText("category_title", label);
    YUI.setText("category_hint", "共 " + list.length + " 件商品");

    var buttons = ["btn_cat_item_1", "btn_cat_item_2", "btn_cat_item_3"];
    for (var j = 0; j < buttons.length; j++) {
        if (j < list.length) {
            var p = list[j];
            var tag = p.tag ? " · " + p.tag : "";
            YUI.setText(buttons[j], p.name + tag + "  " + Shop.formatPrice(p.price));
        } else {
            YUI.setText(buttons[j], "");
        }
    }

    updateStatusBar();
}

function categoryItem1() {
    if (categoryProductIds[0]) goProduct(categoryProductIds[0]);
}
function categoryItem2() {
    if (categoryProductIds[1]) goProduct(categoryProductIds[1]);
}
function categoryItem3() {
    if (categoryProductIds[2]) goProduct(categoryProductIds[2]);
}
