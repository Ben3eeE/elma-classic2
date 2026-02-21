#include "menu/replay_cache.h"
#include "debug/profiler.h"
#include "fs_utils.h"
#include "recorder.h"
#include <filesystem>

replay_cache ReplayCache;

replay_cache::~replay_cache() {
    stop_requested_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void replay_cache::start() {
    if (worker_.joinable()) {
        return;
    }
    worker_ = std::thread([this] {
        START_TIME(cache_timer);
        std::unordered_map<int, std::vector<std::string>> scanned;
        std::unordered_map<std::string, int> scanned_reverse;

        std::filesystem::directory_iterator it;
        try {
            it = std::filesystem::directory_iterator("rec");
        } catch (const std::filesystem::filesystem_error&) {
            // rec/ directory doesn't exist - nothing to scan
            ready_.store(true, std::memory_order_release);
            return;
        }

        for (const auto& entry : it) {
            if (stop_requested_.load(std::memory_order_relaxed)) {
                return;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto& path = entry.path();
            if (path.extension() != ".rec") {
                continue;
            }
            std::string stem = path.stem().string();
            if (stem.size() > MAX_REPLAY_NAME_LEN) {
                continue;
            }

            std::string filename = path.filename().string();
            auto header = recorder::read_header(filename);
            if (!header) {
                continue;
            }

            scanned[header->level_id].push_back(filename);
            scanned_reverse[filename] = header->level_id;
        }

        {
            std::scoped_lock lock(mutex_);
            entries_ = std::move(scanned);
            filename_to_level_ = std::move(scanned_reverse);
            apply_pending();
        }
        END_TIME(cache_timer, "Replay cache")
        ready_.store(true, std::memory_order_release);
    });
}

void replay_cache::wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool replay_cache::is_ready() const { return ready_.load(std::memory_order_acquire); }

std::vector<std::string> replay_cache::all_filenames() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, filenames] : entries_) {
        result.insert(result.end(), filenames.begin(), filenames.end());
    }
    return result;
}

std::vector<std::string> replay_cache::filenames_for_level(int level_id) const {
    std::scoped_lock lock(mutex_);
    auto it = entries_.find(level_id);
    if (it != entries_.end()) {
        return it->second;
    }
    return {};
}

void replay_cache::upsert(const std::string& filename, int level_id) {
    std::scoped_lock lock(mutex_);
    if (!ready_.load(std::memory_order_acquire)) {
        pending_.emplace_back(filename, level_id);
        return;
    }
    apply_one(filename, level_id);
}

void replay_cache::apply_one(const std::string& filename, int level_id) {
    auto it = filename_to_level_.find(filename);
    if (it != filename_to_level_.end()) {
        if (it->second == level_id) {
            return;
        }
        auto& old_bucket = entries_[it->second];
        std::erase(old_bucket, filename);
    }
    entries_[level_id].push_back(filename);
    filename_to_level_[filename] = level_id;
}

void replay_cache::apply_pending() {
    for (const auto& [filename, level_id] : pending_) {
        apply_one(filename, level_id);
    }
    pending_.clear();
}
