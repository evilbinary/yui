#include <stdio.h>

#include "yui_boot.h"
#include "backend.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int argc, char* argv[]) {
    const char* json_path = "app/tests/login.json";
    const char* assets_dir = "app/assets";

    if (argc > 1 && argv[1] && argv[1][0]) {
        json_path = argv[1];
    }
    if (argc > 2 && argv[2] && argv[2][0]) {
        assets_dir = argv[2];
    }

    if (yui_init(json_path, assets_dir) != 0) {
        fprintf(stderr, "yui-web: init failed for %s\n", json_path);
        return 1;
    }

    backend_run(yui_get_root());
    yui_shutdown();
    return 0;
}
