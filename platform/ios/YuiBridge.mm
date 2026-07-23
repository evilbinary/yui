#import "YuiBridge.h"

#include <unistd.h>

#include "yui_boot.h"

static int g_initialized = 0;

@implementation YuiBridge

+ (void)initWithLayer:(CAMetalLayer*)layer
             jsonPath:(NSString*)jsonPath
           assetsPath:(NSString*)assetsPath
             dataRoot:(NSString*)dataRoot
              density:(float)density {
    const char* json_c = jsonPath.UTF8String;
    const char* assets_c = assetsPath.UTF8String;
    const char* data_root_c = dataRoot.UTF8String;

    if (g_initialized) {
        [YuiBridge shutdown];
    }

    yui_set_native_surface((__bridge void*)layer);
    yui_set_density(density);

    if (data_root_c && data_root_c[0]) {
        if (chdir(data_root_c) != 0) {
            NSLog(@"YuiBridge: chdir to %@ failed", dataRoot);
        }
    }

    if (!json_c || yui_init(json_c, assets_c) != 0) {
        NSLog(@"YuiBridge: yui_init failed for %@", jsonPath);
        return;
    }

    g_initialized = 1;
}

+ (void)resizeWithWidth:(int)width height:(int)height {
    if (g_initialized) {
        yui_resize(width, height);
    }
}

+ (void)setAppFocused:(BOOL)focused {
    if (g_initialized) {
        yui_set_app_focused(focused ? 1 : 0);
    }
}

+ (void)onTouchWithPointerId:(int)pointerId x:(float)x y:(float)y phase:(int)phase {
    if (g_initialized) {
        yui_on_touch(pointerId, (int)x, (int)y, phase);
    }
}

+ (void)tick {
    if (g_initialized) {
        yui_tick();
    }
}

+ (void)shutdown {
    if (g_initialized) {
        yui_shutdown();
        g_initialized = 0;
    }
}

@end
