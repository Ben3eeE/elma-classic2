#include "menu/rec_list.h"
#include "fs_utils.h"
#include "menu/replay_cache.h"

static replay_cache Cache;

void rec_list::build_cache() { Cache.start(); }

bool rec_list::is_cache_ready() { return Cache.is_ready(); }

std::vector<std::string> rec_list::get_replays() {
    std::vector<std::string> filenames;
    recname filename;
    bool done = find_first("rec/*.rec", filename, MAX_REPLAY_NAME_LEN);
    while (!done) {
        filenames.emplace_back(filename);
        done = find_next(filename);
    }
    find_close();
    return filenames;
}

std::vector<std::string> rec_list::replays_for_level(int level_id) {
    Cache.sync(get_replays());
    return Cache.filenames_for_level(level_id);
}

void rec_list::add_new_replay(const std::string& filename, int level_id) {
    Cache.upsert(filename, level_id);
}
