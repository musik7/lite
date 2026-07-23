#pragma once
#include "core.h"
#include "json_parser.h"

enum class ResourceType {
    Document,
    Stylesheet,
    Image,
    Media,
    Font,
    Script,
    TextTrack,
    XHR,
    Fetch,
    EventSource,
    WebSocket,
    Manifest,
    SignedExchange,
    Ping,
    CSPViolationReport,
    Preflight,
    Other
};

struct HTTPHeader {
    StringView name;
    StringView value;
    HTTPHeader* next;
};

struct NetworkRequest {
    StringView request_id;
    StringView url;
    StringView method;
    HTTPHeader* request_headers;
    StringView post_data;
    
    int status_code;
    StringView status_text;
    HTTPHeader* response_headers;
    StringView mime_type;
    
    ResourceType type;
    
    double start_time;
    double response_time;
    double end_time;
    
    size_t transfer_size;
    size_t resource_size;
    
    bool from_cache;
    bool has_error;
    StringView error_message;
    
    NetworkRequest* next;
    
    void add_request_header(ArenaAllocator* arena, StringView name, StringView value) {
        HTTPHeader* h = (HTTPHeader*)arena->allocate(sizeof(HTTPHeader));
        h->name = copy_string(arena, name);
        h->value = copy_string(arena, value);
        h->next = request_headers;
        request_headers = h;
    }
    
    void add_response_header(ArenaAllocator* arena, StringView name, StringView value) {
        HTTPHeader* h = (HTTPHeader*)arena->allocate(sizeof(HTTPHeader));
        h->name = copy_string(arena, name);
        h->value = copy_string(arena, value);
        h->next = response_headers;
        response_headers = h;
    }
    
    StringView get_response_header(StringView name) {
        HTTPHeader* curr = response_headers;
        while (curr) {
            if (curr->name == name) return curr->value;
            curr = curr->next;
        }
        return StringView();
    }
};

class NetworkModel {
    ArenaAllocator* arena;
    NetworkRequest* first_request;
    NetworkRequest* last_request;
    size_t request_count;

public:
    NetworkModel(ArenaAllocator* a) : arena(a), first_request(nullptr), last_request(nullptr), request_count(0) {}

    NetworkRequest* create_request(StringView req_id, StringView url, StringView method, double timestamp) {
        NetworkRequest* req = (NetworkRequest*)arena->allocate(sizeof(NetworkRequest));
        req->request_id = copy_string(arena, req_id);
        req->url = copy_string(arena, url);
        req->method = copy_string(arena, method);
        req->request_headers = nullptr;
        req->post_data = StringView();
        
        req->status_code = 0;
        req->status_text = StringView();
        req->response_headers = nullptr;
        req->mime_type = StringView();
        
        req->type = ResourceType::Other;
        
        req->start_time = timestamp;
        req->response_time = 0.0;
        req->end_time = 0.0;
        
        req->transfer_size = 0;
        req->resource_size = 0;
        
        req->from_cache = false;
        req->has_error = false;
        req->error_message = StringView();
        
        req->next = nullptr;
        
        if (!first_request) {
            first_request = last_request = req;
        } else {
            last_request->next = req;
            last_request = req;
        }
        request_count++;
        
        return req;
    }

    NetworkRequest* get_request(StringView req_id) {
        NetworkRequest* curr = first_request;
        while (curr) {
            if (curr->request_id == req_id) {
                return curr;
            }
            curr = curr->next;
        }
        return nullptr;
    }

    void update_response(StringView req_id, int status_code, StringView status_text, StringView mime_type, double timestamp) {
        NetworkRequest* req = get_request(req_id);
        if (req) {
            req->status_code = status_code;
            req->status_text = copy_string(arena, status_text);
            req->mime_type = copy_string(arena, mime_type);
            req->response_time = timestamp;
        }
    }

    void complete_request(StringView req_id, size_t transfer_size, size_t resource_size, double timestamp) {
        NetworkRequest* req = get_request(req_id);
        if (req) {
            req->transfer_size = transfer_size;
            req->resource_size = resource_size;
            req->end_time = timestamp;
        }
    }

    void fail_request(StringView req_id, StringView error_msg, double timestamp) {
        NetworkRequest* req = get_request(req_id);
        if (req) {
            req->has_error = true;
            req->error_message = copy_string(arena, error_msg);
            req->end_time = timestamp;
        }
    }

    NetworkRequest* get_requests() const { return first_request; }
    size_t get_request_count() const { return request_count; }
    
    void clear() {
        first_request = last_request = nullptr;
        request_count = 0;
    }
};
