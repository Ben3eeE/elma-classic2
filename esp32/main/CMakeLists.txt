# ESP-IDF component CMakeLists.txt for Elma Classic Badge Port
# Includes game source files from the parent directory alongside ESP32-specific code.

set(GAME_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")

# Game source files reused from the main codebase.
# Files NOT included (replaced by stubs or not needed):
#   - main.cpp, platform_sdl.cpp, platform_sdl_keyboard.cpp (replaced by ESP32 platform)
#   - qopen.cpp (replaced by qopen_esp32.cpp for flash-embedded assets)
#   - sound_engine.cpp, wav.cpp (no audio on badge)
#   - recorder.cpp (no replays)
#   - state.cpp, eol_settings.cpp (replaced by hardcoded badge settings)
#   - gl_renderer.cpp (no OpenGL)
#   - menu_*.cpp (replaced by badge_menu.cpp)
#   - editor_*.cpp, EDITUJ.CPP, EDITTOLT.CPP, EDITTOOL.CPP (no editor)
#   - flagtag.cpp (no multiplayer)
#   - fs_utils.cpp (uses <filesystem>)
#   - best_times.cpp, keys.cpp, JATEKOS.CPP, skip.cpp (stubbed)
set(GAME_SOURCES
    # Math
    ${GAME_SRC_DIR}/vect2.cpp
    # Physics
    ${GAME_SRC_DIR}/physics_init.cpp
    ${GAME_SRC_DIR}/physics_move.cpp
    ${GAME_SRC_DIR}/physics_collision.cpp
    ${GAME_SRC_DIR}/segments.cpp
    ${GAME_SRC_DIR}/LEPTET.CPP
    # Level
    ${GAME_SRC_DIR}/level.cpp
    ${GAME_SRC_DIR}/level_load.cpp
    ${GAME_SRC_DIR}/polygon.cpp
    ${GAME_SRC_DIR}/object.cpp
    ${GAME_SRC_DIR}/sprite.cpp
    # Rendering
    ${GAME_SRC_DIR}/pic8.cpp
    ${GAME_SRC_DIR}/D_PIC.CPP
    ${GAME_SRC_DIR}/KIRAJ320.CPP
    ${GAME_SRC_DIR}/affine_pic.cpp
    ${GAME_SRC_DIR}/affine_pic_render.cpp
    ${GAME_SRC_DIR}/anim.cpp
    ${GAME_SRC_DIR}/grass.cpp
    ${GAME_SRC_DIR}/abc8.cpp
    ${GAME_SRC_DIR}/piclist.cpp
    ${GAME_SRC_DIR}/ECSET.CPP
    # Game loop
    ${GAME_SRC_DIR}/LEJATSZO.CPP
    # Utilities
    ${GAME_SRC_DIR}/platform_utils.cpp
    ${GAME_SRC_DIR}/timer.cpp
    # LGR loading (constructor/destructor from original, load/access overridden by lgr_esp32.cpp)
    ${GAME_SRC_DIR}/lgr.cpp
)

# ESP32-specific source files
set(ESP32_SOURCES
    main.cpp
    platform_esp32.cpp
    st7789_driver.cpp
    badge_input.cpp
    badge_menu.cpp
    qopen_esp32.cpp
    lgr_esp32.cpp
    stubs.cpp
    badge_leds.cpp
)

idf_component_register(
    SRCS ${GAME_SOURCES} ${ESP32_SOURCES}
    INCLUDE_DIRS
        "${GAME_SRC_DIR}"
        "${GAME_SRC_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}"
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_ledc
        esp_driver_spi
        esp_driver_rmt
        esp_timer
        spi_flash
)

# Force-include compat.h for all source files to provide ESP32 shims.
# This injects #include "compat.h" at the top of every compiled file,
# defining ESP32_BADGE and providing std::filesystem stubs.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -include "${CMAKE_CURRENT_LIST_DIR}/compat.h"
)

# Compile as C++20 to match the main codebase
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)

# Suppress warnings in the original game code that we can't fix here
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-narrowing
    -Wno-mismatched-new-delete
    -Wno-maybe-uninitialized
)

# Use linker wrapping to intercept fopen for LGR file access.
# lgr.cpp constructor calls fopen("lgr/default.lgr", "rb") — we redirect
# this to fmemopen() over the flash-embedded data.
target_link_options(${COMPONENT_LIB} INTERFACE
    -Wl,--wrap=fopen
)

# Embed game assets in flash.
# Copy elma.res and default.lgr to esp32/assets/ before building.
set(ASSET_DIR "${CMAKE_CURRENT_LIST_DIR}/../assets")

if(EXISTS "${ASSET_DIR}/Elma.res")
    target_add_binary_data(${COMPONENT_LIB} "${ASSET_DIR}/Elma.res" BINARY)
else()
    message(WARNING "Elma.res not found in ${ASSET_DIR} — game will fail to load resources!")
endif()

if(EXISTS "${ASSET_DIR}/Default.lgr")
    target_add_binary_data(${COMPONENT_LIB} "${ASSET_DIR}/Default.lgr" BINARY)
else()
    message(WARNING "Default.lgr not found in ${ASSET_DIR} — game will fail to load graphics!")
endif()
