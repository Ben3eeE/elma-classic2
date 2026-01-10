#ifndef METAL_RENDERER_H
#define METAL_RENDERER_H

#ifdef __APPLE__

// Metal renderer API
int metal_init(void* window, int width, int height);
void metal_upload_frame(const unsigned char* indices, int pitch);
void metal_update_palette(const unsigned int* palette);
void metal_present(void);
void metal_cleanup(void);

#endif // __APPLE__

#endif // METAL_RENDERER_H
