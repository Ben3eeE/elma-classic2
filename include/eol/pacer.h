#ifndef EOL_PACER_H
#define EOL_PACER_H

namespace pacer {

constexpr double PHYS_MAX_DELTA = 0.0055;

void reset();

// Returns true and bumps time_passed if this iter is not frozen by the
// fps_limit cel-freeze check. Caller should sample input on each true return.
// Mirrors eol-client's LimitFPS accept branch.
bool try_real_frame();

// Catch-up iterator. Writes the next physics dt into *out_dt and advances the
// internal time cursor. Returns false when time has caught up to time_passed.
// Fixed at PHYS_MAX_DELTA, last step shrunk to fit.
bool phys_step_next(double* out_dt);

} // namespace pacer

#endif
