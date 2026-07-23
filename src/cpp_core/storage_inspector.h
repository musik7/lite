#pragma once
#include "core.h"
#include "json_parser.h"

// ============ DOM STORAGE ============
enum class StorageType {
    Local,
    Session
};

struct StorageEntry {
    StringView key;
    StringView value;
    StorageEntry* next;
};

class DOMStorageArea {
private:
    ArenaAllocator* arena;
    StringView security_origin;
    StorageType type;
    StorageEntry* first_entry;
    size_t count;

public:
    DOMStorageArea(ArenaAllocator* a, StringView origin, StorageType t) 
        : arena(a), security_origin(copy_string(a, origin)), type(t), first_entry(nullptr), count(0) {}

    StringView get_origin() const { return security_origin; }
    StorageType get_type() const { return type; }
    size_t get_count() const { return count; }
    StorageEntry* get_entries() const { return first_entry; }

    void set_item(StringView key, StringView value) {
        StorageEntry* curr = first_entry;
        while (curr) {
            if (curr->key == key) {
                curr->value = copy_string(arena, value);
                return;
            }
            curr = curr->next;
        }

        StorageEntry* entry = (StorageEntry*)arena->allocate(sizeof(StorageEntry));
        entry->key = copy_string(arena, key);
        entry->value = copy_string(arena, value);
        entry->next = first_entry;
        first_entry = entry;
        count++;
    }

    StringView get_item(StringView key) const {
        StorageEntry* curr = first_entry;
        while (curr) {
            if (curr->key == key) {
                return curr->value;
            }
            curr = curr->next;
        }
        return StringView();
    }

    void remove_item(StringView key) {
        if (!first_entry) return;
        if (first_entry->key == key) {
            first_entry = first_entry->next;
            count--;
            return;
        }
        StorageEntry* curr = first_entry;
        while (curr->next) {
            if (curr->next->key == key) {
                curr->next = curr->next->next;
                count--;
                return;
            }
            curr = curr->next;
        }
    }

    void clear() {
        first_entry = nullptr;
        count = 0;
    }
};

// ============ COOKIE STORE ============
struct CookieItem {
    StringView name;
    StringView value;
    StringView domain;
    StringView path;
    double expires;
    size_t size;
    bool http_only;
    bool secure;
    StringView same_site;
    CookieItem* next;
};

class CookieStore {
private:
    ArenaAllocator* arena;
    CookieItem* first_cookie;
    size_t count;

public:
    CookieStore(ArenaAllocator* a) : arena(a), first_cookie(nullptr), count(0) {}

    size_t get_count() const { return count; }
    CookieItem* get_cookies() const { return first_cookie; }

    void set_cookie(StringView name, StringView value, StringView domain, StringView path = StringView("/"),
                    double expires = 0.0, bool http_only = false, bool secure = false, StringView same_site = StringView("Lax")) {
        CookieItem* curr = first_cookie;
        while (curr) {
            if (curr->name == name && curr->domain == domain && curr->path == path) {
                curr->value = copy_string(arena, value);
                curr->expires = expires;
                curr->http_only = http_only;
                curr->secure = secure;
                curr->same_site = copy_string(arena, same_site);
                curr->size = name.length + value.length;
                return;
            }
            curr = curr->next;
        }

        CookieItem* item = (CookieItem*)arena->allocate(sizeof(CookieItem));
        item->name = copy_string(arena, name);
        item->value = copy_string(arena, value);
        item->domain = copy_string(arena, domain);
        item->path = copy_string(arena, path);
        item->expires = expires;
        item->size = name.length + value.length;
        item->http_only = http_only;
        item->secure = secure;
        item->same_site = copy_string(arena, same_site);
        item->next = first_cookie;
        first_cookie = item;
        count++;
    }

    StringView get_cookie_value(StringView name, StringView domain) const {
        CookieItem* curr = first_cookie;
        while (curr) {
            if (curr->name == name && curr->domain == domain) {
                return curr->value;
            }
            curr = curr->next;
        }
        return StringView();
    }

    void delete_cookie(StringView name, StringView domain, StringView path = StringView("/")) {
        if (!first_cookie) return;
        if (first_cookie->name == name && first_cookie->domain == domain && first_cookie->path == path) {
            first_cookie = first_cookie->next;
            count--;
            return;
        }
        CookieItem* curr = first_cookie;
        while (curr->next) {
            if (curr->next->name == name && curr->next->domain == domain && curr->next->path == path) {
                curr->next = curr->next->next;
                count--;
                return;
            }
            curr = curr->next;
        }
    }

    void clear() {
        first_cookie = nullptr;
        count = 0;
    }
};

// ============ INDEXEDDB MODEL ============
struct IDBEntry {
    StringView key;
    StringView value;
    IDBEntry* next;
};

struct IDBObjectStore {
    StringView name;
    StringView key_path;
    IDBEntry* first_entry;
    size_t count;
    IDBObjectStore* next;

    void put(ArenaAllocator* arena, StringView key, StringView value) {
        IDBEntry* curr = first_entry;
        while (curr) {
            if (curr->key == key) {
                curr->value = copy_string(arena, value);
                return;
            }
            curr = curr->next;
        }
        IDBEntry* entry = (IDBEntry*)arena->allocate(sizeof(IDBEntry));
        entry->key = copy_string(arena, key);
        entry->value = copy_string(arena, value);
        entry->next = first_entry;
        first_entry = entry;
        count++;
    }

    StringView get(StringView key) const {
        IDBEntry* curr = first_entry;
        while (curr) {
            if (curr->key == key) return curr->value;
            curr = curr->next;
        }
        return StringView();
    }

    void delete_key(StringView key) {
        if (!first_entry) return;
        if (first_entry->key == key) {
            first_entry = first_entry->next;
            count--;
            return;
        }
        IDBEntry* curr = first_entry;
        while (curr->next) {
            if (curr->next->key == key) {
                curr->next = curr->next->next;
                count--;
                return;
            }
            curr = curr->next;
        }
    }
};

struct IDBDatabase {
    StringView name;
    StringView origin;
    int version;
    IDBObjectStore* first_store;
    IDBDatabase* next;

    IDBObjectStore* create_object_store(ArenaAllocator* arena, StringView store_name, StringView key_path = StringView("id")) {
        IDBObjectStore* curr = first_store;
        while (curr) {
            if (curr->name == store_name) return curr;
            curr = curr->next;
        }
        IDBObjectStore* store = (IDBObjectStore*)arena->allocate(sizeof(IDBObjectStore));
        store->name = copy_string(arena, store_name);
        store->key_path = copy_string(arena, key_path);
        store->first_entry = nullptr;
        store->count = 0;
        store->next = first_store;
        first_store = store;
        return store;
    }

    IDBObjectStore* get_object_store(StringView store_name) const {
        IDBObjectStore* curr = first_store;
        while (curr) {
            if (curr->name == store_name) return curr;
            curr = curr->next;
        }
        return nullptr;
    }
};

class IndexedDBModel {
private:
    ArenaAllocator* arena;
    IDBDatabase* first_db;

public:
    IndexedDBModel(ArenaAllocator* a) : arena(a), first_db(nullptr) {}

    IDBDatabase* create_database(StringView origin, StringView db_name, int version = 1) {
        IDBDatabase* curr = first_db;
        while (curr) {
            if (curr->origin == origin && curr->name == db_name) {
                curr->version = version;
                return curr;
            }
            curr = curr->next;
        }
        IDBDatabase* db = (IDBDatabase*)arena->allocate(sizeof(IDBDatabase));
        db->name = copy_string(arena, db_name);
        db->origin = copy_string(arena, origin);
        db->version = version;
        db->first_store = nullptr;
        db->next = first_db;
        first_db = db;
        return db;
    }

    IDBDatabase* get_database(StringView origin, StringView db_name) const {
        IDBDatabase* curr = first_db;
        while (curr) {
            if (curr->origin == origin && curr->name == db_name) return curr;
            curr = curr->next;
        }
        return nullptr;
    }

    void delete_database(StringView origin, StringView db_name) {
        if (!first_db) return;
        if (first_db->origin == origin && first_db->name == db_name) {
            first_db = first_db->next;
            return;
        }
        IDBDatabase* curr = first_db;
        while (curr->next) {
            if (curr->next->origin == origin && curr->next->name == db_name) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    void clear_for_origin(StringView origin) {
        while (first_db && first_db->origin == origin) {
            first_db = first_db->next;
        }
        if (!first_db) return;
        IDBDatabase* curr = first_db;
        while (curr->next) {
            if (curr->next->origin == origin) {
                curr->next = curr->next->next;
            } else {
                curr = curr->next;
            }
        }
    }
};

// ============ CACHE STORAGE MODEL ============
struct CacheResponseItem {
    StringView request_url;
    StringView response_type;
    int status_code;
    StringView mime_type;
    size_t response_size;
    double response_time;
    CacheResponseItem* next;
};

struct CacheStore {
    StringView cache_name;
    StringView origin;
    CacheResponseItem* first_response;
    size_t response_count;
    CacheStore* next;

    void put(ArenaAllocator* arena, StringView url, int status, StringView mime, StringView type, size_t size, double timestamp) {
        CacheResponseItem* curr = first_response;
        while (curr) {
            if (curr->request_url == url) {
                curr->status_code = status;
                curr->mime_type = copy_string(arena, mime);
                curr->response_type = copy_string(arena, type);
                curr->response_size = size;
                curr->response_time = timestamp;
                return;
            }
            curr = curr->next;
        }

        CacheResponseItem* item = (CacheResponseItem*)arena->allocate(sizeof(CacheResponseItem));
        item->request_url = copy_string(arena, url);
        item->status_code = status;
        item->mime_type = copy_string(arena, mime);
        item->response_type = copy_string(arena, type);
        item->response_size = size;
        item->response_time = timestamp;
        item->next = first_response;
        first_response = item;
        response_count++;
    }

    bool delete_entry(StringView url) {
        if (!first_response) return false;
        if (first_response->request_url == url) {
            first_response = first_response->next;
            response_count--;
            return true;
        }
        CacheResponseItem* curr = first_response;
        while (curr->next) {
            if (curr->next->request_url == url) {
                curr->next = curr->next->next;
                response_count--;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }
};

class CacheStorageModel {
private:
    ArenaAllocator* arena;
    CacheStore* first_cache;

public:
    CacheStorageModel(ArenaAllocator* a) : arena(a), first_cache(nullptr) {}

    CacheStore* create_cache(StringView origin, StringView cache_name) {
        CacheStore* curr = first_cache;
        while (curr) {
            if (curr->origin == origin && curr->cache_name == cache_name) return curr;
            curr = curr->next;
        }

        CacheStore* cache = (CacheStore*)arena->allocate(sizeof(CacheStore));
        cache->cache_name = copy_string(arena, cache_name);
        cache->origin = copy_string(arena, origin);
        cache->first_response = nullptr;
        cache->response_count = 0;
        cache->next = first_cache;
        first_cache = cache;
        return cache;
    }

    CacheStore* get_cache(StringView origin, StringView cache_name) const {
        CacheStore* curr = first_cache;
        while (curr) {
            if (curr->origin == origin && curr->cache_name == cache_name) return curr;
            curr = curr->next;
        }
        return nullptr;
    }

    void delete_cache(StringView origin, StringView cache_name) {
        if (!first_cache) return;
        if (first_cache->origin == origin && first_cache->cache_name == cache_name) {
            first_cache = first_cache->next;
            return;
        }
        CacheStore* curr = first_cache;
        while (curr->next) {
            if (curr->next->origin == origin && curr->next->cache_name == cache_name) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    CacheStore* get_caches() const { return first_cache; }
};

// ============ SERVICE WORKERS & PWA MANIFEST ============
enum class ServiceWorkerStatus {
    Installing,
    Installed,
    Activating,
    Activated,
    Redundant
};

struct ServiceWorkerRegistration {
    StringView registration_id;
    StringView scope;
    StringView script_url;
    ServiceWorkerStatus status;
    bool is_active;
    bool fetch_handler_registered;
    double last_update_time;
    ServiceWorkerRegistration* next;
};

class ServiceWorkerManager {
private:
    ArenaAllocator* arena;
    ServiceWorkerRegistration* first_sw;

public:
    ServiceWorkerManager(ArenaAllocator* a) : arena(a), first_sw(nullptr) {}

    ServiceWorkerRegistration* register_worker(StringView reg_id, StringView scope, StringView script_url, ServiceWorkerStatus status = ServiceWorkerStatus::Activated) {
        ServiceWorkerRegistration* curr = first_sw;
        while (curr) {
            if (curr->registration_id == reg_id) {
                curr->scope = copy_string(arena, scope);
                curr->script_url = copy_string(arena, script_url);
                curr->status = status;
                curr->is_active = (status == ServiceWorkerStatus::Activated);
                return curr;
            }
            curr = curr->next;
        }

        ServiceWorkerRegistration* sw = (ServiceWorkerRegistration*)arena->allocate(sizeof(ServiceWorkerRegistration));
        sw->registration_id = copy_string(arena, reg_id);
        sw->scope = copy_string(arena, scope);
        sw->script_url = copy_string(arena, script_url);
        sw->status = status;
        sw->is_active = (status == ServiceWorkerStatus::Activated);
        sw->fetch_handler_registered = true;
        sw->last_update_time = 1700000000.0;
        sw->next = first_sw;
        first_sw = sw;
        return sw;
    }

    void unregister_worker(StringView reg_id) {
        if (!first_sw) return;
        if (first_sw->registration_id == reg_id) {
            first_sw = first_sw->next;
            return;
        }
        ServiceWorkerRegistration* curr = first_sw;
        while (curr->next) {
            if (curr->next->registration_id == reg_id) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    ServiceWorkerRegistration* get_workers() const { return first_sw; }
};

struct PWAManifest {
    StringView name;
    StringView short_name;
    StringView start_url;
    StringView display;
    StringView theme_color;
    StringView background_color;
    bool is_valid;
};

// ============ STORAGE INSPECTOR MAIN MANAGER ============
struct StorageQuotaUsage {
    size_t local_storage_bytes;
    size_t session_storage_bytes;
    size_t cookies_bytes;
    size_t indexeddb_bytes;
    size_t cache_storage_bytes;
    size_t total_bytes;
    size_t quota_limit_bytes;
};

class StorageInspector {
private:
    ArenaAllocator* arena;
    DOMStorageArea* local_storage;
    DOMStorageArea* session_storage;
    CookieStore* cookie_store;
    IndexedDBModel* indexeddb_model;
    CacheStorageModel* cache_storage_model;
    ServiceWorkerManager* sw_manager;
    PWAManifest manifest;

public:
    StorageInspector(ArenaAllocator* a, StringView default_origin = StringView("https://localhost:3000")) 
        : arena(a) {
        local_storage = (DOMStorageArea*)arena->allocate(sizeof(DOMStorageArea));
        new (local_storage) DOMStorageArea(a, default_origin, StorageType::Local);

        session_storage = (DOMStorageArea*)arena->allocate(sizeof(DOMStorageArea));
        new (session_storage) DOMStorageArea(a, default_origin, StorageType::Session);

        cookie_store = (CookieStore*)arena->allocate(sizeof(CookieStore));
        new (cookie_store) CookieStore(a);

        indexeddb_model = (IndexedDBModel*)arena->allocate(sizeof(IndexedDBModel));
        new (indexeddb_model) IndexedDBModel(a);

        cache_storage_model = (CacheStorageModel*)arena->allocate(sizeof(CacheStorageModel));
        new (cache_storage_model) CacheStorageModel(a);

        sw_manager = (ServiceWorkerManager*)arena->allocate(sizeof(ServiceWorkerManager));
        new (sw_manager) ServiceWorkerManager(a);

        manifest.name = copy_string(a, StringView("Mobile DevTools PWA"));
        manifest.short_name = copy_string(a, StringView("DevTools"));
        manifest.start_url = copy_string(a, StringView("/"));
        manifest.display = copy_string(a, StringView("standalone"));
        manifest.theme_color = copy_string(a, StringView("#1e1e1e"));
        manifest.background_color = copy_string(a, StringView("#181818"));
        manifest.is_valid = true;
    }

    DOMStorageArea* get_local_storage() { return local_storage; }
    DOMStorageArea* get_session_storage() { return session_storage; }
    CookieStore* get_cookie_store() { return cookie_store; }
    IndexedDBModel* get_indexeddb_model() { return indexeddb_model; }
    CacheStorageModel* get_cache_storage_model() { return cache_storage_model; }
    ServiceWorkerManager* get_sw_manager() { return sw_manager; }
    PWAManifest get_manifest() const { return manifest; }

    StorageQuotaUsage get_usage_and_quota() const {
        StorageQuotaUsage usage = {0, 0, 0, 0, 0, 0, 50 * 1024 * 1024}; // 50MB default quota limit

        // Local Storage
        StorageEntry* entry = local_storage->get_entries();
        while (entry) {
            usage.local_storage_bytes += entry->key.length + entry->value.length;
            entry = entry->next;
        }

        // Session Storage
        entry = session_storage->get_entries();
        while (entry) {
            usage.session_storage_bytes += entry->key.length + entry->value.length;
            entry = entry->next;
        }

        // Cookies
        CookieItem* cookie = cookie_store->get_cookies();
        while (cookie) {
            usage.cookies_bytes += cookie->size;
            cookie = cookie->next;
        }

        // Cache Storage
        CacheStore* cache = cache_storage_model->get_caches();
        while (cache) {
            CacheResponseItem* res = cache->first_response;
            while (res) {
                usage.cache_storage_bytes += res->response_size;
                res = res->next;
            }
            cache = cache->next;
        }

        usage.total_bytes = usage.local_storage_bytes + usage.session_storage_bytes + usage.cookies_bytes + usage.indexeddb_bytes + usage.cache_storage_bytes;
        return usage;
    }

    void clear_all_storage() {
        local_storage->clear();
        session_storage->clear();
        cookie_store->clear();
    }
};
