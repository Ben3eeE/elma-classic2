#include "color_picker.h"
#include "abc8.h"
#include "editor_dialog.h"
#include "M_PIC.H"
#include "menu_pic.h"
#include "pic8.h"
#include "platform_impl.h"
#include <algorithm>
#include <cstdio>
#include <directinput/scancodes.h>

static pic8* BackgroundTile = nullptr;

static void tile_background(pic8* buffer) {
    if (!BackgroundTile) {
        BackgroundTile = new pic8("szoveg1.pcx");
    }
    int y = -47;
    int x1 = 0;
    while (y < SCREEN_HEIGHT) {
        int x = x1;
        x1 += 110;
        while (x > 0) {
            x -= BackgroundTile->get_width();
        }
        while (x < SCREEN_WIDTH) {
            blit8(buffer, BackgroundTile, x, y);
            x += BackgroundTile->get_width();
        }
        y += BackgroundTile->get_height();
    }
}

int color_picker(const char* title, unsigned char* palette_data, unsigned char initial_index) {
    int scale = std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / 480;
    int swatch = std::max(1, 40 * scale);
    int gap = std::max(1, 3 * scale);
    int grid_size = 16 * (swatch + gap) - gap;

    int grid_x = SCREEN_WIDTH / 2 - grid_size / 2;
    int grid_y = SCREEN_HEIGHT / 2 - grid_size / 2;

    int sel_col = initial_index % 16;
    int sel_row = initial_index / 16;

    char index_text[32];

    while (true) {
        handle_events();

        if (was_key_just_pressed(DIK_ESCAPE)) {
            return -1;
        }
        if (was_key_just_pressed(DIK_RETURN)) {
            return sel_row * 16 + sel_col;
        }
        if (was_key_just_pressed(DIK_UP)) {
            sel_row = (sel_row + 15) % 16;
        }
        if (was_key_just_pressed(DIK_DOWN)) {
            sel_row = (sel_row + 1) % 16;
        }
        if (was_key_just_pressed(DIK_LEFT)) {
            sel_col = (sel_col + 15) % 16;
        }
        if (was_key_just_pressed(DIK_RIGHT)) {
            sel_col = (sel_col + 1) % 16;
        }

        // Draw background
        tile_background(BufferMain);

        // Draw title above grid
        MenuFont->write_centered(BufferMain, SCREEN_WIDTH / 2, grid_y - 30, title);

        // Fill grid region with index 0 (black in LGR palettes) to clear gaps
        BufferMain->fill_box(grid_x, grid_y, grid_x + grid_size - 1, grid_y + grid_size - 1, 0);

        // Draw color swatches
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                unsigned char idx = row * 16 + col;
                int x1 = grid_x + col * (swatch + gap);
                int y1 = grid_y + row * (swatch + gap);
                BufferMain->fill_box(x1, y1, x1 + swatch - 1, y1 + swatch - 1, idx);
            }
        }

        // Draw selection highlight: white border inside the swatch (index 255 = white in LGR)
        int sel_x1 = grid_x + sel_col * (swatch + gap);
        int sel_y1 = grid_y + sel_row * (swatch + gap);
        int sel_x2 = sel_x1 + swatch - 1;
        int sel_y2 = sel_y1 + swatch - 1;
        int border = std::max(2, scale + 1);
        for (int i = 0; i < border; i++) {
            BufferMain->line(sel_x1 + i, sel_y1 + i, sel_x2 - i, sel_y1 + i, 255);
            BufferMain->line(sel_x1 + i, sel_y2 - i, sel_x2 - i, sel_y2 - i, 255);
            BufferMain->line(sel_x1 + i, sel_y1 + i, sel_x1 + i, sel_y2 - i, 255);
            BufferMain->line(sel_x2 - i, sel_y1 + i, sel_x2 - i, sel_y2 - i, 255);
        }

        // Draw index display below grid
        snprintf(index_text, sizeof(index_text), "Color: %d", sel_row * 16 + sel_col);
        MenuFont->write_centered(BufferMain, SCREEN_WIDTH / 2, grid_y + grid_size + 10, index_text);

        // Render with MenuPalette for background/text, LGR palette for grid (single present)
        MenuPalette->set();
        blit_overlay_with_palette(BufferMain, grid_x, grid_y, grid_x + grid_size - 1,
                                  grid_y + grid_size - 1, palette_data);
    }
}
