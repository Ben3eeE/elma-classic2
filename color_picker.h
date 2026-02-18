#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

// Show a 16x16 palette color picker dialog.
// Returns 0-255 (selected index) or -1 (ESC cancelled).
int color_picker(const char* title, unsigned char* palette_data, unsigned char initial_index);

#endif
