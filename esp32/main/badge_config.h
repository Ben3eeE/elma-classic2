#ifndef BADGE_CONFIG_H
#define BADGE_CONFIG_H

// Disobey 2025 Badge — ESP32-S3 + ST7789 TFT

// Display
constexpr int BADGE_SCREEN_WIDTH = 320;
constexpr int BADGE_SCREEN_HEIGHT = 170;

// SPI pins for ST7789
constexpr int PIN_DISPLAY_SCK = 4;
constexpr int PIN_DISPLAY_MOSI = 5;
constexpr int PIN_DISPLAY_CS = 6;
constexpr int PIN_DISPLAY_DC = 15;
constexpr int PIN_DISPLAY_RST = 7;
constexpr int PIN_DISPLAY_BL = 19;

// Buttons
constexpr int PIN_BTN_UP = 11;
constexpr int PIN_BTN_DOWN = 1;
constexpr int PIN_BTN_LEFT = 21;
constexpr int PIN_BTN_RIGHT = 2;
constexpr int PIN_BTN_A = 13;
constexpr int PIN_BTN_B = 38;
constexpr int PIN_BTN_START = 12;
constexpr int PIN_BTN_SELECT = 45;
constexpr int PIN_BTN_JOY = 14;

// LEDs (SK6812MINI-EA)
constexpr int PIN_LED_DATA = 18;
constexpr int PIN_LED_ENABLE = 17;
constexpr int LED_COUNT = 10;

constexpr int DISPLAY_SPI_FREQ_HZ = 80000000;

constexpr int TARGET_FPS = 30;
constexpr int FRAME_TIME_MS = 1000 / TARGET_FPS;

constexpr int MAX_DIK_SCANCODES = 256;

#endif
