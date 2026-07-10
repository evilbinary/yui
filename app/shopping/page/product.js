var currentProductId = null;

function onProductLoad() {
    currentProductId = null;
}

function onProductShow() {
    var route = YUI.currentRoute();
    currentProductId = route && route.params ? route.params.id : null;
    var product = currentProductId ? Shop.findProduct(currentProductId) : null;

    if (!product) {
        YUI.setText("product_name", "商品不存在");
        YUI.setText("product_price", "-");
        YUI.setText("product_desc", "该商品已下架或不存在。");
        return;
    }

    var tag = product.tag ? " · " + product.tag : "";
    YUI.setText("product_badge", "货号 " + product.id);
    YUI.setText("product_name", product.name + tag);
    YUI.setText("product_price", Shop.formatPrice(product.price));
    YUI.setText("product_desc", "正品保障 · 7天无理由退换 · 全国包邮");
    updateStatusBar();
}

function productAddToCart() {
    if (!currentProductId) return;
    if (Shop.addToCart(currentProductId)) {
        YUI.log("已加入购物袋：" + currentProductId);
    }
}

function productBuyNow() {
    if (!currentProductId) return;
    Shop.addToCart(currentProductId);
    goCart();
}
