#pragma once
#include "core.h"
#include <stdint.h>

struct SourceMapMapping {
    uint32_t generated_line;
    uint32_t generated_column;
    uint32_t original_line;
    uint32_t original_column;
    uint32_t source_index;
    uint32_t name_index;
    bool has_name;
};

class SourceMapEngine {
    ArenaAllocator* arena;
public:
    SourceMapEngine(ArenaAllocator* a) : arena(a) {}
    
    int decode_vlq(const char** p) {
        int result = 0;
        int shift = 0;
        bool continuation;
        do {
            char c = **p;
            (*p)++;
            int digit = -1;
            if (c >= 'A' && c <= 'Z') digit = c - 'A';
            else if (c >= 'a' && c <= 'z') digit = c - 'a' + 26;
            else if (c >= '0' && c <= '9') digit = c - '0' + 52;
            else if (c == '+') digit = 62;
            else if (c == '/') digit = 63;
            
            if (digit == -1) break;
            
            continuation = (digit & 32) != 0;
            digit &= 31;
            result += (digit << shift);
            shift += 5;
        } while (continuation);
        
        bool is_negative = (result & 1) != 0;
        result >>= 1;
        return is_negative ? -result : result;
    }

    SourceMapMapping* parse_mappings(StringView mappings_str, size_t& out_count) {
        size_t capacity = 1024;
        SourceMapMapping* result = (SourceMapMapping*)arena->allocate(sizeof(SourceMapMapping) * capacity);
        size_t count = 0;
        const char* p = mappings_str.data;
        const char* end = p + mappings_str.length;
        
        uint32_t gen_line = 0, gen_col = 0, orig_line = 0, orig_col = 0, src_idx = 0, name_idx = 0;
        
        while (p < end && count < capacity) {
            if (*p == ';') { gen_line++; gen_col = 0; p++; continue; }
            if (*p == ',') { p++; continue; }
            
            SourceMapMapping m = {0};
            m.generated_line = gen_line;
            gen_col += decode_vlq(&p);
            m.generated_column = gen_col;
            
            if (p < end && *p != ',' && *p != ';') {
                src_idx += decode_vlq(&p); m.source_index = src_idx;
                if (p < end && *p != ',' && *p != ';') {
                    orig_line += decode_vlq(&p); m.original_line = orig_line;
                    if (p < end && *p != ',' && *p != ';') {
                        orig_col += decode_vlq(&p); m.original_column = orig_col;
                        if (p < end && *p != ',' && *p != ';') {
                            name_idx += decode_vlq(&p); m.name_index = name_idx; m.has_name = true;
                        }
                    }
                }
            }
            result[count++] = m;
        }
        out_count = count;
        return result;
    }

    SourceMapMapping* find_mapping(SourceMapMapping* mappings, size_t count, uint32_t line, uint32_t column) {
        if (count == 0 || !mappings) return nullptr;
        size_t left = 0, right = count - 1;
        SourceMapMapping* result = nullptr;
        while (left <= right) {
            size_t mid = left + (right - left) / 2;
            SourceMapMapping& m = mappings[mid];
            if (m.generated_line < line || (m.generated_line == line && m.generated_column <= column)) {
                result = &mappings[mid];
                left = mid + 1;
            } else {
                if (mid == 0) break;
                right = mid - 1;
            }
        }
        if (result && result->generated_line == line) return result;
        return nullptr;
    }
};
