#pragma once
#include "core.h"
#include "elements.h"

// ============ JS VALUE TYPE ENUM ============
enum class JSValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Object,
    Function,
    NodeRef
};

struct JSObject;

// ============ JS VALUE STRUCT ============
struct JSValue {
    JSValueType type;
    union {
        bool bool_val;
        double num_val;
        StringView str_val;
        JSObject* obj_val;
        DOMNode* node_val;
    };

    JSValue() : type(JSValueType::Undefined), num_val(0) {}
    static JSValue Undefined() { JSValue v; v.type = JSValueType::Undefined; return v; }
    static JSValue Null() { JSValue v; v.type = JSValueType::Null; return v; }
    static JSValue Boolean(bool b) { JSValue v; v.type = JSValueType::Boolean; v.bool_val = b; return v; }
    static JSValue Number(double n) { JSValue v; v.type = JSValueType::Number; v.num_val = n; return v; }
    static JSValue String(StringView s) { JSValue v; v.type = JSValueType::String; v.str_val = s; return v; }
    static JSValue Object(JSObject* obj) { JSValue v; v.type = JSValueType::Object; v.obj_val = obj; return v; }
    static JSValue NodeRef(DOMNode* node) { JSValue v; v.type = JSValueType::NodeRef; v.node_val = node; return v; }
};

// ============ PROPERTY ENTRY LINKED LIST ============
struct JSProperty {
    StringView key;
    JSValue value;
    bool is_readonly;
    JSProperty* next;
};

// ============ EVENT LISTENER LINKED LIST ============
struct JSEventListener {
    StringView event_type;
    StringView handler_name; // e.g. "onDOMContentLoaded", "handleClick"
    bool capture;
    JSEventListener* next;
};

// ============ STORAGE ENTRY LINKED LIST ============
struct WebStorageEntry {
    StringView key;
    StringView value;
    WebStorageEntry* next;
};

// ============ STORAGE OBJECT (localStorage / sessionStorage) ============
struct StorageObject {
    WebStorageEntry* first_entry;

    void set_item(ArenaAllocator* arena, StringView key, StringView val) {
        WebStorageEntry* entry = first_entry;
        while (entry) {
            if (entry->key.equals(key)) {
                entry->value = copy_string(arena, val);
                return;
            }
            entry = entry->next;
        }
        WebStorageEntry* new_e = (WebStorageEntry*)arena->allocate(sizeof(WebStorageEntry));
        new_e->key = copy_string(arena, key);
        new_e->value = copy_string(arena, val);
        new_e->next = first_entry;
        first_entry = new_e;
    }

    StringView get_item(StringView key) const {
        WebStorageEntry* entry = first_entry;
        while (entry) {
            if (entry->key.equals(key)) {
                return entry->value;
            }
            entry = entry->next;
        }
        return StringView();
    }

    bool remove_item(StringView key) {
        if (!first_entry) return false;
        if (first_entry->key.equals(key)) {
            first_entry = first_entry->next;
            return true;
        }
        WebStorageEntry* curr = first_entry;
        while (curr->next) {
            if (curr->next->key.equals(key)) {
                curr->next = curr->next->next;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    void clear() {
        first_entry = nullptr;
    }
};

// ============ TIMER TASK STRUCT ============
struct TimerTask {
    uint32_t id;
    StringView handler;
    double delay_ms;
    double scheduled_time;
    bool active;
    TimerTask* next;
};

// ============ JS OBJECT STRUCT ============
struct JSObject {
    StringView class_name; // "Window", "HTMLDocument", "Navigator", "Location", "Performance"
    JSProperty* first_prop;
    JSEventListener* first_listener;

    void set_prop(ArenaAllocator* arena, StringView key, JSValue val, bool readonly = false) {
        JSProperty* prop = first_prop;
        while (prop) {
            if (prop->key.equals(key)) {
                if (!prop->is_readonly) {
                    prop->value = val;
                }
                return;
            }
            prop = prop->next;
        }

        // Add new property
        JSProperty* new_p = (JSProperty*)arena->allocate(sizeof(JSProperty));
        new_p->key = copy_string(arena, key);
        new_p->value = val;
        new_p->is_readonly = readonly;
        new_p->next = first_prop;
        first_prop = new_p;
    }

    JSValue get_prop(StringView key) const {
        JSProperty* prop = first_prop;
        while (prop) {
            if (prop->key.equals(key)) {
                return prop->value;
            }
            prop = prop->next;
        }
        return JSValue::Undefined();
    }

    void add_event_listener(ArenaAllocator* arena, StringView type, StringView handler, bool capture = false) {
        JSEventListener* listener = (JSEventListener*)arena->allocate(sizeof(JSEventListener));
        listener->event_type = copy_string(arena, type);
        listener->handler_name = copy_string(arena, handler);
        listener->capture = capture;
        listener->next = first_listener;
        first_listener = listener;
    }

    bool remove_event_listener(StringView type, StringView handler) {
        if (!first_listener) return false;
        if (first_listener->event_type.equals(type) && first_listener->handler_name.equals(handler)) {
            first_listener = first_listener->next;
            return true;
        }
        JSEventListener* curr = first_listener;
        while (curr->next) {
            if (curr->next->event_type.equals(type) && curr->next->handler_name.equals(handler)) {
                curr->next = curr->next->next;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    size_t count_listeners(StringView type) const {
        size_t c = 0;
        JSEventListener* curr = first_listener;
        while (curr) {
            if (type.length == 0 || curr->event_type.equals(type)) {
                c++;
            }
            curr = curr->next;
        }
        return c;
    }
};

// ============ BROWSER GLOBAL ENVIRONMENT ============
class BrowserEnvironment {
private:
    ArenaAllocator* arena;
    uint32_t next_timer_id;

public:
    JSObject* window;
    JSObject* document_obj;
    JSObject* navigator;
    JSObject* location;
    JSObject* performance;
    StorageObject local_storage;
    StorageObject session_storage;
    TimerTask* first_timer;
    ElementsTree* dom_tree;

    BrowserEnvironment(ArenaAllocator* a, ElementsTree* tree) 
        : arena(a), next_timer_id(1), first_timer(nullptr), dom_tree(tree) {
        
        local_storage.first_entry = nullptr;
        session_storage.first_entry = nullptr;

        // 1. Initialize Window Object
        window = (JSObject*)arena->allocate(sizeof(JSObject));
        window->class_name = copy_string(arena, StringView("Window"));
        window->first_prop = nullptr;
        window->first_listener = nullptr;

        window->set_prop(arena, StringView("innerWidth"), JSValue::Number(1280));
        window->set_prop(arena, StringView("innerHeight"), JSValue::Number(800));
        window->set_prop(arena, StringView("devicePixelRatio"), JSValue::Number(2.0));
        window->set_prop(arena, StringView("name"), JSValue::String(StringView("MainContext")));

        // Self references
        window->set_prop(arena, StringView("window"), JSValue::Object(window), true);
        window->set_prop(arena, StringView("self"), JSValue::Object(window), true);

        // 2. Initialize Document Object
        document_obj = (JSObject*)arena->allocate(sizeof(JSObject));
        document_obj->class_name = copy_string(arena, StringView("HTMLDocument"));
        document_obj->first_prop = nullptr;
        document_obj->first_listener = nullptr;

        document_obj->set_prop(arena, StringView("title"), JSValue::String(StringView("Chromium DevTools C++ Core")));
        document_obj->set_prop(arena, StringView("domain"), JSValue::String(StringView("localhost")));
        document_obj->set_prop(arena, StringView("readyState"), JSValue::String(StringView("complete")));
        if (dom_tree && dom_tree->root) {
            document_obj->set_prop(arena, StringView("body"), JSValue::NodeRef(dom_tree->root));
        }

        window->set_prop(arena, StringView("document"), JSValue::Object(document_obj), true);

        // 3. Initialize Navigator Object
        navigator = (JSObject*)arena->allocate(sizeof(JSObject));
        navigator->class_name = copy_string(arena, StringView("Navigator"));
        navigator->first_prop = nullptr;
        navigator->first_listener = nullptr;

        navigator->set_prop(arena, StringView("userAgent"), JSValue::String(StringView("Mozilla/5.0 (Mobile; Chromium DevTools C++ Core/124.0)")));
        navigator->set_prop(arena, StringView("language"), JSValue::String(StringView("en-US")));
        navigator->set_prop(arena, StringView("hardwareConcurrency"), JSValue::Number(8));
        navigator->set_prop(arena, StringView("onLine"), JSValue::Boolean(true));

        window->set_prop(arena, StringView("navigator"), JSValue::Object(navigator), true);

        // 4. Initialize Location Object
        location = (JSObject*)arena->allocate(sizeof(JSObject));
        location->class_name = copy_string(arena, StringView("Location"));
        location->first_prop = nullptr;
        location->first_listener = nullptr;

        location->set_prop(arena, StringView("href"), JSValue::String(StringView("http://localhost:3000/app")));
        location->set_prop(arena, StringView("protocol"), JSValue::String(StringView("http:")));
        location->set_prop(arena, StringView("host"), JSValue::String(StringView("localhost:3000")));
        location->set_prop(arena, StringView("pathname"), JSValue::String(StringView("/app")));

        window->set_prop(arena, StringView("location"), JSValue::Object(location));

        // 5. Initialize Performance Object
        performance = (JSObject*)arena->allocate(sizeof(JSObject));
        performance->class_name = copy_string(arena, StringView("Performance"));
        performance->first_prop = nullptr;
        performance->first_listener = nullptr;

        performance->set_prop(arena, StringView("timeOrigin"), JSValue::Number(1700000000.0));

        window->set_prop(arena, StringView("performance"), JSValue::Object(performance), true);
    }

    // ============ DOM MANIPULATION BRIDGES ============
    JSValue create_element(StringView tag_name) {
        if (!dom_tree) return JSValue::Null();
        DOMNode* node = dom_tree->create_element(tag_name);
        return JSValue::NodeRef(node);
    }

    JSValue create_text_node(StringView text) {
        if (!dom_tree) return JSValue::Null();
        DOMNode* node = dom_tree->create_text_node(text);
        return JSValue::NodeRef(node);
    }

    bool append_child(DOMNode* parent, DOMNode* child) {
        if (!parent || !child) return false;
        parent->add_child(child);
        return true;
    }

    bool remove_child(DOMNode* parent, DOMNode* child) {
        if (!parent || !child) return false;
        parent->remove_child(child);
        return true;
    }

    // ============ TIMERS & SCHEDULER ============
    uint32_t set_timeout(StringView handler, double delay_ms, double current_time = 0.0) {
        TimerTask* task = (TimerTask*)arena->allocate(sizeof(TimerTask));
        task->id = next_timer_id++;
        task->handler = copy_string(arena, handler);
        task->delay_ms = delay_ms;
        task->scheduled_time = current_time + delay_ms;
        task->active = true;
        task->next = first_timer;
        first_timer = task;
        return task->id;
    }

    bool clear_timeout(uint32_t timer_id) {
        TimerTask* curr = first_timer;
        while (curr) {
            if (curr->id == timer_id && curr->active) {
                curr->active = false;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    uint32_t request_animation_frame(StringView callback_handler, double current_time = 0.0) {
        return set_timeout(callback_handler, 16.666, current_time); // ~60fps target
    }

    size_t process_timers(double current_time, StringView* executed_handlers, size_t max_handlers) {
        size_t count = 0;
        TimerTask* curr = first_timer;
        while (curr && count < max_handlers) {
            if (curr->active && current_time >= curr->scheduled_time) {
                curr->active = false;
                executed_handlers[count++] = curr->handler;
            }
            curr = curr->next;
        }
        return count;
    }

    // Helper: Execute document.querySelector(selector)
    JSValue query_selector(StringView selector) {
        if (!dom_tree || !dom_tree->root) return JSValue::Null();
        DOMNode* found = dom_tree->query_selector(dom_tree->root, selector);
        if (found) {
            return JSValue::NodeRef(found);
        }
        return JSValue::Null();
    }

    // Helper: Simulate event dispatch
    size_t dispatch_event(JSObject* target, StringView event_type) {
        if (!target) return 0;
        return target->count_listeners(event_type);
    }
};
