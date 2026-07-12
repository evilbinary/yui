/**
 * 优物商城 - 手机端多页面购物 Demo
 */

var Shop = {
    user: null,
    redirectAfterLogin: null,
    cart: [],

    catalog: {
        electronics: [
            { id: "e1", name: "无线耳机 Pro", price: 299, tag: "热卖" },
            { id: "e2", name: "智能手表 Max", price: 1299, tag: "新品" },
            { id: "e3", name: "USB-C 扩展坞", price: 159, tag: "" }
        ],
        fashion: [
            { id: "f1", name: "亚麻休闲衬衫", price: 199, tag: "" },
            { id: "f2", name: "轻跑运动鞋", price: 459, tag: "特惠" },
            { id: "f3", name: "帆布托特包", price: 89, tag: "" }
        ],
        books: [
            { id: "b1", name: "JavaScript 设计模式", price: 68, tag: "" },
            { id: "b2", name: "系统设计入门", price: 88, tag: "畅销" },
            { id: "b3", name: "UI 工程实践", price: 72, tag: "" }
        ]
    },

    featured: ["e1", "f2", "b2", "e2"],

    findProduct: function(id) {
        var cat;
        for (cat in this.catalog) {
            if (!this.catalog.hasOwnProperty(cat)) continue;
            var list = this.catalog[cat];
            for (var i = 0; i < list.length; i++) {
                if (list[i].id === id) {
                    return list[i];
                }
            }
        }
        return null;
    },

    getCategoryLabel: function(name) {
        var labels = {
            electronics: "数码电器",
            fashion: "时尚服饰",
            books: "图书文具"
        };
        return labels[name] || name;
    },

    formatPrice: function(price) {
        return "¥" + price;
    },

    cartCount: function() {
        var n = 0;
        for (var i = 0; i < this.cart.length; i++) {
            n += this.cart[i].qty;
        }
        return n;
    },

    cartTotal: function() {
        var sum = 0;
        for (var i = 0; i < this.cart.length; i++) {
            sum += this.cart[i].price * this.cart[i].qty;
        }
        return sum;
    },

    addToCart: function(productId) {
        var product = this.findProduct(productId);
        if (!product) return false;

        for (var i = 0; i < this.cart.length; i++) {
            if (this.cart[i].id === productId) {
                this.cart[i].qty += 1;
                updateCartBadge();
                return true;
            }
        }

        this.cart.push({
            id: product.id,
            name: product.name,
            price: product.price,
            qty: 1
        });
        updateCartBadge();
        return true;
    },

    clearCart: function() {
        this.cart = [];
        updateCartBadge();
    },

    login: function(name, role) {
        this.user = { name: name || "用户", role: role || "user" };
        updateAuthButton();
    },

    logout: function() {
        this.user = null;
        updateAuthButton();
        YUI.navigate("/");
    }
};

var routes = {
    "/": { layerId: "page_home", keepAlive: true },
    "/category/:name": { layerId: "page_category", keepAlive: true },
    "/product/:id": { json: "app/shopping/page/product.json", keepAlive: true },
    "/cart": { layerId: "page_cart", keepAlive: true },
    "/login": { layerId: "page_login", keepAlive: true }
};

var themeMode = "dark";
var themePlatform = "mobile";

function initThemes() {
    var suffix = themePlatform === "mobile" ? "-mobile" : "";
    Theme.load("app/lib/themes/dark" + suffix + ".json", "dark");
    Theme.load("app/lib/themes/light" + suffix + ".json", "light");
    Theme.setCurrent(themeMode);
    Theme.apply();
    updateThemeButton();
}

function updateThemeButton() {
    YUI.setText("btn_theme", themeMode === "dark" ? "暗" : "亮");
}

function toggleTheme() {
    themeMode = themeMode === "dark" ? "light" : "dark";
    Theme.setCurrent(themeMode);
    Theme.apply();
    updateThemeButton();
}

function updateCartBadge() {
    YUI.setText("btn_cart", "袋·" + Shop.cartCount());
}

function updateAuthButton() {
    if (Shop.user) {
        var short = Shop.user.name.length > 2 ? Shop.user.name.slice(0, 2) : Shop.user.name;
        YUI.setText("btn_login", short);
    } else {
        YUI.setText("btn_login", "登录");
    }
}

function updateStatusBar() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    if (!route) {
        YUI.setText("status_text", "未登录");
        return;
    }
    var userText = Shop.user ? Shop.user.name : "游客";
    YUI.setText(
        "status_text",
        route.path + " · " + userText + (YUI.canBack() ? " · 可返回" : "")
    );
}

function onAppLoad() {
    initThemes();

    Router.init({
        outlet: "page_outlet",
        routes: routes
    });

    YUI.beforeRoute(function(from, to, next) {
        if (to.path === "/login") {
            if (Shop.user) {
                next("/");
                return;
            }
            next();
            return;
        }

        if (to.path === "/cart" && !Shop.user) {
            Shop.redirectAfterLogin = "/cart";
            next("/login");
            return;
        }

        next();
    });

    updateCartBadge();
    updateAuthButton();
    YUI.navigate("/");
    updateStatusBar();
}

function onAppShow() {
    updateStatusBar();
    updateCartBadge();
    updateAuthButton();
}

function goHome() {
    YUI.navigate("/");
    updateStatusBar();
}

function goCart() {
    YUI.navigate("/cart");
    updateStatusBar();
}

function goLogin() {
    if (Shop.user) {
        Shop.logout();
        updateStatusBar();
        return;
    }
    YUI.navigate("/login");
    updateStatusBar();
}

function goBack() {
    if (YUI.canBack()) {
        YUI.back();
    }
    updateStatusBar();
}

function goCategory(name) {
    YUI.navigate("/category/" + name);
    updateStatusBar();
}

function goProduct(id) {
    YUI.navigate("/product/" + id);
    updateStatusBar();
}

function captureScreen() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var slug = route ? route.path.replace(/\//g, "_").replace(/:/g, "-") : "home";
    if (!slug || slug === "_") {
        slug = "home";
    }
    var path = "app/shopping/capture/" + slug + ".png";
    var code = YUI.screenshot(path);
    YUI.log("截图 " + path + " code=" + code);
    return code;
}
