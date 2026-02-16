#include "keys.h"
#include "platform_utils.h"
#include <deque>

static constexpr size_t TEXT_INPUT_BUFFER_MAX_SIZE = 64;
static std::deque<char> TextInputBuffer;

void add_char_to_buffer(char text) {
    if (!is_ascii_character(text)) {
        return;
    }
    if (TextInputBuffer.size() >= TEXT_INPUT_BUFFER_MAX_SIZE) {
        return;
    }
    TextInputBuffer.push_back(text);
}

void add_text_to_buffer(const char* text) {
    while (*text && TextInputBuffer.size() < TEXT_INPUT_BUFFER_MAX_SIZE) {
        unsigned char c = *text;

        if (!is_ascii_character(c)) {
            text++;
            continue;
        }

        TextInputBuffer.push_back((char)c);
        text++;
    }
}

void empty_keypress_buffer() { TextInputBuffer.clear(); }

bool has_text_input() { return !TextInputBuffer.empty(); }

char pop_text_input() {
    if (TextInputBuffer.empty()) {
        return 0;
    }
    char c = TextInputBuffer.front();
    TextInputBuffer.pop_front();
    return c;
}
