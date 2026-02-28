#include "platform_impl.h"
#include "platform_esp32.h"
#include "badge_config.h"
#include "badge_input.h"
#include "st7789_driver.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdlib>
#include <cstring>

static const char* TAG = "platform";

static unsigned char* PaletteFramebuffer = nullptr;
static unsigned char** RowPointers = nullptr;

static uint16_t* DisplayBufferA = nullptr;
static uint16_t* DisplayBufferB = nullptr;
static uint16_t* CurrentDisplayBuffer = nullptr;

static uint16_t PaletteLut[256];
static unsigned char CurrentPaletteData[768];

static inline uint16_t rgb888_to_rgb565(unsigned char r, unsigned char g, unsigned char b) {
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (color >> 8) | (color << 8); // byte-swap for SPI
}

static void rebuild_palette_lut() {
    for (int i = 0; i < 256; i++) {
        PaletteLut[i] = rgb888_to_rgb565(
            CurrentPaletteData[i * 3], CurrentPaletteData[i * 3 + 1], CurrentPaletteData[i * 3 + 2]);
    }
}

palette::palette(unsigned char* palette_data) {
    data = malloc(768);
    if (data) {
        memcpy(data, palette_data, 768);
    }
}

palette::~palette() { free(data); }

void palette::set() {
    if (data) {
        memcpy(CurrentPaletteData, data, 768);
        rebuild_palette_lut();
    }
}

void esp32_hardware_init() {
    ESP_LOGI(TAG, "Allocating framebuffers in PSRAM");

    int fb_size = BADGE_SCREEN_WIDTH * BADGE_SCREEN_HEIGHT;

    PaletteFramebuffer =
        (unsigned char*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!PaletteFramebuffer) {
        ESP_LOGE(TAG, "Failed to allocate palette framebuffer");
        abort();
    }
    memset(PaletteFramebuffer, 0, fb_size);

    RowPointers = (unsigned char**)malloc(BADGE_SCREEN_HEIGHT * sizeof(unsigned char*));
    if (!RowPointers) {
        ESP_LOGE(TAG, "Failed to allocate row pointers");
        abort();
    }

    int display_size = fb_size * sizeof(uint16_t);
    DisplayBufferA =
        (uint16_t*)heap_caps_malloc(display_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    DisplayBufferB =
        (uint16_t*)heap_caps_malloc(display_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!DisplayBufferA || !DisplayBufferB) {
        ESP_LOGE(TAG, "Failed to allocate display buffers");
        abort();
    }
    memset(DisplayBufferA, 0, display_size);
    memset(DisplayBufferB, 0, display_size);
    CurrentDisplayBuffer = DisplayBufferA;

    st7789_init();
    badge_input_init();
}

void platform_init() { esp32_hardware_init(); }

void init_sound() {}

void handle_events() {
    badge_input_poll();
    vTaskDelay(1); // yield to FreeRTOS watchdog
}

unsigned char** lock_backbuffer(bool flipped) {
    if (flipped) {
        for (int y = 0; y < BADGE_SCREEN_HEIGHT; y++) {
            RowPointers[y] =
                PaletteFramebuffer + (BADGE_SCREEN_HEIGHT - 1 - y) * BADGE_SCREEN_WIDTH;
        }
    } else {
        for (int y = 0; y < BADGE_SCREEN_HEIGHT; y++) {
            RowPointers[y] = PaletteFramebuffer + y * BADGE_SCREEN_WIDTH;
        }
    }
    return RowPointers;
}

void unlock_backbuffer() {
    int pixel_count = BADGE_SCREEN_WIDTH * BADGE_SCREEN_HEIGHT;
    convert_palette_to_rgb565(PaletteFramebuffer, CurrentDisplayBuffer, pixel_count);
    st7789_send_framebuffer(CurrentDisplayBuffer);
    swap_display_buffer();
}

unsigned char** lock_frontbuffer(bool flipped) { return lock_backbuffer(flipped); }

void unlock_frontbuffer() { unlock_backbuffer(); }

void get_mouse_position(int* x, int* y) {
    *x = 0;
    *y = 0;
}

void set_mouse_position(int, int) {}
bool was_left_mouse_just_clicked() { return false; }
bool was_right_mouse_just_clicked() { return false; }
void show_cursor() {}
void hide_cursor() {}

bool is_key_down(DikScancode code) { return badge_is_key_down(code); }
bool was_key_just_pressed(DikScancode code) { return badge_was_key_just_pressed(code); }
DikScancode get_any_key_just_pressed() { return badge_get_any_key_just_pressed(); }
bool was_key_down(DikScancode code) { return badge_is_key_down(code); }

int get_mouse_wheel_delta() { return 0; }
bool is_fullscreen() { return true; }
long long get_milliseconds() { return (long long)(esp_timer_get_time() / 1000); }
void platform_recreate_window() {}
bool has_window() { return true; }
void message_box(const char* text) { ESP_LOGE(TAG, "Message: %s", text); }

void convert_palette_to_rgb565(const unsigned char* src, uint16_t* dst, int pixel_count) {
    for (int i = 0; i < pixel_count; i++) {
        dst[i] = PaletteLut[src[i]];
    }
}

uint16_t* get_display_buffer() { return CurrentDisplayBuffer; }

void swap_display_buffer() {
    CurrentDisplayBuffer =
        (CurrentDisplayBuffer == DisplayBufferA) ? DisplayBufferB : DisplayBufferA;
}
