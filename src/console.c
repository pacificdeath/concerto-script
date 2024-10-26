#include <string.h>

#include "raylib.h"
#include "main.h"

static void set_console_text(State *state, char *text) {
    for (int i = 0; i < CONSOLE_LINE_COUNT; i++) {
        state->console_text[i][0] = '\0';
    }
    int current_char = 0;
    int current_line = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        switch (text[i]) {
        case '\0':
        case '\n':
            state->console_text[current_line][current_char] = '\0';
            current_line++;
            current_char = 0;
            break;
        default:
            state->console_text[current_line][current_char] = text[i];
            current_char++;
            break;
        }
    }
    state->console_text[current_line][current_char] = '\0';
}

void console_render(State *state) {
    Rectangle rec;

    int line_height = editor_line_height(state);
    int char_width = editor_char_width(line_height);

    int rec_line_size = 5;
    int padding = rec_line_size * 2.0f;

    switch (state->state) {
    default:
    case STATE_COMPILATION_ERROR: {
        rec.x = (state->window_width * 0.25f) - padding;
        rec.y = (state->window_height * 0.25f) - padding;
        rec.width = (state->window_width * 0.5f) + (padding * 2.0f);
        rec.height = (state->window_height * 0.5f) + (padding * 2.0f);
    } break;
    case STATE_EDITOR_FIND_TEXT: {
        int width = char_width * (EDITOR_FIND_MATCHES_TEXT_MAX_LENGTH + 1);
        rec.x = (state->window_width * 0.5f) - (width * 0.5f) - padding;
        rec.y = (state->window_height * 0.7f) - padding;
        rec.width = width + (padding * 2.0f);
        rec.height = (state->window_height * 0.2f) + (padding * 2.0f);
    } break;
    case STATE_EDITOR_GO_TO_LINE: {
        rec.x = (state->window_width * 0.4f) - padding;
        rec.y = (state->window_height * 0.45f) - padding;
        rec.width = (state->window_width * 0.2f) + (padding * 2.0f);
        rec.height = (state->window_height * 0.1f) + (padding * 2.0f);
    } break;
    }

    DrawRectangleRec(rec, CONSOLE_BG_COLOR);
    DrawRectangleLinesEx(rec, rec_line_size, CONSOLE_TEXT_COLOR);
    for (int i = 0; i < CONSOLE_LINE_COUNT; i += 1) {
        for (int j = 0; state->console_text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                padding + rec.x + (j * char_width),
                padding + rec.y + (i * line_height)
            };
            DrawTextCodepoint(state->font, state->console_text[i][j], position, line_height, CONSOLE_TEXT_COLOR);
        }
    }
}
