#include "SPRITE.H"
#include "main.h"
#include "pic8.h"
#include <cstring>

#define MAXBUFFER (20000u)
static int max_run_length = 255;

// Visszaadja hany db pixel kell szinu, max sor vegeig:
static int count_needed_color_pixels(pic8* ppic, int x, int y, unsigned char color) {
    int count = 0;
    while (x < ppic->get_width() && ppic->gpixel(x, y) != color && count < max_run_length) {
        x++;
        count++;
    }
    return count;
}

// Visszaadja hany db pixel nem kell szinu, max sor vegeig:
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
    unsigned char* buffer = NULL;
    buffer = new unsigned char[MAXBUFFER];
    if (!buffer) {
        external_error("sprite_data_8 out of memory!");
        return NULL;
    }
    int xsize = ppic->get_width();
    int ysize = ppic->get_height();
    unsigned buf_index = 0;
    for (int y = 0; y < ysize; y++) {
        // Egy sor feldolgozasa:
        int x = 0;
        while (x < xsize) {
            int needed_length = count_needed_color_pixels(ppic, x, y, color);
            if (needed_length) {
                // kellek jonnek:
                x += needed_length;
                buffer[buf_index++] = 'K';
                buffer[buf_index++] = needed_length;
            } else {
                // nemkellek jonnek:
                int skipped_length = count_skipped_color_pixels(ppic, x, y, color);
                x += skipped_length;
                buffer[buf_index++] = 'N';
                buffer[buf_index++] = skipped_length;
            }
            if (buf_index >= MAXBUFFER) {
                internal_error("sprite_data_8 buffer overflow!");
                delete buffer;
                return NULL;
            }
        }
    }

    // Most letrehozunk egy szegmenst, ami csak olyan hosszu, amilyen kell:

    unsigned char* final_buffer = NULL;
    final_buffer = new unsigned char[buf_index];
    if (!final_buffer) {
        external_error("sprite_data_8 out of memory!");
        delete buffer;
        return NULL;
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
