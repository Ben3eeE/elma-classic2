#include "badge_leds.h"
#include "badge_config.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

static const char* TAG = "leds";

static rmt_channel_handle_t LedChannel = nullptr;
static rmt_encoder_handle_t LedEncoder = nullptr;

constexpr int RMT_RESOLUTION_HZ = 10000000; // 10MHz = 100ns per tick

// GRB pixel buffer for SK6812
static uint8_t PixelBuffer[LED_COUNT * 3];

struct led_encoder_t {
    rmt_encoder_t base;
    rmt_encoder_t* bytes_encoder;
    rmt_encoder_t* copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
};

static size_t led_encode(rmt_encoder_t* encoder, rmt_channel_handle_t channel,
                         const void* primary_data, size_t data_size,
                         rmt_encode_state_t* ret_state) {
    auto* led_enc = reinterpret_cast<led_encoder_t*>(encoder);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_enc->state) {
    case 0:
        encoded_symbols += led_enc->bytes_encoder->encode(
            led_enc->bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_enc->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return encoded_symbols;
        }
        // fall through
    case 1:
        encoded_symbols += led_enc->copy_encoder->encode(
            led_enc->copy_encoder, channel, &led_enc->reset_code,
            sizeof(led_enc->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_enc->state = 0;
            *ret_state = RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
        }
        return encoded_symbols;
    }
    return encoded_symbols;
}

static esp_err_t led_del(rmt_encoder_t* encoder) {
    auto* led_enc = reinterpret_cast<led_encoder_t*>(encoder);
    rmt_del_encoder(led_enc->bytes_encoder);
    rmt_del_encoder(led_enc->copy_encoder);
    delete led_enc;
    return ESP_OK;
}

static esp_err_t led_reset(rmt_encoder_t* encoder) {
    auto* led_enc = reinterpret_cast<led_encoder_t*>(encoder);
    rmt_encoder_reset(led_enc->bytes_encoder);
    rmt_encoder_reset(led_enc->copy_encoder);
    led_enc->state = 0;
    return ESP_OK;
}

void badge_leds_init() {
    ESP_LOGI(TAG, "Initializing LEDs");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << PIN_LED_ENABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)PIN_LED_ENABLE, 1);

    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = (gpio_num_t)PIN_LED_DATA;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = RMT_RESOLUTION_HZ;
    tx_config.mem_block_symbols = 64;
    tx_config.trans_queue_depth = 4;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &LedChannel));

    auto* led_enc = new led_encoder_t();
    memset(led_enc, 0, sizeof(led_encoder_t));
    led_enc->base.encode = led_encode;
    led_enc->base.del = led_del;
    led_enc->base.reset = led_reset;

    // SK6812 timing: T0H=300ns T0L=900ns T1H=600ns T1L=600ns
    rmt_bytes_encoder_config_t bytes_config = {};
    bytes_config.bit0.duration0 = 3;
    bytes_config.bit0.level0 = 1;
    bytes_config.bit0.duration1 = 9;
    bytes_config.bit0.level1 = 0;
    bytes_config.bit1.duration0 = 6;
    bytes_config.bit1.level0 = 1;
    bytes_config.bit1.duration1 = 6;
    bytes_config.bit1.level1 = 0;
    bytes_config.flags.msb_first = 1;
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_config, &led_enc->bytes_encoder));

    rmt_copy_encoder_config_t copy_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_config, &led_enc->copy_encoder));

    // 280us reset signal
    led_enc->reset_code.duration0 = 2800;
    led_enc->reset_code.level0 = 0;
    led_enc->reset_code.duration1 = 2800;
    led_enc->reset_code.level1 = 0;

    LedEncoder = &led_enc->base;
    ESP_ERROR_CHECK(rmt_enable(LedChannel));

    badge_leds_off();
}

void badge_leds_set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < LED_COUNT; i++) {
        PixelBuffer[i * 3 + 0] = g; // GRB order
        PixelBuffer[i * 3 + 1] = r;
        PixelBuffer[i * 3 + 2] = b;
    }

    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;
    rmt_transmit(LedChannel, LedEncoder, PixelBuffer, sizeof(PixelBuffer), &tx_config);
    rmt_tx_wait_all_done(LedChannel, portMAX_DELAY);
}

void badge_leds_off() {
    badge_leds_set_all(0, 0, 0);
}

void badge_leds_flash(uint8_t r, uint8_t g, uint8_t b, int flash_count, int on_ms, int off_ms) {
    for (int i = 0; i < flash_count; i++) {
        badge_leds_set_all(r, g, b);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        badge_leds_off();
        if (i < flash_count - 1) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }
}
