# Asset Files for ESP32 Badge Build

Place the following files in this directory before building:

1. **`elma.res`** — Copy from a working Elasto Mania installation.
   This is the encrypted resource archive containing internal levels (.lev),
   bitmap fonts (.fnt), PCX images, and other game data.

2. **`default.lgr`** — Copy from `lgr/default.lgr` in a working installation.
   This is the default graphics pack containing the palette, textures,
   pictures, masks, bike sprites, and animations.

## Build

```bash
cd esp32
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```
