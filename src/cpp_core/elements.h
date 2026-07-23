#pragma once
#include "core.h"
#include <stdint.h>

enum class NodeType {
    Element,
    Text,
    Comment
};

struct Attribute {
    StringView name;
    StringView value;
    Attribute* next;
};

struct DOMNode {
    NodeType type;
    StringView tag_name;
    StringView text_content;
    
    Attribute* first_attribute;
    DOMNode* first_child;
    DOMNode* next_sibling;
    DOMNode* parent;
    
    void add_child(DOMNode* child) {
        child->parent = this;
        if (!first_child) {
            first_child = child;
        } else {
            DOMNode* curr = first_child;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = child;
        }
    }
    
    void set_attribute(ArenaAllocator* arena, StringView name, StringView value) {
        Attribute* attr = (Attribute*)arena->allocate(sizeof(Attribute));
        attr->name = name;
        attr->value = value;
        attr->next = first_attribute;
        first_attribute = attr;
    }
    
    
    void remove_child(DOMNode* child) {
        if (!first_child) return;
        if (first_child == child) {
            first_child = child->next_sibling;
            child->parent = nullptr;
            child->next_sibling = nullptr;
            return;
        }
        DOMNode* curr = first_child;
        while (curr->next_sibling) {
            if (curr->next_sibling == child) {
                curr->next_sibling = child->next_sibling;
                child->parent = nullptr;
                child->next_sibling = nullptr;
                return;
            }
            curr = curr->next_sibling;
        }
    }

    void insert_before(DOMNode* new_child, DOMNode* ref_child) {
        if (!ref_child) {
            add_child(new_child);
            return;
        }
        new_child->parent = this;
        if (first_child == ref_child) {
            new_child->next_sibling = first_child;
            first_child = new_child;
            return;
        }
        DOMNode* curr = first_child;
        while (curr && curr->next_sibling) {
            if (curr->next_sibling == ref_child) {
                new_child->next_sibling = curr->next_sibling;
                curr->next_sibling = new_child;
                return;
            }
            curr = curr->next_sibling;
        }
    }

    void remove_attribute(StringView name) {
        if (!first_attribute) return;
        if (first_attribute->name == name) {
            first_attribute = first_attribute->next;
            return;
        }
        Attribute* curr = first_attribute;
        while (curr->next) {
            if (curr->next->name == name) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    StringView get_attribute(StringView name) {
        Attribute* curr = first_attribute;
        while (curr) {
            if (curr->name == name) return curr->value;
            curr = curr->next;
        }
        return {nullptr, 0};
    }
};

class ElementsTree {
    ArenaAllocator* arena;
public:
    DOMNode* root;
    
    ElementsTree(ArenaAllocator* a) : arena(a), root(nullptr) {}
    
    DOMNode* create_element(StringView tag) {
        DOMNode* node = (DOMNode*)arena->allocate(sizeof(DOMNode));
        node->type = NodeType::Element;
        node->tag_name = tag;
        node->text_content = {nullptr, 0};
        node->first_attribute = nullptr;
        node->first_child = nullptr;
        node->next_sibling = nullptr;
        node->parent = nullptr;
        return node;
    }

    DOMNode* create_text_node(StringView text) {
        DOMNode* node = (DOMNode*)arena->allocate(sizeof(DOMNode));
        node->type = NodeType::Text;
        node->tag_name = {nullptr, 0};
        node->text_content = text;
        node->first_attribute = nullptr;
        node->first_child = nullptr;
        node->next_sibling = nullptr;
        node->parent = nullptr;
        return node;
    }
    
    
    void get_elements_by_tag_name(DOMNode* node, StringView tag, DOMNode** results, size_t& count, size_t max_results) {
        if (!node || count >= max_results) return;
        
        if (node->type == NodeType::Element && node->tag_name == tag) {
            results[count++] = node;
        }
        
        DOMNode* curr = node->first_child;
        while (curr && count < max_results) {
            get_elements_by_tag_name(curr, tag, results, count, max_results);
            curr = curr->next_sibling;
        }
    }

    void get_elements_by_class_name(DOMNode* node, StringView class_name, DOMNode** results, size_t& count, size_t max_results) {
        if (!node || count >= max_results) return;
        
        if (node->type == NodeType::Element) {
            StringView node_class = node->get_attribute(StringView("class", 5));
            // Simple string matching, in reality should split by space
            if (node_class.length > 0) {
                // Check if class_name is in node_class (space separated)
                size_t start = 0;
                while (start < node_class.length) {
                    while (start < node_class.length && node_class.data[start] == ' ') start++;
                    size_t end = start;
                    while (end < node_class.length && node_class.data[end] != ' ') end++;
                    
                    if (end > start) {
                        StringView current_class(node_class.data + start, end - start);
                        if (current_class == class_name) {
                            results[count++] = node;
                            break;
                        }
                    }
                    start = end + 1;
                }
            }
        }
        
        DOMNode* curr = node->first_child;
        while (curr && count < max_results) {
            get_elements_by_class_name(curr, class_name, results, count, max_results);
            curr = curr->next_sibling;
        }
    }

    DOMNode* query_selector(DOMNode* node, StringView selector) {
        if (!node) return nullptr;
        
        bool match = false;
        if (selector.length > 0) {
            if (selector.data[0] == '.') {
                StringView class_name(selector.data + 1, selector.length - 1);
                match = (node->get_attribute(StringView("class", 5)) == class_name);
            } else if (selector.data[0] == '#') {
                StringView id_name(selector.data + 1, selector.length - 1);
                match = (node->get_attribute(StringView("id", 2)) == id_name);
            } else {
                match = (node->type == NodeType::Element && node->tag_name == selector);
            }
        }

        if (match) return node;
        
        DOMNode* curr = node->first_child;
        while (curr) {
            DOMNode* found = query_selector(curr, selector);
            if (found) return found;
            curr = curr->next_sibling;
        }
        return nullptr;
    }
};

class DOMParser {
    ArenaAllocator* arena;
    ElementsTree* tree;
public:
    DOMParser(ArenaAllocator* a, ElementsTree* t) : arena(a), tree(t) {}

    void parse(StringView html) {
        const char* p = html.data;
        const char* end = p + html.length;
        
        DOMNode* current_parent = nullptr;
        
        while (p < end) {
            // Text node check
            const char* text_start = p;
            while (p < end && *p != '<') p++;
            
            if (p > text_start && current_parent) {
                // Ignore purely whitespace text nodes for simplicity
                bool has_non_whitespace = false;
                for (const char* t = text_start; t < p; t++) {
                    if (*t != ' ' && *t != '\n' && *t != '\r' && *t != '\t') {
                        has_non_whitespace = true;
                        break;
                    }
                }
                if (has_non_whitespace) {
                    DOMNode* text_node = tree->create_text_node(StringView(text_start, p - text_start));
                    current_parent->add_child(text_node);
                }
            }
            
            if (p >= end) break;
            
            if (*p == '<') {
                p++;
                if (p < end && *p == '/') {
                    // Close tag
                    p++;
                    while (p < end && *p != '>') p++;
                    if (p < end && *p == '>') p++;
                    
                    if (current_parent) {
                        current_parent = current_parent->parent;
                    }
                } else {
                    // Open tag
                    const char* tag_start = p;
                    while (p < end && *p != ' ' && *p != '>') p++;
                    StringView tag_name(tag_start, p - tag_start);
                    
                    DOMNode* new_node = tree->create_element(tag_name);
                    
                    if (!tree->root) {
                        tree->root = new_node;
                    }
                    if (current_parent) {
                        current_parent->add_child(new_node);
                    }
                    
                    // Parse attributes
                    while (p < end && *p != '>') {
                        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
                        if (p >= end || *p == '>') break;
                        
                        const char* attr_start = p;
                        while (p < end && *p != '=' && *p != ' ' && *p != '>') p++;
                        StringView attr_name(attr_start, p - attr_start);
                        
                        StringView attr_value = {nullptr, 0};
                        
                        if (p < end && *p == '=') {
                            p++; // skip '='
                            if (p < end && (*p == '"' || *p == '\'')) {
                                char quote = *p;
                                p++;
                                const char* val_start = p;
                                while (p < end && *p != quote) p++;
                                attr_value = StringView(val_start, p - val_start);
                                if (p < end) p++; // skip quote
                            }
                        }
                        
                        new_node->set_attribute(arena, attr_name, attr_value);
                    }
                    
                    if (p < end && *p == '>') {
                        p++;
                    }
                    
                    // Some tags are self-closing, but we will assume well-formed HTML
                    // If it's a void element, we don't push it to current_parent
                    if (tag_name == StringView("br", 2) || tag_name == StringView("img", 3) || tag_name == StringView("meta", 4) || tag_name == StringView("link", 4)) {
                        // don't push
                    } else {
                        current_parent = new_node;
                    }
                }
            }
        }
    }
};
