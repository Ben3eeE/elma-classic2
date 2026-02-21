#include "replay_cache.h"
#include "fs_utils.h"
#include "recorder.h"
#include <algorithm>
#include <filesystem>

replay_cache ReplayCache;

replay_cache::~replay_cache() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void replay_cache::start() {
    if (worker_.joinable()) {
        return;
    }
    worker_ = std::thread([this]() {
        std::unordered_map<int, std::vector<std::string>> scanned;

        std::filesystem::directory_iterator it;
        try {
            it = std::filesystem::directory_iterator("rec");
        } catch (const std::filesystem::filesystem_error&) {
            // rec/ directory doesn't exist — nothing to scan
            ready_.store(true);
            return;
        }

        for (const auto& entry : it) {
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
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            entries_ = std::move(scanned);
        }
        ready_.store(true);
    });
}

void replay_cache::wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool replay_cache::is_ready() const { return ready_.load(); }

std::vector<std::string> replay_cache::all_filenames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, filenames] : entries_) {
        result.insert(result.end(), filenames.begin(), filenames.end());
    }
    return result;
}

std::vector<std::string> replay_cache::filenames_for_level(int level_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(level_id);
    if (it != entries_.end()) {
        return it->second;
    }
    return {};
}

void replay_cache::upsert(const std::string& filename, int level_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Remove existing entry with this filename (level may have changed)
    for (auto& [id, filenames] : entries_) {
        auto it = std::find(filenames.begin(), filenames.end(), filename);
        if (it != filenames.end()) {
            if (id == level_id) {
                return; // Already exists in correct bucket
            }
            filenames.erase(it);
            break;
        }
    }
    entries_[level_id].push_back(filename);
}
