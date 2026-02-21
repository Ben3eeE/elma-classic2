#ifndef REPLAY_CACHE_H
#define REPLAY_CACHE_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class replay_cache {
    std::unordered_map<int, std::vector<std::string>> entries_;
    std::mutex mutex_;
    std::atomic<bool> ready_{false};
    std::thread worker_;

  public:
    ~replay_cache();

    // Start background scan of rec/ directory
    void start();
    // Block until scan completes
    void wait();

    bool is_ready() const;

    // Thread-safe accessors — return copies
    std::vector<std::string> all_filenames() const;
    std::vector<std::string> filenames_for_level(int level_id) const;

    // Called after saving a replay — updates existing entry or inserts new one
    void upsert(const std::string& filename, int level_id);
};

extern replay_cache ReplayCache;

#endif
