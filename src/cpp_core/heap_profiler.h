#pragma once
#include "core.h"

// ============ HEAP GRAPH TYPES ============
enum class HeapNodeType {
    Hidden,
    Array,
    String,
    Object,
    Code,
    Closure,
    RegExp,
    HeapNumber,
    Native,
    DetachedDOM
};

enum class HeapEdgeType {
    ContextVariable,
    Element,
    Property,
    Internal,
    Hidden,
    Shortcut,
    Weak
};

struct HeapNode;

struct HeapEdge {
    HeapEdgeType type;
    StringView name_or_index;
    HeapNode* target_node;
    HeapEdge* next;
};

struct HeapNode {
    size_t id;
    HeapNodeType type;
    StringView name;             // Constructor or class name (e.g. "HTMLDivElement", "Array", "Object")
    size_t self_size;            // Bytes directly consumed by object
    size_t retained_size;        // Total bytes kept alive by this object
    size_t edge_count;
    HeapEdge* first_edge;
    HeapNode* next_node;
    bool visited;

    void add_edge(ArenaAllocator* arena, HeapEdgeType edge_type, StringView edge_name, HeapNode* target) {
        HeapEdge* edge = (HeapEdge*)arena->allocate(sizeof(HeapEdge));
        edge->type = edge_type;
        edge->name_or_index = copy_string(arena, edge_name);
        edge->target_node = target;
        edge->next = first_edge;
        first_edge = edge;
        edge_count++;
    }
};

// ============ DETACHED DOM LEAK DETECTOR ============
struct DetachedDOMNode {
    size_t node_id;
    StringView tag_name;
    StringView element_id;
    size_t retained_bytes;
    StringView retaining_js_variable; // Variable holding reference (e.g. "window.detachedList")
    DetachedDOMNode* next;
};

// ============ ALLOCATION TIMELINE SAMPLER ============
struct AllocationSample {
    double timestamp_ms;
    size_t allocated_bytes;
    size_t deallocated_bytes;
    size_t live_heap_bytes;
    StringView top_allocating_function;
    AllocationSample* next;
};

// ============ HEAP SNAPSHOT MODEL ============
class HeapSnapshot {
private:
    ArenaAllocator* arena;
    size_t snapshot_id;
    StringView title;
    double timestamp;
    HeapNode* root_node;
    HeapNode* first_node;
    size_t node_count;
    size_t total_size_bytes;
    
    DetachedDOMNode* first_detached_dom;
    size_t detached_dom_count;

public:
    HeapSnapshot(ArenaAllocator* a, size_t id, StringView snapshot_title, double time_ms)
        : arena(a), snapshot_id(id), title(copy_string(a, snapshot_title)), timestamp(time_ms),
          root_node(nullptr), first_node(nullptr), node_count(0), total_size_bytes(0),
          first_detached_dom(nullptr), detached_dom_count(0) {
        
        root_node = create_node(HeapNodeType::Object, StringView("Global / Window"), 64);
    }

    size_t get_id() const { return snapshot_id; }
    StringView get_title() const { return title; }
    double get_timestamp() const { return timestamp; }
    size_t get_total_size() const { return total_size_bytes; }
    size_t get_node_count() const { return node_count; }
    HeapNode* get_root() const { return root_node; }
    HeapNode* get_nodes() const { return first_node; }

    HeapNode* create_node(HeapNodeType type, StringView name, size_t self_bytes) {
        HeapNode* node = (HeapNode*)arena->allocate(sizeof(HeapNode));
        node->id = node_count + 1;
        node->type = type;
        node->name = copy_string(arena, name);
        node->self_size = self_bytes;
        node->retained_size = self_bytes; // Initial self size before edge accumulation
        node->edge_count = 0;
        node->first_edge = nullptr;
        node->visited = false;
        node->next_node = first_node;
        first_node = node;
        node_count++;
        total_size_bytes += self_bytes;
        return node;
    }

    void add_detached_dom_leak(size_t node_id, StringView tag_name, StringView elem_id, size_t retained_bytes, StringView var_holder) {
        DetachedDOMNode* leak = (DetachedDOMNode*)arena->allocate(sizeof(DetachedDOMNode));
        leak->node_id = node_id;
        leak->tag_name = copy_string(arena, tag_name);
        leak->element_id = copy_string(arena, elem_id);
        leak->retained_bytes = retained_bytes;
        leak->retaining_js_variable = copy_string(arena, var_holder);
        leak->next = first_detached_dom;
        first_detached_dom = leak;
        detached_dom_count++;
    }

    DetachedDOMNode* get_detached_dom_leaks() const { return first_detached_dom; }
    size_t get_detached_dom_count() const { return detached_dom_count; }

    // Computes retained sizes of all objects in graph traversal
    void calculate_retained_sizes() {
        HeapNode* curr = first_node;
        while (curr) {
            size_t accum = curr->self_size;
            HeapEdge* edge = curr->first_edge;
            while (edge) {
                if (edge->target_node) {
                    accum += edge->target_node->self_size;
                }
                edge = edge->next;
            }
            curr->retained_size = accum;
            curr = curr->next_node;
        }
    }
};

// ============ HEAP PROFILER ENGINE ============
class HeapProfiler {
private:
    ArenaAllocator* arena;
    HeapSnapshot* snapshots[16];
    size_t snapshot_count;
    
    AllocationSample* first_sample;
    size_t sample_count;
    bool is_tracking_allocations;

public:
    HeapProfiler(ArenaAllocator* a) 
        : arena(a), snapshot_count(0), first_sample(nullptr), sample_count(0), is_tracking_allocations(false) {}

    HeapSnapshot* take_snapshot(StringView title = StringView("Snapshot")) {
        if (snapshot_count >= 16) return snapshots[snapshot_count - 1];

        double now = 1700000000.0 + (double)(snapshot_count * 5000);
        HeapSnapshot* snapshot = (HeapSnapshot*)arena->allocate(sizeof(HeapSnapshot));
        new (snapshot) HeapSnapshot(arena, snapshot_count + 1, title, now);
        snapshots[snapshot_count++] = snapshot;
        return snapshot;
    }

    size_t get_snapshot_count() const { return snapshot_count; }
    HeapSnapshot* get_snapshot(size_t index) const {
        if (index < snapshot_count) return snapshots[index];
        return nullptr;
    }

    void start_allocation_tracking() {
        is_tracking_allocations = true;
    }

    void stop_allocation_tracking() {
        is_tracking_allocations = false;
    }

    bool is_tracking() const { return is_tracking_allocations; }

    void record_allocation_sample(double timestamp_ms, size_t alloc_bytes, size_t dealloc_bytes, size_t live_bytes, StringView top_func) {
        AllocationSample* sample = (AllocationSample*)arena->allocate(sizeof(AllocationSample));
        sample->timestamp_ms = timestamp_ms;
        sample->allocated_bytes = alloc_bytes;
        sample->deallocated_bytes = dealloc_bytes;
        sample->live_heap_bytes = live_bytes;
        sample->top_allocating_function = copy_string(arena, top_func);
        sample->next = first_sample;
        first_sample = sample;
        sample_count++;
    }

    AllocationSample* get_samples() const { return first_sample; }
    size_t get_sample_count() const { return sample_count; }
};
