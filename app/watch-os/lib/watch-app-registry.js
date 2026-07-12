/**
 * Watch OS 应用注册表 - 扫描 apps/ 目录，从各应用 JSON 读取元数据
 *
 * 应用 JSON 约定（顶层字段）：
 *   title    显示名称
 *   icon     启动器图标（emoji）
 *   watchApp { id, path?, order?, launcher?, system?, dock? }
 *
 * path 默认 /apps/<id>；系统应用 face/launcher/notifications 在 shellRoutes 中覆盖。
 */

var WatchAppRegistry = {
    appsRoot: "app/watch-os/apps",
    installedRegistry: "app/watch-os/installed/registry.json",
    shellRoutes: {
        "/": { json: "app/watch-os/apps/face/face.json", keepAlive: true },
        "/launcher": { json: "app/watch-os/apps/launcher/launcher.json", keepAlive: true },
        "/notifications": { json: "app/watch-os/apps/notifications/notifications.json", keepAlive: true }
    },
    apps: [],
    routes: {},
    byId: {},

    init: function() {
        this.apps = [];
        this.routes = {};
        this.byId = {};

        for (var path in this.shellRoutes) {
            if (this.shellRoutes.hasOwnProperty(path)) {
                this.routes[path] = this.shellRoutes[path];
            }
        }

        var scanned = this._scanRoot(this.appsRoot, false);
        var installed = this._scanInstalledRegistry();

        this.apps.sort(function(a, b) {
            return (a.order || 100) - (b.order || 100);
        });

        for (var i = 0; i < this.apps.length; i++) {
            var app = this.apps[i];
            this.byId[app.id] = app;
            if (app.path && app.json) {
                this.routes[app.path] = { json: app.json, keepAlive: true };
            }
        }

        var launcherCount = this.getLauncherApps().length;
        YUI.log("WatchAppRegistry: " + this.apps.length + " apps registered (" + scanned + " scanned, " + installed + " installed, " + launcherCount + " in launcher)");
        return this.routes;
    },

    _readFileSafe: function(filePath) {
        if (typeof YUI.readFile !== "function") return null;
        try {
            return YUI.readFile(filePath);
        } catch (e) {
            return null;
        }
    },

    _scanInstalledRegistry: function() {
        var raw = this._readFileSafe(this.installedRegistry);
        if (!raw) return 0;
        var data;
        try {
            data = JSON.parse(raw);
        } catch (e) {
            return 0;
        }
        var list = data.apps || [];
        var count = 0;
        for (var i = 0; i < list.length; i++) {
            var entry = list[i];
            if (!entry.id || !entry.source) continue;
            if (this._registerApp(this._loadAppDir(entry.source, entry.id, true))) count++;
        }
        return count;
    },

    _registerApp: function(app) {
        if (!app) return false;
        var existing = this.byId[app.id];
        if (existing) {
            for (var j = 0; j < this.apps.length; j++) {
                if (this.apps[j].id === app.id) {
                    this.apps[j] = app;
                    break;
                }
            }
        } else {
            this.apps.push(app);
        }
        this.byId[app.id] = app;
        return true;
    },

    _loadInstalledRegistry: function() {
        var raw = this._readFileSafe(this.installedRegistry);
        if (!raw) return { apps: [] };
        try {
            return JSON.parse(raw);
        } catch (e) {
            return { apps: [] };
        }
    },

    _saveInstalledRegistry: function(data) {
        if (typeof YUI.writeFile !== "function") return false;
        return YUI.writeFile(this.installedRegistry, JSON.stringify(data, null, 2));
    },

    _scanRoot: function(root, isInstalled) {
        if (typeof YUI.listDir !== "function") {
            YUI.log("WatchAppRegistry: listDir not available");
            return 0;
        }
        var entries = YUI.listDir(root);
        if (!entries) {
            YUI.log("WatchAppRegistry: cannot list " + root);
            return 0;
        }

        var count = 0;
        for (var i = 0; i < entries.length; i++) {
            var entry = entries[i];
            if (!entry || !entry.isDir) continue;
            var dirName = entry.name;
            if (!dirName || dirName.charAt(0) === ".") continue;
            if (this._registerApp(this._loadAppDir(root, dirName, isInstalled))) count++;
        }
        return count;
    },

    _loadAppDir: function(root, dirName, isInstalled) {
        var base = root + "/" + dirName;
        var pagePath = base + "/" + dirName + ".json";
        var meta = this._parsePageMeta(pagePath, dirName);
        if (!meta) return null;

        return {
            id: meta.id,
            title: meta.title,
            icon: meta.icon,
            path: meta.path,
            json: pagePath,
            root: base,
            order: meta.order,
            launcher: meta.launcher,
            system: meta.system,
            dock: meta.dock,
            installed: isInstalled
        };
    },

    _parsePageMeta: function(pagePath, dirName) {
        var raw = this._readFileSafe(pagePath);
        if (!raw) return null;

        var page;
        try {
            page = JSON.parse(raw);
        } catch (e) {
            YUI.log("WatchAppRegistry: bad json " + pagePath);
            return null;
        }

        var wa = page.watchApp || {};
        var id = wa.id || dirName;
        var title = page.title || wa.title || dirName;
        var icon = page.icon || wa.icon || "📱";
        var path = wa.path || ("/apps/" + id);

        var launcher = true;
        if (wa.launcher === false) launcher = false;
        if (page.launcher === false) launcher = false;

        return {
            id: id,
            title: title,
            icon: icon,
            path: path,
            order: (wa.order != null) ? wa.order : 100,
            launcher: launcher,
            system: wa.system === true,
            dock: wa.dock === true
        };
    },

    getRoutes: function() {
        return this.routes;
    },

    getAll: function() {
        return this.apps;
    },

    findById: function(id) {
        return this.byId[id] || null;
    },

    getLauncherApps: function() {
        var list = [];
        for (var i = 0; i < this.apps.length; i++) {
            var app = this.apps[i];
            if (app.launcher && !app.system) list.push(app);
        }
        return list;
    },

    getDockApps: function(limit) {
        var list = [];
        for (var i = 0; i < this.apps.length; i++) {
            if (this.apps[i].dock && !this.apps[i].system) list.push(this.apps[i]);
        }
        list.sort(function(a, b) { return (a.order || 100) - (b.order || 100); });
        if (limit && list.length > limit) return list.slice(0, limit);
        if (list.length === 0) {
            return this.getLauncherApps().slice(0, limit || 3);
        }
        return list;
    },

    openById: function(id) {
        var app = this.findById(id);
        if (!app) {
            YUI.log("WatchAppRegistry: unknown app " + id);
            return false;
        }
        openWatchApp(app.path);
        return true;
    },

    installPackage: function(sourceRoot, appId) {
        var app = this._loadAppDir(sourceRoot, appId, true);
        if (!app) {
            YUI.log("WatchAppRegistry: invalid package " + sourceRoot);
            return false;
        }

        var reg = this._loadInstalledRegistry();
        for (var i = 0; i < reg.apps.length; i++) {
            if (reg.apps[i].id === appId) return true;
        }
        reg.apps.push({ id: appId, source: sourceRoot });
        if (!this._saveInstalledRegistry(reg)) return false;

        this.init();
        if (typeof Router !== "undefined" && Router.routes) {
            Router.routes = this.routes;
        }
        return true;
    },

    isInstalled: function(appId) {
        var reg = this._loadInstalledRegistry();
        for (var i = 0; i < reg.apps.length; i++) {
            if (reg.apps[i].id === appId) return true;
        }
        return false;
    }
};
