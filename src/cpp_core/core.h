#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// ============ STRING VIEW ============
struct StringView {
    const char* data;
    size_t length;

    StringView() : data(nullptr), length(0) {}
    StringView(const char* str) {
        data = str;
        length = 0;
        if (str) {
            while (str[length]) length++;
        }
    }
    StringView(const char* str, size_t len) : data(str), length(len) {}

    bool equals(const StringView& other) const {
        if (length != other.length) return false;
        for (size_t i = 0; i < length; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
    
    bool equals(const char* str) const {
        size_t i = 0;
        for (; i < length && str[i]; ++i) {
            if (data[i] != str[i]) return false;
        }
        return i == length && str[i] == '\0';
    }

    bool operator==(const StringView& other) const {
        return equals(other);
    }
};

// ============ ARENA ALLOCATOR ============
class ArenaAllocator {
private:
    struct Chunk {
        char* buffer;
        size_t capacity;
        size_t offset;
        Chunk* next;
        bool owned;
    };
    
    Chunk* head;
    Chunk* current;
    size_t default_chunk_size;

    Chunk* allocate_chunk(size_t cap) {
        Chunk* c = (Chunk*)malloc(sizeof(Chunk));
        if (!c) return nullptr;
        c->buffer = (char*)malloc(cap);
        c->capacity = cap;
        c->offset = 0;
        c->next = nullptr;
        c->owned = true;
        return c;
    }

public:
    ArenaAllocator(char* buf, size_t cap) {
        default_chunk_size = cap > 0 ? cap : 1024 * 1024;
        head = (Chunk*)malloc(sizeof(Chunk));
        head->buffer = buf;
        head->capacity = cap;
        head->offset = 0;
        head->next = nullptr;
        head->owned = false;
        current = head;
    }

    ArenaAllocator(size_t chunk_size = 1024 * 1024) {
        default_chunk_size = chunk_size > 0 ? chunk_size : 1024 * 1024;
        head = allocate_chunk(default_chunk_size);
        current = head;
    }

    ~ArenaAllocator() {
        Chunk* c = head;
        while (c) {
            Chunk* next = c->next;
            if (c->owned && c->buffer) {
                free(c->buffer);
            }
            free(c);
            c = next;
        }
    }

    void* allocate(size_t size, size_t alignment = 8) {
        size_t padding = (alignment - (current->offset % alignment)) % alignment;
        
        if (current->offset + padding + size > current->capacity) {
            size_t new_cap = default_chunk_size;
            if (size + alignment > new_cap) {
                new_cap = size + alignment;
            }
            Chunk* new_chunk = allocate_chunk(new_cap);
            if (!new_chunk) return nullptr;
            current->next = new_chunk;
            current = new_chunk;
            padding = (alignment - (current->offset % alignment)) % alignment;
        }
        
        current->offset += padding;
        void* ptr = current->buffer + current->offset;
        current->offset += size;
        return ptr;
    }

    void reset() {
        Chunk* c = head;
        while (c) {
            c->offset = 0;
            c = c->next;
        }
        current = head;
    }
        
    size_t get_used() const {
        size_t total = 0;
        Chunk* c = head;
        while (c) {
            total += c->offset;
            c = c->next;
        }
        return total;
    }
        
    size_t get_capacity() const {
        size_t total = 0;
        Chunk* c = head;
        while (c) {
            total += c->capacity;
            c = c->next;
        }
        return total;
    }
};

// ============ UTILITIES ============
inline char* copy_string(ArenaAllocator* arena, const char* str) {
    if (!str) return nullptr;
    size_t len = 0;
    while (str[len]) len++;
    char* copy = (char*)arena->allocate(len + 1);
    if (copy) {
        for (size_t i = 0; i < len; ++i) copy[i] = str[i];
        copy[len] = '\0';
    }
    return copy;
}

inline char* copy_string(ArenaAllocator* arena, StringView sv) {
    if (!sv.data || sv.length == 0) return nullptr;
    char* copy = (char*)arena->allocate(sv.length + 1);
    if (copy) {
        for (size_t i = 0; i < sv.length; ++i) copy[i] = sv.data[i];
        copy[sv.length] = '\0';
    }
    return copy;
}
