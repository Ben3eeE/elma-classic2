#ifndef EOL_TABLE_H
#define EOL_TABLE_H

#include <string>
#include <vector>

class abc8;
class pic8;

enum class ColAlign { Left, Center, Right };
enum class TableAlign { Left, Center, Right };

struct eol_column {
    int width;
    ColAlign alignment;
};

class eol_table {
    std::string title_;
    std::vector<eol_column> columns_;
    std::vector<std::vector<std::string>> rows_;
    int max_rows_ = 0;

  public:
    void set_title(const std::string& title);
    void add_column(int width, ColAlign alignment = ColAlign::Left);
    void clear_columns();

    void add_row(std::vector<std::string> values);
    void clear_rows();
    int row_count() const;

    // Limit visible rows to the last N entries (0 = no limit, wraps into columns)
    void set_max_rows(int max_rows);

    void render(pic8* dest, abc8* title_font, abc8* data_font,
                TableAlign alignment = TableAlign::Center) const;
};

#endif
