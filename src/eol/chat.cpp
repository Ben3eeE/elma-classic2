#include "eol/chat.h"
#include "abc8.h"
#include "main.h"
#include <algorithm>
#include <string>

chat* GameChat = nullptr;

chat::chat() {}

void chat::add_line(std::string_view text) {
    lines_.push_back({std::string(text), stopwatch()});
    if ((int)lines_.size() > MAX_LINES) {
        lines_.erase(lines_.begin());
    }
}

void chat::clear() { lines_.clear(); }

void chat::render(pic8* surface, abc8* font) {
    int visible_count = std::min((int)lines_.size(), VISIBLE_LINES);
    int start = (int)lines_.size() - visible_count;

    // Backbuffer is flipped: y=0 is bottom of display.
    // Newest line at lowest y, older lines above.
    // Always reserve bottom area for input prompt with extra gap.
    int base_y = MARGIN_Y + LINE_HEIGHT + 8;

    for (int i = 0; i < visible_count; i++) {
        int line_index = start + (visible_count - 1 - i);
        int y = base_y + i * LINE_HEIGHT;
        font->write(surface, MARGIN_X, y, lines_[line_index].text.c_str());
    }
}
