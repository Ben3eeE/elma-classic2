#include "eol/fps.h"
#include "eol/settings.h"
#include "eol/status_messages.h"
#include <cmath>
#include <format>

int CurrentFps = 0;
bool LiveFpsLimitEnabled = false;
float LiveFpsLimit = 0.0f;

namespace fps {

namespace {

// Shorter window for the first update to get a quicker initial reading
constexpr int COUNTER_INITIAL_INTERVAL_MS = 100;
// After first update use a longer interval for more stable readings
constexpr int COUNTER_UPDATE_INTERVAL_MS = 1000;

int recent_frames = 0;
long long counter_window_ms = 0;
bool counter_initial = true;

} // namespace

void reset_counter(long long now_ms) {
    recent_frames = 0;
    counter_window_ms = now_ms;
    counter_initial = true;
    CurrentFps = 0;
}

void tick_counter(long long now_ms) {
    recent_frames++;

    if (!EolSettings->show_fps_info()) {
        return;
    }

    const long long window_target =
        counter_initial ? COUNTER_INITIAL_INTERVAL_MS : COUNTER_UPDATE_INTERVAL_MS;

    const long long window_elapsed = now_ms - counter_window_ms;

    if (window_elapsed >= window_target) {
        CurrentFps = (int)std::round((double)recent_frames / (double)window_elapsed * 1000.0);

        if (counter_initial) {
            counter_initial = false;
        } else {
            counter_window_ms = now_ms;
            recent_frames = 0;
        }
    }
}

// Two distinct states are at play here:
//   - Live: what the pacer is currently using (LiveFpsLimit / LiveFpsLimitEnabled).
//     Doesn't change mid-run.
//   - Pending: what EolSettings holds. Applied to Live at the next run start.
//
// Each public operation reduces to: compute a wanted Pending state, apply it,
// then tell the user whether it changes Live or matches it.
namespace {

struct fps_pending {
    float limit;
    bool enabled;
};

void set_pending(fps_pending p) {
    EolSettings->set_fps_limit(p.limit);
    EolSettings->set_fps_limit_enabled(p.enabled);
}

bool live_matches(fps_pending p) {
    if (!p.enabled) {
        return !LiveFpsLimitEnabled;
    }
    return LiveFpsLimitEnabled && LiveFpsLimit == p.limit;
}

void request(fps_pending wanted) {
    bool was_already_pending = (wanted.limit == EolSettings->fps_limit() &&
                                wanted.enabled == EolSettings->fps_limit_enabled());
    set_pending(wanted);

    if (live_matches(wanted)) {
        if (wanted.enabled) {
            StatusMessages->add(std::format("FPS is already limited to {:g}", LiveFpsLimit));
        } else {
            StatusMessages->add("FPS limiter is already off");
        }
    } else if (!was_already_pending && !EolSettings->show_fps_info()) {
        if (wanted.enabled) {
            StatusMessages->add(
                std::format("Setting FPS limit to {:g} when the next run starts", wanted.limit));
        } else {
            StatusMessages->add("Turning FPS limiter off when the next run starts");
        }
    }
}

} // namespace

void put_change_pending(float new_limit) { request({new_limit, true}); }

void request_on() { request({EolSettings->fps_limit(), true}); }

void request_off() { request({LiveFpsLimit, false}); }

} // namespace fps
