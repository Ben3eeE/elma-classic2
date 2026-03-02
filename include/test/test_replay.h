#ifndef TEST_REPLAY_H
#define TEST_REPLAY_H

#include <cstddef>
#include <cstdint>

struct replay_result {
    bool finished;
    bool died;
    long long time_centiseconds;
    size_t frames_consumed;
};

replay_result run_replay(const char* level_name, const uint8_t* kdata, const uint32_t* mdata,
                         size_t frame_count, uint32_t ms_start);

struct replay_test_case {
    const char* name;
    const char* level;
    const uint8_t* kdata;
    const uint32_t* mdata;
    size_t frame_count;
    uint32_t ms_start;
    bool expect_finished;
    bool expect_died;
    long long expected_time;
    size_t expected_frames;
};

#define REPLAY_DATA(prefix) prefix##_kdata, prefix##_mdata, prefix##_frame_count, prefix##_ms_start

int run_replay_tests(const replay_test_case* tests, size_t count);

#endif
