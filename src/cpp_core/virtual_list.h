#pragma once
#include "core.h"

struct VirtualListState {
    int total_items;
    int item_height;
    int viewport_height;
    int scroll_y;
};

class VirtualList {
public:
    static void compute_visible_range(const VirtualListState& state, int& start_idx, int& end_idx) {
        start_idx = state.scroll_y / state.item_height;
        int visible_count = (state.viewport_height / state.item_height) + 2; // +2 for partials
        end_idx = start_idx + visible_count;
        if (start_idx < 0) start_idx = 0;
        if (end_idx > state.total_items) end_idx = state.total_items;
    }
};
