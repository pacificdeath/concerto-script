#include <stdio.h>
#include <string.h>

#include "raylib.h"

#define EDITOR_FILENAME_MAX_LENGTH 256
#define EDITOR_LINE_CAPACITY 256
#define EDITOR_LINE_MAX_LENGTH 256
#define AUTO_CLICKABLE_KEYS_AMOUNT 6
#define AUTOCLICK_LOWER_THRESHOLD 0.3F
#define AUTOCLICK_UPPER_THRESHOLD 0.33F
#define EDITOR_BG_COLOR (Color) { 0, 0, 0, 215 }
#define EDITOR_NORMAL_COLOR (Color) { 255, 255, 255, 255 }
#define EDITOR_PLAY_COLOR (Color) { 0, 255, 0, 255 }
#define EDITOR_WAIT_COLOR (Color) { 255, 0, 0, 255 }
#define EDITOR_KEYWORD_COLOR (Color) { 0, 127, 255, 255 }
#define EDITOR_PAREN_COLOR (Color) { 255, 255, 0, 255 }
#define EDITOR_SPACE_COLOR (Color) { 255, 255, 255, 32 }
#define EDITOR_CURSOR_COLOR (Color) { 255, 255, 0, 127 }
#define EDITOR_LINENUMBER_COLOR (Color) { 127, 127, 127, 255 }
#define EDITOR_COMMENT_COLOR (Color) { 255, 0, 255, 127 }

typedef enum Editor_Command {
    EDITOR_COMMAND_NONE,
    EDITOR_COMMAND_PLAY,
} Editor_Command;

typedef struct Editor {
    char lines[EDITOR_LINE_CAPACITY][EDITOR_LINE_MAX_LENGTH];
    char word[EDITOR_LINE_MAX_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_line;
    char current_file[EDITOR_FILENAME_MAX_LENGTH];
    int autoclick_key;
    float autoclick_down_time;
    int auto_clickable_keys[AUTO_CLICKABLE_KEYS_AMOUNT];
    int visual_vertical_offset;
    Editor_Command command;
} Editor;

static void editor_clear(Editor *editor) {
    for (int i = 0; i < EDITOR_LINE_CAPACITY; i++) {
        editor->lines[i][0] = '\0';
    }
}

static void editor_new_line(Editor *editor) {
    if (editor->line_count >= EDITOR_LINE_CAPACITY - 1) {
        // TODO: this is currently a silent failure and it should not at all be that
        return;
    }
    if (strlen(editor->lines[editor->cursor_line]) >= EDITOR_LINE_MAX_LENGTH - 2) {
        // TODO: this is currently a silent failure and it should not at all be that
        return;
    }
    editor->line_count += 1;
    char *cut_line = malloc(sizeof *cut_line * EDITOR_LINE_MAX_LENGTH);
    {
        int i;
        for (i = 0; editor->lines[editor->cursor_line][editor->cursor_x + i] != '\0'; i += 1) {
            cut_line[i] = editor->lines[editor->cursor_line][editor->cursor_x + i];
        }
        cut_line[i] = '\0';
    }
    editor->lines[editor->cursor_line][editor->cursor_x] = '\0';
    for (int i = editor->line_count - 1; i > editor->cursor_line; i--) {
        strcpy(editor->lines[i], editor->lines[i - 1]);
    }
    editor->cursor_line++;
    editor->cursor_x = 0;
    strcpy(editor->lines[editor->cursor_line], cut_line);
    free(cut_line);
}

static void editor_add_char(Editor *editor, char c) {
    char next = editor->lines[editor->cursor_line][editor->cursor_x];
    editor->lines[editor->cursor_line][editor->cursor_x] = c;
    editor->cursor_x++;
    int j = editor->cursor_x;
    while (1) {
        char tmp = editor->lines[editor->cursor_line][j];
        editor->lines[editor->cursor_line][j] = next;
        next = tmp;
        if (editor->lines[editor->cursor_line][j] == '\0') {
            break;
        }
        j++;
    }
    editor->lines[editor->cursor_line][j + 1] = '\0';
}

static void editor_trigger_key(Editor *editor, int key, bool ctrl) {
    switch (key) {
    case KEY_RIGHT:
        if (ctrl && editor->line_count > editor->cursor_line && editor->lines[editor->cursor_line][editor->cursor_x] == '\0') {
            editor->cursor_line++;
            editor->cursor_x = 0;
        } else if (editor->lines[editor->cursor_line][editor->cursor_x] != '\0') {
            if (ctrl) {
                bool found_whitespace = false;
                int i = editor->cursor_x;
                while (1) {
                    if (editor->lines[editor->cursor_line][i] == '\0') {
                        editor->cursor_x = i;
                        break;
                    }
                    if (!found_whitespace && isspace(editor->lines[editor->cursor_line][i])) {
                        found_whitespace = true;
                    } else if (found_whitespace && !isspace(editor->lines[editor->cursor_line][i])) {
                        editor->cursor_x = i;
                        break;
                    }
                    i++;
                }
            } else {
                editor->cursor_x++;
            }
        }
        break;
    case KEY_LEFT:
        if (ctrl && editor->cursor_x == 0 && editor->cursor_line > 0) {
            editor->cursor_line--;
            editor->cursor_x = strlen(editor->lines[editor->cursor_line]);
        } else if (editor->cursor_x > 0) {
            if (ctrl) {
                int i = editor->cursor_x;
                while (i > 0 && editor->lines[editor->cursor_line][i] != ' ') {
                    i--;
                }
                bool found_word = false;
                while (1) {
                    if (i == 0) {
                        editor->cursor_x = 0;
                        break;
                    }
                    if (!found_word && editor->lines[editor->cursor_line][i] != ' ') {
                        found_word = true;
                    } else if (found_word && editor->lines[editor->cursor_line][i] == ' ') {
                        editor->cursor_x = i + 1;
                        break;
                    }
                    i--;
                }
            } else {
                editor->cursor_x--;
            }
        }
        break;
    case KEY_DOWN:
        {
            if (editor->cursor_line < editor->line_count - 1) {
                editor->cursor_line++;
            }
            int len = strlen(editor->lines[editor->cursor_line]);
            if (editor->cursor_x >= len) {
                editor->cursor_x = len;
            }
        }
        break;
    case KEY_UP:
        {
            if (editor->cursor_line > 0) {
                editor->cursor_line--;
            }
            int len = strlen(editor->lines[editor->cursor_line]);
            if (editor->cursor_x >= len) {
                editor->cursor_x = len;
            }
        }
        break;
    case KEY_BACKSPACE:
        if (editor->cursor_x > 0) {
            editor->cursor_x--;
            int i;
            for (i = editor->cursor_x; editor->lines[editor->cursor_line][i] != '\0'; i++) {
                editor->lines[editor->cursor_line][i] = editor->lines[editor->cursor_line][i + 1];
            }
            editor->lines[editor->cursor_line][i] = '\0';
        } else if (editor->cursor_line > 0) {
            int line_len = strlen(editor->lines[editor->cursor_line]);
            int prev_line_len = strlen(editor->lines[editor->cursor_line - 1]);
            if ((line_len + prev_line_len) < EDITOR_LINE_MAX_LENGTH - 1) {
                strcat(editor->lines[editor->cursor_line - 1], editor->lines[editor->cursor_line]);
                for (int i = editor->cursor_line; i < editor->line_count; i++) {
                    strcpy(editor->lines[i], editor->lines[i + 1]);
                }
                editor->cursor_line--;
                editor->cursor_x = prev_line_len;
                editor->line_count--;
            } else {
                //TODO
            }
        }
        break;
    case KEY_ENTER:
        editor_new_line(editor);
        break;
    }
}

Editor *editor_allocate() {
    Editor *editor = malloc(sizeof *editor);

    editor->cursor_x = 0;
    editor->cursor_line = 0;
    editor->line_count = 1;

    editor->autoclick_key = KEY_NULL;
    editor->autoclick_down_time = 0.0f;
    editor->auto_clickable_keys[0] = KEY_RIGHT;
    editor->auto_clickable_keys[1] = KEY_LEFT;
    editor->auto_clickable_keys[2] = KEY_DOWN;
    editor->auto_clickable_keys[3] = KEY_UP;
    editor->auto_clickable_keys[4] = KEY_BACKSPACE;
    editor->auto_clickable_keys[5] = KEY_ENTER;

    editor->visual_vertical_offset = 0;

    editor->command = EDITOR_COMMAND_NONE;

    editor_clear(editor);
    return editor;
}

void editor_load_file(Editor *editor, char *filename) {
    FILE *file;
    char line[4096];
    file = fopen(filename, "r");
    if (file == NULL) {
        fclose(file);
        return;
    }
    editor->line_count = 1;
    int i;
    for (i = 0; fgets(line, sizeof(line), file) != NULL; i += 1) {
        strcpy(editor->lines[i], line);
        int len = strlen(editor->lines[i]);
        editor->lines[i][len - 1] = '\0';
        editor->line_count++;
    }
    if (i > 0) {
        editor->line_count = i;
    }
    strcpy(editor->current_file, filename);
    fclose(file);
}

void editor_save_file(Editor *editor, char *filename) {
    FILE *file;
    file = fopen(filename, "w");
    if (file == NULL) {
        fclose(file);
        return;
    }
    for (int i = 0; i < editor->line_count; i++) {
        int len = strlen(editor->lines[i]);
        editor->lines[i][len] = '\n';
        fwrite(editor->lines[i], sizeof(char), len, file);
        editor->lines[i][len] = '\0';
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
}

void editor_input(Editor *editor, float delta_time) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_P)) {
        editor->command = EDITOR_COMMAND_PLAY;
        return;
    }
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (char c = 32; c < 127; c++) {
        if (IsKeyPressed(c) && editor->cursor_x < EDITOR_LINE_MAX_LENGTH - 2 && editor->cursor_x < EDITOR_LINE_MAX_LENGTH - 1) {
            if (shift) {
                switch (c) {
                case '1':
                    editor_add_char(editor, '!');
                    break;
                case '3':
                    editor_add_char(editor, '#');
                    break;
                case '9':
                    editor_add_char(editor, '(');
                    break;
                case '0':
                    editor_add_char(editor, ')');
                    break;
                }
                continue;
            } else {
                editor_add_char(editor, c);
            }
        } else {
            // TODO: this is currently a silent failure and it should not at all be that
        }
    }
    if (IsKeyPressed(KEY_KP_ADD)) {
        editor_add_char(editor, '+');
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT)) {
        editor_add_char(editor, '-');
    }
    if (IsKeyPressed(KEY_KP_MULTIPLY)) {
        editor_add_char(editor, '*');
    }
    if (IsKeyPressed(KEY_KP_DIVIDE)) {
        editor_add_char(editor, '/');
    }
    if (IsKeyPressed(KEY_TAB)) {
        int line_len = strlen(editor->lines[editor->cursor_line]);
        bool bad = false;
        int spaces;
        for (spaces = 1; (editor->cursor_x + spaces) % 4 != 0; spaces++) {
            if (line_len + spaces >= EDITOR_LINE_MAX_LENGTH - 2) {
                bad = true;
                break;
            }
        }
        if (!bad) {
            for (int i = 0; i < spaces; i++) {
                editor_add_char(editor, ' ');
            }
        }
    }
    for (int i = 0; i < AUTO_CLICKABLE_KEYS_AMOUNT; i += 1) {
        int key = editor->auto_clickable_keys[i];
        if (IsKeyPressed(key)) {
            editor->autoclick_key = key;
            editor->autoclick_down_time = 0.0f;
            editor_trigger_key(editor, key, ctrl);
        } else if (IsKeyDown(key) && editor->autoclick_key == key) {
            editor->autoclick_down_time += delta_time;
            if (editor->autoclick_down_time > AUTOCLICK_UPPER_THRESHOLD) {
                editor->autoclick_down_time = AUTOCLICK_LOWER_THRESHOLD;
                editor_trigger_key(editor, key, ctrl);
            }
        } else if (editor->autoclick_key == key) {
            editor->autoclick_key = KEY_NULL;
            editor->autoclick_down_time = 0.0f;
        }
    }
}

void editor_render(Editor *editor, int window_width, int window_height, Font *font) {
    int editor_x = 20;
    int editor_y = 20;
    int editor_width = window_width - (2 * editor_x);
    int editor_height = window_height - (2 * editor_y);

    int lines = 30;
    int line_height = editor_height / lines;
    int char_width = line_height * 0.6;

    DrawRectangle(editor_x, editor_y, editor_width, editor_height, EDITOR_BG_COLOR);

    char line_number_str[5];
    int line_number_offset = (5 * char_width);

    if (editor->cursor_line > (editor->visual_vertical_offset + lines - 1)) {
        editor->visual_vertical_offset = editor->cursor_line - lines + 1;
    } else if (editor->cursor_line < editor->visual_vertical_offset) {
        editor->visual_vertical_offset = editor->cursor_line;
    }

    for (int i = 0; i < lines; i++) {
        int line_idx = editor->visual_vertical_offset + i;

        bool rest_is_comment = false;
        bool found_word = false;
        Color color = EDITOR_NORMAL_COLOR;

        sprintf(line_number_str, "%4i", line_idx + 1);
        int line_number_str_len = strlen(line_number_str);

        for (int j = 0; j < line_number_str_len; j++) {
            Vector2 position = { editor_x + (j * char_width), editor_y + (i * line_height) };
            DrawTextCodepoint(*font, line_number_str[j], position, line_height, EDITOR_LINENUMBER_COLOR);
        }

        for (int j = 0; editor->lines[line_idx][j] != '\0'; j++) {
            char c = editor->lines[line_idx][j];

            if (found_word && !isalpha(c)) {
                found_word = false;
                color = EDITOR_NORMAL_COLOR;
            }

            if (!rest_is_comment && !found_word) {
                if (isalpha(c)) {
                    found_word = true;
                    int char_offset;
                    for (
                        char_offset = 0;
                        !isspace(editor->lines[line_idx][j + char_offset]) &&
                        is_valid_in_identifier(editor->lines[line_idx][j + char_offset]);
                        char_offset += 1
                    ) {
                        editor->word[char_offset] = editor->lines[line_idx][j + char_offset];
                    }
                    editor->word[char_offset] = '\0';
                    if (strcmp(editor->word, "PLAY") == 0) {
                        color = EDITOR_PLAY_COLOR;
                    } else if (strcmp(editor->word, "WAIT") == 0) {
                        color = EDITOR_WAIT_COLOR;
                    } else if (
                        strcmp(editor->word, "BPM") == 0 ||
                        strcmp(editor->word, "DURATION") == 0 ||
                        strcmp(editor->word, "SETDURATION") == 0 ||
                        strcmp(editor->word, "SCALE") == 0 ||
                        strcmp(editor->word, "SETSCALE") == 0 ||
                        strcmp(editor->word, "RISE") == 0 ||
                        strcmp(editor->word, "FALL") == 0 ||
                        strcmp(editor->word, "SEMIRISE") == 0 ||
                        strcmp(editor->word, "SEMIFALL") == 0 ||
                        strcmp(editor->word, "DEFINE") == 0 ||
                        strcmp(editor->word, "REPEAT") == 0) {
                        color = EDITOR_KEYWORD_COLOR;
                    } else {
                        color = EDITOR_NORMAL_COLOR;
                    }
                } else {
                    switch (c) {
                    case '!':
                        rest_is_comment = true;
                        color = EDITOR_COMMENT_COLOR;
                        break;
                    case '(':
                    case ')':
                        color = EDITOR_PAREN_COLOR;
                        break;
                    case ' ':
                        color = EDITOR_SPACE_COLOR;
                        c = '-';
                        break;
                    default:
                        color = EDITOR_NORMAL_COLOR;
                        break;
                    }
                }
            }

            Vector2 position = {
                editor_x + line_number_offset  + (j * char_width),
                editor_y + (i * line_height)
            };

            DrawTextCodepoint(*font, c, position, line_height, color);
        }
    }

    DrawRectangle(
        editor_x + line_number_offset + (editor->cursor_x * char_width),
        editor_y + ((editor->cursor_line - editor->visual_vertical_offset) * line_height),
        char_width,
        line_height,
        EDITOR_CURSOR_COLOR
    );
}


void editor_free(Editor *editor) {
    free(editor);
}
