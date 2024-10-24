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
    int console_width = state->window_width - (2 * console_x);
    int console_height = state->window_height - (2 * console_y);
    int lines = 20;
    int line_height = console_height / lines;
    int char_width = line_height * 0.6;
    DrawRectangle(console_x, console_y, console_width, console_height / 2, CONSOLE_BG_COLOR);
    for (int i = 0; i < CONSOLE_LINE_COUNT; i += 1) {
        for (int j = 0; state->console_text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                console_x + (j * char_width),
                console_y + (i * line_height)
            };
            DrawTextCodepoint(state->font, state->console_text[i][j], position, line_height, CONSOLE_TEXT_COLOR);
        }
    }
}
