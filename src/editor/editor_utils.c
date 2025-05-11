#ifndef EDITOR_UTILS_C
#define EDITOR_UTILS_C

#include <ctype.h>
#include "../main.h"

static void console_set_text(State *state, char *text);
static void console_get_highlighted_text(State *state, char *buffer);
static void register_undo(State *state, Editor_Action_Type type, Editor_Coord coord, void *data);

inline static float editor_line_height(State *state) {
    int monitor = GetCurrentMonitor();
    return (float)GetMonitorHeight(monitor) / (float)state->editor.visible_lines;
}

inline static int editor_window_line_count(State *state) {
    return GetScreenHeight() / editor_line_height(state);
}

inline static int editor_char_width(float line_height) {
    return line_height * 0.6f;
}

static char keyboard_key_to_char(State *state, KeyboardKey key, bool shift) {
    if (is_alphabetic(key)) {
        if (is_uppercase(key)) {
            if (shift) {
                return key;
            }
            return key + 32;
        }
        if (shift) {
            return key - 32;
        }
        return key;
    }
    switch (state->keyboard_layout) {
    case KEYBOARD_LAYOUT_DEFAULT: {
        if (is_numeric(key)) {
            if (shift) {
                switch (key) {
                default: return key;
                case '1': return '!';
                case '2': return '@';
                case '3': return '#';
                case '4': return '$';
                case '5': return '%';
                case '6': return '^';
                case '7': return '&';
                case '8': return '*';
                case '9': return '(';
                case '0': return ')';
                }
            }
            return key;
        }
    } break;
    case KEYBOARD_LAYOUT_SWEDISH: {
        if (is_numeric(key)) {
            if (shift) {
                switch (key) {
                default: return key;
                case '1': return '!';
                case '2': return '\"';
                case '3': return '#';
                case '4': return '$';
                case '5': return '%';
                case '6': return '&';
                case '7': return '/';
                case '8': return '(';
                case '9': return ')';
                case '0': return '=';
                }
            }
            return key;
        }
    } break;
    }
    switch (key) {
    default: return '_';
    case ' ': return key;
    }
}

static void add_editor_line(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    e->line_count += 1;
    {
        int i;
        for (i = 0; e->lines[coord.y][coord.x + i] != '\0'; i++) {
            ASSERT(i < EDITOR_LINE_MAX_LENGTH);
            e->whatever_buffer[i] = e->lines[coord.y][coord.x + i];
        }
        ASSERT(i < EDITOR_LINE_MAX_LENGTH);
        e->whatever_buffer[i] = '\0';
    }
    ASSERT(coord.y < EDITOR_LINE_CAPACITY);
    ASSERT(coord.x < EDITOR_LINE_MAX_LENGTH);
    e->lines[coord.y][coord.x] = '\0';
    for (int i = e->line_count - 1; i > (coord.y + 1); i--) {

        ASSERT(i < EDITOR_LINE_MAX_LENGTH);
        ASSERT((i - 1) >= 0);

        strcpy(e->lines[i], e->lines[i - 1]);
    }
    coord.y++;
    ASSERT(coord.y < EDITOR_LINE_CAPACITY);
    strcpy(e->lines[coord.y], e->whatever_buffer);
}

static void delete_editor_line(State *state, int line_idx) {
    Editor *e = &state->editor;
    int line_len = TextLength(e->lines[line_idx]);
    int prev_line_len = TextLength(e->lines[line_idx - 1]);
    ASSERT((line_len + prev_line_len) < EDITOR_LINE_MAX_LENGTH - 1);
    strcat(e->lines[line_idx - 1], e->lines[line_idx]);
    for (int i = line_idx; i < e->line_count; i++) {
        ASSERT((i + 1) < EDITOR_LINE_CAPACITY);
        strcpy(e->lines[i], e->lines[i + 1]);
    }
    e->line_count--;
}

static void add_editor_char(State *state, char character, Editor_Coord coord) {
    ASSERT(coord.y < EDITOR_LINE_CAPACITY);
    ASSERT(coord.x < EDITOR_LINE_MAX_LENGTH);
    Editor *e = &state->editor;
    if (TextLength(e->lines[coord.y]) > EDITOR_LINE_MAX_LENGTH - 2) {
        return;
    }
    char next = e->lines[coord.y][coord.x];
    e->lines[coord.y][coord.x] = character;
    coord.x++;
    while (true) {
        ASSERT(coord.x < EDITOR_LINE_MAX_LENGTH);
        char tmp = e->lines[coord.y][coord.x];
        e->lines[coord.y][coord.x] = next;
        next = tmp;
        if (e->lines[coord.y][coord.x] == '\0') {
            break;
        }
        coord.x++;
    }
    coord.x++;
    ASSERT(coord.x < EDITOR_LINE_MAX_LENGTH);
    e->lines[coord.y][coord.x] = '\0';
}

static void delete_editor_char(State *state, Editor_Coord coord) {
    ASSERT(coord.y < EDITOR_LINE_CAPACITY);
    ASSERT(coord.x < EDITOR_LINE_MAX_LENGTH);
    Editor *e = &state->editor;
    int i;
    for (i = coord.x; e->lines[coord.y][i] != '\0'; i++) {
        ASSERT((i + 1) < EDITOR_LINE_MAX_LENGTH);
        e->lines[coord.y][i] = e->lines[coord.y][i + 1];
    }
    e->lines[coord.y][i] = '\0';
}

static Editor_Coord add_editor_string(State *state, char *string, Editor_Coord coord) {
    int idx = 0;
    bool more_chars = true;
    while (more_chars) {
        switch (string[idx]) {
        case '\0': {
            more_chars = false;
        } break;
        case '\n': {
            add_editor_line(state, coord);
            coord.y++;
            coord.x = 0;
        } break;
        default: {
            add_editor_char(state, string[idx], coord);
            coord.x++;
        } break;
        }
        idx++;
    }
    return coord;
}

static void delete_editor_string(State *state, Editor_Coord start, Editor_Coord end) {
    ASSERT(start.y < EDITOR_LINE_CAPACITY);
    ASSERT(start.x < EDITOR_LINE_MAX_LENGTH);
    ASSERT(end.y < EDITOR_LINE_CAPACITY);
    ASSERT(end.x < EDITOR_LINE_MAX_LENGTH);
    Editor *e = &state->editor;
    if (start.y < end.y) {
        e->lines[start.y][start.x] = '\0';
        int i;
        for (i = 0; i < strlen(e->lines[end.y]) - end.x; i++) {
            ASSERT(i < EDITOR_WHATEVER_BUFFER_LENGTH);
            e->whatever_buffer[i] = e->lines[end.y][i + end.x];
        }
        ASSERT(i < EDITOR_WHATEVER_BUFFER_LENGTH);
        e->whatever_buffer[i] = '\0';
        strcat(e->lines[start.y], e->whatever_buffer);
        int delete_amount = end.y - start.y;
        for (i = start.y + 1; i < e->line_count; i++) {
            ASSERT(i < EDITOR_LINE_CAPACITY);
            int copy_line = i + delete_amount;
            ASSERT(copy_line < EDITOR_LINE_CAPACITY);
            if (copy_line >= e->line_count) {
                e->lines[i][0] = '\0';
            } else {
                strcpy(e->lines[i], e->lines[copy_line]);
            }
        }
        e->line_count -= delete_amount;
        ASSERT(e->line_count > 0);
    } else {
        int i;
        for (i = 0; i < strlen(e->lines[start.y]) - end.x; i++) {
            ASSERT(i < EDITOR_WHATEVER_BUFFER_LENGTH);
            ASSERT((i + end.x) < EDITOR_LINE_MAX_LENGTH);
            e->whatever_buffer[i] = e->lines[start.y][i + end.x];
        }
        ASSERT(i < EDITOR_WHATEVER_BUFFER_LENGTH);
        e->whatever_buffer[i] = '\0';
        e->lines[start.y][start.x] = '\0';
        ASSERT((strlen(e->lines[start.y]) + strlen(e->whatever_buffer)) < EDITOR_LINE_MAX_LENGTH);
        strcat(e->lines[start.y], e->whatever_buffer);
    }
}

static char *copy_editor_string(State *state, Editor_Coord start, Editor_Coord end) {
    Editor *e = &state->editor;
    char *string = (char *)dyn_mem_alloc(sizeof(char) * e->line_count * EDITOR_LINE_CAPACITY);
    int str_idx = 0;
    if (start.y < end.y) {
        int delete_amount = end.y - start.y;
        for (int i = start.x; e->lines[start.y][i] != '\0'; i++) {
            string[str_idx++] = e->lines[start.y][i];
        }
        string[str_idx++] = '\n';
        for (int i = start.y + 1; i < (start.y + delete_amount); i++) {
            for (int j = 0; e->lines[i][j] != '\0'; j++) {
                string[str_idx++] = e->lines[i][j];
            }
            string[str_idx++] = '\n';
        }
        for (int i = 0; i < end.x; i++) {
            string[str_idx++] = e->lines[end.y][i];
        }
        string[str_idx++] = '\0';
    } else {
        for (int i = start.x; i < end.x; i++) {
            string[str_idx++] = e->lines[start.y][i];
        }
        string[str_idx++] = '\0';
    }
    return string;
}

static void snap_visual_vertical_offset_to_cursor(State *state) {
    Editor *e = &state->editor;
    float window_line_count = editor_window_line_count(state);
    if (e->cursor.y > (e->visual_vertical_offset + window_line_count - 1)) {
        e->visual_vertical_offset = e->cursor.y - window_line_count + 1;
    } else if (e->cursor.y < e->visual_vertical_offset) {
        e->visual_vertical_offset = e->cursor.y;
    }
}

static void center_visual_vertical_offset_around_cursor(State *state) {
    Editor *e = &state->editor;
    int middle_line = e->visible_lines / 2;
    if (e->cursor.y > middle_line) {
        e->visual_vertical_offset = e->cursor.y - middle_line;
    } else {
        e->visual_vertical_offset = 0;
    }
}

static void set_cursor_x(State *state, int x) {
    Editor *e = &state->editor;
    e->cursor.x = x;
    e->selection_x = x;
    e->selection_y = e->cursor.y;
    if (state->state == STATE_EDITOR) {
        e->cursor_anim_time = 0.0;
        e->preferred_x = x;
        snap_visual_vertical_offset_to_cursor(state);
    }
}

static void set_cursor_y(State *state, int line) {
    Editor *e = &state->editor;
    e->cursor.y = line;
    e->selection_y = line;
    int len = strlen(e->lines[line]);
    e->cursor.x = (len > e->preferred_x)
        ? e->preferred_x
        : len;
    e->selection_x = e->cursor.x;
    e->cursor_anim_time = 0.0;
}

static void set_cursor_selection_x(State *state, int x) {
    state->editor.cursor.x = x;
    state->editor.cursor_anim_time = 0.0;
    state->editor.preferred_x = x;
    snap_visual_vertical_offset_to_cursor(state);
}

static void set_cursor_selection_y(State *state, int line) {
    Editor *e = &state->editor;
    state->editor.cursor.y = line;
    state->editor.cursor_anim_time = 0.0;
    int len = strlen(e->lines[line]);
    e->cursor.x = (len > e->preferred_x)
        ? e->preferred_x
        : len;
    snap_visual_vertical_offset_to_cursor(state);
}

static bool cursor_selection_active(State *state) {
    bool same_line = state->editor.selection_y == state->editor.cursor.y;
    bool same_char = state->editor.selection_x == state->editor.cursor.x;
    return !same_line || !same_char;
}

static Editor_Selection_Data get_cursor_selection_data(State *state) {
    Editor *e = &state->editor;
    Editor_Selection_Data data;
    if (e->cursor.y < e->selection_y) {
        data.start.y = e->cursor.y;
        data.end.y = e->selection_y;
        data.start.x = e->cursor.x;
        data.end.x = e->selection_x;
    } else if (e->cursor.y > e->selection_y) {
        data.start.y = e->selection_y;
        data.end.y = e->cursor.y;
        data.start.x = e->selection_x;
        data.end.x = e->cursor.x;
    } else {
        data.start.y = e->cursor.y;
        data.end.y = e->cursor.y;
        if (e->cursor.x < e->selection_x) {
            data.start.x = e->cursor.x;
            data.end.x = e->selection_x;
        } else if (e->cursor.x > e->selection_x) {
            data.start.x = e->selection_x;
            data.end.x = e->cursor.x;
        } else {
            data.start.x = e->cursor.x;
            data.end.x = e->cursor.x;
        }
    }
    return data;
}

static bool cursor_delete_selection(State *state) {
    Editor *e = &state->editor;
    if (!cursor_selection_active(state)) {
        return false;
    }
    Editor_Selection_Data selection_data = get_cursor_selection_data(state);
    Editor_Coord action_coord = selection_data.start;
    register_undo(state, EDITOR_ACTION_DELETE_STRING, action_coord, &selection_data);
    delete_editor_string(state, selection_data.start, selection_data.end);
    set_cursor_y(state, selection_data.start.y);
    set_cursor_x(state, selection_data.start.x);
    return true;
}

static void go_to_definition(State *state) {
    Editor *e = &state->editor;

    char ident[EDITOR_LINE_MAX_LENGTH];
    int ident_offset = -1;
    while ((e->cursor.x + ident_offset) >= 0 && is_valid_in_identifier(e->lines[e->cursor.y][e->cursor.x + ident_offset])) {
        ident_offset--;
    }
    ident_offset++;
    int ident_len = 0;
    while (is_valid_in_identifier(e->lines[e->cursor.y][e->cursor.x + ident_offset])) {
        ident[ident_len] = e->lines[e->cursor.y][e->cursor.x + ident_offset];
        ident_offset++;
        ident_len++;
    }
    ident[ident_len] = '\0';
    int ident_line_idx = 0;
    int ident_char_idx = 0;
    int ident_idx = 0;
    bool ident_found = false;

    char *define = "define";
    int define_len = 6;
    int define_idx = 0;
    bool define_found = false;

    for (int i = 0; i < e->line_count; i++) {
        if (ident_found) {
            break;
        }
        for (int j = 0; e->lines[i][j] != '\0'; j++) {
            if (!define_found) {
                bool define_char_matches = e->lines[i][j] == define[define_idx];
                if (define_char_matches) {
                    define_idx++;
                }
                if (define_idx == define_len) {
                    define_found = true;
                }
            }
            if (is_valid_in_identifier(e->lines[i][j])) {
                bool ident_char_matches = e->lines[i][j] == ident[ident_idx];
                if (ident_char_matches) {
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
                    define_idx = 0;
                    define_found = false;
                    ident_idx = 0;
                }
            }
        }
    }
    if (ident_found) {
        set_cursor_y(state, ident_line_idx);
        set_cursor_x(state, ident_char_idx);
        center_visual_vertical_offset_around_cursor(state);
    }
}

static void cursor_new_line(State *state) {
    Editor *e = &state->editor;
    cursor_delete_selection(state);
    if (e->line_count >= EDITOR_LINE_CAPACITY - 1) {
        // TODO: this is currently a silent failure and it should not at all be that
        return;
    }
    add_editor_line(state, e->cursor);
    set_cursor_y(state, e->cursor.y + 1);
    set_cursor_x(state, 0);
    Editor_Coord action_coord = e->cursor;
    register_undo(state, EDITOR_ACTION_ADD_LINE, action_coord, NULL);
}

static void cursor_add_char(State *state, char character) {
    Editor *e = &state->editor;
    cursor_delete_selection(state);
    Editor_Coord action_coord = e->cursor;
    register_undo(state, EDITOR_ACTION_ADD_CHAR, action_coord, NULL);
    add_editor_char(state, character, e->cursor);
    set_cursor_x(state, e->cursor.x + 1);
}

static void cursor_delete_char(State *state) {
    Editor *e = &state->editor;
    if (e->cursor.x > 0) {
        set_cursor_x(state, e->cursor.x - 1);
        char char_to_be_deleted = e->lines[e->cursor.y][e->cursor.x];
        Editor_Coord action_coord = e->cursor;
        register_undo(state, EDITOR_ACTION_DELETE_CHAR, action_coord, &char_to_be_deleted);
        delete_editor_char(state, e->cursor);
    } else if (e->cursor.y > 0) {
        int new_y = e->cursor.y - 1;
        int new_x = TextLength(e->lines[new_y]);
        delete_editor_line(state, e->cursor.y);
        set_cursor_y(state, new_y);
        set_cursor_x(state, new_x);
        Editor_Coord action_coord = e->cursor;
        register_undo(state, EDITOR_ACTION_DELETE_LINE, action_coord, NULL);
    }
}

static void trigger_key(State *state, int key, bool ctrl, bool shift) {
    Editor *e = &state->editor;

    void (*set_target_y)(State *, int) = shift
        ? set_cursor_selection_y
        : set_cursor_y;

    void (*set_target_x)(State *, int) = shift
        ? set_cursor_selection_x
        : set_cursor_x;

    switch (key) {
    case KEY_RIGHT:
        if (ctrl &&
            e->cursor.y < (e->line_count - 1) &&
            e->lines[e->cursor.y][e->cursor.x] == '\0'
        ) {
            set_target_y(state, e->cursor.y + 1);
            set_target_x(state, 0);
        } else if (e->lines[e->cursor.y][e->cursor.x] != '\0') {
            if (ctrl) {
                bool found_whitespace = false;
                int i = e->cursor.x;
                while (1) {
                    if (e->lines[e->cursor.y][i] == '\0') {
                        set_target_x(state, i);
                        break;
                    }
                    if (!found_whitespace && isspace(e->lines[e->cursor.y][i])) {
                        found_whitespace = true;
                    } else if (found_whitespace && !isspace(e->lines[e->cursor.y][i])) {
                        set_target_x(state, i);
                        break;
                    }
                    i++;
                }
            } else {
                set_target_x(state, e->cursor.x + 1);
            }
        } else {
            // potentially reset selection
            set_target_x(state, e->cursor.x);
        }
        break;
    case KEY_LEFT:
        if (ctrl && e->cursor.x == 0 && e->cursor.y > 0) {
            set_target_y(state, e->cursor.y - 1);
            set_target_x(state, strlen(e->lines[e->cursor.y]));
        } else if (e->cursor.x > 0) {
            if (ctrl) {
                int i = e->cursor.x;
                while (i > 0 && e->lines[e->cursor.y][i] != ' ') {
                    i--;
                }
                bool found_word = false;
                while (1) {
                    if (i == 0) {
                        set_target_x(state, 0);
                        break;
                    }
                    if (!found_word && e->lines[e->cursor.y][i] != ' ') {
                        found_word = true;
                    } else if (found_word && e->lines[e->cursor.y][i] == ' ') {
                        set_target_x(state, i + 1);
                        break;
                    }
                    i--;
                }
            } else {
                set_target_x(state, e->cursor.x - 1);
            }
        } else {
            // potentially reset selection
            set_target_x(state, e->cursor.x);
        }
        break;
    case KEY_DOWN:
        {
            int line = e->cursor.y;
            if (e->cursor.y < e->line_count - 1) {
                line++;
            }
            set_target_y(state, line);
            snap_visual_vertical_offset_to_cursor(state);
        }
        break;
    case KEY_UP:
        {
            int line = e->cursor.y;
            if (e->cursor.y > 0) {
                line--;
            }
            set_target_y(state, line);
            snap_visual_vertical_offset_to_cursor(state);
        }
        break;
    case KEY_BACKSPACE:
        if (cursor_delete_selection(state)) {
            break;
        }
        cursor_delete_char(state);
        break;
    case KEY_ENTER:
        cursor_new_line(state);
        break;
    }
}

static void editor_set_cursor_x_first_non_whitespace(State *state) {
    int x;
    for (x = 0; state->editor.lines[state->editor.cursor.y][x] == ' '; x++);
    set_cursor_x(state, x);
}

static bool auto_click(State *state, KeyboardKey key) {
    Editor *e = &state->editor;

    if (IsKeyPressed(key)) {
        e->autoclick_key = key;
        e->autoclick_down_time = 0.0f;
        return true;
    } else if (IsKeyDown(key) && e->autoclick_key == key) {
        e->autoclick_down_time += state->delta_time;
        if (e->autoclick_down_time > AUTOCLICK_UPPER_THRESHOLD) {
            e->autoclick_down_time = AUTOCLICK_LOWER_THRESHOLD;
            return true;
        }
    } else if (e->autoclick_key == key) {
        e->autoclick_key = KEY_NULL;
        e->autoclick_down_time = 0.0f;
    }
    return false;
}

#endif
