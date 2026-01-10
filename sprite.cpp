#include "sprite.h"
#include "main.h"
#include "pic8.h"
#include <cstring>

constexpr unsigned MAXBUFFER = 20000u;
constexpr int max_run_length = 255;

// Returns how many pixels are not the specified color, up to max row end:
static int count_needed_color_pixels(pic8* ppic, int x, int y, unsigned char color) {
    int count = 0;
    while (x < ppic->get_width() && ppic->gpixel(x, y) != color && count < max_run_length) {
        x++;
        count++;
    }
    return count;
}

// Returns how many pixels are the specified color, up to max row end:
static int count_skipped_color_pixels(pic8* ppic, int x, int y, unsigned char color) {
    int count = 0;
    while (x < ppic->get_width() && ppic->gpixel(x, y) == color && count < max_run_length) {
        x++;
        count++;
    }
    return count;
}

unsigned char* sprite_data_8(pic8* ppic, unsigned char color, unsigned short* sprite_length) {
    *sprite_length = 0;
    unsigned char* buffer = nullptr;
    buffer = new unsigned char[MAXBUFFER];
    if (!buffer) {
        external_error("sprite_data_8 out of memory!");
        return nullptr;
    }
    int xsize = ppic->get_width();
    int ysize = ppic->get_height();
    unsigned buf_index = 0;
    for (int y = 0; y < ysize; y++) {
        // Process one row:
        int x = 0;
        while (x < xsize) {
            int needed_length = count_needed_color_pixels(ppic, x, y, color);
            if (needed_length) {
                // Needed pixels:
                x += needed_length;
                buffer[buf_index++] = 'K';
                buffer[buf_index++] = needed_length;
            } else {
                // Skipped pixels:
                int skipped_length = count_skipped_color_pixels(ppic, x, y, color);
                x += skipped_length;
                buffer[buf_index++] = 'N';
                buffer[buf_index++] = skipped_length;
            }
            if (buf_index >= MAXBUFFER) {
                internal_error("sprite_data_8 buffer overflow!");
                delete buffer;
                return nullptr;
            }
        }
    }

    // Now create a buffer that is only as long as needed:

    unsigned char* final_buffer = nullptr;
    final_buffer = new unsigned char[buf_index];
    if (!final_buffer) {
        external_error("sprite_data_8 out of memory!");
        delete buffer;
        return nullptr;
    }
    memcpy(final_buffer, buffer, buf_index);

    *sprite_length = buf_index;

    delete buffer;
    return final_buffer;
}

void make_sprite(pic8* ppic, int index) {
    ppic->transparency_data = sprite_data_8(ppic, index, &ppic->transparency_data_length);
}

void make_sprite(pic8* ppic) { make_sprite(ppic, ppic->gpixel(0, 0)); }
