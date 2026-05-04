#ifndef EOL_PACER_H
#define EOL_PACER_H

namespace pacer {

constexpr double PHYS_MAX_DELTA = 0.0055;

void reset();

void tick();

bool phys_step_next(double* out_dt);

} // namespace pacer

#endif
