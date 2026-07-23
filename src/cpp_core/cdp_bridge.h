#pragma once
#include "core.h"
#include "json_parser.h"


enum class CDPDomain {
    DOM,
    CSS,
    Page,
    Network,
    Debugger,
    Runtime,
    Profiler,
    Log,
    Unknown
};

class CDPHelper {
public:
    static CDPDomain parse_domain(StringView method) {
        if (method.length == 0) return CDPDomain::Unknown;
        
        size_t dot_idx = 0;
        while (dot_idx < method.length && method.data[dot_idx] != '.') dot_idx++;
        
        if (dot_idx == method.length) return CDPDomain::Unknown;
        
        StringView domain_str(method.data, dot_idx);
        if (domain_str == StringView("DOM", 3)) return CDPDomain::DOM;
        if (domain_str == StringView("CSS", 3)) return CDPDomain::CSS;
        if (domain_str == StringView("Page", 4)) return CDPDomain::Page;
        if (domain_str == StringView("Network", 7)) return CDPDomain::Network;
        if (domain_str == StringView("Debugger", 8)) return CDPDomain::Debugger;
        if (domain_str == StringView("Runtime", 7)) return CDPDomain::Runtime;
        if (domain_str == StringView("Profiler", 8)) return CDPDomain::Profiler;
        if (domain_str == StringView("Log", 3)) return CDPDomain::Log;
        
        return CDPDomain::Unknown;
    }
};

enum class CdpMessageType {
    Unknown,
    Request,
    Response,
    Notification
};

struct CdpMessage {
    CdpMessageType type;
    int id;
    StringView method;
    CDPDomain domain;
    JsonNode* params;
    JsonNode* result;
    JsonNode* error;
};

// Callback type for method handlers
typedef void (*CdpMethodHandler)(const CdpMessage& msg, void* user_data);

struct CdpHandlerEntry {
    StringView method;
    CDPDomain domain;
    CdpMethodHandler handler;
    void* user_data;
    CdpHandlerEntry* next;
};

class CdpBridge {
private:
    ArenaAllocator* arena;
    JsonParser* parser;
    CdpHandlerEntry* handlers;
    
    // Fallback/wildcard handler
    CdpMethodHandler default_handler;
    void* default_user_data;

public:
    CdpBridge(ArenaAllocator* alloc, JsonParser* p) 
        : arena(alloc), parser(p), handlers(nullptr), default_handler(nullptr), default_user_data(nullptr) {}

    void register_handler(const char* method_cstr, CdpMethodHandler handler, void* user_data = nullptr) {
        size_t len = 0;
        while(method_cstr[len]) len++;
        
        CdpHandlerEntry* entry = (CdpHandlerEntry*)arena->allocate(sizeof(CdpHandlerEntry));
        if (entry) {
            entry->method = {method_cstr, len};
            entry->handler = handler;
            entry->user_data = user_data;
            entry->next = handlers;
            handlers = entry;
        }
    }

    void set_default_handler(CdpMethodHandler handler, void* user_data = nullptr) {
        default_handler = handler;
        default_user_data = user_data;
    }

    CdpMessage parse_message(const char* json_str) {
        CdpMessage msg = {CdpMessageType::Unknown, 0, {nullptr, 0}, CDPDomain::Unknown, nullptr, nullptr, nullptr};
        
        JsonNode* root = parser->parse(json_str);
        if (!root || root->type != JsonType::Object) {
            return msg;
        }

        JsonNode* id_node = root->get("id");
        if (id_node && id_node->type == JsonType::Number) {
            msg.id = (int)id_node->number_val;
        }

        JsonNode* method_node = root->get("method");
        if (method_node && method_node->type == JsonType::String) {
            msg.method = method_node->string_val;
            msg.domain = CDPHelper::parse_domain(msg.method);
            msg.type = (msg.id > 0) ? CdpMessageType::Request : CdpMessageType::Notification;
            msg.params = root->get("params");
        } else if (msg.id > 0) {
            msg.type = CdpMessageType::Response;
            msg.result = root->get("result");
            msg.error = root->get("error");
        }

        return msg;
    }

    void process_message(const char* json_str) {
        CdpMessage msg = parse_message(json_str);
        if (msg.type == CdpMessageType::Unknown) return;

        if (msg.type == CdpMessageType::Notification || msg.type == CdpMessageType::Request) {
            CdpHandlerEntry* curr = handlers;
            bool handled = false;
            while (curr) {
                if (curr->method == msg.method) {
                    curr->handler(msg, curr->user_data);
                    handled = true;
                    break; 
                }
                curr = curr->next;
            }
            if (!handled && default_handler) {
                default_handler(msg, default_user_data);
            }
        } else if (msg.type == CdpMessageType::Response) {
            if (default_handler) {
                default_handler(msg, default_user_data);
            }
        }
    }
};

// --- WEBSOCKET FRAME ENCODER / DECODER FOR CDP REMOTE DEBUGGING ---
struct WebSocketFrame {
    bool fin;
    uint8_t opcode; // 0x1 = text (CDP JSON), 0x2 = binary, 0x8 = close, 0x9 = ping, 0x0A = pong
    bool masked;
    uint32_t masking_key;
    uint64_t payload_len;
    const char* payload_data;
};

class WebSocketCDPAgent {
private:
    ArenaAllocator* arena;
    CdpBridge* bridge;

public:
    WebSocketCDPAgent(ArenaAllocator* alloc, CdpBridge* b) : arena(alloc), bridge(b) {}

    // Decode RFC 6455 WebSocket frame from raw byte buffer
    static WebSocketFrame decode_frame(const uint8_t* raw_buf, size_t buf_len) {
        WebSocketFrame frame = {false, 0, false, 0, 0, nullptr};
        if (!raw_buf || buf_len < 2) return frame;

        frame.fin = (raw_buf[0] & 0x80) != 0;
        frame.opcode = raw_buf[0] & 0x0F;
        frame.masked = (raw_buf[1] & 0x80) != 0;

        size_t header_len = 2;
        uint8_t payload_byte = raw_buf[1] & 0x7F;

        if (payload_byte == 126) {
            if (buf_len < 4) return frame;
            frame.payload_len = ((uint64_t)raw_buf[2] << 8) | raw_buf[3];
            header_len += 2;
        } else if (payload_byte == 127) {
            if (buf_len < 10) return frame;
            frame.payload_len = 0;
            for (int i = 0; i < 8; i++) {
                frame.payload_len = (frame.payload_len << 8) | raw_buf[4 + i];
            }
            header_len += 8;
        } else {
            frame.payload_len = payload_byte;
        }

        uint8_t mask_key[4] = {0};
        if (frame.masked) {
            if (buf_len < header_len + 4) return frame;
            for (int i = 0; i < 4; i++) {
                mask_key[i] = raw_buf[header_len + i];
            }
            frame.masking_key = ((uint32_t)mask_key[0] << 24) | ((uint32_t)mask_key[1] << 16) | ((uint32_t)mask_key[2] << 8) | mask_key[3];
            header_len += 4;
        }

        if (buf_len < header_len + frame.payload_len) return frame;

        // Pointer to frame payload
        frame.payload_data = (const char*)(raw_buf + header_len);
        return frame;
    }

    // Process raw incoming WebSocket text frame containing CDP message
    void process_ws_frame(const uint8_t* raw_buf, size_t buf_len) {
        WebSocketFrame frame = decode_frame(raw_buf, buf_len);
        if (frame.opcode == 0x1 && frame.payload_data && frame.payload_len > 0) { // Text frame
            // Copy & unmask payload if masked
            char* text_buf = (char*)arena->allocate(frame.payload_len + 1);
            if (!text_buf) return;

            uint8_t mask_key[4] = {
                (uint8_t)((frame.masking_key >> 24) & 0xFF),
                (uint8_t)((frame.masking_key >> 16) & 0xFF),
                (uint8_t)((frame.masking_key >> 8) & 0xFF),
                (uint8_t)(frame.masking_key & 0xFF)
            };

            for (size_t i = 0; i < frame.payload_len; i++) {
                if (frame.masked) {
                    text_buf[i] = frame.payload_data[i] ^ mask_key[i % 4];
                } else {
                    text_buf[i] = frame.payload_data[i];
                }
            }
            text_buf[frame.payload_len] = '\0';

            // Route CDP message
            bridge->process_message(text_buf);
        }
    }

    // Encode outgoing text frame (CDP Response / Notification) to RFC 6455 WebSocket format
    static size_t encode_ws_text_frame(const char* text_payload, size_t payload_len, uint8_t* out_buf, size_t out_capacity) {
        if (!text_payload || !out_buf) return 0;

        size_t header_len = 2;
        if (payload_len > 125 && payload_len <= 65535) {
            header_len += 2;
        } else if (payload_len > 65535) {
            header_len += 8;
        }

        if (out_capacity < header_len + payload_len) return 0;

        out_buf[0] = 0x81; // FIN = 1, Opcode = 0x1 (Text)
        if (payload_len <= 125) {
            out_buf[1] = (uint8_t)payload_len; // Unmasked
        } else if (payload_len <= 65535) {
            out_buf[1] = 126;
            out_buf[2] = (uint8_t)((payload_len >> 8) & 0xFF);
            out_buf[3] = (uint8_t)(payload_len & 0xFF);
        } else {
            out_buf[1] = 127;
            for (int i = 0; i < 8; i++) {
                out_buf[2 + i] = (uint8_t)((payload_len >> ((7 - i) * 8)) & 0xFF);
            }
        }

        for (size_t i = 0; i < payload_len; i++) {
            out_buf[header_len + i] = (uint8_t)text_payload[i];
        }

        return header_len + payload_len;
    }
};
