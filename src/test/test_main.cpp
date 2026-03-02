#include "test/test_replay.h"
#include <array>

static const replay_test_case replay_tests[] = {};

int main() {
    int failed = run_replay_tests(replay_tests, std::size(replay_tests));

    return failed > 0 ? 1 : 0;
}
