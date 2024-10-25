#include <string.h>

#include "raylib.h"
#include "main.h"

void console_print_str(State *state, char *text) {
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
    int console_x = state->window_width / 8;
    int console_y = state->window_width / 8;
    Rectangle rec = {
        console_x,
        console_y,
        state->window_width - (2 * console_x),
        state->window_height - (2 * console_y),
    };
    int rec_line_size = 5;
    int lines = 20;
    int line_height = rec.height / lines;
    int char_width = line_height * 0.6;
    DrawRectangleRec(rec, CONSOLE_BG_COLOR);
    DrawRectangleLinesEx(rec, rec_line_size, CONSOLE_TEXT_COLOR);
    int padding = 10;
    for (int i = 0; i < CONSOLE_LINE_COUNT; i += 1) {
        for (int j = 0; state->console_text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                padding + console_x + (j * char_width),
                padding + console_y + (i * line_height)
            };
            DrawTextCodepoint(state->font, state->console_text[i][j], position, line_height, CONSOLE_TEXT_COLOR);
        }
    }
}
