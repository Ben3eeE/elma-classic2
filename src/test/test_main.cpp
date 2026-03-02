#include "test/replay_data_death.h"
#include "test/replay_data_vsync.h"
#include "test/test_replay.h"
#include <array>

static const replay_test_case replay_tests[] = {
    {"vsync", "vsync.lev", REPLAY_DATA(replay_vsync), true, false, 847, 8015},
    {"left volt death", "vsync.lev", REPLAY_DATA(death), false, true, 0, 78},
};

int main() {
    int failed = run_replay_tests(replay_tests, std::size(replay_tests));

    return failed > 0 ? 1 : 0;
}
