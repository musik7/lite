#pragma once
#include "core.h"

enum class TracePhase {
    Begin = 'B',
    End = 'E',
    Complete = 'X',
    Instant = 'I'
};


struct ProfileCallFrame {
    StringView function_name;
    StringView script_id;
    StringView url;
    int line_number;
    int column_number;
};

struct ProfileNode {
    int id;
    ProfileCallFrame call_frame;
    int hit_count;
    
    ProfileNode* children;
    size_t children_count;
    
    ProfileNode* next; // Sibling list
};

struct CPUProfile {
    ProfileNode* head;
    double start_time;
    double end_time;
    
    int* samples;
    double* time_deltas;
    size_t sample_count;
};

class ProfilerEngine {
    ArenaAllocator* arena;

public:
    ProfilerEngine(ArenaAllocator* a) : arena(a) {}

    CPUProfile* create_profile(double start, double end) {
        CPUProfile* profile = (CPUProfile*)arena->allocate(sizeof(CPUProfile));
        profile->head = nullptr;
        profile->start_time = start;
        profile->end_time = end;
        profile->samples = nullptr;
        profile->time_deltas = nullptr;
        profile->sample_count = 0;
        return profile;
    }

    ProfileNode* add_node(ProfileNode* parent, int id, StringView func_name, StringView url, int line, int col) {
        ProfileNode* node = (ProfileNode*)arena->allocate(sizeof(ProfileNode));
        node->id = id;
        node->call_frame.function_name = copy_string(arena, func_name);
        node->call_frame.url = copy_string(arena, url);
        node->call_frame.script_id = StringView();
        node->call_frame.line_number = line;
        node->call_frame.column_number = col;
        node->hit_count = 0;
        node->children = nullptr;
        node->children_count = 0;
        node->next = nullptr;
        
        if (parent) {
            node->next = parent->children;
            parent->children = node;
            parent->children_count++;
        }
        
        return node;
    }
};
