#include "eol/pacer.h"
#include "eol/fps.h"
#include "eol_settings.h"
#include "main.h"
#include "platform/implementation.h"
#include <algorithm>
#include <cstdint>

namespace pacer {

namespace {
constexpr int LIMITER_TIMEOUT_MS = 33;

constexpr double TIME_PASSED_FACTOR = STOPWATCH_MULTIPLIER * 0.0024;

static double time_ = 0.0;
static double time_passed_ = 0.0;

// eol-client-style time-freeze limiter.
static long long start_time_ms = 0;
static long long last_real_frame_ms = 0;
static uint64_t total_real_frames = 0;

} // namespace

void reset() {
    long long now = get_milliseconds();
    LiveFpsLimitEnabled = EolSettings->fps_limit_enabled();
    LiveFpsLimit = EolSettings->fps_limit();

    time_ = 0.0;
    time_passed_ = 0.0;

    start_time_ms = now;
    last_real_frame_ms = now;
    total_real_frames = 0;

    fps::reset_counter(now);
}

void tick() {
    long long now_ms = get_milliseconds();

    if (LiveFpsLimitEnabled) {
        long long elapsed = now_ms - start_time_ms;
        long long since_last = now_ms - last_real_frame_ms;
        bool over_limit =
            (total_real_frames * 1000ULL > (uint64_t)elapsed * (uint64_t)LiveFpsLimit);
        if (over_limit && since_last <= LIMITER_TIMEOUT_MS) {
            return;
        }
    }

    total_real_frames++;
    last_real_frame_ms = now_ms;

    long long elapsed = now_ms - start_time_ms;
    double phys_time = (double)elapsed * TIME_PASSED_FACTOR;
    phys_time = std::max(phys_time, 0.000001);
    time_passed_ = phys_time;

    fps::tick_counter(now_ms);
}

bool phys_step_next(double* out_dt) {
    if (time_ + 0.000001 > time_passed_) {
        return false;
    }
    double dt = PHYS_MAX_DELTA;
    if (time_ + dt > time_passed_) {
        dt = time_passed_ - time_;
    }
    *out_dt = dt;
    time_ += dt;
    return true;
}

} // namespace pacer
