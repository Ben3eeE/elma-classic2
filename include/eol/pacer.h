#ifndef EOL_PACER_H
#define EOL_PACER_H

namespace pacer {

constexpr double PHYS_MAX_DELTA = 0.0055;

void reset();

// Per-iter pacer tick. Advances `time_passed_` to current wall-clock-derived cel time and
// ticks the fps counter, unless the fps_limit cel-freeze check says this iter is over
// budget — in which case `time_passed_` is left where it was and the next phys_step_next
// will be a no-op. Mirrors eol-client's LimitFPS accept branch.
void tick();

// Catch-up iterator. Writes the next physics dt into *out_dt and advances the
// internal time cursor. Returns false when time has caught up to time_passed.
// Fixed at PHYS_MAX_DELTA, last step shrunk to fit.
bool phys_step_next(double* out_dt);

} // namespace pacer

#endif
