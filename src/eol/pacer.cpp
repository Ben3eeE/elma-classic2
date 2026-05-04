#include "eol/pacer.h"

namespace pacer {

static double time_ = 0.0;
static double time_passed_ = 0.0;

void reset() {
    time_ = 0.0;
    time_passed_ = 0.0;
}

void set_time_passed(double cel) { time_passed_ = cel; }

bool phys_step_next(double* out_dt) {
    if (time_ + 0.000001 >= time_passed_) {
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
