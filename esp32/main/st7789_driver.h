#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include <cstdint>

void st7789_init();
void st7789_wait_transfer();
void st7789_send_framebuffer(const uint16_t* buffer);
void st7789_set_backlight(uint8_t brightness);

#endif
