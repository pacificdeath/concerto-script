#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "main.h"

int editor_line_height(int editor_size_y) {
    return editor_size_y / EDITOR_MAX_VISUAL_LINES;
}

int editor_char_width(int line_height) {
    return line_height * 0.6;
}

int editor_line_number_padding(int char_width) {
    return char_width * EDITOR_LINE_NUMBER_PADDING;
}

static void editor_snap_visual_vertical_offset_to_cursor(State *state) {
    Editor *e = &state->editor;
    if (e->cursor_line > (e->visual_vertical_offset + EDITOR_MAX_VISUAL_LINES - 1)) {
        e->visual_vertical_offset = e->cursor_line - EDITOR_MAX_VISUAL_LINES + 1;
    } else if (e->cursor_line < e->visual_vertical_offset) {
        e->visual_vertical_offset = e->cursor_line;
    }
}

static void editor_set_cursor_x(State *state, int x) {
    state->editor.cursor_x = x;
    state->editor.selection_x = x;
    state->editor.selection_line = state->editor.cursor_line;
    state->editor.cursor_anim_time = 0.0;
    if (state->editor.state == EDITOR_STATE_WRITE) {
        editor_snap_visual_vertical_offset_to_cursor(state);
    }
}

static void editor_set_cursor_line(State *state, int line) {
    Editor *e = &state->editor;
    e->cursor_line = line;
    e->selection_line = line;
    e->selection_x = e->cursor_x;
    e->cursor_anim_time = 0.0;
    if (e->state == EDITOR_STATE_WRITE) {
        editor_snap_visual_vertical_offset_to_cursor(state);
    }
}

static void editor_set_selection_x(State *state, int x) {
    state->editor.cursor_x = x;
    state->editor.cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(state);
}

static void editor_set_selection_line(State *state, int line) {
    state->editor.cursor_line = line;
    state->editor.cursor_anim_time = 0.0;
    editor_snap_visual_vertical_offset_to_cursor(state);
}

static bool editor_selection_active(State *state) {
    bool same_line = state->editor.selection_line == state->editor.cursor_line;
    bool same_char = state->editor.selection_x == state->editor.cursor_x;
    return !same_line || !same_char;
}

static Editor_Selection_Data editor_get_selection_data(State *state) {
    Editor *e = &state->editor;
    int start_line;
    Editor_Selection_Data data;
    if (e->cursor_line < e->selection_line) {
        data.start_line = e->cursor_line;
        data.end_line = e->selection_line;
        data.start_x = e->cursor_x;
        data.end_x = e->selection_x;
    } else if (e->cursor_line > e->selection_line) {
        data.start_line = e->selection_line;
        data.end_line = e->cursor_line;
        data.start_x = e->selection_x;
        data.end_x = e->cursor_x;
    } else {
        data.start_line = e->cursor_line;
        data.end_line = e->cursor_line;
        if (e->cursor_x < e->selection_x) {
            data.start_x = e->cursor_x;
            data.end_x = e->selection_x;
        } else if (e->cursor_x > e->selection_x) {
            data.start_x = e->selection_x;
            data.end_x = e->cursor_x;
        } else {
            data.start_x = e->cursor_x;
            data.end_x = e->cursor_x;
        }
    }
    return data;
}

static bool editor_delete_selection(State *state) {
    Editor *e = &state->editor;
    if (!editor_selection_active(state)) {
        return false;
    }
    Editor_Selection_Data selection_data = editor_get_selection_data(state);
    char str_buffer[2048];
    if (selection_data.start_line < selection_data.end_line) {
        e->lines[selection_data.start_line][selection_data.start_x] = '\0';
        int delete_amount = selection_data.end_line - selection_data.start_line;
        int i;
        for (i = 0; i < strlen(e->lines[selection_data.end_line]) - selection_data.end_x; i++) {
            str_buffer[i] = e->lines[selection_data.end_line][i + selection_data.end_x];
        }
        str_buffer[i] = '\0';
        strcat(e->lines[selection_data.start_line], str_buffer);
        for (i = selection_data.start_line + 1; i < e->line_count; i++) {
            int copy_line = i + delete_amount;
            if (copy_line >= e->line_count - 1) {
                e->lines[i][0] = '\0';
            } else {
                strcpy(e->lines[i], e->lines[copy_line]);
            }
        }
        e->line_count -= delete_amount;
    } else {
        int i;
        for (i = 0; i < strlen(e->lines[selection_data.start_line]) - selection_data.end_x; i++) {
            str_buffer[i] = e->lines[selection_data.start_line][i + selection_data.end_x];
        }
        str_buffer[i] = '\0';
        e->lines[selection_data.start_line][selection_data.start_x] = '\0';
        strcat(e->lines[selection_data.start_line], str_buffer);
    }
    editor_set_cursor_line(state, selection_data.start_line);
    editor_set_cursor_x(state, selection_data.start_x);
    return true;
}

static void editor_new_line(State *state) {
    Editor *e = &state->editor;
    editor_delete_selection(state);
    if (e->line_count >= EDITOR_LINE_CAPACITY - 1) {
        // TODO: this is currently a silent failure and it should not at all be that
        return;
    }
    e->line_count += 1;
    char *cut_line = malloc(sizeof *cut_line * EDITOR_LINE_MAX_LENGTH);
    {
        int i;
        for (i = 0; e->lines[e->cursor_line][e->cursor_x + i] != '\0'; i += 1) {
            cut_line[i] = e->lines[e->cursor_line][e->cursor_x + i];
        }
        cut_line[i] = '\0';
    }
    e->lines[e->cursor_line][e->cursor_x] = '\0';
    for (int i = e->line_count - 1; i > e->cursor_line; i--) {
        strcpy(e->lines[i], e->lines[i - 1]);
    }
    editor_set_cursor_line(state, e->cursor_line + 1);
    editor_set_cursor_x(state, 0);
    strcpy(e->lines[e->cursor_line], cut_line);
    free(cut_line);
}

static void editor_add_char(State *state, char c) {
    Editor *e = &state->editor;
    editor_delete_selection(state);
    char next = e->lines[e->cursor_line][e->cursor_x];
    e->lines[e->cursor_line][e->cursor_x] = c;
    editor_set_cursor_x(state, e->cursor_x + 1);
    int j = e->cursor_x;
    while (1) {
        char tmp = e->lines[e->cursor_line][j];
        e->lines[e->cursor_line][j] = next;
        next = tmp;
        if (e->lines[e->cursor_line][j] == '\0') {
            break;
        }
        j++;
    }
    e->lines[e->cursor_line][j + 1] = '\0';
}

static void editor_trigger_key(State *state, int key, bool ctrl, bool shift) {
    Editor *e = &state->editor;

    void (*set_target_line)(State *, int) = shift
        ? editor_set_selection_line
        : editor_set_cursor_line;

    void (*set_target_x)(State *, int) = shift
        ? editor_set_selection_x
        : editor_set_cursor_x;

    switch (key) {
    case KEY_RIGHT:
        if (ctrl &&
            e->cursor_line < (e->line_count - 1) &&
            e->lines[e->cursor_line][e->cursor_x] == '\0'
        ) {
            set_target_line(state, e->cursor_line + 1);
            set_target_x(state, 0);
        } else if (e->lines[e->cursor_line][e->cursor_x] != '\0') {
            if (ctrl) {
                bool found_whitespace = false;
                int i = e->cursor_x;
                while (1) {
                    if (e->lines[e->cursor_line][i] == '\0') {
                        set_target_x(state, i);
                        break;
                    }
                    if (!found_whitespace && isspace(e->lines[e->cursor_line][i])) {
                        found_whitespace = true;
                    } else if (found_whitespace && !isspace(e->lines[e->cursor_line][i])) {
                        set_target_x(state, i);
                        break;
                    }
                    i++;
                }
            } else {
                set_target_x(state, e->cursor_x + 1);
            }
        } else {
            // potentially reset selection
            set_target_x(state, e->cursor_x);
        }
        break;
    case KEY_LEFT:
        if (ctrl && e->cursor_x == 0 && e->cursor_line > 0) {
            set_target_line(state, e->cursor_line - 1);
            set_target_x(state, strlen(e->lines[e->cursor_line]));
        } else if (e->cursor_x > 0) {
            if (ctrl) {
                int i = e->cursor_x;
                while (i > 0 && e->lines[e->cursor_line][i] != ' ') {
                    i--;
                }
                bool found_word = false;
                while (1) {
                    if (i == 0) {
                        set_target_x(state, 0);
                        break;
                    }
                    if (!found_word && e->lines[e->cursor_line][i] != ' ') {
                        found_word = true;
                    } else if (found_word && e->lines[e->cursor_line][i] == ' ') {
                        set_target_x(state, i + 1);
                        break;
                    }
                    i--;
                }
            } else {
                set_target_x(state, e->cursor_x - 1);
            }
        } else {
            // potentially reset selection
            set_target_x(state, e->cursor_x);
        }
        break;
    case KEY_DOWN:
        {
            int line = e->cursor_line;
            if (e->cursor_line < e->line_count - 1) {
                line++;
            }
            set_target_line(state, line);
            int len = strlen(e->lines[e->cursor_line]);
            int x = e->cursor_x >= len ? len : e->cursor_x;
            set_target_x(state, x);
        }
        break;
    case KEY_UP:
        {
            int line = e->cursor_line;
            if (e->cursor_line > 0) {
                line--;
            }
            set_target_line(state, line);
            int len = strlen(e->lines[e->cursor_line]);
            int x = e->cursor_x >= len ? len : e->cursor_x;
            set_target_x(state, x);
        }
        break;
    case KEY_BACKSPACE:
        if (editor_delete_selection(state)) {
            break;
        }
        if (e->cursor_x > 0) {
            editor_set_cursor_x(state, e->cursor_x - 1);
            int i;
            for (i = e->cursor_x; e->lines[e->cursor_line][i] != '\0'; i++) {
                e->lines[e->cursor_line][i] = e->lines[e->cursor_line][i + 1];
            }
            e->lines[e->cursor_line][i] = '\0';
        } else if (e->cursor_line > 0) {
            int line_len = strlen(e->lines[e->cursor_line]);
            int prev_line_len = strlen(e->lines[e->cursor_line - 1]);
            if ((line_len + prev_line_len) < EDITOR_LINE_MAX_LENGTH - 1) {
                strcat(e->lines[e->cursor_line - 1], e->lines[e->cursor_line]);
                for (int i = e->cursor_line; i < e->line_count; i++) {
                    strcpy(e->lines[i], e->lines[i + 1]);
                }
                editor_set_cursor_line(state, e->cursor_line - 1);
                editor_set_cursor_x(state, prev_line_len);
                e->line_count--;
            } else {
                //TODO
            }
        }
        break;
    case KEY_ENTER:
        editor_new_line(state);
        break;
    }
}

void editor_init(State *state) {
    Editor *e = &state->editor;
    e->state = EDITOR_STATE_WRITE;
    e->line_count = 1;
    e->autoclick_key = KEY_NULL;
    e->auto_clickable_keys[0] = KEY_RIGHT;
    e->auto_clickable_keys[1] = KEY_LEFT;
    e->auto_clickable_keys[2] = KEY_DOWN;
    e->auto_clickable_keys[3] = KEY_UP;
    e->auto_clickable_keys[4] = KEY_BACKSPACE;
    e->auto_clickable_keys[5] = KEY_ENTER;
}

void editor_load_file(State *state, char *filename) {
    FILE *file;
    char line[4096];
    file = fopen(filename, "r");
    if (file == NULL) {
        return;
    }
    state->editor.line_count = 1;
    int i;
    for (i = 0; fgets(line, sizeof(line), file) != NULL; i += 1) {
        strcpy(state->editor.lines[i], line);
        int len = strlen(state->editor.lines[i]);
        state->editor.lines[i][len - 1] = '\0';
        state->editor.line_count++;
    }
    if (i > 0) {
        state->editor.line_count = i;
    }
    strcpy(state->editor.current_file, filename);
    fclose(file);
}

void editor_save_file(State *state, char *filename) {
    FILE *file;
    file = fopen(filename, "w");
    if (file == NULL) {
        return;
    }
    for (int i = 0; i < state->editor.line_count; i++) {
        int len = strlen(state->editor.lines[i]);
        state->editor.lines[i][len] = '\n';
        fwrite(state->editor.lines[i], sizeof(char), len, file);
        state->editor.lines[i][len] = '\0';
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
}

static void editor_set_cursor_x_first_non_whitespace(State *state) {
    int x;
    for (x = 0; state->editor.lines[state->editor.cursor_line][x] == ' '; x++);
    editor_set_cursor_x(state, x);
}

static void copy_to_clipboard(State *state) {
    Editor *e = &state->editor;
    int start_line;
    int end_line;
    int start_x;
    int end_x;
    if (e->cursor_line < e->selection_line) {
        start_line = e->cursor_line;
        start_x = e->cursor_x;
        end_line = e->selection_line;
        end_x = e->selection_x;
    } else if (e->selection_line < e->cursor_line) {
        start_line = e->selection_line;
        start_x = e->selection_x;
        end_line = e->cursor_line;
        end_x = e->cursor_x;
    } else {
        start_line = e->cursor_line;
        end_line = e->cursor_line;
        if (e->cursor_x < e->selection_x) {
            start_x = e->cursor_x;
            end_x = e->selection_x;
        } else if (e->selection_x < e->cursor_x) {
            start_x = e->selection_x;
            end_x = e->cursor_x;
        } else {
            return;
        }
    }
    e->clipboard_line_count = 1 + (end_line - start_line);
    for (int line = 0; line < e->clipboard_line_count; line++) {
        int line_len = strlen(e->lines[line + start_line]);
        int start = line == 0 ? start_x : 0;
        int end = line == e->clipboard_line_count - 1 ? end_x : line_len;
        int x;
        for (x = 0; x < end - start; x++) {
            e->clipboard_lines[line][x] = e->lines[line + start_line][x + start];
        }
        e->clipboard_lines[line][x] = '\0';
    }
}

static void cut_to_clipboard(State *state) {
    Editor *e = &state->editor;
    copy_to_clipboard(state);
    editor_delete_selection(state);
}

static void paste_clipboard(State *state) {
    Editor *e = &state->editor;
    if ((e->line_count + e->clipboard_line_count) > EDITOR_LINE_CAPACITY) {
        return;
    }
    Editor_Selection_Data data = editor_get_selection_data(state);
    char clipboard_line_lengths[e->clipboard_line_count];
    for (int i = 0; i < e->clipboard_line_count; i++) {
        clipboard_line_lengths[i] = strlen(e->clipboard_lines[i]);
        int old_line_len = strlen(e->lines[e->cursor_line + i]);
        int new_line_len;
        if (e->clipboard_line_count > 1) {
            if (i == 0) {
                new_line_len = data.start_x + clipboard_line_lengths[0];
            } else if (i == e->clipboard_line_count - 1) {
                new_line_len += old_line_len - data.end_x + clipboard_line_lengths[i];
            } else {
                new_line_len = clipboard_line_lengths[i];
            }
        } else {
            new_line_len = old_line_len + clipboard_line_lengths[i];
        }
        if (new_line_len >= EDITOR_LINE_MAX_LENGTH - 1) {
            return;
        }
    }
    editor_delete_selection(state);
    for (int i = 0; i < e->clipboard_line_count; i++) {
        for (int j = 0; j < clipboard_line_lengths[i]; j++) {
            editor_add_char(state, e->clipboard_lines[i][j]);
        }
        if (e->clipboard_line_count > 1 && i < e->clipboard_line_count - 1) {
            editor_new_line(state);
        }
    }
}

void editor_input(State *state) {
    Editor *e = &state->editor;

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool play_and_stop = ctrl && IsKeyPressed(KEY_P);

    switch (e->state) {
    default: return;
    case EDITOR_STATE_WRITE: {
        if (play_and_stop) {
            e->state = EDITOR_STATE_READY_TO_PLAY;
            return;
        }
        bool go_to_definition = ctrl && IsKeyPressed(KEY_G);
        if (go_to_definition) {
            char ident[EDITOR_LINE_MAX_LENGTH];
            int ident_offset = 0;
            while ((e->cursor_x + ident_offset) >= 0 && is_valid_in_identifier(e->lines[e->cursor_line][e->cursor_x + ident_offset])) {
                ident_offset--;
            }
            ident_offset++;
            int ident_len = 0;
            while (is_valid_in_identifier(e->lines[e->cursor_line][e->cursor_x + ident_offset])) {
                ident[ident_len] = e->lines[e->cursor_line][e->cursor_x + ident_offset];
                ident_offset++;
                ident_len++;
            }
            ident[ident_len] = '\0';
            int ident_line_idx = 0;
            int ident_char_idx = 0;
            int ident_idx = 0;
            bool ident_found = false;

            char *define = "DEFINE";
            int define_len = 6;
            int define_idx = 0;
            bool define_found = false;

            for (int i = 0; i < e->line_count; i++) {
                if (ident_found) {
                    break;
                }
                for (int j = 0; e->lines[i][j] != '\0'; j++) {
                    if (define_found) {
                        if (is_valid_in_identifier(e->lines[i][j])) {
                            if (e->lines[i][j] == ident[ident_idx]) {
                                ident_idx++;
                                int next_idx = j + 1;
                                char next = e->lines[i][next_idx];
                                if (ident_idx == ident_len && !is_valid_in_identifier(next) || next == '\0') {
                                    ident_line_idx = i;
                                    ident_char_idx = next_idx - ident_len;
                                    ident_found = true;
                                    break;
                                }
                            } else {
                                ident_idx = 0;
                                define_idx = 0;
                                define_found = false;
                            }
                        }
                    } else if (e->lines[i][j] == define[define_idx]) {
                        define_idx++;
                        if (define_idx == define_len) {
                            define_found = true;
                        }
                    }
                }
            }
            if (ident_found) {
                editor_set_cursor_line(state, ident_line_idx);
                editor_set_cursor_x(state, ident_char_idx);
                int middle_line = EDITOR_MAX_VISUAL_LINES / 2;
                if (ident_line_idx > middle_line) {
                    e->visual_vertical_offset = ident_line_idx - middle_line;
                } else {
                    e->visual_vertical_offset = 0;
                }
            }
            return;
        }
    } break;
    case EDITOR_STATE_PLAY: {
        if (play_and_stop) {
            e->state = EDITOR_STATE_INTERRUPT;
        }
    } return;
    }

    float scroll = GetMouseWheelMove();
    if (scroll != 0.0f) {
        e->visual_vertical_offset -= (scroll * EDITOR_SCROLL_MULTIPLIER);
        if (e->visual_vertical_offset < 0) {
            e->visual_vertical_offset = 0;
        } else if (e->visual_vertical_offset > e->line_count - 1) {
            e->visual_vertical_offset = e->line_count - 1;
        }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        //mouse_pos.x -= EDITOR_WINDOW_OFFSET_X;
        //mouse_pos.y -= EDITOR_WINDOW_OFFSET_Y;
        if (mouse_pos.x < 0 ||
            mouse_pos.y < 0 ||
            mouse_pos.x > state->window_width ||
            mouse_pos.x > state->window_height) {
            return;
        }
        int line_height = editor_line_height(state->window_height);
        int requested_line = (int)(mouse_pos.y / line_height);
        requested_line += e->visual_vertical_offset;
        if (requested_line >= e->line_count) {
            requested_line = e->line_count - 1;
        }
        int char_width = editor_char_width(line_height);
        int requested_char = roundf((mouse_pos.x / char_width) - EDITOR_LINE_NUMBER_PADDING);
        int len = strlen(e->lines[requested_line]);
        if (requested_char < 0) {
            requested_char = 0;
        }
        if (requested_char >= len) {
            requested_char = len;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            editor_set_cursor_line(state, requested_line);
            editor_set_cursor_x(state, requested_char);
        } else {
            editor_set_selection_line(state, requested_line);
            editor_set_selection_x(state, requested_char);
        }
        return;
    }
    if (ctrl && IsKeyPressed(KEY_C)) {
        copy_to_clipboard(state);
        return;
    }
    if (ctrl && IsKeyPressed(KEY_X)) {
        cut_to_clipboard(state);
        return;
    }
    if (ctrl && IsKeyPressed(KEY_V)) {
        paste_clipboard(state);
        return;
    }
    for (char c = 32; c < 127; c++) {
        if (IsKeyPressed(c) && e->cursor_x < EDITOR_LINE_MAX_LENGTH - 2 && e->cursor_x < EDITOR_LINE_MAX_LENGTH - 1) {
            if (shift) {
                switch (c) {
                case '1':
                    editor_add_char(state, '!');
                    return;
                case '3':
                    editor_add_char(state, '#');
                    return;
                case '9':
                    editor_add_char(state, '(');
                    return;
                case '0':
                    editor_add_char(state, ')');
                    return;
                }
            } else {
                editor_add_char(state, c);
            }
        } else {
            // TODO: this is currently a silent failure and it should not at all be that
        }
    }
    if (IsKeyPressed(KEY_KP_ADD)) {
        editor_add_char(state, '+');
        return;
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT)) {
        editor_add_char(state, '-');
        return;
    }
    if (IsKeyPressed(KEY_KP_MULTIPLY)) {
        editor_add_char(state, '*');
        return;
    }
    if (IsKeyPressed(KEY_KP_DIVIDE)) {
        editor_add_char(state, '/');
        return;
    }
    if (IsKeyPressed(KEY_TAB)) {
        bool selection_active = editor_selection_active(state);
        int start_line;
        int end_line;
        if (e->cursor_line < e->selection_line) {
            start_line = e->cursor_line;
            end_line = e->selection_line;
        } else {
            start_line = e->selection_line;
            end_line = e->cursor_line;
        }
        bool bad = false;
        int spaces_len = 1 + (end_line - start_line);
        int spaces[spaces_len];
        for (int i = 0; i < spaces_len; i++) {
            if (selection_active || shift) {
                editor_set_cursor_line(state, start_line + i);
                editor_set_cursor_x_first_non_whitespace(state);
            }
            int line_len = strlen(e->lines[start_line + i]);
            if (shift) {
                if (e->cursor_x == 0) {
                    spaces[i] = 0;
                    continue;
                }
                for (
                    spaces[i] = 1;
                    spaces[i] < line_len - 1
                        && e->lines[start_line + i][spaces[i]] == ' '
                        && (e->cursor_x - spaces[i]) % 4 != 0;
                    spaces[i]++
                );
            } else {
                for (spaces[i] = 1; (e->cursor_x + spaces[i]) % 4 != 0; spaces[i]++) {
                    if (line_len + spaces[i] >= EDITOR_LINE_MAX_LENGTH - 2) {
                        bad = true;
                        break;
                    }
                }
            }
        }
        if (bad) { return; }
        if (shift) {
            for (int i = 0; i < spaces_len; i++) {
                int line_idx = start_line + i;
                int line_len = strlen(e->lines[line_idx]);
                for (int j = spaces[i]; j < line_len; j++) {
                    e->lines[line_idx][j - spaces[i]] = e->lines[line_idx][j];
                }
                e->lines[line_idx][line_len - spaces[i]] = '\0';
                editor_set_cursor_x(state, e->cursor_x - spaces[i]);
            }
        } else {
            if (selection_active) {
                for (int i = 0; i < spaces_len; i++) {
                    editor_set_cursor_line(state, start_line + i);
                    editor_set_cursor_x(state, 0);
                    for (int j = 0; j < spaces[i]; j++) {
                        editor_add_char(state, ' ');
                    }
                }
            } else {
                for (int j = 0; j < spaces[0]; j++) {
                    editor_add_char(state, ' ');
                }
            }
        }
        if (selection_active) {
            e->cursor_line = start_line;
            e->cursor_x = 0;
            e->selection_line = end_line;
            e->selection_x = strlen(e->lines[end_line]);
        }
        return;
    }
    for (int i = 0; i < AUTO_CLICKABLE_KEYS_AMOUNT; i += 1) {
        int key = e->auto_clickable_keys[i];
        if (IsKeyPressed(key)) {
            e->autoclick_key = key;
            e->autoclick_down_time = 0.0f;
            editor_trigger_key(state, key, ctrl, shift);
        } else if (IsKeyDown(key) && e->autoclick_key == key) {
            e->autoclick_down_time += state->delta_time;
            if (e->autoclick_down_time > AUTOCLICK_UPPER_THRESHOLD) {
                e->autoclick_down_time = AUTOCLICK_LOWER_THRESHOLD;
                editor_trigger_key(state, key, ctrl, shift);
            }
        } else if (e->autoclick_key == key) {
            e->autoclick_key = KEY_NULL;
            e->autoclick_down_time = 0.0f;
        }
    }
}

static void render_cursor(Editor_Cursor_Render_Data data) {
    Vector2 start = {
        .x = data.line_number_offset + (data.x * data.char_width),
        .y = ((data.line - data.visual_vertical_offset) * data.line_height)
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
        data.line_number_offset + (data.start_x * data.char_width),
        ((data.line - data.visual_vertical_offset) * data.line_height),
        ((data.end_x - data.start_x) * data.char_width),
        data.line_height,
        EDITOR_SELECTION_COLOR
    );
}

static void editor_render_base(State *state, float line_height, float char_width, int line_number_padding) {
    Editor *e = &state->editor;
    char line_number_str[EDITOR_LINE_NUMBER_PADDING];

    for (int i = 0; i < EDITOR_MAX_VISUAL_LINES; i++) {
        int line_idx = e->visual_vertical_offset + i;

        if (line_idx >= e->line_count) {
            break;
        }

        bool rest_is_comment = false;
        bool found_word = false;
        Color color = EDITOR_NORMAL_COLOR;

        sprintf(line_number_str, "%4i", line_idx + 1);
        int line_number_str_len = strlen(line_number_str);

        for (int j = 0; j < line_number_str_len; j++) {
            Vector2 position = { j * char_width, i * line_height };
            DrawTextCodepoint(state->font, line_number_str[j], position, line_height, EDITOR_LINENUMBER_COLOR);
        }

        for (int j = 0; e->lines[line_idx][j] != '\0'; j++) {
            char c = e->lines[line_idx][j];

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
                        !isspace(e->lines[line_idx][j + char_offset]) &&
                        is_valid_in_identifier(e->lines[line_idx][j + char_offset]);
                        char_offset += 1
                    ) {
                        e->word[char_offset] = e->lines[line_idx][j + char_offset];
                    }
                    e->word[char_offset] = '\0';
                    if (strcmp(e->word, "PLAY") == 0) {
                        color = EDITOR_PLAY_COLOR;
                    } else if (strcmp(e->word, "WAIT") == 0) {
                        color = EDITOR_WAIT_COLOR;
                    } else if (
                        strcmp(e->word, "START") == 0 ||
                        strcmp(e->word, "DEFINE") == 0 ||
                        strcmp(e->word, "REPEAT") == 0 ||
                        strcmp(e->word, "ROUNDS") == 0 ||
                        strcmp(e->word, "SEMI") == 0 ||
                        strcmp(e->word, "BPM") == 0 ||
                        strcmp(e->word, "DURATION") == 0 ||
                        strcmp(e->word, "SCALE") == 0 ||
                        strcmp(e->word, "RISE") == 0 ||
                        strcmp(e->word, "FALL") == 0) {
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
                line_number_padding  + (j * char_width),
                (i * line_height)
            };

            DrawTextCodepoint(state->font, c, position, line_height, color);
        }
    }

}

static void editor_render_state_write(State *state) {
    Editor *e = &state->editor;

    int line_height = editor_line_height(state->window_height);
    int char_width = editor_char_width(line_height);

    int line_number_padding = editor_line_number_padding(char_width);

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (editor_selection_active(state)) {
        Editor_Selection_Data selection_data = editor_get_selection_data(state);
        Editor_Selection_Render_Data selection_render_data = {
            .line = selection_data.start_line,
            .line_number_offset = line_number_padding,
            .visual_vertical_offset = e->visual_vertical_offset,
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
            selection_render_data.end_x = strlen(e->lines[selection_render_data.line]);
            render_selection(selection_render_data);

            selection_render_data.start_x = 0;
            for (int i = selection_data.start_line + 1; i < selection_data.end_line; i++) {
                selection_render_data.line = i;
                selection_render_data.end_x = strlen(e->lines[i]);
                render_selection(selection_render_data);
            }

            selection_render_data.line = selection_data.end_line;
            selection_render_data.start_x = 0;
            selection_render_data.end_x = selection_data.end_x;
            render_selection(selection_render_data);
        }
    }
    e->cursor_anim_time = e->cursor_anim_time + (state->delta_time * EDITOR_CURSOR_ANIMATION_SPEED);
    float pi2 = PI * 2.0f;
    if (e->cursor_anim_time > pi2) {
        e->cursor_anim_time -= pi2;
    }
    float alpha = (cos(e->cursor_anim_time) + 1.0) * 127.5;
    Editor_Cursor_Render_Data cursor_data = {
        .line = e->cursor_line,
        .line_number_offset = line_number_padding,
        .visual_vertical_offset = e->visual_vertical_offset,
        .x = e->cursor_x,
        .line_height = line_height,
        .char_width = char_width,
        .alpha = alpha
    };
    render_cursor(cursor_data);
}

void editor_render_state_wait_to_play(State *state) {
    int line_height = editor_line_height(state->window_height);
    int char_width = editor_char_width(line_height);

    int line_number_padding = editor_line_number_padding(char_width);

    editor_render_base(state, line_height, char_width, line_number_padding);
}

void editor_render_state_play(State *state) {
    Editor *e = &state->editor;

    int line_height = editor_line_height(state->window_height);
    int char_width = editor_char_width(line_height);

    int line_number_padding = editor_line_number_padding(char_width);

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (state->current_sound == NULL) {
        return;
    }

    int r = EDITOR_MAX_VISUAL_LINES / 2;

    float rec_line_size = 5.0f;
    Rectangle rec;

    Tone *tone = &state->current_sound->tone;

    rec.x = -(rec_line_size) + line_number_padding + (tone->char_idx * char_width);

    editor_set_cursor_x(state, tone->char_idx);
    if (tone->line_idx < r) {
        editor_set_cursor_line(state, tone->line_idx);

        e->visual_vertical_offset = 0;
        rec.y = -(rec_line_size) + (tone->line_idx * line_height);
    } else {
        editor_set_cursor_line(state, e->visual_vertical_offset + r);

        e->visual_vertical_offset = tone->line_idx - r;
        rec.y = -(rec_line_size) + (r * line_height);
    }

    rec.width = (2 * rec_line_size) + char_width * tone->char_count;

    rec.height = (2 * rec_line_size) + line_height;

    Color color = { .b = 255, .a = 255 };
    if (tone->idx % 2 == 0) {
        color.r = 255;
        color.g = 0;
    } else {
        color.r = 0;
        color.g = 255;
    }
    DrawRectangleLinesEx(rec, 5, color);
}
