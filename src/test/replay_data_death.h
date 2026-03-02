#ifndef REPLAY_DATA_DEATH_H
#define REPLAY_DATA_DEATH_H

#include <array>
#include <cstddef>
#include <cstdint>

// Hold left volt (0x04) for 100 frames at 16ms (~60fps).
// The bike flips and dies after ~1.241 seconds.

constexpr size_t DEATH_FRAME_COUNT = 100;

static constexpr auto death_kdata_arr = [] {
    std::array<uint8_t, DEATH_FRAME_COUNT> a{};
    a.fill(0x04);
    return a;
}();

static constexpr auto death_mdata_arr = [] {
    std::array<uint32_t, DEATH_FRAME_COUNT> a{};
    a.fill(16);
    return a;
}();

static const uint8_t* death_kdata = death_kdata_arr.data();
static const uint32_t* death_mdata = death_mdata_arr.data();
static const size_t death_frame_count = DEATH_FRAME_COUNT;
static const uint32_t death_ms_start = 0;

#endif
