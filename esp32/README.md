# Elasto Mania — Disobey 2025 Badge

ESP32-S3 port targeting the Disobey 2025 conference badge (320x170 ST7789 TFT, D-pad + buttons, 10x SK6812 LEDs).

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)

## Assets

Copy from a working Elasto Mania installation into `assets/`:

- `Elma.res` — resource archive (levels, fonts, images)
- `Default.lgr` — default graphics pack (palette, textures, sprites)

## Build & Flash

```bash
cd esp32
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On macOS the port is typically `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`.