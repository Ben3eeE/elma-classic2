#ifndef GAME_GHOST_LIST_H
#define GAME_GHOST_LIST_H

#include "game/replay_bike.h"
#include <string>
#include <vector>

class ghost_list {
  public:
    void clear() { ghosts.clear(); }
    // Load the first bike of `path` as a ghost if it is a replay of `level_id`
    bool add(const std::string& path, int level_id);
    bool empty() const { return ghosts.empty(); }

    void rewind();
    // Returns true once every ghost has run out of frames
    bool advance(double time, bool rewinding);

    const std::vector<replay_bike>& all() const { return ghosts; }

  private:
    std::vector<replay_bike> ghosts;
};

extern ghost_list Ghosts;

#endif
