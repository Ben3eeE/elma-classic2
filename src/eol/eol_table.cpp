#include "eol/eol_table.h"
#include "KIRAJZOL.H"
#include "abc8.h"
#include "pic8.h"
#include <algorithm>

constexpr int LINE_SPACING = 12;
constexpr int TITLE_GAP = 22;
constexpr int TITLE_OFFSET = 4;
constexpr int GROUP_GAP = 40;

void eol_table::set_title(const std::string& title) { title_ = title; }

void eol_table::add_column(int width, ColAlign alignment) {
    columns_.push_back({width, alignment});
}

void eol_table::clear_columns() { columns_.clear(); }

void eol_table::add_row(std::vector<std::string> values) { rows_.push_back(std::move(values)); }

void eol_table::clear_rows() { rows_.clear(); }

int eol_table::row_count() const { return static_cast<int>(rows_.size()); }

void eol_table::set_max_rows(int max_rows) { max_rows_ = max_rows; }

void eol_table::render(pic8* dest, abc8* title_font, abc8* data_font, TableAlign alignment) const {
    if (!dest || !title_font || !data_font) {
        return;
    }

    int y_top = (Cysize * 5) / 6 + LINE_SPACING * 3;

    int x_center;
    switch (alignment) {
    case TableAlign::Left:
        x_center = Cxsize / 4;
        break;
    case TableAlign::Center:
        x_center = Cxsize / 2;
        break;
    case TableAlign::Right:
        x_center = (Cxsize * 3) / 4;
        break;
    }

    // Render title centered
    if (!title_.empty()) {
        title_font->write_centered(dest, x_center, y_top - TITLE_OFFSET, title_.c_str());
    }

    int total_rows = static_cast<int>(rows_.size());
    if (total_rows <= 0 || columns_.empty()) {
        return;
    }

    // Calculate max rows that fit vertically
    int screen_max_rows = std::max((y_top - TITLE_GAP) / LINE_SPACING, 1);

    // When max_rows_ is set, show the last max_rows_ entries without wrapping
    // When max_rows_ is 0, wrap into column groups
    int first_row;
    int visible_count;
    int max_rows_per_group;
    if (max_rows_ > 0) {
        max_rows_per_group = std::min(max_rows_, screen_max_rows);
        visible_count = std::min(total_rows, max_rows_per_group);
        first_row = total_rows - visible_count;
    } else {
        max_rows_per_group = screen_max_rows;
        visible_count = total_rows;
        first_row = 0;
    }

    // Total width of one column group
    int total_col_width = 0;
    for (const auto& col : columns_) {
        total_col_width += col.width;
    }

    // Number of column groups needed
    int num_groups = (visible_count + max_rows_per_group - 1) / max_rows_per_group;

    // Column group layout: each group is total_col_width wide with GROUP_GAP between them.
    // Left-aligned tables grow rightward, right-aligned grow leftward, center grows both ways.
    int group_stride = total_col_width + GROUP_GAP;
    int all_groups_width = num_groups * group_stride - GROUP_GAP;
    int groups_base_x;
    switch (alignment) {
    case TableAlign::Left:
        groups_base_x = x_center - total_col_width / 2;
        break;
    case TableAlign::Center:
        groups_base_x = x_center - all_groups_width / 2;
        break;
    case TableAlign::Right:
        groups_base_x = x_center + total_col_width / 2 - all_groups_width;
        break;
    }

    for (int i = 0; i < visible_count; i++) {
        int row_idx = first_row + i;

        int group = i / max_rows_per_group;
        int row_in_group = i % max_rows_per_group;

        int y = y_top - LINE_SPACING * row_in_group - TITLE_GAP;
        int group_x = groups_base_x + group * group_stride;

        const auto& row = rows_[row_idx];
        int col_x = group_x;
        for (int c = 0; c < static_cast<int>(columns_.size()); c++) {
            const char* text = (c < static_cast<int>(row.size())) ? row[c].c_str() : "";
            int w = columns_[c].width;

            switch (columns_[c].alignment) {
            case ColAlign::Left:
                data_font->write(dest, col_x, y, text);
                break;
            case ColAlign::Center:
                data_font->write_centered(dest, col_x + w / 2, y, text);
                break;
            case ColAlign::Right:
                data_font->write_right_align(dest, col_x + w, y, text);
                break;
            }

            col_x += w;
        }
    }
}
