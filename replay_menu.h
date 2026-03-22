#ifndef REPLAY_MENU_H
#define REPLAY_MENU_H

#include <string>
#include <vector>

enum class LoadReplayResult { Success, Fail, Abort };

std::vector<std::string> find_replay_files();
LoadReplayResult validate_replay_level(int level_id, const std::string& filename);
LoadReplayResult load_replay(const std::string& filename);

void menu_replay_all();
void menu_replay_level(int level_id);

#endif
