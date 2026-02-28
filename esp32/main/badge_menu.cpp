#include "badge_menu.h"
#include "badge_config.h"

#include "M_PIC.H"
#include "abc8.h"
#include "level.h"
#include "main.h"
#include "pic8.h"
#include "platform_impl.h"
#include "qopen.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstring>
#include <directinput/scancodes.h>

static abc8* BadgeFont = nullptr;

constexpr unsigned char COLOR_BLACK = 254;
constexpr unsigned char COLOR_GREEN = 248;

static int LastSelected = 0;
static int LastScrollOffset = 0;

constexpr int REPEAT_INITIAL_MS = 400;
constexpr int REPEAT_RATE_MS = 80;

void badge_menu_init() {
    if (!BadgeFont) {
        BadgeFont = new abc8("kisbetu1.abc");
    }
}

static void draw_menu(pic8* buffer, int selected, int scroll_offset) {
    buffer->fill_box(COLOR_BLACK);

    constexpr int LINE_HEIGHT = 14;
    constexpr int HEADER_Y = 20;
    constexpr int LIST_START_Y = 38;
    constexpr int LEFT_MARGIN = 10;

    char header[40];
    snprintf(header, sizeof(header), "ELASTO MANIA  [%d]", selected + 1);
    BadgeFont->write(buffer, LEFT_MARGIN, HEADER_Y, header);

    int visible_lines = (BADGE_SCREEN_HEIGHT - LIST_START_Y) / LINE_HEIGHT;
    for (int i = 0; i < visible_lines; i++) {
        int level_index = scroll_offset + i;
        if (level_index >= INTERNAL_LEVEL_COUNT)
            break;

        int y = LIST_START_Y + i * LINE_HEIGHT;
        const char* name = get_internal_level_name(level_index);

        char line[40];
        snprintf(line, sizeof(line), "%2d. %s", level_index + 1, name);

        if (level_index == selected) {
            // Font has negative y_offset so text renders above y
            buffer->fill_box(LEFT_MARGIN - 2, y - 12, BADGE_SCREEN_WIDTH - LEFT_MARGIN, y,
                             COLOR_GREEN);
        }
        BadgeFont->write(buffer, LEFT_MARGIN, y, line);
    }
}

int badge_level_selector() {
    badge_menu_init();

    int selected = LastSelected;
    int scroll_offset = LastScrollOffset;
    constexpr int LINE_HEIGHT = 14;
    int visible_lines = (BADGE_SCREEN_HEIGHT - 38) / LINE_HEIGHT;

    if (selected >= INTERNAL_LEVEL_COUNT) {
        selected = INTERNAL_LEVEL_COUNT - 1;
    }
    if (scroll_offset > selected) {
        scroll_offset = selected;
    }
    if (selected >= scroll_offset + visible_lines) {
        scroll_offset = selected - visible_lines + 1;
    }

    long long up_press_time = 0;
    long long down_press_time = 0;
    bool up_repeating = false;
    bool down_repeating = false;

    while (true) {
        handle_events();
        long long now = get_milliseconds();

        bool move_up = false;
        bool move_down = false;

        if (was_key_just_pressed(DIK_UP)) {
            move_up = true;
            up_press_time = now;
            up_repeating = false;
        } else if (is_key_down(DIK_UP)) {
            long long held = now - up_press_time;
            if (!up_repeating && held >= REPEAT_INITIAL_MS) {
                move_up = true;
                up_repeating = true;
                up_press_time = now;
            } else if (up_repeating && held >= REPEAT_RATE_MS) {
                move_up = true;
                up_press_time = now;
            }
        }

        if (was_key_just_pressed(DIK_DOWN)) {
            move_down = true;
            down_press_time = now;
            down_repeating = false;
        } else if (is_key_down(DIK_DOWN)) {
            long long held = now - down_press_time;
            if (!down_repeating && held >= REPEAT_INITIAL_MS) {
                move_down = true;
                down_repeating = true;
                down_press_time = now;
            } else if (down_repeating && held >= REPEAT_RATE_MS) {
                move_down = true;
                down_press_time = now;
            }
        }

        if (move_up && selected > 0) {
            selected--;
            if (selected < scroll_offset)
                scroll_offset = selected;
        }
        if (move_down && selected < INTERNAL_LEVEL_COUNT - 1) {
            selected++;
            if (selected >= scroll_offset + visible_lines)
                scroll_offset = selected - visible_lines + 1;
        }

        if (was_key_just_pressed(DIK_RETURN) || was_key_just_pressed(DIK_SPACE)) {
            LastSelected = selected;
            LastScrollOffset = scroll_offset;
            return selected + 1;
        }

        if (was_key_just_pressed(DIK_ESCAPE)) {
            LastSelected = selected;
            LastScrollOffset = scroll_offset;
            return 0;
        }

        pic8* buffer = lockbackbuffer_pic(false);
        draw_menu(buffer, selected, scroll_offset);
        unlockbackbuffer_pic();

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
    }
}
