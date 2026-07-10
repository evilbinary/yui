/**
 * YUI Router - 纯 JS 路由
 *
 * Phase 2: 静态 layerId + navigate/back/replace
 * Phase 3: 动态 json + renderFromJson(append) + keepAlive 缓存
 * Phase 4: beforeRoute / beforeEach 路由守卫
 */

var Router = {
    outlet: "page_outlet",
    routes: {},
    stack: [],
    cache: {},
    guards: [],

    init: function(config) {
        config = config || {};
        if (config.outlet) this.outlet = config.outlet;
        if (config.routes) this.routes = config.routes;

        var self = this;
        YUI.navigate = function(path, params, options) {
            return self.navigate(path, params, options);
        };
        YUI.back = function() {
            return self.back();
        };
        YUI.replace = function(path, params, options) {
            return self.replace(path, params, options);
        };
        YUI.currentRoute = function() {
            return self.current();
        };
        YUI.canBack = function() {
            return self.canBack();
        };
        YUI.beforeRoute = function(guard) {
            return self.beforeEach(guard);
        };

        if (config.defaultRoute) {
            this.navigate(config.defaultRoute, config.defaultParams || {});
        }
    },

    beforeEach: function(guard) {
        if (typeof guard === "function") {
            this.guards.push(guard);
        }
    },

    current: function() {
        if (this.stack.length === 0) return null;
        var entry = this.stack[this.stack.length - 1];
        return {
            path: entry.path,
            pattern: entry.pattern,
            params: entry.params,
            layerId: entry.layerId,
            keepAlive: entry.keepAlive,
            dynamic: entry.dynamic === true
        };
    },

    canBack: function() {
        return this.stack.length > 1;
    },

    matchRoute: function(path) {
        if (!path) return null;
        if (this.routes[path]) {
            return { pattern: path, route: this.routes[path], params: {} };
        }

        var patterns = [];
        for (var key in this.routes) {
            if (this.routes.hasOwnProperty(key)) {
                patterns.push(key);
            }
        }
        patterns.sort(function(a, b) { return b.length - a.length; });

        for (var i = 0; i < patterns.length; i++) {
            var params = this._matchPattern(patterns[i], path);
            if (params) {
                return { pattern: patterns[i], route: this.routes[patterns[i]], params: params };
            }
        }
        return null;
    },

    _matchPattern: function(pattern, path) {
        var patternParts = pattern.split("/");
        var pathParts = path.split("/");
        if (patternParts.length !== pathParts.length) return null;

        var params = {};
        for (var i = 0; i < patternParts.length; i++) {
            var seg = patternParts[i];
            if (!seg) continue;
            if (seg.charAt(0) === ":") {
                params[seg.slice(1)] = decodeURIComponent(pathParts[i] || "");
            } else if (seg !== pathParts[i]) {
                return null;
            }
        }
        return params;
    },

    navigate: function(path, params, options) {
        return this._go(path, params, options, false);
    },

    replace: function(path, params, options) {
        return this._go(path, params, options, true);
    },

    _go: function(path, params, options, isReplace) {
        options = options || {};
        params = params || {};

        var matched = this.matchRoute(path);
        if (!matched) {
            YUI.log("Router: no route matched for " + path);
            return false;
        }

        var self = this;
        var from = this.current();
        var to = {
            path: path,
            pattern: matched.pattern,
            params: this._mergeParams(matched.params, params),
            route: matched.route
        };

        return this._runGuards(from, to, isReplace, function() {
            if (from) {
                self._leave(from, isReplace);
                if (isReplace) {
                    self.stack.pop();
                }
            } else {
                self._hideAllRouteLayers(to.route.layerId || null);
            }

            var entry = self._enter(to);
            if (!entry) return false;
            self.stack.push(entry);
            return true;
        });
    },

    back: function() {
        if (!this.canBack()) {
            return false;
        }

        var leaving = this.stack.pop();
        this._leave(leaving, false);
        var returning = this.current();
        if (returning) {
            this._showEntry(returning);
        }
        return true;
    },

    _mergeParams: function(routeParams, extraParams) {
        var merged = {};
        var key;
        for (key in routeParams) {
            if (routeParams.hasOwnProperty(key)) merged[key] = routeParams[key];
        }
        for (key in extraParams) {
            if (extraParams.hasOwnProperty(key)) merged[key] = extraParams[key];
        }
        return merged;
    },

    _runGuards: function(from, to, isReplace, done) {
        var index = 0;
        var self = this;

        function next(redirectPath) {
            if (redirectPath === false) {
                YUI.log("Router: navigation cancelled by guard");
                return false;
            }
            if (typeof redirectPath === "string") {
                if (isReplace) return self.replace(redirectPath, to.params);
                return self.navigate(redirectPath, to.params);
            }
            if (index >= self.guards.length) {
                return done();
            }
            var guard = self.guards[index++];
            return guard(from, to, next);
        }

        return next();
    },

    _hideAllRouteLayers: function(exceptLayerId) {
        var path;
        for (path in this.routes) {
            if (!this.routes.hasOwnProperty(path)) continue;
            var layerId = this.routes[path].layerId;
            if (layerId && layerId !== exceptLayerId) {
                YUI.hide(layerId);
            }
        }
        for (path in this.cache) {
            if (!this.cache.hasOwnProperty(path)) continue;
            var cachedId = this.cache[path].layerId;
            if (cachedId && cachedId !== exceptLayerId) {
                YUI.hide(cachedId);
            }
        }
    },

    _cacheKey: function(to) {
        return to.path;
    },

    _findChildById: function(layerId) {
        if (typeof yui !== "undefined" && yui.find) {
            return yui.find(layerId);
        }
        return null;
    },

    _enter: function(to) {
        var route = to.route;
        var keepAlive = route.keepAlive !== false;
        var cacheKey = this._cacheKey(to);

        if (route.layerId) {
            if (this.cache[cacheKey]) {
                YUI.show(this.cache[cacheKey].layerId);
                return this.cache[cacheKey];
            }

            YUI.show(route.layerId);
            var staticEntry = {
                path: to.path,
                pattern: to.pattern,
                params: to.params,
                layerId: route.layerId,
                keepAlive: keepAlive,
                dynamic: false
            };
            if (keepAlive) {
                this.cache[cacheKey] = staticEntry;
            }
            return staticEntry;
        }

        if (route.json) {
            if (keepAlive && this.cache[cacheKey]) {
                YUI.show(this.cache[cacheKey].layerId);
                return this.cache[cacheKey];
            }

            var jsonStr = this._loadJson(route.json, to.params);
            if (!jsonStr) {
                YUI.log("Router: failed to load json for " + to.path);
                return null;
            }

            var layerId = route.layerId || this._extractLayerId(jsonStr);
            if (keepAlive && layerId && this._findChildById(layerId)) {
                YUI.show(layerId);
                var cached = this.cache[cacheKey];
                if (cached) return cached;
            }

            var code = YUI.renderFromJson(this.outlet, jsonStr, keepAlive);

            if (code !== 0) {
                YUI.log("Router: failed to mount json route, code=" + code);
                return null;
            }

            layerId = layerId || this._extractLayerId(jsonStr) || ("route_" + to.path.replace(/\//g, "_"));
            var dynamicEntry = {
                path: to.path,
                pattern: to.pattern,
                params: to.params,
                layerId: layerId,
                keepAlive: keepAlive,
                dynamic: true,
                json: route.json
            };

            if (keepAlive) {
                this.cache[cacheKey] = dynamicEntry;
            }
            return dynamicEntry;
        }

        YUI.log("Router: route must define layerId or json: " + to.path);
        return null;
    },

    _showEntry: function(entry) {
        if (!entry || !entry.layerId) return;
        YUI.show(entry.layerId);
    },

    _leave: function(entry, forceRemove) {
        if (!entry || !entry.layerId) return;

        if (forceRemove || entry.keepAlive === false) {
            var update = {};
            update[this.outlet] = { remove: entry.layerId };
            YUI.update(update);
            if (this.cache[entry.path]) {
                delete this.cache[entry.path];
            }
            return;
        }

        YUI.hide(entry.layerId);
    },

    _loadJson: function(jsonPath, params) {
        var content = null;
        if (typeof YUI.readFile === "function") {
            content = YUI.readFile(jsonPath);
        }
        if (!content) {
            YUI.log("Router: cannot read " + jsonPath);
            return null;
        }

        var key;
        for (key in params) {
            if (params.hasOwnProperty(key)) {
                content = content.split(":" + key).join(params[key]);
            }
        }
        return content;
    },

    _extractLayerId: function(jsonStr) {
        var match = jsonStr.match(/"id"\s*:\s*"([^"]+)"/);
        return match ? match[1] : null;
    }
};
