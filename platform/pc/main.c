#include <stdio.h>

#include "yui_boot.h"
#include "backend.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    extern int __argc;
    extern char** __argv;
    return main(__argc, __argv);
}
#endif

int main(int argc, char* argv[]) {
    const char* json_path = "app/app.json";
    const char* assets_dir = NULL;

    if (argc > 1) {
        json_path = argv[1];
    }
    if (argc > 2) {
        assets_dir = argv[2];
    }

    if (yui_init(json_path, assets_dir) != 0) {
        fprintf(stderr, "yui-pc: init failed\n");
        return 1;
    }

    backend_run(yui_get_root());
    yui_shutdown();
    return 0;
}
