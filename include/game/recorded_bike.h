#ifndef GAME_RECORDED_BIKE_H
#define GAME_RECORDED_BIKE_H

#include "game/recorder.h"

struct motorst;

struct turning_data {
    int flipped;
    double turn_time;
    double turn_phase;
};

struct bike_metadata {
    double volt_time;
    bool volt_is_right;

    bool turn_key_previous;
    bool one_turn_used;

    double arm_position;

    turning_data bike_turning;
    turning_data camera_turning;

    void reset();
};

void update_graphical_metadata(bike_metadata& meta, motorst* mot, recorder* rec, bool update_rec,
                               double time);
// During rewind, compute animation state from the recorder's event list
// instead of relying on the forward-only state machine.
void rewind_override_animations(bike_metadata& meta, motorst* mot, recorder* rec, double time);

#endif
