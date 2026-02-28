#include "badge_input.h"
#include "badge_config.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <cstring>

static const char* TAG = "input";

struct button_map {
    int gpio_pin;
    int scancode;
    bool active_high;
};

static const button_map ButtonMap[] = {
    {PIN_BTN_UP, DIK_UP, false},
    {PIN_BTN_DOWN, DIK_DOWN, false},
    {PIN_BTN_LEFT, DIK_LEFT, false},
    {PIN_BTN_RIGHT, DIK_RIGHT, false},
    {PIN_BTN_A, DIK_SPACE, false},     // Gas
    {PIN_BTN_B, DIK_LSHIFT, false},    // Alovolt
    {PIN_BTN_START, DIK_RETURN, false}, // Turn / confirm
    {PIN_BTN_SELECT, DIK_ESCAPE, false}, // Back (GPIO45 strapping pin, active low)
    {PIN_BTN_JOY, DIK_RSHIFT, false},
};

constexpr int BUTTON_COUNT = sizeof(ButtonMap) / sizeof(ButtonMap[0]);

static bool CurrentState[MAX_DIK_SCANCODES];
static bool PreviousState[MAX_DIK_SCANCODES];

void badge_input_init() {
    ESP_LOGI(TAG, "Initializing badge buttons");

    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << ButtonMap[i].gpio_pin;
        io_conf.mode = GPIO_MODE_INPUT;

        if (ButtonMap[i].active_high) {
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        } else {
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
    }

    memset(CurrentState, 0, sizeof(CurrentState));
    memset(PreviousState, 0, sizeof(PreviousState));
}

void badge_input_clear() {
    memset(CurrentState, 0, sizeof(CurrentState));
    memset(PreviousState, 0, sizeof(PreviousState));
}

void badge_input_poll() {
    memcpy(PreviousState, CurrentState, sizeof(CurrentState));
    memset(CurrentState, 0, sizeof(CurrentState));

    for (int i = 0; i < BUTTON_COUNT; i++) {
        int level = gpio_get_level((gpio_num_t)ButtonMap[i].gpio_pin);
        bool pressed = ButtonMap[i].active_high ? (level == 1) : (level == 0);
        if (pressed) {
            CurrentState[ButtonMap[i].scancode] = true;
        }
    }
}

bool badge_is_key_down(int code) {
    if (code < 0 || code >= MAX_DIK_SCANCODES)
        return false;
    return CurrentState[code];
}

bool badge_was_key_just_pressed(int code) {
    if (code < 0 || code >= MAX_DIK_SCANCODES)
        return false;
    return CurrentState[code] && !PreviousState[code];
}

int badge_get_any_key_just_pressed() {
    for (int i = 1; i < MAX_DIK_SCANCODES; i++) {
        if (CurrentState[i] && !PreviousState[i]) {
            return i;
        }
    }
    return DIK_UNKNOWN;
}
