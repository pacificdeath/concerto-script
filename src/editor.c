#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"

#define EDITOR_MAX_VISUAL_LINES 30
#define EDITOR_WINDOW_OFFSET_X 20
#define EDITOR_WINDOW_OFFSET_Y 20
#define EDITOR_FILENAME_MAX_LENGTH 256
#define EDITOR_LINE_CAPACITY 256
#define EDITOR_LINE_MAX_LENGTH 256
#define EDITOR_LINE_NUMBER_PADDING 5
#define AUTO_CLICKABLE_KEYS_AMOUNT 6
#define AUTOCLICK_LOWER_THRESHOLD 0.3F
#define AUTOCLICK_UPPER_THRESHOLD 0.33F
#define EDITOR_CURSOR_WIDTH 3
#define EDITOR_CURSOR_ANIMATION_SPEED 10.0f
#define EDITOR_SCROLL_MULTIPLIER 3
#define EDITOR_BG_COLOR (Color) { 0, 0, 0, 215 }
#define EDITOR_NORMAL_COLOR (Color) { 255, 255, 255, 255 }
#define EDITOR_PLAY_COLOR (Color) { 0, 255, 0, 255 }
#define EDITOR_WAIT_COLOR (Color) { 255, 0, 0, 255 }
#define EDITOR_KEYWORD_COLOR (Color) { 0, 127, 255, 255 }
#define EDITOR_PAREN_COLOR (Color) { 255, 255, 0, 255 }
#define EDITOR_SPACE_COLOR (Color) { 255, 255, 255, 32 }
#define EDITOR_CURSOR_COLOR (Color) { 255, 0, 255, 255 }
#define EDITOR_SELECTION_COLOR (Color) { 0, 255, 255, 64 }
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
    int selection_x;
    int selection_line;
    char current_file[EDITOR_FILENAME_MAX_LENGTH];
    int autoclick_key;
    float autoclick_down_time;
    int auto_clickable_keys[AUTO_CLICKABLE_KEYS_AMOUNT];
    int visual_vertical_offset;
    float cursor_anim_time;
    Editor_Command command;
} Editor;

typedef struct Editor_Cursor_Render_Data {
    int line;
    int x;
    int line_number_offset;
    int visual_vertical_offset;
    int char_width;
    int line_height;
    float alpha;
} Editor_Cursor_Render_Data;

typedef struct Editor_Selection_Render_Data {
    int line;
    int line_number_offset;
    int visual_vertical_offset;
    int start_x;
    int end_x;
    int char_width;
    int line_height;
} Editor_Selection_Render_Data;

typedef struct Editor_Selection_Data {
    int start_line;
    int end_line;
    int start_x;
    int end_x;
} Editor_Selection_Data;

int editor_width(int window_width) {
    return window_width - (2 * EDITOR_WINDOW_OFFSET_X);
}

int editor_height(int window_height) {
    return window_height - (2 * EDITOR_WINDOW_OFFSET_Y);
}

int editor_line_height(int editor_size_y) {
    return editor_size_y / EDITOR_MAX_VISUAL_LINES;
}

int editor_char_width(int line_height) {
    return line_height * 0.6;
}

int editor_line_number_padding(int char_width) {
    return char_width * EDITOR_LINE_NUMBER_PADDING;
}

static void editor_snap_visual_vertical_offset_to_cursor(Editor *editor) {
    if (editor->cursor_line > (editor->visual_vertical_offset + EDITOR_MAX_VISUAL_LINES - 1)) {
        editor->visual_vertical_offset = editor->cursor_line - EDITOR_MAX_VISUAL_LINES + 1;
    } else if (editor->cursor_line < editor->visual_vertical_offset) {
        editor->visual_vertical_offset = editor->cursor_line;
    }
}

static void editor_set_cursor_x(Editor *editor, int x) {
    editor->cursor_x = x;
    editor->selection_x = x;
    editor->selection_line = editor->cursor_line;
    editor->cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(editor);
}

static void editor_set_cursor_line(Editor *editor, int line) {
    editor->cursor_line = line;
    editor->selection_line = line;
    editor->selection_x = editor->cursor_x;
    editor->cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(editor);
}

static void editor_set_selection_x(Editor *editor, int x) {
    editor->cursor_x = x;
    editor->cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(editor);
}

static void editor_set_selection_line(Editor *editor, int line) {
    editor->cursor_line = line;
    editor->cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(editor);
}

static bool editor_selection_active(Editor *editor) {
    bool same_line = editor->selection_line == editor->cursor_line;
    bool same_char = editor->selection_x == editor->cursor_x;
    return !same_line || !same_char;
}

static Editor_Selection_Data editor_get_selection_data(Editor *editor) {
    int start_line;
    Editor_Selection_Data selection_data;
    if (editor->cursor_line < editor->selection_line) {
        selection_data.start_line = editor->cursor_line;
        selection_data.end_line = editor->selection_line;
        selection_data.start_x = editor->cursor_x;
        selection_data.end_x = editor->selection_x;
    } else if (editor->cursor_line > editor->selection_line) {
        selection_data.start_line = editor->selection_line;
        selection_data.end_line = editor->cursor_line;
        selection_data.start_x = editor->selection_x;
        selection_data.end_x = editor->cursor_x;
    } else {
        selection_data.start_line = editor->cursor_line;
        selection_data.end_line = editor->cursor_line;
        if (editor->cursor_x < editor->selection_x) {
            selection_data.start_x = editor->cursor_x;
            selection_data.end_x = editor->selection_x;
        } else if (editor->cursor_x > editor->selection_x) {
            selection_data.start_x = editor->selection_x;
            selection_data.end_x = editor->cursor_x;
        } else {
            selection_data.start_x = editor->cursor_x;
            selection_data.end_x = editor->cursor_x;
        }
    }
    return selection_data;
}

static bool editor_delete_selection(Editor *editor) {
    if (!editor_selection_active(editor)) {
        return false;
    }
    Editor_Selection_Data selection_data = editor_get_selection_data(editor);
    char str_buffer[2048];
    if (selection_data.start_line < selection_data.end_line) {
        editor->lines[selection_data.start_line][selection_data.start_x] = '\0';
        int delete_amount = selection_data.end_line - selection_data.start_line;
        int i;
        for (i = 0; i < strlen(editor->lines[selection_data.end_line]) - selection_data.end_x; i++) {
            str_buffer[i] = editor->lines[selection_data.end_line][i + selection_data.end_x];
        }
        str_buffer[i] = '\0';
        strcat(editor->lines[selection_data.start_line], str_buffer);
        for (i = selection_data.start_line + 1; i < editor->line_count; i++) {
            int copy_line = i + delete_amount;
            if (copy_line >= editor->line_count - 1) {
                editor->lines[i][0] = '\0';
            } else {
                strcpy(editor->lines[i], editor->lines[copy_line]);
            }
        }
        editor->line_count -= delete_amount;
    } else {
        int i;
        for (i = 0; i < strlen(editor->lines[selection_data.start_line]) - selection_data.end_x; i++) {
            str_buffer[i] = editor->lines[selection_data.start_line][i + selection_data.end_x];
        }
        str_buffer[i] = '\0';
        editor->lines[selection_data.start_line][selection_data.start_x] = '\0';
        strcat(editor->lines[selection_data.start_line], str_buffer);
    }
    editor_set_cursor_line(editor, selection_data.start_line);
    editor_set_cursor_x(editor, selection_data.start_x);
    return true;
}

static void editor_new_line(Editor *editor) {
    editor_delete_selection(editor);
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
    editor_set_cursor_line(editor, editor->cursor_line + 1);
    editor_set_cursor_x(editor, 0);
    strcpy(editor->lines[editor->cursor_line], cut_line);
    free(cut_line);
}

static void editor_add_char(Editor *editor, char c) {
    editor_delete_selection(editor);
    char next = editor->lines[editor->cursor_line][editor->cursor_x];
    editor->lines[editor->cursor_line][editor->cursor_x] = c;
    editor_set_cursor_x(editor, editor->cursor_x + 1);
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

static void editor_trigger_key(Editor *editor, int key, bool ctrl, bool shift) {
    void (*set_target_line)(Editor *, int) = shift
        ? editor_set_selection_line
        : editor_set_cursor_line;

    void (*set_target_x)(Editor *, int) = shift
        ? editor_set_selection_x
        : editor_set_cursor_x;

    switch (key) {
    case KEY_RIGHT:
        if (ctrl &&
            editor->cursor_line < (editor->line_count - 1) &&
            editor->lines[editor->cursor_line][editor->cursor_x] == '\0'
        ) {
            set_target_line(editor, editor->cursor_line + 1);
            set_target_x(editor, 0);
        } else if (editor->lines[editor->cursor_line][editor->cursor_x] != '\0') {
            if (ctrl) {
                bool found_whitespace = false;
                int i = editor->cursor_x;
                while (1) {
                    if (editor->lines[editor->cursor_line][i] == '\0') {
                        set_target_x(editor, i);
                        break;
                    }
                    if (!found_whitespace && isspace(editor->lines[editor->cursor_line][i])) {
                        found_whitespace = true;
                    } else if (found_whitespace && !isspace(editor->lines[editor->cursor_line][i])) {
                        set_target_x(editor, i);
                        break;
                    }
                    i++;
                }
            } else {
                set_target_x(editor, editor->cursor_x + 1);
            }
        } else {
            // potentially reset selection
            set_target_x(editor, editor->cursor_x);
        }
        break;
    case KEY_LEFT:
        if (ctrl && editor->cursor_x == 0 && editor->cursor_line > 0) {
            set_target_line(editor, editor->cursor_line - 1);
            set_target_x(editor, strlen(editor->lines[editor->cursor_line]));
        } else if (editor->cursor_x > 0) {
            if (ctrl) {
                int i = editor->cursor_x;
                while (i > 0 && editor->lines[editor->cursor_line][i] != ' ') {
                    i--;
                }
                bool found_word = false;
                while (1) {
                    if (i == 0) {
                        set_target_x(editor, 0);
                        break;
                    }
                    if (!found_word && editor->lines[editor->cursor_line][i] != ' ') {
                        found_word = true;
                    } else if (found_word && editor->lines[editor->cursor_line][i] == ' ') {
                        set_target_x(editor, i + 1);
                        break;
                    }
                    i--;
                }
            } else {
                set_target_x(editor, editor->cursor_x - 1);
            }
        } else {
            // potentially reset selection
            set_target_x(editor, editor->cursor_x);
        }
        break;
    case KEY_DOWN:
        {
            int line = editor->cursor_line;
            if (editor->cursor_line < editor->line_count - 1) {
                line++;
            }
            set_target_line(editor, line);
            int len = strlen(editor->lines[editor->cursor_line]);
            int x = editor->cursor_x >= len ? len : editor->cursor_x;
            set_target_x(editor, x);
        }
        break;
    case KEY_UP:
        {
            int line = editor->cursor_line;
            if (editor->cursor_line > 0) {
                line--;
            }
            set_target_line(editor, line);
            int len = strlen(editor->lines[editor->cursor_line]);
            int x = editor->cursor_x >= len ? len : editor->cursor_x;
            set_target_x(editor, x);
        }
        break;
    case KEY_BACKSPACE:
        if (editor_delete_selection(editor)) {
            break;
        }
        if (editor->cursor_x > 0) {
            editor_set_cursor_x(editor, editor->cursor_x - 1);
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
                editor_set_cursor_line(editor, editor->cursor_line - 1);
                editor_set_cursor_x(editor, prev_line_len);
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
    editor->selection_x = 0;
    editor->selection_line = 0;
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

    for (int i = 0; i < EDITOR_LINE_CAPACITY; i++) {
        editor->lines[i][0] = '\0';
    }

    editor->cursor_anim_time = 0.0f;
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

void editor_input(Editor *editor, int window_width, int window_height, float delta_time) {
    float scroll = GetMouseWheelMove();
    if (scroll != 0.0f) {
        editor->visual_vertical_offset -= (scroll * EDITOR_SCROLL_MULTIPLIER);
        if (editor->visual_vertical_offset < 0) {
            editor->visual_vertical_offset = 0;
        } else if (editor->visual_vertical_offset > editor->line_count - 1) {
            editor->visual_vertical_offset = editor->line_count - 1;
        }
    }
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        mouse_pos.x -= EDITOR_WINDOW_OFFSET_X;
        mouse_pos.y -= EDITOR_WINDOW_OFFSET_Y;
        int editor_w = editor_width(window_width);
        int editor_h = editor_height(window_height);
        if (mouse_pos.x < 0 ||
            mouse_pos.y < 0 ||
            mouse_pos.x > editor_w ||
            mouse_pos.x > editor_h) {
            return;
        }
        int line_height = editor_line_height(editor_h);
        int requested_line = (int)(mouse_pos.y / line_height);
        requested_line += editor->visual_vertical_offset;
        if (requested_line >= editor->line_count) {
            requested_line = editor->line_count - 1;
        }
        int char_width = editor_char_width(line_height);
        int requested_char = roundf((mouse_pos.x / char_width) - EDITOR_LINE_NUMBER_PADDING);
        int len = strlen(editor->lines[requested_line]);
        if (requested_char < 0) {
            requested_char = 0;
        }
        if (requested_char >= len) {
            requested_char = len;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            editor_set_cursor_line(editor, requested_line);
            editor_set_cursor_x(editor, requested_char);
        } else {
            editor_set_selection_line(editor, requested_line);
            editor_set_selection_x(editor, requested_char);
        }
        return;
    }
    if (ctrl && IsKeyPressed(KEY_P)) {
        editor->command = EDITOR_COMMAND_PLAY;
        return;
    }
    for (char c = 32; c < 127; c++) {
        if (IsKeyPressed(c) && editor->cursor_x < EDITOR_LINE_MAX_LENGTH - 2 && editor->cursor_x < EDITOR_LINE_MAX_LENGTH - 1) {
            if (shift) {
                switch (c) {
                case '1':
                    editor_add_char(editor, '!');
                    return;
                case '3':
                    editor_add_char(editor, '#');
                    return;
                case '9':
                    editor_add_char(editor, '(');
                    return;
                case '0':
                    editor_add_char(editor, ')');
                    return;
                }
            } else {
                editor_add_char(editor, c);
            }
        } else {
            // TODO: this is currently a silent failure and it should not at all be that
        }
    }
    if (IsKeyPressed(KEY_KP_ADD)) {
        editor_add_char(editor, '+');
        return;
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT)) {
        editor_add_char(editor, '-');
        return;
    }
    if (IsKeyPressed(KEY_KP_MULTIPLY)) {
        editor_add_char(editor, '*');
        return;
    }
    if (IsKeyPressed(KEY_KP_DIVIDE)) {
        editor_add_char(editor, '/');
        return;
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
        return;
    }
    for (int i = 0; i < AUTO_CLICKABLE_KEYS_AMOUNT; i += 1) {
        int key = editor->auto_clickable_keys[i];
        if (IsKeyPressed(key)) {
            editor->autoclick_key = key;
            editor->autoclick_down_time = 0.0f;
            editor_trigger_key(editor, key, ctrl, shift);
        } else if (IsKeyDown(key) && editor->autoclick_key == key) {
            editor->autoclick_down_time += delta_time;
            if (editor->autoclick_down_time > AUTOCLICK_UPPER_THRESHOLD) {
                editor->autoclick_down_time = AUTOCLICK_LOWER_THRESHOLD;
                editor_trigger_key(editor, key, ctrl, shift);
            }
        } else if (editor->autoclick_key == key) {
            editor->autoclick_key = KEY_NULL;
            editor->autoclick_down_time = 0.0f;
        }
    }
}

static void render_cursor(Editor_Cursor_Render_Data data) {
    Vector2 start = {
        .x = EDITOR_WINDOW_OFFSET_X + data.line_number_offset + (data.x * data.char_width),
        .y = EDITOR_WINDOW_OFFSET_Y + ((data.line - data.visual_vertical_offset) * data.line_height)
    };
    Vector2 end = {
        .x = start.x,
        .y = start.y + data.line_height
    };
    Color color = EDITOR_CURSOR_COLOR;
    color.a = data.alpha;
    DrawLineEx(start, end, EDITOR_CURSOR_WIDTH, color);
}

static void render_selection(Editor_Selection_Render_Data data) {
    DrawRectangle(
        EDITOR_WINDOW_OFFSET_X + data.line_number_offset + (data.start_x * data.char_width),
        EDITOR_WINDOW_OFFSET_Y + ((data.line - data.visual_vertical_offset) * data.line_height),
        ((data.end_x - data.start_x) * data.char_width),
        data.line_height,
        EDITOR_SELECTION_COLOR
    );
}

void editor_render(Editor *editor, int window_width, int window_height, Font *font, float delta_time) {
    int editor_w = editor_width(window_width);
    int editor_h = editor_height(window_height);

    int line_height = editor_line_height(editor_h);
    int char_width = editor_char_width(line_height);

    DrawRectangle(EDITOR_WINDOW_OFFSET_X, EDITOR_WINDOW_OFFSET_Y, editor_w, editor_h, EDITOR_BG_COLOR);

    char line_number_str[EDITOR_LINE_NUMBER_PADDING];
    int line_number_padding = editor_line_number_padding(char_width);

    for (int i = 0; i < EDITOR_MAX_VISUAL_LINES; i++) {
        int line_idx = editor->visual_vertical_offset + i;

        bool rest_is_comment = false;
        bool found_word = false;
        Color color = EDITOR_NORMAL_COLOR;

        sprintf(line_number_str, "%4i", line_idx + 1);
        int line_number_str_len = strlen(line_number_str);

        for (int j = 0; j < line_number_str_len; j++) {
            Vector2 position = {
                EDITOR_WINDOW_OFFSET_X + (j * char_width),
                EDITOR_WINDOW_OFFSET_Y + (i * line_height)
            };
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
                    if (strcmp(editor->word, "PLAY") == 0 ||
                        strcmp(editor->word, "SLIDEUP") == 0 ||
                        strcmp(editor->word, "SLIDEDOWN") == 0) {
                        color = EDITOR_PLAY_COLOR;
                    } else if (strcmp(editor->word, "WAIT") == 0) {
                        color = EDITOR_WAIT_COLOR;
                    } else if (
                        strcmp(editor->word, "SEMI") == 0 ||
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
                EDITOR_WINDOW_OFFSET_X + line_number_padding  + (j * char_width),
                EDITOR_WINDOW_OFFSET_Y + (i * line_height)
            };

            DrawTextCodepoint(*font, c, position, line_height, color);
        }
    }

    if (editor_selection_active(editor)) {
        Editor_Selection_Data selection_data = editor_get_selection_data(editor);
        Editor_Selection_Render_Data selection_render_data = {
            .line = selection_data.start_line,
            .line_number_offset = line_number_padding,
            .visual_vertical_offset = editor->visual_vertical_offset,
            .start_x = selection_data.start_x,
            .end_x = 0, // might differ in first selection render
            .line_height = line_height,
            .char_width = char_width
        };
        if (selection_data.start_line == selection_data.end_line) {
            selection_render_data.start_x = selection_data.start_x;
            selection_render_data.end_x = selection_data.end_x;
            render_selection(selection_render_data);
        } else {
            selection_render_data.start_x = selection_data.start_x;
            selection_render_data.end_x = strlen(editor->lines[selection_render_data.line]);
            render_selection(selection_render_data);

            selection_render_data.start_x = 0;
            for (int i = selection_data.start_line + 1; i < selection_data.end_line; i++) {
                selection_render_data.line = i;
                selection_render_data.end_x = strlen(editor->lines[i]);
                render_selection(selection_render_data);
            }

            selection_render_data.line = selection_data.end_line;
            selection_render_data.start_x = 0;
            selection_render_data.end_x = selection_data.end_x;
            render_selection(selection_render_data);
        }
    }
    editor->cursor_anim_time = editor->cursor_anim_time + (delta_time * EDITOR_CURSOR_ANIMATION_SPEED);
    float pi2 = PI * 2.0f;
    if (editor->cursor_anim_time > pi2) {
        editor->cursor_anim_time -= pi2;
    }
    float alpha = (cos(editor->cursor_anim_time) + 1.0) * 127.5;
    Editor_Cursor_Render_Data cursor_data = {
        .line = editor->cursor_line,
        .line_number_offset = line_number_padding,
        .visual_vertical_offset = editor->visual_vertical_offset,
        .x = editor->cursor_x,
        .line_height = line_height,
        .char_width = char_width,
        .alpha = alpha
    };
    render_cursor(cursor_data);
}

void editor_free(Editor *editor) {
    free(editor);
}
