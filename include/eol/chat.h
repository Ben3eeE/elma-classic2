#ifndef EOL_CHAT_H
#define EOL_CHAT_H

#include <string>
#include <vector>

class abc8;
class pic8;

struct chat_line {
    std::string text;
    double timestamp;
};

class chat {
  public:
    chat();

    void add_line(std::string_view text);
    void clear();
    void render(pic8* surface, abc8* font);

  private:
    static constexpr int MAX_LINES = 200;
    static constexpr int VISIBLE_LINES = 5;
    static constexpr int LINE_HEIGHT = 12;
    static constexpr int MARGIN_X = 20;
    static constexpr int MARGIN_Y = 2;

    std::vector<chat_line> lines_;
};

extern chat* GameChat;

#endif
