#pragma once
#include "core.h"
#include "json_parser.h"

enum class ConsoleMessageLevel {
    Verbose,
    Info,
    Warning,
    Error
};

enum class ConsoleMessageType {
    Log,
    Debug,
    Info,
    Error,
    Warning,
    Dir,
    DirXML,
    Table,
    Trace,
    Clear,
    StartGroup,
    StartGroupCollapsed,
    EndGroup,
    Assert,
    Profile,
    ProfileEnd,
    Count,
    TimeEnd
};

struct CallFrame {
    StringView function_name;
    StringView script_id;
    StringView url;
    int line_number;
    int column_number;
    CallFrame* next;
};

struct StackTrace {
    StringView description;
    CallFrame* call_frames;
    StackTrace* parent;
};

struct ConsoleMessage {
    ConsoleMessageType type;
    ConsoleMessageLevel level;
    StringView text;
    StringView url;
    int line;
    int column;
    
    // In CDP, arguments are RemoteObjects. We simplify for now to StringViews of JSON or descriptions
    StringView* args; 
    size_t args_count;

    StackTrace* stack_trace;
    double timestamp;
    StringView execution_context_id;

    ConsoleMessage* next;
};

struct TimerEntry {
    StringView label;
    double start_time;
    TimerEntry* next;
};

class ConsoleEngine {
    ArenaAllocator* arena;
    ConsoleMessage* first_message;
    ConsoleMessage* last_message;
    TimerEntry* first_timer;
    
    size_t message_count;

public:
    ConsoleEngine(ArenaAllocator* a) : arena(a), first_message(nullptr), last_message(nullptr), first_timer(nullptr), message_count(0) {}

    ConsoleMessage* add_message(ConsoleMessageType type, ConsoleMessageLevel level, StringView text, double ts, StackTrace* stack = nullptr) {
        ConsoleMessage* msg = (ConsoleMessage*)arena->allocate(sizeof(ConsoleMessage));
        msg->type = type;
        msg->level = level;
        msg->text = copy_string(arena, text);
        msg->timestamp = ts;
        msg->stack_trace = stack;
        msg->args = nullptr;
        msg->args_count = 0;
        msg->url = StringView();
        msg->line = 0;
        msg->column = 0;
        msg->next = nullptr;

        if (!first_message) {
            first_message = last_message = msg;
        } else {
            last_message->next = msg;
            last_message = msg;
        }
        message_count++;
        return msg;
    }

    void clear() {
        first_message = last_message = nullptr;
        message_count = 0;
        add_message(ConsoleMessageType::Clear, ConsoleMessageLevel::Info, StringView("Console was cleared", 19), 0.0);
    }

    void time(StringView label, double ts) {
        TimerEntry* entry = (TimerEntry*)arena->allocate(sizeof(TimerEntry));
        entry->label = copy_string(arena, label);
        entry->start_time = ts;
        entry->next = first_timer;
        first_timer = entry;
    }

    double timeEnd(StringView label, double ts) {
        TimerEntry* curr = first_timer;
        TimerEntry* prev = nullptr;
        while (curr) {
            if (curr->label == label) {
                double duration = ts - curr->start_time;
                if (prev) prev->next = curr->next;
                else first_timer = curr->next;
                
                // Usually timeEnd also logs a message
                // add_message(ConsoleMessageType::TimeEnd, ConsoleMessageLevel::Info, label + ": " + duration + "ms");
                return duration;
            }
            prev = curr;
            curr = curr->next;
        }
        return -1.0; // Not found
    }

    ConsoleMessage* get_messages() const { return first_message; }
    size_t get_message_count() const { return message_count; }
};
