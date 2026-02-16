#include "eol/chat.h"
#include "main.h"
#include <string>

chat::chat() {}

void chat::add_line(std::string_view text) {
    lines_.push_back({std::string(text), stopwatch()});
    if ((int)lines_.size() > MAX_LINES) {
        lines_.erase(lines_.begin());
    }
}

void chat::clear() { lines_.clear(); }
