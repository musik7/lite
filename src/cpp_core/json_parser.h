#pragma once
#include "core.h"
#include <stdint.h>

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonNode;

struct JsonProperty {
    StringView key;
    JsonNode* value;
    JsonProperty* next;
};

struct JsonNode {
    JsonType type;
    union {
        bool bool_val;
        double number_val;
        StringView string_val;
        struct {
            JsonNode* head;
            JsonNode* tail;
            size_t count;
        } array_val;
        struct {
            JsonProperty* head;
            JsonProperty* tail;
            size_t count;
        } object_val;
    };
    JsonNode* next; // For array linked list

    JsonNode* get(const char* key) const {
        if (type != JsonType::Object) return nullptr;
        size_t key_len = 0;
        while (key[key_len]) key_len++;

        JsonProperty* prop = object_val.head;
        while (prop) {
            if (prop->key.length == key_len) {
                bool match = true;
                for (size_t i = 0; i < key_len; ++i) {
                    if (prop->key.data[i] != key[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) return prop->value;
            }
            prop = prop->next;
        }
        return nullptr;
    }
};

class JsonParser {
private:
    ArenaAllocator* arena;
    const char* p;
    const char* end;

    void skip_whitespace() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
            p++;
        }
    }

    bool match(const char* str, size_t len) {
        if (end - p >= len) {
            for (size_t i = 0; i < len; ++i) {
                if (p[i] != str[i]) return false;
            }
            p += len;
            return true;
        }
        return false;
    }

    StringView parse_string() {
        if (p >= end || *p != '"') return {nullptr, 0};
        p++; // skip "
        const char* start = p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                p++; // skip \
                if (p < end) p++;
            } else {
                p++;
            }
        }
        StringView result = {start, (size_t)(p - start)};
        if (p < end && *p == '"') p++;
        return result;
    }

    double parse_number() {
        double result = 0;
        double sign = 1.0;
        if (p < end && *p == '-') {
            sign = -1.0;
            p++;
        }
        while (p < end && *p >= '0' && *p <= '9') {
            result = result * 10 + (*p - '0');
            p++;
        }
        if (p < end && *p == '.') {
            p++;
            double frac = 0.1;
            while (p < end && *p >= '0' && *p <= '9') {
                result += (*p - '0') * frac;
                frac *= 0.1;
                p++;
            }
        }
        // Basic exponent handling
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            double exp_sign = 1.0;
            if (p < end && *p == '-') {
                exp_sign = -1.0;
                p++;
            } else if (p < end && *p == '+') {
                p++;
            }
            double exp = 0;
            while (p < end && *p >= '0' && *p <= '9') {
                exp = exp * 10 + (*p - '0');
                p++;
            }
            double m = 1.0;
            for (int i = 0; i < exp; ++i) m *= 10.0;
            if (exp_sign < 0) result /= m;
            else result *= m;
        }
        return result * sign;
    }

    JsonNode* parse_value() {
        skip_whitespace();
        if (p >= end) return nullptr;

        JsonNode* node = (JsonNode*)arena->allocate(sizeof(JsonNode));
        if (!node) return nullptr;
        node->next = nullptr;

        if (*p == '{') {
            node->type = JsonType::Object;
            node->object_val.head = nullptr;
            node->object_val.tail = nullptr;
            node->object_val.count = 0;
            p++;
            skip_whitespace();
            while (p < end && *p != '}') {
                skip_whitespace();
                StringView key = parse_string();
                skip_whitespace();
                if (p < end && *p == ':') p++;
                skip_whitespace();
                JsonNode* val = parse_value();

                if (val) {
                    JsonProperty* prop = (JsonProperty*)arena->allocate(sizeof(JsonProperty));
                    if (prop) {
                        prop->key = key;
                        prop->value = val;
                        prop->next = nullptr;
                        if (!node->object_val.head) {
                            node->object_val.head = prop;
                            node->object_val.tail = prop;
                        } else {
                            node->object_val.tail->next = prop;
                            node->object_val.tail = prop;
                        }
                        node->object_val.count++;
                    }
                }

                skip_whitespace();
                if (p < end && *p == ',') p++;
                skip_whitespace();
            }
            if (p < end && *p == '}') p++;
        } else if (*p == '[') {
            node->type = JsonType::Array;
            node->array_val.head = nullptr;
            node->array_val.tail = nullptr;
            node->array_val.count = 0;
            p++;
            skip_whitespace();
            while (p < end && *p != ']') {
                JsonNode* elem = parse_value();
                if (elem) {
                    if (!node->array_val.head) {
                        node->array_val.head = elem;
                        node->array_val.tail = elem;
                    } else {
                        node->array_val.tail->next = elem;
                        node->array_val.tail = elem;
                    }
                    node->array_val.count++;
                }
                skip_whitespace();
                if (p < end && *p == ',') p++;
                skip_whitespace();
            }
            if (p < end && *p == ']') p++;
        } else if (*p == '"') {
            node->type = JsonType::String;
            node->string_val = parse_string();
        } else if (*p == 't') {
            node->type = JsonType::Bool;
            node->bool_val = true;
            match("true", 4);
        } else if (*p == 'f') {
            node->type = JsonType::Bool;
            node->bool_val = false;
            match("false", 5);
        } else if (*p == 'n') {
            node->type = JsonType::Null;
            match("null", 4);
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            node->type = JsonType::Number;
            node->number_val = parse_number();
        } else {
            return nullptr;
        }

        return node;
    }

public:
    JsonParser(ArenaAllocator* alloc) : arena(alloc) {}

    JsonNode* parse(StringView json) {
        p = json.data;
        end = json.data + json.length;
        return parse_value();
    }
    
    JsonNode* parse(const char* json_cstr) {
        size_t len = 0;
        while(json_cstr[len]) len++;
        StringView json = {json_cstr, len};
        return parse(json);
    }
};
