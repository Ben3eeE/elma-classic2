#include "status_messages.h"

#include "../abc8.h"
#include "../pic8.h"
#include "../platform_impl.h"

status_messages StatusMessages;

void status_messages::add(const std::string& text) {
    messages_.push_back({text, get_milliseconds()});
    while (static_cast<int>(messages_.size()) > MAX_STATUS_MESSAGES) {
        messages_.pop_front();
    }
}

void status_messages::remove_expired() {
    long long now = get_milliseconds();
    while (!messages_.empty()) {
        auto& front = messages_.front();
        bool expired_seen =
            front.first_seen > 0 && now - front.first_seen > STATUS_MESSAGE_LIFETIME_MS;
        bool expired_age = now - front.created_at > MAX_STATUS_MESSAGE_AGE_MS;
        if (!expired_seen && !expired_age) {
            break;
        }
        messages_.pop_front();
    }
}

void status_messages::render(pic8* dest, abc8* font) {
    constexpr int LINE_SPACING = 12;

    int x = dest->get_width() / 4;
    int top = dest->get_height() - 37;
    int index = 0;
    for (auto& msg : *this) {
        int y = top - index * LINE_SPACING;
        font->write(dest, x, y, msg.text.c_str());
        index++;
    }
}

void status_messages::clear() { messages_.clear(); }

status_messages::iterator status_messages::begin() {
    remove_expired();
    long long now = get_milliseconds();
    for (auto& msg : messages_) {
        if (msg.first_seen == 0) {
            msg.first_seen = now;
        }
    }
    return iterator(messages_.begin());
}

status_messages::iterator status_messages::end() {
    remove_expired();
    return iterator(messages_.end());
}
