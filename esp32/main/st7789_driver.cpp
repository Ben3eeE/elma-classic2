#include "st7789_driver.h"
#include "badge_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

static const char* TAG = "st7789";

static spi_device_handle_t SpiDevice;

constexpr int DMA_MAX_TRANSFER_SIZE = 4092;
constexpr int FRAME_BYTES = BADGE_SCREEN_WIDTH * BADGE_SCREEN_HEIGHT * 2;

// ST7789 commands
constexpr uint8_t CMD_SWRESET = 0x01;
constexpr uint8_t CMD_SLPOUT = 0x11;
constexpr uint8_t CMD_NORON = 0x13;
constexpr uint8_t CMD_INVON = 0x21;
constexpr uint8_t CMD_DISPON = 0x29;
constexpr uint8_t CMD_CASET = 0x2A;
constexpr uint8_t CMD_RASET = 0x2B;
constexpr uint8_t CMD_RAMWR = 0x2C;
constexpr uint8_t CMD_MADCTL = 0x36;
constexpr uint8_t CMD_COLMOD = 0x3A;

constexpr uint8_t MADCTL_MX = 0x40;
constexpr uint8_t MADCTL_MV = 0x20;
constexpr uint8_t MADCTL_RGB = 0x00;

static uint8_t* DmaBounceBuffer = nullptr;

static void gpio_set_dc(bool data) {
    gpio_set_level((gpio_num_t)PIN_DISPLAY_DC, data ? 1 : 0);
}

static void send_cmd(uint8_t cmd) {
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_data[0] = cmd;
    t.flags = SPI_TRANS_USE_TXDATA;
    gpio_set_dc(false);
    spi_device_polling_transmit(SpiDevice, &t);
}

static void send_data(const uint8_t* data, int len) {
    if (len == 0)
        return;
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    gpio_set_dc(true);
    spi_device_polling_transmit(SpiDevice, &t);
}

static void send_data_byte(uint8_t val) { send_data(&val, 1); }

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // 170-pixel display centered in 240-address space
    constexpr uint16_t COL_OFFSET = 0;
    constexpr uint16_t ROW_OFFSET = 35;

    uint8_t data[4];

    send_cmd(CMD_CASET);
    data[0] = (x0 + COL_OFFSET) >> 8;
    data[1] = (x0 + COL_OFFSET) & 0xFF;
    data[2] = (x1 + COL_OFFSET) >> 8;
    data[3] = (x1 + COL_OFFSET) & 0xFF;
    send_data(data, 4);

    send_cmd(CMD_RASET);
    data[0] = (y0 + ROW_OFFSET) >> 8;
    data[1] = (y0 + ROW_OFFSET) & 0xFF;
    data[2] = (y1 + ROW_OFFSET) >> 8;
    data[3] = (y1 + ROW_OFFSET) & 0xFF;
    send_data(data, 4);

    send_cmd(CMD_RAMWR);
}

void st7789_init() {
    ESP_LOGI(TAG, "Initializing ST7789 display");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << PIN_DISPLAY_DC) | (1ULL << PIN_DISPLAY_RST);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    gpio_set_level((gpio_num_t)PIN_DISPLAY_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)PIN_DISPLAY_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_DISPLAY_MOSI;
    bus_cfg.sclk_io_num = PIN_DISPLAY_SCK;
    bus_cfg.miso_io_num = -1;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = DMA_MAX_TRANSFER_SIZE;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = DISPLAY_SPI_FREQ_HZ;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = PIN_DISPLAY_CS;
    dev_cfg.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &SpiDevice));

    send_cmd(CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    send_cmd(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    send_cmd(CMD_COLMOD);
    send_data_byte(0x55); // 16-bit RGB565

    send_cmd(CMD_MADCTL);
    send_data_byte(MADCTL_MV | MADCTL_MX | MADCTL_RGB);

    send_cmd(CMD_INVON); // required for correct ST7789 colors
    send_cmd(CMD_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));

    set_window(0, 0, BADGE_SCREEN_WIDTH - 1, BADGE_SCREEN_HEIGHT - 1);

    DmaBounceBuffer = (uint8_t*)heap_caps_malloc(DMA_MAX_TRANSFER_SIZE, MALLOC_CAP_DMA);
    if (!DmaBounceBuffer) {
        ESP_LOGE(TAG, "Failed to allocate DMA bounce buffer!");
    }

    st7789_set_backlight(255);
    ESP_LOGI(TAG, "ST7789 initialized: %dx%d landscape", BADGE_SCREEN_WIDTH, BADGE_SCREEN_HEIGHT);
}

void st7789_wait_transfer() {}

void st7789_send_framebuffer(const uint16_t* buffer) {
    set_window(0, 0, BADGE_SCREEN_WIDTH - 1, BADGE_SCREEN_HEIGHT - 1);
    gpio_set_dc(true);

    const uint8_t* src = (const uint8_t*)buffer;
    int remaining = FRAME_BYTES;

    while (remaining > 0) {
        int chunk = remaining > DMA_MAX_TRANSFER_SIZE ? DMA_MAX_TRANSFER_SIZE : remaining;
        memcpy(DmaBounceBuffer, src, chunk);

        spi_transaction_t t = {};
        t.length = chunk * 8;
        t.tx_buffer = DmaBounceBuffer;
        spi_device_polling_transmit(SpiDevice, &t);

        src += chunk;
        remaining -= chunk;
    }
}

void st7789_set_backlight(uint8_t brightness) {
    static bool ledc_initialized = false;
    if (!ledc_initialized) {
        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
        timer_cfg.timer_num = LEDC_TIMER_0;
        timer_cfg.freq_hz = 5000;
        timer_cfg.clk_cfg = LEDC_AUTO_CLK;
        ledc_timer_config(&timer_cfg);

        ledc_channel_config_t ch_cfg = {};
        ch_cfg.gpio_num = PIN_DISPLAY_BL;
        ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        ch_cfg.channel = LEDC_CHANNEL_0;
        ch_cfg.timer_sel = LEDC_TIMER_0;
        ch_cfg.duty = 0;
        ch_cfg.hpoint = 0;
        ledc_channel_config(&ch_cfg);
        ledc_initialized = true;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
