#pragma once
#include "core.h"

enum class DiffOp {
    Keep,
    Insert,
    Delete
};

struct DiffResult {
    DiffOp op;
    StringView text;
    DiffResult* next;
};

class DiffEngine {
    ArenaAllocator* arena;
public:
    DiffEngine(ArenaAllocator* a) : arena(a) {}

    // A very simple diff algorithm for testing
    DiffResult* compute_diff(StringView old_text, StringView new_text) {
        size_t prefix = 0;
        while (prefix < old_text.length && prefix < new_text.length && old_text.data[prefix] == new_text.data[prefix]) {
            prefix++;
        }
        
        size_t old_suffix = 0;
        size_t new_suffix = 0;
        while (old_suffix < old_text.length - prefix && new_suffix < new_text.length - prefix && 
               old_text.data[old_text.length - 1 - old_suffix] == new_text.data[new_text.length - 1 - new_suffix]) {
            old_suffix++;
            new_suffix++;
        }
        
        DiffResult* first = nullptr;
        DiffResult* last = nullptr;
        
        auto add_res = [&](DiffOp op, StringView text) {
            if (text.length == 0) return;
            DiffResult* r = (DiffResult*)arena->allocate(sizeof(DiffResult));
            r->op = op;
            r->text = copy_string(arena, text);
            r->next = nullptr;
            if (!first) first = last = r;
            else { last->next = r; last = r; }
        };
        
        add_res(DiffOp::Keep, StringView(old_text.data, prefix));
        
        size_t old_mid_len = old_text.length - prefix - old_suffix;
        size_t new_mid_len = new_text.length - prefix - new_suffix;
        
        if (old_mid_len > 0) {
            add_res(DiffOp::Delete, StringView(old_text.data + prefix, old_mid_len));
        }
        if (new_mid_len > 0) {
            add_res(DiffOp::Insert, StringView(new_text.data + prefix, new_mid_len));
        }
        
        add_res(DiffOp::Keep, StringView(old_text.data + old_text.length - old_suffix, old_suffix));
        
        return first;
    }
};
