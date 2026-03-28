#ifndef REC_LIST_H
#define REC_LIST_H

#include <string>
#include <vector>

class rec_list {
  public:
    // Scan rec/ folder and return all .rec filenames
    static std::vector<std::string> get_replays();
};

#endif
