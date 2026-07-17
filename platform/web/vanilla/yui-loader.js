(function () {
  var params = new URLSearchParams(window.location.search);
  var jsonPath = params.get("json") || "app/tests/login.json";
  var assetsDir = params.get("assets") || "app/assets";
  var wasmBases = ["yui/", "public/yui/"];

  function setStatus(text) {
    var el = document.getElementById("status");
    if (!el) {
      el = document.createElement("div");
      el.id = "status";
      document.body.appendChild(el);
    }
    el.textContent = text;
  }

  function loadScript(src) {
    return new Promise(function (resolve, reject) {
      var script = document.createElement("script");
      script.src = src;
      script.async = true;
      script.onload = function () { resolve(); };
      script.onerror = function () { reject(new Error("failed to load " + src)); };
      document.body.appendChild(script);
    });
  }

  function tryLoadWasm() {
    var index = 0;
    function next() {
      if (index >= wasmBases.length) {
        return Promise.reject(new Error(
          "playground.js not found. Run: ya -p em -m release -b yui-web.html"
        ));
      }
      var base = wasmBases[index++];
      return loadScript(base + "playground.js").then(function () {
        return base;
      }).catch(function () {
        return next();
      });
    }
    return next();
  }

  setStatus("loading wasm…");

  tryLoadWasm()
    .then(function (wasmBase) {
      if (typeof YuiModule !== "function") {
        throw new Error("YuiModule not found (build with MODULARIZE=1)");
      }

      var canvas = document.getElementById("canvas");
      return YuiModule({
        canvas: canvas,
        arguments: [jsonPath, assetsDir],
        locateFile: function (path) {
          if (path.endsWith(".wasm")) {
            return wasmBase + "playground.wasm";
          }
          if (path.endsWith(".data")) {
            return wasmBase + "playground.data";
          }
          return wasmBase + path;
        }
      });
    })
    .then(function (module) {
      setStatus(jsonPath);
      if (module.callMain) {
        module.callMain(module.arguments || []);
      }
    })
    .catch(function (err) {
      console.error(err);
      setStatus(err.message);
    });
})();
