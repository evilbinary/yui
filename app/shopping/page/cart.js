function onCartLoad() {}

function onCartShow() {
    refreshCartView();
    updateStatusBar();
}

function refreshCartView() {
    var userName = Shop.user ? Shop.user.name : "未知";
    YUI.setText("cart_user", "当前用户：" + userName);

    var lines = ["cart_line_1", "cart_line_2", "cart_line_3"];
    if (Shop.cart.length === 0) {
        YUI.setText("cart_line_1", "购物袋是空的，去挑点好物吧～");
        YUI.setText("cart_line_2", "");
        YUI.setText("cart_line_3", "");
    } else {
        for (var i = 0; i < lines.length; i++) {
            if (i < Shop.cart.length) {
                var item = Shop.cart[i];
                YUI.setText(
                    lines[i],
                    item.name + " ×" + item.qty + "  " + Shop.formatPrice(item.price * item.qty)
                );
            } else {
                YUI.setText(lines[i], "");
            }
        }
    }

    YUI.setText("cart_total", "合计  " + Shop.formatPrice(Shop.cartTotal()));
}

function cartClear() {
    Shop.clearCart();
    refreshCartView();
}

function cartContinue() {
    goHome();
}
