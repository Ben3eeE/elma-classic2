#ifndef EOL_PACER_H
#define EOL_PACER_H

namespace pacer {

constexpr double PHYS_MAX_DELTA = 0.0055;

void reset();

// Tell the pacer that wall-clock time has advanced to `cel` cel-units. Used as
// the catch-up target by phys_step_next.
void set_time_passed(double cel);

bool phys_step_next(double* out_dt);

} // namespace pacer

#endif
