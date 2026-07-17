(function () {
  var params = new URLSearchParams(window.location.search);
  var jsonPath = params.get("json") || "app/watch-os/app.json";
  var assetsDir = params.get("assets") || "app/assets";
  var wasmBase = "yui/";

  function fitCanvas(canvas) {
    if (!canvas || !(canvas.width > 0) || !(canvas.height > 0)) {
      return;
    }
    var dpr = window.devicePixelRatio || 1;
    var logicalW = canvas.width / dpr;
    var logicalH = canvas.height / dpr;
    var scale = Math.min(
      window.innerWidth / logicalW,
      window.innerHeight / logicalH,
      1
    );
    var w = Math.floor(logicalW * scale);
    var h = Math.floor(logicalH * scale);
    canvas.style.width = w + "px";
    canvas.style.height = h + "px";
    canvas.style.left = Math.floor((window.innerWidth - w) / 2) + "px";
    canvas.style.top = Math.floor((window.innerHeight - h) / 2) + "px";
  }

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
    return loadScript(wasmBase + "yui-web.js").then(function () {
      return wasmBase;
    }).catch(function () {
      return Promise.reject(new Error(
        "yui-web.js not found in yui/. Run: ya -p em -m release -b yui-web.js"
      ));
    });
  }

  setStatus("loading wasm…");

  tryLoadWasm()
    .then(function (wasmBase) {
      if (typeof YuiModule !== "function") {
        throw new Error("YuiModule not found (build with MODULARIZE=1)");
      }

      var canvas = document.getElementById("canvas");
      return YuiModule({
        noInitialRun: true,
        canvas: canvas,
        arguments: [jsonPath, assetsDir],
        locateFile: function (path) {
          if (path.endsWith(".wasm")) {
            return wasmBase + "yui-web.wasm";
          }
          if (path.endsWith(".data")) {
            return wasmBase + "yui-web.data";
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
      var canvas = document.getElementById("canvas");
      fitCanvas(canvas);
      window.addEventListener("resize", function () {
        fitCanvas(canvas);
      });
    })
    .catch(function (err) {
      console.error(err);
      setStatus(err.message);
    });
})();
