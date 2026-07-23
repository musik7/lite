#pragma once
#include "core.h"

enum class DrawCmdType {
    Rect,
    Text
};

struct DrawCommand {
    DrawCmdType type;
    int x, y, w, h;
    StringView color;
    StringView text; // For Text commands
    DrawCommand* next;
};

class CanvasRenderer {
    ArenaAllocator* arena;
    DrawCommand* first_cmd;
    DrawCommand* last_cmd;
public:
    CanvasRenderer(ArenaAllocator* a) : arena(a), first_cmd(nullptr), last_cmd(nullptr) {}

    void clear() {
        first_cmd = nullptr;
        last_cmd = nullptr;
    }

    void draw_rect(int x, int y, int w, int h, StringView color) {
        DrawCommand* cmd = (DrawCommand*)arena->allocate(sizeof(DrawCommand));
        cmd->type = DrawCmdType::Rect;
        cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
        cmd->color = color;
        cmd->next = nullptr;
        if (!first_cmd) first_cmd = last_cmd = cmd;
        else { last_cmd->next = cmd; last_cmd = cmd; }
    }

    void draw_text(int x, int y, StringView text, StringView color) {
        DrawCommand* cmd = (DrawCommand*)arena->allocate(sizeof(DrawCommand));
        cmd->type = DrawCmdType::Text;
        cmd->x = x; cmd->y = y; cmd->text = text;
        cmd->color = color;
        cmd->next = nullptr;
        if (!first_cmd) first_cmd = last_cmd = cmd;
        else { last_cmd->next = cmd; last_cmd = cmd; }
    }
    
    DrawCommand* get_commands() { return first_cmd; }
};
