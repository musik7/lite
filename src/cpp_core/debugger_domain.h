#pragma once
#include "core.h"

// ============ SCOPE CHAIN & REMOTE OBJECT ============
enum class ScopeType {
    Local,
    Closure,
    Catch,
    Block,
    Script,
    Global
};

struct RemoteObject {
    StringView name;
    StringView type;       // "number", "string", "boolean", "object", "function", "undefined"
    StringView value;      // Value representation or preview string
    StringView object_id;  // Non-empty for complex objects
    RemoteObject* next;
};

struct Scope {
    ScopeType type;
    StringView name;       // Scope name (e.g. closure name, catch param, block)
    RemoteObject* first_object;
    size_t object_count;
    Scope* next;

    void add_variable(ArenaAllocator* arena, StringView name, StringView type, StringView val, StringView obj_id = StringView("")) {
        RemoteObject* obj = (RemoteObject*)arena->allocate(sizeof(RemoteObject));
        obj->name = copy_string(arena, name);
        obj->type = copy_string(arena, type);
        obj->value = copy_string(arena, val);
        obj->object_id = copy_string(arena, obj_id);
        obj->next = first_object;
        first_object = obj;
        object_count++;
    }
};

// ============ CALL FRAME & ASYNC STACK TRACE ============
struct Location {
    StringView script_id;
    StringView url;
    int line_number;
    int column_number;
};

struct DebuggerCallFrame {
    StringView call_frame_id;
    StringView function_name;
    Location location;
    Scope* first_scope;
    size_t scope_count;
    DebuggerCallFrame* next;

    Scope* add_scope(ArenaAllocator* arena, ScopeType type, StringView scope_name = StringView("")) {
        Scope* scope = (Scope*)arena->allocate(sizeof(Scope));
        scope->type = type;
        scope->name = copy_string(arena, scope_name);
        scope->first_object = nullptr;
        scope->object_count = 0;
        scope->next = first_scope;
        first_scope = scope;
        scope_count++;
        return scope;
    }
};

enum class AsyncTaskType {
    Promise,
    SetTimeout,
    EventListener,
    XHR
};

struct AsyncStackTrace {
    AsyncTaskType type;
    StringView description; // e.g. "Promise.then", "setTimeout", "onClick"
    DebuggerCallFrame* call_frames;
    size_t frame_count;
    AsyncStackTrace* parent; // Nested async stack trace link
};

// ============ BREAKPOINT ENGINE ============
struct Breakpoint {
    StringView breakpoint_id;
    StringView script_id;
    StringView url;
    int line_number;
    int column_number;
    StringView condition;  // Optional JS condition expression
    bool enabled;
    size_t hit_count;
    Breakpoint* next;
};

class BreakpointEngine {
private:
    ArenaAllocator* arena;
    Breakpoint* first_bp;
    size_t count;
    int id_counter;

public:
    BreakpointEngine(ArenaAllocator* a) : arena(a), first_bp(nullptr), count(0), id_counter(1) {}

    Breakpoint* set_breakpoint(StringView url, int line, int col = 0, StringView condition = StringView("")) {
        char buf[32];
        int len = 0;
        int temp = id_counter++;
        if (temp == 0) { buf[len++] = '0'; }
        else {
            char rev[16]; int r = 0;
            while (temp > 0) { rev[r++] = (char)('0' + (temp % 10)); temp /= 10; }
            for (int i = r - 1; i >= 0; i--) buf[len++] = rev[i];
        }
        buf[len] = '\0';

        StringView bp_id = copy_string(arena, StringView(buf, len));

        Breakpoint* bp = (Breakpoint*)arena->allocate(sizeof(Breakpoint));
        bp->breakpoint_id = bp_id;
        bp->script_id = StringView("");
        bp->url = copy_string(arena, url);
        bp->line_number = line;
        bp->column_number = col;
        bp->condition = copy_string(arena, condition);
        bp->enabled = true;
        bp->hit_count = 0;
        bp->next = first_bp;
        first_bp = bp;
        count++;
        return bp;
    }

    Breakpoint* find_matching_breakpoint(StringView url, int line, int col = 0) {
        Breakpoint* curr = first_bp;
        while (curr) {
            if (curr->enabled && (curr->url == url || curr->script_id == url) && curr->line_number == line) {
                if (curr->column_number == 0 || curr->column_number == col) {
                    curr->hit_count++;
                    return curr;
                }
            }
            curr = curr->next;
        }
        return nullptr;
    }

    void remove_breakpoint(StringView bp_id) {
        if (!first_bp) return;
        if (first_bp->breakpoint_id == bp_id) {
            first_bp = first_bp->next;
            count--;
            return;
        }
        Breakpoint* curr = first_bp;
        while (curr->next) {
            if (curr->next->breakpoint_id == bp_id) {
                curr->next = curr->next->next;
                count--;
                return;
            }
            curr = curr->next;
        }
    }

    void toggle_breakpoint(StringView bp_id, bool enable) {
        Breakpoint* curr = first_bp;
        while (curr) {
            if (curr->breakpoint_id == bp_id) {
                curr->enabled = enable;
                return;
            }
            curr = curr->next;
        }
    }

    size_t get_count() const { return count; }
    Breakpoint* get_breakpoints() const { return first_bp; }
};

// ============ EXECUTION STATE MACHINE & DEBUGGER DOMAIN ============
enum class DebuggerState {
    Running,
    Paused,
    SteppingOver,
    SteppingInto,
    SteppingOut
};

enum class PauseReason {
    None,
    BreakpointHit,
    PauseCommand,
    DOMBreakpoint,
    EventListenerBreakpoint,
    Exception
};

class DebuggerDomain {
private:
    ArenaAllocator* arena;
    BreakpointEngine bp_engine;
    DebuggerState state;
    PauseReason pause_reason;
    StringView pause_description;
    
    DebuggerCallFrame* current_call_stack;
    size_t stack_depth;
    
    AsyncStackTrace* current_async_stack;

public:
    DebuggerDomain(ArenaAllocator* a) 
        : arena(a), bp_engine(a), state(DebuggerState::Running),
          pause_reason(PauseReason::None), pause_description(StringView("")),
          current_call_stack(nullptr), stack_depth(0), current_async_stack(nullptr) {}

    BreakpointEngine* get_breakpoint_engine() { return &bp_engine; }

    DebuggerState get_state() const { return state; }
    PauseReason get_pause_reason() const { return pause_reason; }
    StringView get_pause_description() const { return pause_description; }

    void resume() {
        state = DebuggerState::Running;
        pause_reason = PauseReason::None;
        pause_description = StringView("");
    }

    void step_over() {
        state = DebuggerState::SteppingOver;
    }

    void step_into() {
        state = DebuggerState::SteppingInto;
    }

    void step_out() {
        state = DebuggerState::SteppingOut;
    }

    void pause(PauseReason reason = PauseReason::PauseCommand, StringView description = StringView("User paused execution")) {
        state = DebuggerState::Paused;
        pause_reason = reason;
        pause_description = copy_string(arena, description);
    }

    DebuggerCallFrame* push_call_frame(StringView frame_id, StringView func_name, StringView script_id, StringView url, int line, int col) {
        DebuggerCallFrame* frame = (DebuggerCallFrame*)arena->allocate(sizeof(DebuggerCallFrame));
        frame->call_frame_id = copy_string(arena, frame_id);
        frame->function_name = copy_string(arena, func_name);
        frame->location.script_id = copy_string(arena, script_id);
        frame->location.url = copy_string(arena, url);
        frame->location.line_number = line;
        frame->location.column_number = col;
        frame->first_scope = nullptr;
        frame->scope_count = 0;
        frame->next = current_call_stack;
        current_call_stack = frame;
        stack_depth++;

        // Check if hitting a breakpoint
        Breakpoint* hit_bp = bp_engine.find_matching_breakpoint(url, line, col);
        if (hit_bp) {
            pause(PauseReason::BreakpointHit, StringView("Breakpoint hit"));
        }

        return frame;
    }

    void pop_call_frame() {
        if (current_call_stack) {
            current_call_stack = current_call_stack->next;
            if (stack_depth > 0) stack_depth--;
        }
    }

    void clear_call_stack() {
        current_call_stack = nullptr;
        stack_depth = 0;
    }

    DebuggerCallFrame* get_call_stack() const { return current_call_stack; }
    size_t get_stack_depth() const { return stack_depth; }

    // Async stack trace linkage
    AsyncStackTrace* create_async_stack_trace(AsyncTaskType type, StringView description, DebuggerCallFrame* frames, AsyncStackTrace* parent = nullptr) {
        AsyncStackTrace* async_st = (AsyncStackTrace*)arena->allocate(sizeof(AsyncStackTrace));
        async_st->type = type;
        async_st->description = copy_string(arena, description);
        async_st->call_frames = frames;
        async_st->parent = parent;
        current_async_stack = async_st;
        return async_st;
    }

    AsyncStackTrace* get_async_stack_trace() const { return current_async_stack; }
};
