#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "ios_metal_glue.h"

#include <vector>
#include <cstddef>

static CAMetalLayer* g_layer = nil;
static id<MTLDevice> g_device = nil;
static id<MTLCommandQueue> g_queue = nil;
static id<MTLRenderPipelineState> g_pipeline = nil;
static int g_width = 0;
static int g_height = 0;

struct MetalRect {
    float x, y, w, h;
    float r, g, b, a;
};

struct MetalVertex {
    float px, py;
    float r, g, b, a;
};

static std::vector<MetalRect> g_rects;

static const char* g_shader_src = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut color_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.color = in.color;
    return out;
}

fragment float4 color_fragment(VertexOut in [[stage_in]]) {
    return in.color;
}
)";

static int ios_metal_build_pipeline(void) {
    if (!g_device || g_pipeline) {
        return g_pipeline ? 1 : 0;
    }

    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:g_shader_src];
    id<MTLLibrary> library = [g_device newLibraryWithSource:source options:nil error:&error];
    if (!library) {
        return 0;
    }

    id<MTLFunction> vertex = [library newFunctionWithName:@"color_vertex"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"color_fragment"];
    if (!vertex || !fragment) {
        return 0;
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vertex;
    desc.fragmentFunction = fragment;
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
    vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[0].offset = 0;
    vertexDesc.attributes[0].bufferIndex = 0;
    vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
    vertexDesc.attributes[1].offset = (NSUInteger)offsetof(MetalVertex, r);
    vertexDesc.attributes[1].bufferIndex = 0;
    vertexDesc.layouts[0].stride = sizeof(MetalVertex);
    desc.vertexDescriptor = vertexDesc;

    g_pipeline = [g_device newRenderPipelineStateWithDescriptor:desc error:&error];
    return g_pipeline ? 1 : 0;
}

void ios_metal_init(void* metal_layer) {
    CAMetalLayer* layer = (__bridge CAMetalLayer*)metal_layer;
    if (!layer) {
        return;
    }

    g_layer = layer;
    g_device = layer.device;
    if (!g_device) {
        g_device = MTLCreateSystemDefaultDevice();
        layer.device = g_device;
    }
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;

    g_queue = [g_device newCommandQueue];
    ios_metal_build_pipeline();

    g_width = (int)layer.drawableSize.width;
    g_height = (int)layer.drawableSize.height;
}

void ios_metal_shutdown(void) {
    g_rects.clear();
    g_pipeline = nil;
    g_queue = nil;
    g_device = nil;
    g_layer = nil;
    g_width = 0;
    g_height = 0;
}

void ios_metal_resize(int width, int height) {
    g_width = width;
    g_height = height;
}

void ios_metal_clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    g_rects.clear();
}

void ios_metal_draw_rect(float x, float y, float w, float h,
                         unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    MetalRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    rect.r = r / 255.0f;
    rect.g = g / 255.0f;
    rect.b = b / 255.0f;
    rect.a = a / 255.0f;
    g_rects.push_back(rect);
}

static void ios_metal_push_quad(std::vector<MetalVertex>& verts, float x0, float y0, float x1, float y1,
                                float r, float g, float b, float a) {
    MetalVertex tri[6] = {
        {x0, y0, r, g, b, a},
        {x1, y0, r, g, b, a},
        {x0, y1, r, g, b, a},
        {x0, y1, r, g, b, a},
        {x1, y0, r, g, b, a},
        {x1, y1, r, g, b, a},
    };
    verts.insert(verts.end(), tri, tri + 6);
}

void ios_metal_present(void) {
    if (!g_layer || !g_device || !g_queue || !g_pipeline || g_width <= 0 || g_height <= 0) {
        return;
    }

    id<CAMetalDrawable> drawable = [g_layer nextDrawable];
    if (!drawable) {
        return;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(30.0 / 255.0, 30.0 / 255.0, 30.0 / 255.0, 1.0);

    id<MTLCommandBuffer> cmd = [g_queue commandBuffer];
    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];
    [enc setRenderPipelineState:g_pipeline];

    if (!g_rects.empty()) {
        std::vector<MetalVertex> verts;
        verts.reserve(g_rects.size() * 6);
        for (const MetalRect& rect : g_rects) {
            float x0 = (rect.x / g_width) * 2.0f - 1.0f;
            float y0 = 1.0f - ((rect.y + rect.h) / g_height) * 2.0f;
            float x1 = ((rect.x + rect.w) / g_width) * 2.0f - 1.0f;
            float y1 = 1.0f - (rect.y / g_height) * 2.0f;
            ios_metal_push_quad(verts, x0, y0, x1, y1, rect.r, rect.g, rect.b, rect.a);
        }

        id<MTLBuffer> buffer = [g_device newBufferWithBytes:verts.data()
                                                     length:verts.size() * sizeof(MetalVertex)
                                                    options:MTLResourceStorageModeShared];
        [enc setVertexBuffer:buffer offset:0 atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)verts.size()];
    }

    [enc endEncoding];
    [cmd presentDrawable:drawable];
    [cmd commit];
    g_rects.clear();
}

int ios_metal_ready(void) {
    return g_layer && g_device && g_queue && g_pipeline ? 1 : 0;
}
