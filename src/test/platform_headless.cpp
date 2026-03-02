#include "pic8.h"
#include "platform/implementation.h"
#include "test/platform_headless.h"
#include <chrono>
#include <cstdio>
#include <cstring>

static unsigned char DummyBuffer[480 * 640];

static bool HeadlessKeyState[256] = {};

void set_headless_key(DikScancode code, bool down) { HeadlessKeyState[(unsigned)code] = down; }

void clear_headless_keys() { memset(HeadlessKeyState, 0, sizeof(HeadlessKeyState)); }

void platform_init() {}

void platform_recreate_window() {}

void init_sound() {}

long long get_milliseconds() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void handle_events() {}

void platform_apply_fullscreen_mode() {}

bool is_key_down(DikScancode code) { return HeadlessKeyState[(unsigned)code]; }
bool was_key_just_pressed(DikScancode) { return false; }
DikScancode get_any_key_just_pressed() { return 0; }
bool was_key_down(DikScancode) { return false; }

void get_mouse_position(int* x, int* y) {
    *x = 0;
    *y = 0;
}
void set_mouse_position(int, int) {}
bool was_left_mouse_just_clicked() { return false; }
bool was_right_mouse_just_clicked() { return false; }
void show_cursor() {}
void hide_cursor() {}
int get_mouse_wheel_delta() { return 0; }

void lock_backbuffer(pic8& view, bool flipped) {
    view.subview(640, 480, DummyBuffer, 640, flipped);
}

void unlock_backbuffer() {}

void lock_frontbuffer(pic8& view, bool flipped) {
    view.subview(640, 480, DummyBuffer, 640, flipped);
}

void unlock_frontbuffer() {}

palette::palette(unsigned char*)
    : data(nullptr) {}
palette::~palette() {}
void palette::set() {}

void message_box(const char* text) { fprintf(stderr, "Message: %s\n", text); }

bool has_window() { return false; }
bool is_fullscreen() { return false; }
