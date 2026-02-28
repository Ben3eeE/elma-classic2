#include "badge_config.h"
#include "badge_input.h"
#include "badge_leds.h"
#include "badge_menu.h"

#include "ECSET.H"
#include "EDITUJ.H"
#include "KIRAJZOL.H"
#include "LEJATSZO.H"
#include "LEPTET.H"
#include "M_PIC.H"
#include "menu_pic.h"
#include "abc8.h"
#include "eol_settings.h"
#include "level.h"
#include "lgr.h"
#include "main.h"
#include "physics_init.h"
#include "pic8.h"
#include "platform_impl.h"
#include "qopen.h"
#include "recorder.h"
#include "segments.h"
#include "state.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <directinput/scancodes.h>

static const char* TAG = "elma";

static double StopwatchStartTime = 0.0;

double stopwatch() { return get_milliseconds() * STOPWATCH_MULTIPLIER - StopwatchStartTime; }

void stopwatch_reset() { StopwatchStartTime = get_milliseconds() * STOPWATCH_MULTIPLIER; }

void delay(int milliseconds) {
    double current_time = stopwatch();
    while (stopwatch() / STOPWATCH_MULTIPLIER < current_time / STOPWATCH_MULTIPLIER + milliseconds) {
        if (was_key_just_pressed(DIK_ESCAPE)) {
            return;
        }
        handle_events();
    }
}

bool ErrorGraphicsLoaded = false;

void quit() { esp_restart(); }

int random_range(int maximum) { return rand() % maximum; }

void internal_error(const char* text1, const char* text2, const char* text3) {
    ESP_LOGE(TAG, "INTERNAL ERROR: %s %s %s", text1, text2 ? text2 : "", text3 ? text3 : "");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}

void external_error(const char* text1, const char* text2, const char* text3) {
    ESP_LOGE(TAG, "EXTERNAL ERROR: %s %s %s", text1, text2 ? text2 : "", text3 ? text3 : "");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}

static void init_game() {
    ESP_LOGI(TAG, "Initializing Elma Classic for Disobey 2025 Badge");

    SCREEN_WIDTH = BADGE_SCREEN_WIDTH;
    SCREEN_HEIGHT = BADGE_SCREEN_HEIGHT;

    EolSettings = new eol_settings();
    EolSettings->set_zoom_textures(true);
    EolSettings->set_zoom(0.85);
    EolSettings->set_pictures_in_background(true);

    platform_init();
    badge_leds_init();
    init_qopen();

    State = new state();
    Rec1 = new recorder();
    Rec2 = new recorder();

    init_physics_data();
    lgrfile::load_lgr_file("default");

    if (Lgr && Lgr->pal) {
        Lgr->pal->set();
    } else {
        ESP_LOGW(TAG, "No LGR palette available!");
    }

    init_menu_pictures();
    init_renderer();
    ErrorGraphicsLoaded = true;

    ESP_LOGI(TAG, "Initialization complete");
}

static void play_level(int level_index) {
    char filename[20];
    snprintf(filename, sizeof(filename), "QWQUU%03d.LEV", level_index);
    ESP_LOGI(TAG, "Playing level %d: %s", level_index, filename);

    delete Ptop;
    Ptop = new level(filename);

    lgrfile::load_lgr_file(Ptop->lgr_name);
    Ptop->discard_missing_lgr_assets(Lgr);

    delete Segments;
    Segments = new segments(Ptop);
    if (HeadRadius > Motor1->left_wheel.radius) {
        Segments->setup_collision_grid(HeadRadius);
    } else {
        Segments->setup_collision_grid(Motor1->left_wheel.radius);
    }

    canvas::create_canvases();

    Single = 1;
    Tag = 0;

    badge_input_clear();
    handle_events();

    long result = lejatszo(filename, CameraMode::Normal);

    if (result > 0) {
        ESP_LOGI(TAG, "Level completed! Time: %ld.%02ld", result / 100, result % 100);
        badge_leds_flash(0, 255, 0, 3, 200, 200);
    } else if (result == -1) {
        ESP_LOGI(TAG, "Level exited (died or ESC)");
        badge_leds_flash(255, 0, 0, 3, 200, 200);
    } else {
        ESP_LOGI(TAG, "Level exited with code: %ld", result);
    }
}

extern "C" void app_main() {
    srand((unsigned)clock());
    init_game();

    while (true) {
        int selected = badge_level_selector();
        if (selected > 0 && selected <= INTERNAL_LEVEL_COUNT) {
            play_level(selected);
        }
    }
}
