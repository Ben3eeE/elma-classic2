#ifndef VK_RENDERER_H
#define VK_RENDERER_H

#include <SDL.h>

int vk_init(SDL_Window* window, int width, int height);
void vk_upload_frame(const unsigned char* indices, int pitch);
void vk_update_palette(const unsigned int* palette);
void vk_present();
void vk_cleanup();

#endif // VK_RENDERER_H
