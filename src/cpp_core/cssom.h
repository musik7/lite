#pragma once
#include "core.h"
#include "elements.h"

struct CSSProperty {
    StringView name;
    StringView value;
    CSSProperty* next;
};

struct CSSStyleDeclaration {
    CSSProperty* first_property;

    void set_property(ArenaAllocator* arena, StringView name, StringView value) {
        CSSProperty* prop = (CSSProperty*)arena->allocate(sizeof(CSSProperty));
        prop->name = name;
        prop->value = value;
        prop->next = first_property;
        first_property = prop;
    }

    
    void remove_property(StringView name) {
        if (!first_property) return;
        if (first_property->name == name) {
            first_property = first_property->next;
            return;
        }
        CSSProperty* curr = first_property;
        while (curr->next) {
            if (curr->next->name == name) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    StringView get_property(StringView name) {
        CSSProperty* curr = first_property;
        while (curr) {
            if (curr->name == name) return curr->value;
            curr = curr->next;
        }
        return {nullptr, 0};
    }
};

struct CSSStyleRule {
    StringView selector;
    int specificity;
    int index; // document order
    CSSStyleDeclaration style;
    CSSStyleRule* next;
};

class StyleSheet {
    ArenaAllocator* arena;
    int rule_count;
public:
    CSSStyleRule* first_rule;

    StyleSheet(ArenaAllocator* a) : arena(a), rule_count(0), first_rule(nullptr) {}

    int calculate_specificity(StringView selector) {
        if (selector.length == 0) return 0;
        if (selector.data[0] == '#') return 100;
        if (selector.data[0] == '.') return 10;
        return 1;
    }

    
    void insert_rule(ArenaAllocator* arena_arg, StringView selector, CSSStyleDeclaration style) {
        CSSStyleRule* rule = (CSSStyleRule*)arena_arg->allocate(sizeof(CSSStyleRule));
        rule->selector = copy_string(arena_arg, selector);
        rule->specificity = calculate_specificity(rule->selector);
        rule->index = rule_count++;
        rule->style = style;
        
        // Insert at end (highest document order index, but our list logic usually pushes to head. 
        // Let's just push to tail so it has higher document order index but keeps the list ordered?
        // Actually, our engine sorts by index anyway.
        rule->next = first_rule;
        first_rule = rule;
    }

    void delete_rule(StringView selector) {
        if (!first_rule) return;
        if (first_rule->selector == selector) {
            first_rule = first_rule->next;
            return;
        }
        CSSStyleRule* curr = first_rule;
        while (curr->next) {
            if (curr->next->selector == selector) {
                curr->next = curr->next->next;
                return;
            }
            curr = curr->next;
        }
    }

    void parse(StringView css) {
        const char* p = css.data;
        const char* end = p + css.length;

        auto skip_whitespace = [&]() {
            while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) p++;
        };

        while (p < end) {
            skip_whitespace();
            if (p >= end) break;

            const char* sel_start = p;
            while (p < end && *p != '{') p++;
            if (p >= end) break;
            
            StringView selector(sel_start, p - sel_start);
            while (selector.length > 0 && (selector.data[selector.length - 1] == ' ' || selector.data[selector.length - 1] == '\n' || selector.data[selector.length - 1] == '\t' || selector.data[selector.length - 1] == '\r')) {
                selector.length--;
            }

            CSSStyleRule* rule = (CSSStyleRule*)arena->allocate(sizeof(CSSStyleRule));
            rule->selector = selector;
            rule->specificity = calculate_specificity(selector);
            rule->index = rule_count++;
            rule->style.first_property = nullptr;
            
            // Insert at head
            rule->next = first_rule;
            first_rule = rule;

            p++; // skip '{'

            while (p < end && *p != '}') {
                skip_whitespace();
                if (p >= end || *p == '}') break;

                const char* prop_start = p;
                while (p < end && *p != ':' && *p != '}') p++;
                if (p >= end || *p == '}') break;

                StringView prop_name(prop_start, p - prop_start);
                while (prop_name.length > 0 && (prop_name.data[prop_name.length - 1] == ' ' || prop_name.data[prop_name.length - 1] == '\n' || prop_name.data[prop_name.length - 1] == '\r')) prop_name.length--;

                p++; // skip ':'
                skip_whitespace();

                const char* val_start = p;
                while (p < end && *p != ';' && *p != '}') p++;
                
                StringView prop_val(val_start, p - val_start);
                while (prop_val.length > 0 && (prop_val.data[prop_val.length - 1] == ' ' || prop_val.data[prop_val.length - 1] == '\n' || prop_val.data[prop_val.length - 1] == '\r')) prop_val.length--;

                rule->style.set_property(arena, prop_name, prop_val);

                if (p < end && *p == ';') p++;
            }
            if (p < end && *p == '}') p++;
        }
    }
};

class StyleEngine {
    ArenaAllocator* arena;
    StyleSheet* stylesheet;
public:
    StyleEngine(ArenaAllocator* a, StyleSheet* ss) : arena(a), stylesheet(ss) {}

    bool matches(StringView selector, DOMNode* node) {
        if (selector.length == 0) return false;

        if (selector.data[0] == '.') { 
            StringView class_name(selector.data + 1, selector.length - 1);
            StringView node_class = node->get_attribute(StringView("class", 5));
            return node_class == class_name;
        } else if (selector.data[0] == '#') { 
            StringView id_name(selector.data + 1, selector.length - 1);
            StringView node_id = node->get_attribute(StringView("id", 2));
            return node_id == id_name;
        } else { 
            return node->tag_name == selector;
        }
    }

    void parse_inline_style(ArenaAllocator* arena, StringView inline_css, CSSStyleDeclaration& decl) {
        if (inline_css.length == 0) return;
        
        size_t p = 0;
        while (p < inline_css.length) {
            // Skip whitespace
            while (p < inline_css.length && (inline_css.data[p] == ' ' || inline_css.data[p] == '\n' || inline_css.data[p] == '\t')) p++;
            if (p >= inline_css.length) break;
            
            size_t name_start = p;
            while (p < inline_css.length && inline_css.data[p] != ':' && !(inline_css.data[p] == ' ' || inline_css.data[p] == '\n' || inline_css.data[p] == '\t')) p++;
            size_t name_end = p;
            
            while (p < inline_css.length && inline_css.data[p] != ':') p++;
            if (p < inline_css.length) p++; // Skip ':'
            
            while (p < inline_css.length && (inline_css.data[p] == ' ' || inline_css.data[p] == '\n' || inline_css.data[p] == '\t')) p++;
            size_t val_start = p;
            
            while (p < inline_css.length && inline_css.data[p] != ';') p++;
            size_t val_end = p;
            while (val_end > val_start && (inline_css.data[val_end - 1] == ' ' || inline_css.data[val_end - 1] == '\n' || inline_css.data[val_end - 1] == '\t')) val_end--;
            
            if (name_end > name_start && val_end > val_start) {
                decl.set_property(arena, 
                    copy_string(arena, StringView(inline_css.data + name_start, name_end - name_start)),
                    copy_string(arena, StringView(inline_css.data + val_start, val_end - val_start))
                );
            }
            if (p < inline_css.length) p++; // Skip ';'
        }
    }

    CSSStyleDeclaration resolve_style(DOMNode* node) {
        CSSStyleDeclaration final_style;
        final_style.first_property = nullptr;
        
        if (!node || node->type != NodeType::Element) return final_style;
        
        // 1. Collect matched rules
        CSSStyleRule* matched_rules[128];
        int match_count = 0;
        
        CSSStyleRule* curr = stylesheet->first_rule;
        while (curr && match_count < 128) {
            if (matches(curr->selector, node)) {
                matched_rules[match_count++] = curr;
            }
            curr = curr->next;
        }
        
        // 2. Sort rules by specificity (ascending) then index (ascending)
        for (int i = 0; i < match_count - 1; i++) {
            for (int j = 0; j < match_count - i - 1; j++) {
                bool swap = false;
                if (matched_rules[j]->specificity > matched_rules[j+1]->specificity) {
                    swap = true;
                } else if (matched_rules[j]->specificity == matched_rules[j+1]->specificity) {
                    if (matched_rules[j]->index > matched_rules[j+1]->index) {
                        swap = true;
                    }
                }
                if (swap) {
                    CSSStyleRule* temp = matched_rules[j];
                    matched_rules[j] = matched_rules[j+1];
                    matched_rules[j+1] = temp;
                }
            }
        }
        
        // 3. Apply properties in sorted order (so higher precedence overwrites)
        for (int i = 0; i < match_count; i++) {
            CSSProperty* prop = matched_rules[i]->style.first_property;
            while (prop) {
                final_style.set_property(arena, prop->name, prop->value);
                prop = prop->next;
            }
        }
        
        // 4. Inline styles (highest precedence)
        StringView inline_style = node->get_attribute(StringView("style", 5));
        if (inline_style.length > 0) {
            parse_inline_style(arena, inline_style, final_style);
        }
        
        return final_style;
    }
};
