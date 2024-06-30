#include <string.h>

#include "raylib.h"

#define CONSOLE_LINE_COUNT 8
#define CONSOLE_LINE_MAX_LENGTH 64
#define CONSOLE_BG_COLOR (Color) {64, 64, 64, 255}
#define CONSOLE_TEXT_COLOR (Color) {0, 255, 255, 255}

// TODO: this console thing can in some situations write into the editor which is obviously a bad thing
// I have not investigated it further though, I'm thinking it's up to you now, now I'm gonna go have fun 

typedef struct Console {
    char text[CONSOLE_LINE_COUNT][CONSOLE_LINE_MAX_LENGTH];
} Console;

Console *console_allocate() {
    Console *console = (Console *)malloc(sizeof(Console));
    return console;
}

void console_print_str(Console *console, char *text) {
    for (int i = 0; i < CONSOLE_LINE_COUNT; i++) {
        console->text[i][0] = '\0';
    }
    int current_char = 0;
    int current_line = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        switch (text[i]) {
        case '\0':
        case '\n':
            console->text[current_line][current_char] = '\0';
            current_line++;
            current_char = 0;
            break;
        default:
            console->text[current_line][current_char] = text[i];
            current_char++;
            break;
        }
    }
    console->text[current_line][current_char] = '\0';
}

void console_render(Console *console, int window_width, int window_height, Font *font) {
    int console_x = window_width / 8;
    int console_y = window_width / 8;
    int console_width = window_width - (2 * console_x);
    int console_height = window_height - (2 * console_y);
    int lines = 20;
    int line_height = console_height / lines;
    int char_width = line_height * 0.6;
    DrawRectangle(console_x, console_y, console_width, console_height / 2, CONSOLE_BG_COLOR);
    for (int i = 0; i < CONSOLE_LINE_COUNT; i += 1) {
        for (int j = 0; console->text[i][j] != '\0'; j += 1) {
            Vector2 position = {
                console_x + (j * char_width),
                console_y + (i * line_height)
            };
            DrawTextCodepoint(*font, console->text[i][j], position, line_height, CONSOLE_TEXT_COLOR);
        }
    }
}

void console_free(Console *console) {
    free(console);
}