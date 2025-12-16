#ifdef __APPLE__

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>
#include "metal_renderer.h"
#include <cstring>

// Metal shader source
static const char* shaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex VertexOut vertex_main(uint vertexID [[vertex_id]]) {
    float2 positions[6] = {
        float2(-1.0,  1.0), float2(-1.0, -1.0), float2( 1.0, -1.0),
        float2(-1.0,  1.0), float2( 1.0, -1.0), float2( 1.0,  1.0)
    };
    float2 texCoords[6] = {
        float2(0.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0),
        float2(0.0, 0.0), float2(1.0, 1.0), float2(1.0, 0.0)
    };
    
    VertexOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.texCoord = texCoords[vertexID];
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              texture2d<float> indexTexture [[texture(0)]],
                              texture2d<float> paletteTexture [[texture(1)]]) {
    constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);
    
    float index = indexTexture.sample(textureSampler, in.texCoord).r;
    float4 color = paletteTexture.sample(textureSampler, float2(index, 0.5));
    return color;
}
)";

static id<MTLDevice> device = nil;
static id<MTLCommandQueue> commandQueue = nil;
static id<MTLRenderPipelineState> pipelineState = nil;
static id<MTLTexture> indexTexture = nil;
static id<MTLTexture> paletteTexture = nil;
static CAMetalLayer* metalLayer = nil;
static int frameWidth = 0;
static int frameHeight = 0;
static id<MTLTexture> offscreenTexture = nil;
static dispatch_queue_t presentQueue = nil;
static int skipped_presents = 0;

int metal_init(void* window, int width, int height) {
    @autoreleasepool {
        frameWidth = width;
        frameHeight = height;
        
        // Get Metal device
        device = MTLCreateSystemDefaultDevice();
        if (!device) {
            return -1;
        }
        
        // Create command queue
        commandQueue = [device newCommandQueue];
        
        // Setup Metal layer
        NSWindow* nsWindow = (__bridge NSWindow*)window;
        metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.displaySyncEnabled = NO; // Disable vsync for max FPS
        
        // Disable all forms of synchronization
        if (@available(macOS 10.13, *)) {
            metalLayer.allowsNextDrawableTimeout = NO;
        }
        metalLayer.presentsWithTransaction = NO;
        
        // Set maximum drawable count to allow more buffering
        metalLayer.maximumDrawableCount = 3;
        
        // Critical: Make nextDrawable non-blocking
        if (@available(macOS 10.15.4, *)) {
            metalLayer.allowsNextDrawableTimeout = YES;
        }
        
        NSView* contentView = [nsWindow contentView];
        [contentView setWantsLayer:YES];
        [contentView setLayer:metalLayer];
        
        CGSize drawableSize = contentView.bounds.size;
        drawableSize.width *= contentView.window.backingScaleFactor;
        drawableSize.height *= contentView.window.backingScaleFactor;
        metalLayer.drawableSize = drawableSize;
        
        // Compile shaders
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:@(shaderSource)
                                                      options:nil
                                                        error:&error];
        if (!library) {
            NSLog(@"Failed to compile shaders: %@", error);
            return -1;
        }
        
        id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
        id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];
        
        // Create render pipeline
        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.colorAttachments[0].pixelFormat = metalLayer.pixelFormat;
        
        pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (!pipelineState) {
            NSLog(@"Failed to create pipeline state: %@", error);
            return -1;
        }
        
        // Create index texture (R8) with shared storage for fast CPU writes
        MTLTextureDescriptor* indexTextureDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
            width:width
            height:height
            mipmapped:NO];
        indexTextureDesc.usage = MTLTextureUsageShaderRead;
        indexTextureDesc.storageMode = MTLStorageModeShared; // Fast CPU→GPU upload
        indexTextureDesc.cpuCacheMode = MTLCPUCacheModeWriteCombined; // Optimize for write-only access
        indexTexture = [device newTextureWithDescriptor:indexTextureDesc];
        
        // Create palette texture (256x1 RGBA8)
        MTLTextureDescriptor* paletteTextureDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:256
            height:1
            mipmapped:NO];
        paletteTextureDesc.usage = MTLTextureUsageShaderRead;
        paletteTextureDesc.storageMode = MTLStorageModeShared;
        paletteTextureDesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
        paletteTexture = [device newTextureWithDescriptor:paletteTextureDesc];
        
        // Create offscreen render target
        MTLTextureDescriptor* offscreenDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:drawableSize.width
            height:drawableSize.height
            mipmapped:NO];
        offscreenDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        offscreenDesc.storageMode = MTLStorageModePrivate; // GPU-only, fastest
        offscreenTexture = [device newTextureWithDescriptor:offscreenDesc];
        
        // Create background queue for presentation (concurrent for max throughput)
        presentQueue = dispatch_queue_create("com.elma.present", DISPATCH_QUEUE_CONCURRENT);
        
        return 0;
    }
}

void metal_upload_frame(const unsigned char* indices, int pitch) {
    @autoreleasepool {
        MTLRegion region = MTLRegionMake2D(0, 0, frameWidth, frameHeight);
        [indexTexture replaceRegion:region
                        mipmapLevel:0
                          withBytes:indices
                        bytesPerRow:pitch];
    }
}

void metal_update_palette(const unsigned int* palette) {
    @autoreleasepool {
        // Convert RGBA to BGRA for Metal texture
        // Input palette: AARRGGBB (big endian)
        // Output BGRA8: BBGGRRAA (little endian in memory, so swap R and B)
        unsigned int bgra_palette[256];
        for (int i = 0; i < 256; i++) {
            unsigned int rgba = palette[i];
            unsigned int a = (rgba >> 24) & 0xFF;
            unsigned int r = (rgba >> 16) & 0xFF;
            unsigned int g = (rgba >> 8) & 0xFF;
            unsigned int b = rgba & 0xFF;
            // Repack as BGRA
            bgra_palette[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
        
        MTLRegion region = MTLRegionMake2D(0, 0, 256, 1);
        [paletteTexture replaceRegion:region
                          mipmapLevel:0
                            withBytes:bgra_palette
                          bytesPerRow:256 * 4];
    }
}

void metal_present(void) {
    @autoreleasepool {
        // Render to offscreen texture (fast, no blocking)
        id<MTLCommandBuffer> offscreenCommandBuffer = [commandQueue commandBuffer];
        
        MTLRenderPassDescriptor* offscreenPass = [MTLRenderPassDescriptor renderPassDescriptor];
        offscreenPass.colorAttachments[0].texture = offscreenTexture;
        offscreenPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        offscreenPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        offscreenPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        
        id<MTLRenderCommandEncoder> renderEncoder = [offscreenCommandBuffer renderCommandEncoderWithDescriptor:offscreenPass];
        [renderEncoder setRenderPipelineState:pipelineState];
        [renderEncoder setFragmentTexture:indexTexture atIndex:0];
        [renderEncoder setFragmentTexture:paletteTexture atIndex:1];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        [renderEncoder endEncoding];
        
        [offscreenCommandBuffer commit];
        
        // Schedule present on background queue - don't block main game loop!
        dispatch_async(presentQueue, ^{
            @autoreleasepool {
                id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
                if (drawable) {
                    id<MTLCommandBuffer> blitCommandBuffer = [commandQueue commandBuffer];
                    
                    id<MTLBlitCommandEncoder> blitEncoder = [blitCommandBuffer blitCommandEncoder];
                    [blitEncoder copyFromTexture:offscreenTexture
                                     sourceSlice:0
                                     sourceLevel:0
                                    sourceOrigin:MTLOriginMake(0, 0, 0)
                                      sourceSize:MTLSizeMake(offscreenTexture.width, offscreenTexture.height, 1)
                                       toTexture:drawable.texture
                                destinationSlice:0
                                destinationLevel:0
                               destinationOrigin:MTLOriginMake(0, 0, 0)];
                    [blitEncoder endEncoding];
                    
                    [blitCommandBuffer presentDrawable:drawable];
                    [blitCommandBuffer commit];
                } else {
                    skipped_presents++;
                }
            }
        });
    }
}

void metal_cleanup(void) {
    @autoreleasepool {
        indexTexture = nil;
        paletteTexture = nil;
        pipelineState = nil;
        commandQueue = nil;
        device = nil;
        metalLayer = nil;
    }
}

#endif // __APPLE__
