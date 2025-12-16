#ifndef METAL_RENDERER_H
#define METAL_RENDERER_H

#ifdef __APPLE__

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Metal renderer
int metal_init(void* window, int width, int height);

// Upload 8-bit indexed frame data
void metal_upload_frame(const unsigned char* indices, int pitch);

// Update palette (256 RGBA colors)
void metal_update_palette(const unsigned int* palette);

// Present frame to screen
void metal_present(void);

// Cleanup
void metal_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // __APPLE__

#endif // METAL_RENDERER_H
