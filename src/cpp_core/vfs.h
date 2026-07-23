#pragma once
#include "core.h"

enum class FileType {
    File,
    Directory,
    Symlink
};

struct VFSNode {
    StringView name;
    FileType type;
    StringView content; // For files
    StringView mime_type;
    size_t size;
    double last_modified;
    
    VFSNode* parent;
    VFSNode* first_child;
    VFSNode* next_sibling;

    void add_child(VFSNode* child) {
        child->parent = this;
        child->next_sibling = first_child;
        first_child = child;
    }

    void remove_child(VFSNode* child) {
        if (!first_child || !child) return;
        if (first_child == child) {
            first_child = child->next_sibling;
            child->next_sibling = nullptr;
            child->parent = nullptr;
            return;
        }
        VFSNode* curr = first_child;
        while (curr->next_sibling) {
            if (curr->next_sibling == child) {
                curr->next_sibling = child->next_sibling;
                child->next_sibling = nullptr;
                child->parent = nullptr;
                return;
            }
            curr = curr->next_sibling;
        }
    }
};

class VirtualFileSystem {
    ArenaAllocator* arena;
    VFSNode* root;

    VFSNode* find_node(VFSNode* start, StringView path_segment) {
        VFSNode* curr = start->first_child;
        while (curr) {
            if (curr->name == path_segment) return curr;
            curr = curr->next_sibling;
        }
        return nullptr;
    }

public:
    VirtualFileSystem(ArenaAllocator* a) : arena(a) {
        root = (VFSNode*)arena->allocate(sizeof(VFSNode));
        root->name = StringView("/", 1);
        root->type = FileType::Directory;
        root->content = StringView();
        root->mime_type = StringView();
        root->size = 0;
        root->last_modified = 0;
        root->parent = nullptr;
        root->first_child = nullptr;
        root->next_sibling = nullptr;
    }

    VFSNode* get_root() const { return root; }

    VFSNode* resolve_path(StringView path, bool create_dirs = false) {
        if (path.length == 0 || path.data[0] != '/') return nullptr;

        VFSNode* curr = root;
        size_t start = 1;
        
        while (start < path.length) {
            size_t end = start;
            while (end < path.length && path.data[end] != '/') end++;
            
            if (end > start) {
                StringView segment(path.data + start, end - start);
                VFSNode* next = find_node(curr, segment);
                
                if (!next) {
                    if (create_dirs) {
                        next = (VFSNode*)arena->allocate(sizeof(VFSNode));
                        next->name = copy_string(arena, segment);
                        next->type = (end < path.length) ? FileType::Directory : FileType::File;
                        next->content = StringView();
                        next->mime_type = StringView();
                        next->size = 0;
                        next->last_modified = 0;
                        next->first_child = nullptr;
                        curr->add_child(next);
                    } else {
                        return nullptr;
                    }
                }
                curr = next;
            }
            start = end + 1;
        }
        return curr;
    }

    VFSNode* write_file(StringView path, StringView content, StringView mime_type = StringView(), double timestamp = 0) {
        VFSNode* node = resolve_path(path, true);
        if (node && node->type == FileType::File) {
            node->content = copy_string(arena, content);
            node->size = content.length;
            node->mime_type = copy_string(arena, mime_type);
            node->last_modified = timestamp;
            return node;
        }
        return nullptr;
    }

    StringView read_file(StringView path) {
        VFSNode* node = resolve_path(path);
        if (node && node->type == FileType::File) {
            return node->content;
        }
        return StringView();
    }

    VFSNode* create_directory(StringView path) {
        if (path.length == 0 || path.data[0] != '/') return nullptr;
        VFSNode* curr = root;
        size_t start = 1;

        while (start < path.length) {
            size_t end = start;
            while (end < path.length && path.data[end] != '/') end++;

            if (end > start) {
                StringView segment(path.data + start, end - start);
                VFSNode* next = find_node(curr, segment);

                if (!next) {
                    next = (VFSNode*)arena->allocate(sizeof(VFSNode));
                    next->name = copy_string(arena, segment);
                    next->type = FileType::Directory;
                    next->content = StringView();
                    next->mime_type = StringView();
                    next->size = 0;
                    next->last_modified = 0;
                    next->first_child = nullptr;
                    curr->add_child(next);
                }
                curr = next;
            }
            start = end + 1;
        }
        return curr;
    }

    bool delete_node(StringView path) {
        VFSNode* node = resolve_path(path);
        if (!node || node == root || !node->parent) return false;
        node->parent->remove_child(node);
        return true;
    }

    bool move_node(StringView src_path, StringView dest_path) {
        VFSNode* src_node = resolve_path(src_path);
        if (!src_node || src_node == root || !src_node->parent) return false;

        size_t last_slash = 0;
        for (size_t i = 0; i < dest_path.length; i++) {
            if (dest_path.data[i] == '/') last_slash = i;
        }

        StringView dest_dir_path = (last_slash == 0) ? StringView("/", 1) : StringView(dest_path.data, last_slash);
        StringView new_filename(dest_path.data + last_slash + 1, dest_path.length - last_slash - 1);

        VFSNode* target_dir = resolve_path(dest_dir_path, true);
        if (!target_dir || target_dir->type != FileType::Directory) return false;

        src_node->parent->remove_child(src_node);

        if (new_filename.length > 0) {
            src_node->name = copy_string(arena, new_filename);
        }

        target_dir->add_child(src_node);
        return true;
    }
    
    // Quick search for file names
    void search_files(VFSNode* dir, StringView query, VFSNode** results, size_t& count, size_t max_results) {
        if (!dir || count >= max_results) return;
        
        VFSNode* curr = dir->first_child;
        while (curr) {
            if (curr->type == FileType::File) {
                // Simple substring search (case sensitive for now)
                bool match = false;
                if (query.length == 0) match = true;
                else {
                    for (size_t i = 0; i <= curr->name.length - query.length; i++) {
                        bool found = true;
                        for (size_t j = 0; j < query.length; j++) {
                            if (curr->name.data[i+j] != query.data[j]) {
                                found = false;
                                break;
                            }
                        }
                        if (found) {
                            match = true;
                            break;
                        }
                    }
                }
                
                if (match && count < max_results) {
                    results[count++] = curr;
                }
            } else if (curr->type == FileType::Directory) {
                search_files(curr, query, results, count, max_results);
            }
            curr = curr->next_sibling;
        }
    }
};
