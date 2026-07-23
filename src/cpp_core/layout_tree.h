#pragma once
#include "core.h"
#include "elements.h"
#include "cssom.h"

enum class DisplayType {
    None,
    Block,
    Inline,
    Flex
};

struct LayoutBox {
    double x, y, width, height;
    double margin_top, margin_right, margin_bottom, margin_left;
    double padding_top, padding_right, padding_bottom, padding_left;
};

struct LayoutNode {
    DOMNode* dom_node;
    DisplayType display;
    LayoutBox box;
    
    StringView bg_color;
    StringView color;
    
    LayoutNode* first_child;
    LayoutNode* next_sibling;
    LayoutNode* parent;

    void add_child(LayoutNode* child) {
        child->parent = this;
        if (!first_child) {
            first_child = child;
        } else {
            LayoutNode* curr = first_child;
            while (curr->next_sibling) {
                curr = curr->next_sibling;
            }
            curr->next_sibling = child;
        }
    }
    
    LayoutNode* hit_test(double px, double py) {
        LayoutNode* curr = first_child;
        while (curr) {
            LayoutNode* hit = curr->hit_test(px, py);
            if (hit) return hit;
            curr = curr->next_sibling;
        }
        if (px >= box.x && px <= box.x + box.width && 
            py >= box.y && py <= box.y + box.height) {
            return this;
        }
        return nullptr;
    }
};

class LayoutEngine {
    ArenaAllocator* arena;
    StyleEngine* style_engine;
    
    double parse_px(StringView val) {
        if (val.length == 0) return 0;
        double v = 0;
        for (size_t i = 0; i < val.length; i++) {
            if (val.data[i] >= '0' && val.data[i] <= '9') {
                v = v * 10 + (val.data[i] - '0');
            } else {
                break;
            }
        }
        return v;
    }

public:
    LayoutEngine(ArenaAllocator* a, StyleEngine* se) : arena(a), style_engine(se) {}

    LayoutNode* build_tree(DOMNode* dom) {
        if (!dom) return nullptr;

        CSSStyleDeclaration style = style_engine->resolve_style(dom);
        
        StringView display_val = style.get_property(StringView("display", 7));
        if (display_val == StringView("none", 4)) return nullptr;

        LayoutNode* layout = (LayoutNode*)arena->allocate(sizeof(LayoutNode));
        layout->dom_node = dom;
        layout->first_child = nullptr;
        layout->next_sibling = nullptr;
        layout->parent = nullptr;
        
        layout->box = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        layout->display = DisplayType::Block; 
        if (dom->type == NodeType::Text || display_val == StringView("inline", 6)) {
            layout->display = DisplayType::Inline;
        } else if (display_val == StringView("flex", 4)) {
            layout->display = DisplayType::Flex;
        }

        layout->box.width = parse_px(style.get_property(StringView("width", 5)));
        layout->box.height = parse_px(style.get_property(StringView("height", 6)));
        
        layout->box.margin_top = parse_px(style.get_property(StringView("margin-top", 10)));
        layout->box.margin_bottom = parse_px(style.get_property(StringView("margin-bottom", 13)));
        layout->box.margin_left = parse_px(style.get_property(StringView("margin-left", 11)));
        layout->box.margin_right = parse_px(style.get_property(StringView("margin-right", 12)));
        
        StringView margin_all = style.get_property(StringView("margin", 6));
        if (margin_all.length > 0) {
            layout->box.margin_top = layout->box.margin_bottom = layout->box.margin_left = layout->box.margin_right = parse_px(margin_all);
        }

        layout->bg_color = style.get_property(StringView("background-color", 16));
        layout->color = style.get_property(StringView("color", 5));

        DOMNode* child = dom->first_child;
        while (child) {
            LayoutNode* child_layout = build_tree(child);
            if (child_layout) {
                layout->add_child(child_layout);
            }
            child = child->next_sibling;
        }

        return layout;
    }

    void layout(LayoutNode* root, double containing_width, double start_x, double start_y) {
        if (!root) return;
        
        root->box.x = start_x + root->box.margin_left;
        root->box.y = start_y + root->box.margin_top;
        
        if (root->box.width == 0 && root->display == DisplayType::Block) {
            root->box.width = containing_width - root->box.margin_left - root->box.margin_right;
        }
        if (root->dom_node && root->dom_node->type == NodeType::Text) {
             root->box.width = root->dom_node->text_content.length * 8.0; 
             root->box.height = 16.0; 
        }

        double current_y = root->box.y;
        double current_x = root->box.x;

        LayoutNode* child = root->first_child;
        while (child) {
            if (child->display == DisplayType::Block) {
                layout(child, root->box.width, root->box.x, current_y);
                current_y += child->box.height + child->box.margin_top + child->box.margin_bottom;
            } else if (child->display == DisplayType::Inline) {
                layout(child, root->box.width, current_x, current_y);
                current_x += child->box.width + child->box.margin_left + child->box.margin_right;
            }
            child = child->next_sibling;
        }

        if (root->box.height == 0) {
            if (root->first_child && root->first_child->display == DisplayType::Inline) {
                 root->box.height = 16.0; 
            } else {
                 root->box.height = current_y - root->box.y;
            }
        }
    }
};
