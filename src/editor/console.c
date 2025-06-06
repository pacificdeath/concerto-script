#include <string.h>

#include "../main.h"
#include "editor_utils.c"

void console_set_text(State *state, const char *text) {
    Editor *e = &state->editor;
    for (int i = 0; i < CONSOLE_LINE_CAPACITY; i++) {
        e->console_text[i][0] = '\0';
    }
    int current_char = 0;
    int current_line = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        switch (text[i]) {
        case '\0':
        case '\n':
            ASSERT(current_line < CONSOLE_LINE_CAPACITY);
            e->console_text[current_line][current_char] = '\0';
            current_line++;
            current_char = 0;
            break;
        default:
            ASSERT(current_char < CONSOLE_LINE_MAX_LENGTH);
            e->console_text[current_line][current_char] = text[i];
            current_char++;
            break;
        }
    }
    e->console_text[current_line][current_char] = '\0';
    e->console_line_count = current_line + 1;
}

void console_get_highlighted_text(State *state, char *buffer) {
    Editor *e = &state->editor;
    int char_idx = 0;
    while (e->console_text[e->console_highlight_idx][char_idx] != '\n' && e->console_text[e->console_highlight_idx][char_idx] != '\0') {
        buffer[char_idx] = e->console_text[e->console_highlight_idx][char_idx];
        char_idx++;
    }
    buffer[char_idx] = '\0';
}

void console_render(State *state) {
    Editor *e = &state->editor;

    const int window_width = GetScreenWidth();
    const int window_height = GetScreenHeight();

    Rectangle rec;

    float line_height = editor_line_height(state);
    float char_width = editor_char_width(line_height);

    float rec_line_size = 5;
    float padding = rec_line_size * 2.0f;

    Color fg_color;

    switch (state->state) {
    default:
    case STATE_EDITOR_FILE_EXPLORER_PROGRAMS: {
        rec.x = (window_width * 0.1f) - padding;
        rec.y = (window_height * 0.1f) - padding;
        rec.width = (window_width * 0.8f) + (padding * 2.0f);
        fg_color = e->theme.console_foreground;
    } break;
    case STATE_EDITOR_THEME_ERROR:
    case STATE_EDITOR_SAVE_FILE_ERROR:
    case STATE_COMPILATION_ERROR: {
        rec.x = (window_width * 0.25f) - padding;
        rec.y = (window_height * 0.25f) - padding;
        rec.width = (window_width * 0.5f) + (padding * 2.0f);
        fg_color = e->theme.console_foreground_error;
    } break;
    case STATE_EDITOR_FIND_TEXT: {
        int width = char_width * (EDITOR_FINDER_BUFFER_MAX + 1);
        rec.x = (window_width * 0.5f) - (width * 0.5f) - padding;
        rec.y = (window_height * 0.7f) - padding;
        rec.width = width + (padding * 2.0f);
        fg_color = e->theme.console_foreground;
    } break;
    }

    rec.height = (e->console_line_count * line_height) + (padding * 2.0f);

    DrawRectangleRec(rec, e->theme.console_bg);
    if (e->console_highlight_idx > 0) {
        Rectangle highlight_rec = {
            .x = rec.x + padding,
            .y = rec.y + padding + e->console_highlight_idx * line_height,
            .width = rec.width - padding * 2.0f,
            .height = line_height,
        };
        DrawRectangleRec(highlight_rec, e->theme.console_highlight);
    }
    DrawRectangleLinesEx(rec, rec_line_size, fg_color);
    for (int i = 0; i < CONSOLE_LINE_CAPACITY; i += 1) {
        for (int j = 0; e->console_text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                padding + rec.x + (j * char_width),
                padding + rec.y + (i * line_height)
            };
            DrawTextCodepoint(e->font, e->console_text[i][j], position, line_height, fg_color);
        }
    }
}
