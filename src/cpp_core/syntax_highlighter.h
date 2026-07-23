#pragma once
#include "core.h"

enum class TokenType {
    Keyword,
    Identifier,
    Number,
    String,
    Punctuation,
    Whitespace,
    Unknown
};

struct Token {
    TokenType type;
    StringView text;
    Token* next;
};

class SyntaxHighlighter {
    ArenaAllocator* arena;
public:
    SyntaxHighlighter(ArenaAllocator* a) : arena(a) {}

    Token* tokenize(StringView code) {
        Token* first = nullptr;
        Token* last = nullptr;
        const char* p = code.data;
        const char* end = p + code.length;

        while (p < end) {
            Token* t = (Token*)arena->allocate(sizeof(Token));
            t->next = nullptr;
            const char* start = p;
            
            if (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') {
                while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) p++;
                t->type = TokenType::Whitespace;
            } else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
                while (p < end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) p++;
                StringView word(start, p - start);
                if (word == StringView("function", 8) || word == StringView("const", 5) || word == StringView("let", 3) || word == StringView("var", 3)) {
                    t->type = TokenType::Keyword;
                } else {
                    t->type = TokenType::Identifier;
                }
            } else if (*p >= '0' && *p <= '9') {
                while (p < end && *p >= '0' && *p <= '9') p++;
                t->type = TokenType::Number;
            } else if (*p == '"' || *p == '\'') {
                char quote = *p++;
                while (p < end && *p != quote) p++;
                if (p < end) p++;
                t->type = TokenType::String;
            } else {
                p++;
                t->type = TokenType::Punctuation;
            }
            
            t->text = StringView(start, p - start);
            if (!first) first = last = t;
            else { last->next = t; last = t; }
        }
        return first;
    }
};
