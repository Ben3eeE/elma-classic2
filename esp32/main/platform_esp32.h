#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

#include <cstdint>

void esp32_hardware_init();
void convert_palette_to_rgb565(const unsigned char* src, uint16_t* dst, int pixel_count);
uint16_t* get_display_buffer();
void swap_display_buffer();

#endif
