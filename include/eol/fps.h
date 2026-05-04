#ifndef EOL_FPS_H
#define EOL_FPS_H

extern int CurrentFps;
extern bool LiveFpsLimitEnabled;
extern float LiveFpsLimit;

namespace fps {

void reset_counter(long long now_ms);

// Notify the counter that one real frame has been accepted at `now_ms`.
void tick_counter(long long now_ms);

// Console / menu hooks. These modify EolSettings — the limiter only picks
// the new values up at the next pacer::reset() (deferred-change semantics).
void put_change_pending(float new_limit);
void request_on();
void request_off();

} // namespace fps

#endif
