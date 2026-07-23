#pragma once
#include "core.h"
#include "vfs.h"
#include "elements.h"
#include "cssom.h"
#include "json_parser.h"
#include "lighthouse_audit.h"
#include "browser_bindings.h"
#include "heap_profiler.h"
#include "network_model.h"
#include "storage_inspector.h"
#include "profiler.h"
#include "source_map.h"
#include "cdp_bridge.h"
#include "console.h"
#include "canvas_renderer.h"
#include "debugger_domain.h"
#include "virtual_list.h"
#include "syntax_highlighter.h"
#include "layout_tree.h"
#include "diff_engine.h"
#include "trace_model.h"
#include <new>

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

// ============ EXTERN "C" WASM EXPORT ABI LAYER ============
#ifdef __cplusplus
extern "C" {
#endif

// --- 1. ARENA MEMORY MANAGEMENT ---
EMSCRIPTEN_KEEPALIVE
inline ArenaAllocator* devtools_wasm_create_arena(size_t capacity) {
    return new ArenaAllocator(capacity > 0 ? capacity : 65536);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_destroy_arena(ArenaAllocator* arena) {
    if (arena) delete arena;
}

// --- 2. VIRTUAL FILE SYSTEM (VFS) CRUD ---
EMSCRIPTEN_KEEPALIVE
inline VirtualFileSystem* devtools_wasm_create_vfs(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(VirtualFileSystem));
    return new (mem) VirtualFileSystem(arena);
}

EMSCRIPTEN_KEEPALIVE
inline bool devtools_wasm_vfs_write_file(ArenaAllocator* arena, VirtualFileSystem* vfs, const char* path, const char* content) {
    if (!arena || !vfs || !path || !content) return false;
    return vfs->write_file(StringView(path), StringView(content));
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_vfs_read_file(VirtualFileSystem* vfs, const char* path) {
    if (!vfs || !path) return "";
    StringView res = vfs->read_file(StringView(path));
    return res.data ? res.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline bool devtools_wasm_vfs_delete_node(VirtualFileSystem* vfs, const char* path) {
    if (!vfs || !path) return false;
    return vfs->delete_node(StringView(path));
}

EMSCRIPTEN_KEEPALIVE
inline bool devtools_wasm_vfs_move_node(VirtualFileSystem* vfs, const char* src_path, const char* dest_path) {
    if (!vfs || !src_path || !dest_path) return false;
    return vfs->move_node(StringView(src_path), StringView(dest_path));
}

// --- 3. DOM & HTML PARSER ---
EMSCRIPTEN_KEEPALIVE
inline ElementsTree* devtools_wasm_parse_html(ArenaAllocator* arena, const char* html) {
    if (!arena || !html) return nullptr;
    void* mem = arena->allocate(sizeof(ElementsTree));
    ElementsTree* tree = new (mem) ElementsTree(arena);
    DOMParser parser(arena, tree);
    parser.parse(StringView(html));
    return tree;
}

EMSCRIPTEN_KEEPALIVE
inline DOMNode* devtools_wasm_query_selector(ElementsTree* tree, const char* selector) {
    if (!tree || !tree->root || !selector) return nullptr;
    return tree->query_selector(tree->root, StringView(selector));
}

// --- 4. LIGHTHOUSE AUDIT ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline LighthouseReport* devtools_wasm_run_lighthouse_audit(
    ArenaAllocator* arena,
    const char* url,
    double fcp_ms,
    double lcp_ms,
    double tbt_ms,
    double cls_score,
    double speed_index_ms,
    double inp_ms
) {
    if (!arena || !url) return nullptr;
    LighthouseAuditEngine engine(arena);
    CoreWebVitals vitals = { fcp_ms, lcp_ms, tbt_ms, cls_score, speed_index_ms, inp_ms };
    return engine.run_audit(StringView(url), vitals);
}

EMSCRIPTEN_KEEPALIVE
inline int devtools_wasm_get_lighthouse_perf_score(LighthouseReport* report) {
    return report ? report->performance_score : 0;
}

// --- 5. BROWSER ENVIRONMENT & STORAGE & TIMERS ---
EMSCRIPTEN_KEEPALIVE
inline BrowserEnvironment* devtools_wasm_create_browser_env(ArenaAllocator* arena, ElementsTree* tree) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(BrowserEnvironment));
    return new (mem) BrowserEnvironment(arena, tree);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_storage_set_item(ArenaAllocator* arena, BrowserEnvironment* env, const char* key, const char* val) {
    if (arena && env && key && val) {
        env->local_storage.set_item(arena, StringView(key), StringView(val));
    }
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_storage_get_item(BrowserEnvironment* env, const char* key) {
    if (!env || !key) return "";
    StringView val = env->local_storage.get_item(StringView(key));
    return val.data ? val.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline uint32_t devtools_wasm_set_timeout(BrowserEnvironment* env, const char* handler, double delay_ms) {
    if (!env || !handler) return 0;
    return env->set_timeout(StringView(handler), delay_ms);
}

// --- 6. HEAP PROFILER & MEMORY ---
EMSCRIPTEN_KEEPALIVE
inline HeapSnapshot* devtools_wasm_take_heap_snapshot(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    HeapProfiler profiler(arena);
    return profiler.take_snapshot();
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_get_heap_node_count(HeapSnapshot* snapshot) {
    return snapshot ? snapshot->get_node_count() : 0;
}

// --- 7. NETWORK MODEL & REQUEST TRACKING ---
EMSCRIPTEN_KEEPALIVE
inline NetworkModel* devtools_wasm_create_network_model(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(NetworkModel));
    return new (mem) NetworkModel(arena);
}

EMSCRIPTEN_KEEPALIVE
inline NetworkRequest* devtools_wasm_network_create_request(NetworkModel* model, const char* id, const char* url, const char* method, double ts) {
    if (!model || !id || !url || !method) return nullptr;
    return model->create_request(StringView(id), StringView(url), StringView(method), ts);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_network_update_response(NetworkModel* model, const char* id, int status, const char* status_text, const char* mime, double ts) {
    if (model && id) {
        model->update_response(StringView(id), status, StringView(status_text ? status_text : ""), StringView(mime ? mime : ""), ts);
    }
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_network_get_count(NetworkModel* model) {
    return model ? model->get_request_count() : 0;
}

// --- 8. CSSOM & STYLESHEET RESOLVER ---
EMSCRIPTEN_KEEPALIVE
inline StyleSheet* devtools_wasm_create_stylesheet(ArenaAllocator* arena, const char* css_content) {
    if (!arena || !css_content) return nullptr;
    void* mem = arena->allocate(sizeof(StyleSheet));
    StyleSheet* ss = new (mem) StyleSheet(arena);
    ss->parse(StringView(css_content));
    return ss;
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_resolve_element_property(ArenaAllocator* arena, StyleSheet* ss, DOMNode* node, const char* prop_name) {
    if (!arena || !ss || !node || !prop_name) return "";
    StyleEngine engine(arena, ss);
    CSSStyleDeclaration resolved = engine.resolve_style(node);
    StringView val = resolved.get_property(StringView(prop_name));
    if (val.length == 0 || !val.data) return "";
    StringView null_term = copy_string(arena, val);
    return null_term.data ? null_term.data : "";
}

// --- 9. CPU PERFORMANCE PROFILER ---
EMSCRIPTEN_KEEPALIVE
inline CPUProfile* devtools_wasm_create_cpu_profile(ArenaAllocator* arena, double start_ts, double end_ts) {
    if (!arena) return nullptr;
    ProfilerEngine profiler(arena);
    return profiler.create_profile(start_ts, end_ts);
}

EMSCRIPTEN_KEEPALIVE
inline ProfileNode* devtools_wasm_add_cpu_profile_node(ArenaAllocator* arena, ProfileNode* parent, int id, const char* fn_name, const char* url, int line, int col) {
    if (!arena || !fn_name || !url) return nullptr;
    ProfilerEngine profiler(arena);
    return profiler.add_node(parent, id, StringView(fn_name), StringView(url), line, col);
}

// --- 10. SOURCE MAP VLQ DECODER ---
EMSCRIPTEN_KEEPALIVE
inline SourceMapMapping* devtools_wasm_parse_sourcemap_mappings(ArenaAllocator* arena, const char* mappings_str, size_t* out_count) {
    if (!arena || !mappings_str || !out_count) return nullptr;
    SourceMapEngine engine(arena);
    return engine.parse_mappings(StringView(mappings_str), *out_count);
}

// --- 11. CDP PROTOCOL ROUTER BRIDGE ---
EMSCRIPTEN_KEEPALIVE
inline CdpBridge* devtools_wasm_create_cdp_bridge(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* parser_mem = arena->allocate(sizeof(JsonParser));
    JsonParser* parser = new (parser_mem) JsonParser(arena);
    void* bridge_mem = arena->allocate(sizeof(CdpBridge));
    return new (bridge_mem) CdpBridge(arena, parser);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_cdp_process_message(CdpBridge* bridge, const char* json_str) {
    if (bridge && json_str) {
        bridge->process_message(json_str);
    }
}

// --- 12. DEVTOOLS CONSOLE ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline ConsoleEngine* devtools_wasm_create_console(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(ConsoleEngine));
    return new (mem) ConsoleEngine(arena);
}

EMSCRIPTEN_KEEPALIVE
inline ConsoleMessage* devtools_wasm_console_add_log(ConsoleEngine* console_obj, const char* text, double ts) {
    if (!console_obj || !text) return nullptr;
    return console_obj->add_message(ConsoleMessageType::Log, ConsoleMessageLevel::Info, StringView(text), ts);
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_console_get_count(ConsoleEngine* console_obj) {
    return console_obj ? console_obj->get_message_count() : 0;
}

// --- 13. CANVAS RENDERER QUEUE ---
EMSCRIPTEN_KEEPALIVE
inline CanvasRenderer* devtools_wasm_create_canvas_renderer(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(CanvasRenderer));
    return new (mem) CanvasRenderer(arena);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_canvas_draw_rect(CanvasRenderer* canvas, int x, int y, int w, int h, const char* color) {
    if (canvas && color) {
        canvas->draw_rect(x, y, w, h, StringView(color));
    }
}

// --- 14. DEBUGGER & BREAKPOINTS ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline DebuggerDomain* devtools_wasm_create_debugger(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(DebuggerDomain));
    return new (mem) DebuggerDomain(arena);
}

EMSCRIPTEN_KEEPALIVE
inline Breakpoint* devtools_wasm_debugger_set_breakpoint(DebuggerDomain* dbg, const char* url, int line) {
    if (!dbg || !url) return nullptr;
    return dbg->get_breakpoint_engine()->set_breakpoint(StringView(url), line);
}

// --- 15. FULL STORAGE INSPECTOR (LocalStorage, SessionStorage, Cookies, IndexedDB, Cache Storage, SW & PWA) ---
EMSCRIPTEN_KEEPALIVE
inline StorageInspector* devtools_wasm_create_storage_inspector(ArenaAllocator* arena, const char* origin) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(StorageInspector));
    return new (mem) StorageInspector(arena, StringView(origin ? origin : "https://localhost:3000"));
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_storage_get_quota_total(StorageInspector* inspector) {
    if (!inspector) return 0;
    return inspector->get_usage_and_quota().total_bytes;
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_storage_get_quota_limit(StorageInspector* inspector) {
    if (!inspector) return 0;
    return inspector->get_usage_and_quota().quota_limit_bytes;
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_storage_clear_all(StorageInspector* inspector) {
    if (inspector) inspector->clear_all_storage();
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_storage_set_local_item(StorageInspector* inspector, const char* key, const char* val) {
    if (inspector && key && val) {
        inspector->get_local_storage()->set_item(StringView(key), StringView(val));
    }
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_storage_get_local_item(StorageInspector* inspector, const char* key) {
    if (!inspector || !key) return "";
    StringView val = inspector->get_local_storage()->get_item(StringView(key));
    return val.data ? val.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_storage_set_session_item(StorageInspector* inspector, const char* key, const char* val) {
    if (inspector && key && val) {
        inspector->get_session_storage()->set_item(StringView(key), StringView(val));
    }
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_storage_get_session_item(StorageInspector* inspector, const char* key) {
    if (!inspector || !key) return "";
    StringView val = inspector->get_session_storage()->get_item(StringView(key));
    return val.data ? val.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_cookie_set(StorageInspector* inspector, const char* name, const char* value, const char* domain, const char* path, double expires, bool http_only, bool secure, const char* same_site) {
    if (inspector && name && value && domain) {
        inspector->get_cookie_store()->set_cookie(
            StringView(name), StringView(value), StringView(domain),
            StringView(path ? path : "/"), expires, http_only, secure, StringView(same_site ? same_site : "Lax")
        );
    }
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_cookie_get(StorageInspector* inspector, const char* name, const char* domain) {
    if (!inspector || !name || !domain) return "";
    StringView val = inspector->get_cookie_store()->get_cookie_value(StringView(name), StringView(domain));
    return val.data ? val.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_cookie_get_count(StorageInspector* inspector) {
    return inspector ? inspector->get_cookie_store()->get_count() : 0;
}

EMSCRIPTEN_KEEPALIVE
inline IDBDatabase* devtools_wasm_idb_create_db(StorageInspector* inspector, const char* origin, const char* db_name, int version) {
    if (!inspector || !origin || !db_name) return nullptr;
    return inspector->get_indexeddb_model()->create_database(StringView(origin), StringView(db_name), version);
}

EMSCRIPTEN_KEEPALIVE
inline IDBObjectStore* devtools_wasm_idb_create_store(IDBDatabase* db, ArenaAllocator* arena, const char* store_name, const char* key_path) {
    if (!db || !arena || !store_name) return nullptr;
    return db->create_object_store(arena, StringView(store_name), StringView(key_path ? key_path : "id"));
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_idb_put(IDBObjectStore* store, ArenaAllocator* arena, const char* key, const char* value) {
    if (store && arena && key && value) {
        store->put(arena, StringView(key), StringView(value));
    }
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_idb_get(IDBObjectStore* store, const char* key) {
    if (!store || !key) return "";
    StringView val = store->get(StringView(key));
    return val.data ? val.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline CacheStore* devtools_wasm_cache_create(StorageInspector* inspector, const char* origin, const char* cache_name) {
    if (!inspector || !origin || !cache_name) return nullptr;
    return inspector->get_cache_storage_model()->create_cache(StringView(origin), StringView(cache_name));
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_cache_put(CacheStore* cache, ArenaAllocator* arena, const char* url, int status, const char* mime, const char* type, size_t size, double ts) {
    if (cache && arena && url) {
        cache->put(arena, StringView(url), status, StringView(mime ? mime : "text/html"), StringView(type ? type : "basic"), size, ts);
    }
}

EMSCRIPTEN_KEEPALIVE
inline ServiceWorkerRegistration* devtools_wasm_sw_register(StorageInspector* inspector, const char* reg_id, const char* scope, const char* script_url, int status_code) {
    if (!inspector || !reg_id || !scope || !script_url) return nullptr;
    return inspector->get_sw_manager()->register_worker(StringView(reg_id), StringView(scope), StringView(script_url), static_cast<ServiceWorkerStatus>(status_code));
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_pwa_get_manifest_name(StorageInspector* inspector) {
    if (!inspector) return "";
    return inspector->get_manifest().name.data ? inspector->get_manifest().name.data : "";
}

// --- 16. JSON PARSER ---
EMSCRIPTEN_KEEPALIVE
inline JsonParser* devtools_wasm_create_json_parser(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(JsonParser));
    return new (mem) JsonParser(arena);
}

EMSCRIPTEN_KEEPALIVE
inline JsonNode* devtools_wasm_json_parse(JsonParser* parser, const char* json_str) {
    if (!parser || !json_str) return nullptr;
    return parser->parse(json_str);
}

EMSCRIPTEN_KEEPALIVE
inline int devtools_wasm_json_get_type(JsonNode* node) {
    return node ? static_cast<int>(node->type) : 0;
}

EMSCRIPTEN_KEEPALIVE
inline const char* devtools_wasm_json_get_string(ArenaAllocator* arena, JsonNode* node) {
    if (!node || node->type != JsonType::String || !arena) return "";
    StringView null_term = copy_string(arena, node->string_val);
    return null_term.data ? null_term.data : "";
}

EMSCRIPTEN_KEEPALIVE
inline double devtools_wasm_json_get_number(JsonNode* node) {
    if (!node || node->type != JsonType::Number) return 0.0;
    return node->number_val;
}

EMSCRIPTEN_KEEPALIVE
inline bool devtools_wasm_json_get_bool(JsonNode* node) {
    if (!node || node->type != JsonType::Bool) return false;
    return node->bool_val;
}

EMSCRIPTEN_KEEPALIVE
inline JsonNode* devtools_wasm_json_get_property(JsonNode* node, const char* key) {
    if (!node || !key) return nullptr;
    return node->get(key);
}

// --- 17. VIRTUAL LIST COMPUTE ---
EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_virtual_list_compute(int total_items, int item_height, int viewport_height, int scroll_y, int* out_start, int* out_end) {
    if (!out_start || !out_end) return;
    VirtualListState state = {total_items, item_height, viewport_height, scroll_y};
    VirtualList::compute_visible_range(state, *out_start, *out_end);
}

// --- 18. SYNTAX HIGHLIGHTER ---
EMSCRIPTEN_KEEPALIVE
inline SyntaxHighlighter* devtools_wasm_create_highlighter(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(SyntaxHighlighter));
    return new (mem) SyntaxHighlighter(arena);
}

EMSCRIPTEN_KEEPALIVE
inline Token* devtools_wasm_highlighter_tokenize(SyntaxHighlighter* highlighter, const char* code) {
    if (!highlighter || !code) return nullptr;
    size_t len = 0;
    while (code[len]) len++;
    return highlighter->tokenize(StringView(code, len));
}

// --- 19. LAYOUT ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline LayoutEngine* devtools_wasm_create_layout_engine(ArenaAllocator* arena, StyleEngine* style_engine) {
    if (!arena || !style_engine) return nullptr;
    void* mem = arena->allocate(sizeof(LayoutEngine));
    return new (mem) LayoutEngine(arena, style_engine);
}

EMSCRIPTEN_KEEPALIVE
inline LayoutNode* devtools_wasm_layout_build_tree(LayoutEngine* engine, DOMNode* dom) {
    if (!engine || !dom) return nullptr;
    return engine->build_tree(dom);
}

EMSCRIPTEN_KEEPALIVE
inline LayoutNode* devtools_wasm_layout_hit_test(LayoutNode* root, double x, double y) {
    if (!root) return nullptr;
    return root->hit_test(x, y);
}

// --- 20. MYERS DIFF ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline DiffEngine* devtools_wasm_create_diff_engine(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(DiffEngine));
    return new (mem) DiffEngine(arena);
}

EMSCRIPTEN_KEEPALIVE
inline DiffResult* devtools_wasm_diff_compute(DiffEngine* engine, const char* old_txt, const char* new_txt) {
    if (!engine || !old_txt || !new_txt) return nullptr;
    size_t old_len = 0; while (old_txt[old_len]) old_len++;
    size_t new_len = 0; while (new_txt[new_len]) new_len++;
    return engine->compute_diff(StringView(old_txt, old_len), StringView(new_txt, new_len));
}

// --- 21. TRACE MODEL ENGINE ---
EMSCRIPTEN_KEEPALIVE
inline TraceModel* devtools_wasm_create_trace_model(ArenaAllocator* arena) {
    if (!arena) return nullptr;
    void* mem = arena->allocate(sizeof(TraceModel));
    return new (mem) TraceModel(arena);
}

EMSCRIPTEN_KEEPALIVE
inline TraceNode* devtools_wasm_trace_parse_events(TraceModel* model, JsonNode* root_array, size_t* out_count) {
    if (!model || !root_array || !out_count) return nullptr;
    return model->parse_events(root_array, *out_count);
}

// --- 22. WEBSOCKET REMOTE DEBUGGING CDP AGENT ---
EMSCRIPTEN_KEEPALIVE
inline WebSocketCDPAgent* devtools_wasm_create_ws_cdp_agent(ArenaAllocator* arena, CdpBridge* bridge) {
    if (!arena || !bridge) return nullptr;
    void* mem = arena->allocate(sizeof(WebSocketCDPAgent));
    return new (mem) WebSocketCDPAgent(arena, bridge);
}

EMSCRIPTEN_KEEPALIVE
inline void devtools_wasm_ws_cdp_agent_process_frame(WebSocketCDPAgent* agent, const uint8_t* raw_buf, size_t buf_len) {
    if (!agent || !raw_buf) return;
    agent->process_ws_frame(raw_buf, buf_len);
}

EMSCRIPTEN_KEEPALIVE
inline size_t devtools_wasm_ws_cdp_agent_encode_frame(const char* text_payload, size_t payload_len, uint8_t* out_buf, size_t out_capacity) {
    return WebSocketCDPAgent::encode_ws_text_frame(text_payload, payload_len, out_buf, out_capacity);
}

#ifdef __cplusplus
}
#endif

