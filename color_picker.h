#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

// Show a full-screen 16x16 palette color picker.
// Returns 0-255 (selected index) or -1 (ESC cancelled).
int color_picker(unsigned char* palette_data, unsigned char initial_index);

#endif
