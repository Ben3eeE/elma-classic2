#ifndef REC_LIST_H
#define REC_LIST_H

#include <string>
#include <vector>

class rec_list {
  public:
    // Build cache of replay headers in the background
    static void build_cache();
    // Check if the background cache has finished building
    static bool is_cache_ready();
    // Scan rec/ folder and return all .rec filenames
    static std::vector<std::string> get_replays();
    // Return filenames for replays matching a given level_id
    static std::vector<std::string> replays_for_level(int level_id);
    // Notify that a new replay was saved so the cache can be updated
    static void add_new_replay(const std::string& filename, int level_id);
};

#endif
