#include "game/ghost_list.h"
#include <filesystem>

ghost_list Ghosts;

bool ghost_list::add(const std::string& path, int level_id) {
    replay_bike& ghost = ghosts.emplace_back();
    if (recorder::load_single(path, ghost.bike.rec) != level_id) {
        ghosts.pop_back();
        return false;
    }
    ghost.name = std::filesystem::path(path).stem().string();
    return true;
}

void ghost_list::rewind() {
    for (replay_bike& g : ghosts) {
        g.bike.rec.rewind();
        g.bike.meta.reset();
    }
}

bool ghost_list::advance(double time, bool rewinding) {
    bool all_finished = true;
    for (replay_bike& g : ghosts) {
        if (g.advance(time, rewinding)) {
            all_finished = false;
        }
    }
    return all_finished;
}
