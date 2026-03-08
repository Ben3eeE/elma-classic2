#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <SDL.h>

int gl_init(SDL_Window* sdl_window, int width, int height, int pitch);
void gl_upload_frame(const unsigned char* indices, int pitch);
void gl_update_palette(const void* palette);
void gl_set_viewport(int native_w, int native_h, int offset_x, int offset_y, int scaled_w,
                     int scaled_h);
bool gl_is_initialized();
void gl_present();
int gl_resize(int width, int height, int pitch);
void gl_cleanup();

#endif // GL_RENDERER_H
