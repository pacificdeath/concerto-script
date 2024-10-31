#include <string.h>

#include "raylib.h"
#include "main.h"

void console_set_text(State *state, char *text) {
    for (int i = 0; i < CONSOLE_LINE_CAPACITY; i++) {
        state->console_text[i][0] = '\0';
    }
    int current_char = 0;
    int current_line = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        switch (text[i]) {
        case '\0':
        case '\n':
            #if DEBUG
            if (current_line >= CONSOLE_LINE_CAPACITY - 2) {
                printf("Console: NO! BAD LINE! VERY VERY BAD LINE!");
                exit(1);
            }
            #endif
            state->console_text[current_line][current_char] = '\0';
            current_line++;
            current_char = 0;
            break;
        default:
            #if DEBUG
            if (current_char >= CONSOLE_LINE_MAX_LENGTH - 2) {
                printf("Console: NO! BAD CHAR! VERY VERY BAD CHAR!");
                exit(1);
            }
            #endif
            state->console_text[current_line][current_char] = text[i];
            current_char++;
            break;
        }
    }
    state->console_text[current_line][current_char] = '\0';
    state->console_line_count = current_line + 1;
}

void console_get_highlighted_text(State *state, char *buffer) {
    int char_idx = 0;
    while (state->console_text[state->console_highlight_idx][char_idx] != '\n' && state->console_text[state->console_highlight_idx][char_idx] != '\0') {
        buffer[char_idx] = state->console_text[state->console_highlight_idx][char_idx];
        char_idx++;
    }
    buffer[char_idx] = '\0';
}

void console_render(State *state) {
    Rectangle rec;

    int line_height = editor_line_height(state);
    int char_width = editor_char_width(line_height);

    int rec_line_size = 5;
    int padding = rec_line_size * 2.0f;

    Color fg_color;

    switch (state->state) {
    default:
    case STATE_EDITOR_FILE_EXPLORER: {
        rec.x = (state->window_width * 0.1f) - padding;
        rec.y = (state->window_height * 0.1f) - padding;
        rec.width = (state->window_width * 0.8f) + (padding * 2.0f);
        fg_color = CONSOLE_FG_DEFAULT_COLOR;
    } break;
    case STATE_EDITOR_SAVE_FILE_ERROR:
    case STATE_COMPILATION_ERROR: {
        rec.x = (state->window_width * 0.25f) - padding;
        rec.y = (state->window_height * 0.25f) - padding;
        rec.width = (state->window_width * 0.5f) + (padding * 2.0f);
        fg_color = CONSOLE_FG_ERROR_COLOR;
    } break;
    case STATE_EDITOR_FIND_TEXT: {
        int width = char_width * (EDITOR_FINDER_BUFFER_MAX + 1);
        rec.x = (state->window_width * 0.5f) - (width * 0.5f) - padding;
        rec.y = (state->window_height * 0.7f) - padding;
        rec.width = width + (padding * 2.0f);
        fg_color = CONSOLE_FG_DEFAULT_COLOR;
    } break;
    }

    rec.height = (state->console_line_count * line_height) + (padding * 2.0f);

    DrawRectangleRec(rec, CONSOLE_BG_COLOR);
    DrawRectangleLinesEx(rec, rec_line_size, fg_color);
    for (int i = 0; i < CONSOLE_LINE_CAPACITY; i += 1) {
        for (int j = 0; state->console_text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                padding + rec.x + (j * char_width),
                padding + rec.y + (i * line_height)
            };
            DrawTextCodepoint(state->font, state->console_text[i][j], position, line_height, fg_color);
        }
    }

    if (state->console_highlight_idx > 0) {
        rec.x += padding;
        rec.y += padding + state->console_highlight_idx * line_height;
        rec.width -= padding * 2.0f;
        rec.height = line_height;
        DrawRectangleRec(rec, CONSOLE_HIGHLIGHT_COLOR);
    }
}
