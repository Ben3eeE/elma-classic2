#ifndef DX_RENDERER_H
#define DX_RENDERER_H

#ifdef _WIN32

// DirectX 12 renderer API
int dx_init(void* hwnd, int width, int height);
void dx_upload_frame(const unsigned char* indices, int pitch);
void dx_update_palette(const unsigned int* palette);
void dx_present(void);
void dx_cleanup(void);

#endif // _WIN32

#endif // DX_RENDERER_H
