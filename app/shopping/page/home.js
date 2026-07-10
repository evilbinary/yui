function onHomeLoad() {
    refreshHomeFeatured();
}

function onHomeShow() {
    refreshHomeFeatured();
    updateStatusBar();
}

function refreshHomeFeatured() {
    var ids = Shop.featured;
    var buttons = ["btn_feat_1", "btn_feat_2", "btn_feat_3", "btn_feat_4"];
    for (var i = 0; i < buttons.length; i++) {
        var product = Shop.findProduct(ids[i]);
        if (product) {
            var tag = product.tag ? " · " + product.tag : "";
            YUI.setText(buttons[i], product.name + tag + "  " + Shop.formatPrice(product.price));
        }
    }
}

function homeGoElectronics() { goCategory("electronics"); }
function homeGoFashion() { goCategory("fashion"); }
function homeGoBooks() { goCategory("books"); }

function homeFeat1() { goProduct(Shop.featured[0]); }
function homeFeat2() { goProduct(Shop.featured[1]); }
function homeFeat3() { goProduct(Shop.featured[2]); }
function homeFeat4() { goProduct(Shop.featured[3]); }
