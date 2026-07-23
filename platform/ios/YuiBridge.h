#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

NS_ASSUME_NONNULL_BEGIN

@interface YuiBridge : NSObject

+ (void)initWithLayer:(CAMetalLayer*)layer
             jsonPath:(NSString*)jsonPath
           assetsPath:(NSString*)assetsPath
             dataRoot:(NSString*)dataRoot
              density:(float)density;

+ (void)resizeWithWidth:(int)width height:(int)height;
+ (void)setAppFocused:(BOOL)focused;
+ (void)onTouchWithPointerId:(int)pointerId x:(float)x y:(float)y phase:(int)phase;
+ (void)tick;
+ (void)shutdown;

@end

NS_ASSUME_NONNULL_END
