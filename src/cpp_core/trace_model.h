#pragma once
#include "core.h"
#include "json_parser.h"
#include "profiler.h"
#include <stdint.h>

struct TraceNode {
    StringView name;
    StringView cat;
    TracePhase phase;
    double ts; // timestamp in microseconds
    double dur; // duration in microseconds
    int pid;
    int tid;

    // Tree hierarchy
    TraceNode* parent;
    TraceNode* first_child;
    TraceNode* next_sibling;
    uint32_t depth;
};

class TraceModel {
private:
    ArenaAllocator* arena;

    static constexpr int MAX_THREADS = 64;
    struct ThreadStack {
        int tid;
        TraceNode* top;
        TraceNode* root_head;
        TraceNode* root_tail;
    };
    ThreadStack threads[MAX_THREADS];
    int thread_count = 0;

    ThreadStack* get_thread_stack(int tid) {
        for (int i = 0; i < thread_count; ++i) {
            if (threads[i].tid == tid) return &threads[i];
        }
        if (thread_count < MAX_THREADS) {
            ThreadStack* ts = &threads[thread_count++];
            ts->tid = tid;
            ts->top = nullptr;
            ts->root_head = nullptr;
            ts->root_tail = nullptr;
            return ts;
        }
        return nullptr;
    }

public:
    TraceModel(ArenaAllocator* alloc) : arena(alloc) {}

    // Parsing an array of JSON objects into TraceNode structs
    TraceNode* parse_events(JsonNode* root_array, size_t& out_count) {
        if (!root_array || root_array->type != JsonType::Array) {
            out_count = 0;
            return nullptr;
        }
        
        size_t count = root_array->array_val.count;
        TraceNode* events = (TraceNode*)arena->allocate(sizeof(TraceNode) * count);
        if (!events) {
            out_count = 0;
            return nullptr;
        }

        size_t idx = 0;
        JsonNode* curr = root_array->array_val.head;
        while (curr) {
            if (curr->type == JsonType::Object) {
                TraceNode& ev = events[idx];
                ev.name = {nullptr, 0};
                ev.cat = {nullptr, 0};
                ev.phase = (TracePhase)0;
                ev.ts = 0;
                ev.dur = 0;
                ev.pid = 0;
                ev.tid = 0;
                ev.parent = nullptr;
                ev.first_child = nullptr;
                ev.next_sibling = nullptr;
                ev.depth = 0;

                JsonNode* n = curr->get("name");
                if (n && n->type == JsonType::String) ev.name = n->string_val;

                JsonNode* c = curr->get("cat");
                if (c && c->type == JsonType::String) ev.cat = c->string_val;

                JsonNode* ph = curr->get("ph");
                if (ph && ph->type == JsonType::String && ph->string_val.length > 0) {
                    ev.phase = (TracePhase)ph->string_val.data[0];
                }

                JsonNode* ts = curr->get("ts");
                if (ts && ts->type == JsonType::Number) ev.ts = ts->number_val;

                JsonNode* dur = curr->get("dur");
                if (dur && dur->type == JsonType::Number) ev.dur = dur->number_val;

                JsonNode* pid = curr->get("pid");
                if (pid && pid->type == JsonType::Number) ev.pid = (int)pid->number_val;

                JsonNode* tid = curr->get("tid");
                if (tid && tid->type == JsonType::Number) ev.tid = (int)tid->number_val;

                idx++;
            }
            curr = curr->next;
        }
        out_count = idx;
        return events;
    }

    // Build parent-child relationships from a flat list of events based on 'B', 'E', and 'X' phases
    void build_call_stacks(TraceNode* events, size_t count) {
        thread_count = 0;

        for (size_t i = 0; i < count; ++i) {
            TraceNode* ev = &events[i];
            ThreadStack* stack = get_thread_stack(ev->tid);
            if (!stack) continue;

            if (ev->phase == TracePhase::Begin) {
                if (stack->top) {
                    ev->parent = stack->top;
                    ev->depth = stack->top->depth + 1;
                    
                    // Add as child
                    if (!stack->top->first_child) {
                        stack->top->first_child = ev;
                    } else {
                        TraceNode* sibling = stack->top->first_child;
                        while (sibling->next_sibling) {
                            sibling = sibling->next_sibling;
                        }
                        sibling->next_sibling = ev;
                    }
                } else {
                    ev->depth = 0;
                    if (!stack->root_head) {
                        stack->root_head = ev;
                        stack->root_tail = ev;
                    } else {
                        stack->root_tail->next_sibling = ev;
                        stack->root_tail = ev;
                    }
                }
                stack->top = ev;
            } else if (ev->phase == TracePhase::End) {
                if (stack->top) {
                    // Update dur of 'B' event to match 'E' event timestamp
                    stack->top->dur = ev->ts - stack->top->ts;
                    stack->top = stack->top->parent;
                }
            } else if (ev->phase == TracePhase::Complete) {
                // Complete event (already has dur)
                if (stack->top) {
                    ev->parent = stack->top;
                    ev->depth = stack->top->depth + 1;
                    
                    if (!stack->top->first_child) {
                        stack->top->first_child = ev;
                    } else {
                        TraceNode* sibling = stack->top->first_child;
                        while (sibling->next_sibling) {
                            sibling = sibling->next_sibling;
                        }
                        sibling->next_sibling = ev;
                    }
                } else {
                    ev->depth = 0;
                    if (!stack->root_head) {
                        stack->root_head = ev;
                        stack->root_tail = ev;
                    } else {
                        stack->root_tail->next_sibling = ev;
                        stack->root_tail = ev;
                    }
                }
                // 'X' events don't push to stack because they are self-contained
            }
        }
    }

    // Get the top-level trace events for a specific thread
    TraceNode* get_roots_for_thread(int tid) {
        ThreadStack* stack = get_thread_stack(tid);
        if (stack) return stack->root_head;
        return nullptr;
    }
};
