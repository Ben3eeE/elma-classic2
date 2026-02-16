#ifndef EOL_CHAT_H
#define EOL_CHAT_H

#include <string>
#include <vector>

struct chat_line {
    std::string text;
    double timestamp;
};

class chat {
  public:
    chat();

    void add_line(std::string_view text);
    void clear();

  private:
    static constexpr int MAX_LINES = 200;

    std::vector<chat_line> lines_;
};

#endif
