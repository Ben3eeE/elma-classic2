#ifndef BADGE_INPUT_H
#define BADGE_INPUT_H

#include <directinput/scancodes.h>

// Initialize GPIO pins for badge buttons
void badge_input_init();

// Poll button states. Call once per frame before checking keys.
void badge_input_poll();

// Clear all button state (call before entering game loop to avoid carryover)
void badge_input_clear();

// Query button state using DirectInput scancodes
bool badge_is_key_down(int code);
bool badge_was_key_just_pressed(int code);
int badge_get_any_key_just_pressed();

#endif
