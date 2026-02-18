#include "color_picker.h"
#include "M_PIC.H"
#include "menu_pic.h"
#include "pic8.h"
#include "platform_impl.h"
#include <algorithm>
#include <directinput/scancodes.h>

int color_picker(unsigned char* palette_data, unsigned char initial_index) {
    int scale = std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / 480;
    int swatch = std::max(1, 40 * scale);
    int gap = std::max(1, 3 * scale);
    int grid_size = 16 * (swatch + gap) - gap;

    int grid_x = SCREEN_WIDTH / 2 - grid_size / 2;
    int grid_y = SCREEN_HEIGHT / 2 - grid_size / 2;

    int sel_col = initial_index % 16;
    int sel_row = initial_index / 16;

    palette lgr_palette(palette_data);

    while (true) {
        handle_events();

        if (was_key_just_pressed(DIK_ESCAPE)) {
            MenuPalette->set();
            return -1;
        }
        if (was_key_just_pressed(DIK_RETURN)) {
            MenuPalette->set();
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

        // Clear screen with index 0 (black in LGR palettes)
        BufferMain->fill_box(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 0);

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

        // Render entire screen with LGR palette
        lgr_palette.set();
        bltfront(BufferMain);
    }
}
