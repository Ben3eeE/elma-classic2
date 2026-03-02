#include "test/test_replay.h"
#include "LEJATSZO.H"
#include "level.h"
#include "physics_forces.h"
#include "physics_init.h"
#include "segments.h"
#include "test/platform_headless.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern level* Ptop;
extern segments* Segments;

struct replay_timer {
    double time;
    double time_passed;
    double delta;
    double time_adelta;
    int frame_real;
    int mainloop;
    uint32_t ms_time;
    double time_start;
};

static void timer_init(replay_timer* t, uint32_t ms_start) {
    t->time = 0.0;
    t->time_adelta = 0.0;
    t->delta = 0.0;
    t->frame_real = 1;
    t->mainloop = 1;
    t->ms_time = ms_start;
    t->time_start = (double)t->ms_time * 0.182;
    t->time_passed = 0.0;
}

static int timer_next(replay_timer* t, uint32_t ms_delta) {
    if (!t->mainloop) {
        t->time += t->delta;
        if (t->time > t->time_passed - 0.000001) {
            t->mainloop = 1;
        }
    }

    if (t->mainloop) {
        t->ms_time += ms_delta;
        double gettime = (double)t->ms_time * 0.182 - t->time_start;
        t->time_passed = gettime * 0.0024;
        t->time_passed = std::max(t->time_passed, 0.000001);
        t->mainloop = 0;
    }

    if (t->time <= t->time_passed - 0.000001) {
        t->delta = 0.0055;
        if (t->time + 0.0055 > t->time_passed) {
            t->delta = t->time_passed - t->time;
        }
        t->time_adelta = t->time + t->delta;
        t->frame_real = (t->time_adelta > t->time_passed - 0.000001) ? 1 : 0;
        return 1;
    }

    t->frame_real = 1;
    t->mainloop = 1;
    return 2;
}

replay_result run_replay(const char* level_name, const uint8_t* kdata, const uint32_t* mdata,
                         size_t frame_count, uint32_t ms_start) {
    replay_result result = {};

    level* lev = new level(level_name);
    Ptop = lev;

    init_physics_data();
    lev->flip_objects();
    lev->sort_objects();

    Kajakell = lev->initialize_objects(Motor1);

    Segments = new segments(lev);
    double max_radius = Motor1->left_wheel.radius;
    max_radius = std::max(Motor1->right_wheel.radius, max_radius);
    max_radius = std::max(HeadRadius, max_radius);
    Segments->setup_collision_grid(max_radius);

    reset_motor_forces(Motor1);
    reset_event_buffer();

    player_keys test_keys = {};
    test_keys.gas = DikScancode(1);
    test_keys.brake = DikScancode(2);
    test_keys.right_volt = DikScancode(3);
    test_keys.left_volt = DikScancode(4);
    test_keys.turn = DikScancode(5);
    test_keys.alovolt = DikScancode(0);
    test_keys.brake_alias = DikScancode(0);

    valtozok valt = {};
    memset(&valt, 0, sizeof(valt));
    valt.baljobbv_f.ucsoford = -1000.0;
    valt.baljobbv_f.ucsoforgas = -1000.0;
    valt.baljobbv_h.ucsoford = -1000.0;
    valt.baljobbv_h.ucsoforgas = -1000.0;
    valt.utolsougras = -100.0;

    Rec1 = new recorder();
    Rec1->erase("");

    viewtimest vt = {};
    int masikshow = 0;

    replay_timer timer;
    timer_init(&timer, ms_start);

    uint8_t current_input = 0;
    uint32_t current_ms_delta = 0;

    int died = 0;
    long finish_time = 0;
    size_t input_frame = 0;

    while (!died && !finish_time) {
        if (timer.frame_real) {
            if (input_frame >= frame_count) {
                break;
            }
            current_input = kdata[input_frame];
            current_ms_delta = mdata[input_frame];
            input_frame++;

            clear_headless_keys();
            if (current_input & 0x01) {
                set_headless_key(test_keys.gas, true);
            }
            if (current_input & 0x02) {
                set_headless_key(test_keys.brake, true);
            }
            if (current_input & 0x04) {
                set_headless_key(test_keys.left_volt, true);
            }
            if (current_input & 0x08) {
                set_headless_key(test_keys.right_volt, true);
            }
            if (current_input & 0x10) {
                set_headless_key(test_keys.turn, true);
            }
        }

        int r = timer_next(&timer, current_ms_delta);

        if (r == 1) {
            belsoresz(Motor1, &test_keys, &valt, Rec1, &finish_time, &died, timer.time,
                      timer.delta);
        }

        if (timer.frame_real && !died && !finish_time) {
            kulsoresz(Motor1, &test_keys, &valt, Rec1, &vt, timer.time, &masikshow, 0);
        }

        if (r == 2) {
            continue;
        }
    }

    result.finished = (finish_time > 0);
    result.died = (died != 0);
    if (finish_time > 0) {
        result.time_centiseconds = finish_time;
    }
    result.frames_consumed = input_frame;

    Rec1 = nullptr;
    delete Segments;
    Segments = nullptr;
    delete lev;
    Ptop = nullptr;

    return result;
}

int run_replay_tests(const replay_test_case* tests, size_t count) {
    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < count; i++) {
        const replay_test_case& tc = tests[i];
        printf("Test: replay %s (%s)\n", tc.name, tc.level);

        replay_result result =
            run_replay(tc.level, tc.kdata, tc.mdata, tc.frame_count, tc.ms_start);

        auto check = [&](const char* label, bool condition) {
            if (condition) {
                printf("  PASS: %s\n", label);
                passed++;
            } else {
                printf("  FAIL: %s\n", label);
                failed++;
            }
        };

        check("finished", result.finished == tc.expect_finished);
        check("died", result.died == tc.expect_died);
        if (tc.expected_time > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "time is %ld centiseconds", tc.expected_time);
            check(buf, result.time_centiseconds == tc.expected_time);
        }
        if (tc.expected_frames > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "frames consumed is %zu", tc.expected_frames);
            check(buf, result.frames_consumed == tc.expected_frames);
        }
    }

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed;
}
