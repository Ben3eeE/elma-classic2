#ifndef BADGE_LEDS_H
#define BADGE_LEDS_H

#include <cstdint>

void badge_leds_init();
void badge_leds_set_all(uint8_t r, uint8_t g, uint8_t b);
void badge_leds_off();
void badge_leds_flash(uint8_t r, uint8_t g, uint8_t b, int flash_count, int on_ms, int off_ms);

#endif
