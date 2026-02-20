#ifndef STATUS_MESSAGES_H
#define STATUS_MESSAGES_H

#include <deque>
#include <string>

class pic8;
class abc8;

constexpr int MAX_STATUS_MESSAGES = 3;
constexpr long long STATUS_MESSAGE_LIFETIME_MS = 8000;
constexpr long long MAX_STATUS_MESSAGE_AGE_MS = 30000;

struct status_message {
    std::string text;
    long long created_at = 0;
    long long first_seen = 0;
};

class status_messages {
  private:
    std::deque<status_message> messages_;

    void remove_expired();

  public:
    void add(const std::string& text);
    void render(pic8* dest, abc8* font);
    void clear();

    class iterator {
      public:
        using inner = std::deque<status_message>::iterator;

        explicit iterator(inner it)
            : it_(it) {}

        status_message& operator*() { return *it_; }
        status_message* operator->() { return &(*it_); }
        iterator& operator++() {
            ++it_;
            return *this;
        }
        bool operator!=(const iterator& other) const { return it_ != other.it_; }
        bool operator==(const iterator& other) const { return it_ == other.it_; }

      private:
        inner it_;
    };

    iterator begin();
    iterator end();
};

extern status_messages StatusMessages;

#endif
