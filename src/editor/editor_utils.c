#ifndef EDITOR_UTILS_C
#define EDITOR_UTILS_C

#include <ctype.h>
#include "../main.h"

static void console_set_text(State *state, const char *text);
static void console_get_highlighted_text(State *state, char *buffer);
static void register_undo(State *state, Editor_Action_Type type, Editor_Coord coord, void *data);

inline static void editor_clear(State *state) {
    Editor *e = &state->editor;
    for (int i = 0; i < e->lines.length; i++) {
        DynArray *line = dyn_array_get(&e->lines, i);
        dyn_array_release(line);
    }
    dyn_array_clear(&e->lines);

    DynArray init_line = {0};
    dyn_array_alloc(&init_line, sizeof(char));
    dyn_array_push(&e->lines, &init_line);
}

inline static float editor_line_height(State *state) {
    int monitor = GetCurrentMonitor();
    return (float)GetMonitorHeight(monitor) / (float)state->editor.visible_lines;
}

inline static int editor_char_width(float line_height) {
    return line_height * 0.6f;
}

inline static int editor_window_line_count(State *state) {
    return GetScreenHeight() / editor_line_height(state);
}

inline static int editor_line_max_chars(State *state) {
    return GetScreenWidth() / editor_char_width(editor_line_height(state));
}

inline static int editor_line_number_string(char buffer[16], int line_number) {
    snprintf(buffer, 16, "%4d", line_number);
    return strlen(buffer);
}

Editor_Coord editor_wrapped_coord(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    for (int i = 0; i < e->wrap_line_count; i++) {
        if (e->wrap_lines[i].logical_idx == coord.y) {
            return (Editor_Coord) {
                .y = e->wrap_lines[i].visual_idx + (coord.x / e->wrap_idx),
                .x = coord.x % e->wrap_idx,
            };
        }
    }
    return (Editor_Coord){0};
}

static void editor_update_wrapped_line_data(State *state) {
    Editor *e = &state->editor;

    char line_number_string[16];
    int max_line_number_length = editor_line_number_string(line_number_string, e->lines.length);
    e->wrap_idx = editor_line_max_chars(state) - max_line_number_length;

    e->wrap_line_count = editor_window_line_count(state);
    int visual_idx = 0;

    int i;
    for (i = 0; true; i++) {
        int line_idx = e->visual_vertical_offset + i;
        if (line_idx >= e->lines.length) {
            break;
        }

        DynArray *line = dyn_array_get(&e->lines, line_idx);

        int wraps = (line->length == 0) ? 0 : (line->length - 1) / e->wrap_idx;

        e->wrap_line_count -= wraps;
        e->wrap_lines[i].logical_idx = line_idx;
        e->wrap_lines[i].visual_idx = visual_idx;
        e->wrap_lines[i].wrap_amount = wraps;

        int visual_lines_for_line = wraps + 1;

        if ((visual_idx + visual_lines_for_line) > e->visible_lines) {
            break;
        }

        visual_idx += visual_lines_for_line;
    }
}

inline static bool is_line_on_screen(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    Editor_Coord wrapped_coord = editor_wrapped_coord(state, coord);
    if (wrapped_coord.y < e->visual_vertical_offset) {
        return false;
    }
    if (wrapped_coord.y >= e->visual_vertical_offset + e->wrap_line_count) {
        return false;
    }
    return true;
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

    DynArray new_line;
    dyn_array_alloc(&new_line, sizeof(char));

    DynArray *prev_line = dyn_array_get(&e->lines, coord.y);

    if (coord.x < prev_line->length) {
        int copy_amount = prev_line->length - coord.x;
        char *copy_start = dyn_array_get(prev_line, coord.x);

        dyn_array_insert(&new_line, 0, copy_start, copy_amount);
        dyn_array_resize(prev_line, coord.x);
    }

    dyn_array_insert(&e->lines, coord.y + 1, &new_line, 1);
}

static void delete_editor_line(State *state, int line_idx) {
    Editor *e = &state->editor;

    DynArray *line_to_delete = dyn_array_get(&e->lines, line_idx);
    DynArray *prev_line = dyn_array_get(&e->lines, line_idx - 1);

    if (line_to_delete->length > 0) {
        char *line_to_delete_inner = dyn_array_get(line_to_delete, 0);
        dyn_array_insert(prev_line, prev_line->length, line_to_delete_inner, line_to_delete->length);
    }

    dyn_array_release(line_to_delete);
    dyn_array_remove(&e->lines, line_idx, 1);
}

static void add_editor_char(State *state, char c, Editor_Coord coord) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, coord.y);
    dyn_array_insert(line, coord.x, &c, 1);
}

static void delete_editor_char(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, coord.y);
    dyn_array_remove(line, coord.x, 1);
}

static Editor_Coord add_editor_string(State *state, DynArray *string, Editor_Coord coord) {
    for (int i = 0; i < string->length; i++) {
        char c = dyn_char_get(string, i);
        switch (c) {
        case '\n': {
            add_editor_line(state, coord);
            coord.y++;
            coord.x = 0;
        } break;
        default: {
            add_editor_char(state, c, coord);
            coord.x++;
        } break;
        }
    }
    return coord;
}

static void delete_editor_string(State *state, Editor_Coord start, Editor_Coord end) {
    Editor *e = &state->editor;
    DynArray *start_line = dyn_array_get(&e->lines, start.y);
    if (start.y < end.y) {
        DynArray *end_line = dyn_array_get(&e->lines, end.y);
        dyn_array_remove(start_line, start.x, start_line->length - start.x);
        int i;
        for (i = 0; i < end_line->length - end.x; i++) {
            char c = dyn_char_get(end_line, i + end.x);
            dyn_array_push(start_line, &c);
        }
        int remove_start_idx = start.y + 1;
        int remove_amount = end.y - start.y;
        for (int i = remove_start_idx; i < remove_amount; i++) {
            DynArray *line = dyn_array_get(&e->lines, i);
            dyn_array_release(line);
        }
        dyn_array_remove(&e->lines, remove_start_idx, remove_amount);
    } else {
        int remove_amount = end.x - start.x;
        dyn_array_remove(start_line, start.x, remove_amount);
    }
}

static void copy_editor_string(State *state, DynArray *string, Editor_Coord start, Editor_Coord end) {
    Editor *e = &state->editor;
    dyn_array_clear(string);
    const char new_line = '\n';
    if (start.y < end.y) {
        DynArray *start_line = dyn_array_get(&e->lines, start.y);
        if (start.x < (start_line->length - 1)) {
            dyn_array_insert(string, string->length, dyn_array_get(start_line, start.x), start_line->length - start.x);
        }
        dyn_array_push(string, &new_line);

        for (int i = start.y + 1; i < end.y; i++) {
            DynArray *middle_line = dyn_array_get(&e->lines, i);
            if (middle_line->length > 0) {
                dyn_array_insert(string, string->length, dyn_array_get(middle_line, 0), middle_line->length);
            }
            dyn_array_push(string, &new_line);
        }

        DynArray *end_line = dyn_array_get(&e->lines, end.y);
        if (end_line->length > 0 && end.x > 0) {
            dyn_array_insert(string, string->length, dyn_array_get(end_line, 0), end.x);
        }
    } else {
        DynArray *line = dyn_array_get(&e->lines, start.y);
        int insert_amount = end.x - start.x;
        dyn_array_insert(string, 0, dyn_array_get(line, start.x), insert_amount);
    }
}

static void snap_visual_vertical_offset_to_cursor(State *state) {
    Editor *e = &state->editor;
    if (e->cursor.y > (e->visual_vertical_offset + e->wrap_line_count - 1)) {
        e->visual_vertical_offset = e->cursor.y - e->wrap_line_count + 1;
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

static void set_cursor_y(State *state, int line_idx) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, line_idx);
    e->cursor.y = line_idx;
    e->selection_y = line_idx;
    e->cursor.x = (line->length > e->preferred_x)
        ? e->preferred_x
        : line->length;
    e->selection_x = e->cursor.x;
    e->cursor_anim_time = 0.0;
}

static void set_cursor_selection_x(State *state, int x) {
    state->editor.cursor.x = x;
    state->editor.cursor_anim_time = 0.0;
    state->editor.preferred_x = x;
    snap_visual_vertical_offset_to_cursor(state);
}

static void set_cursor_selection_y(State *state, int line_idx) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, line_idx);
    state->editor.cursor.y = line_idx;
    state->editor.cursor_anim_time = 0.0;
    e->cursor.x = (line->length > e->preferred_x)
        ? e->preferred_x
        : line->length;
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

    dyn_array_clear(&e->whatever_buffer);

    int ident_offset = 1;
    DynArray *cursor_line = dyn_array_get(&e->lines, e->cursor.y);
    while ((e->cursor.x - ident_offset) >= 0 && is_valid_in_identifier(dyn_char_get(cursor_line, e->cursor.x + ident_offset))) {
        ident_offset++;
    }
    ident_offset++;
    int ident_len = 0;
    while (is_valid_in_identifier(dyn_char_get(cursor_line, e->cursor.x + ident_offset))) {
        char c = dyn_char_get(cursor_line, e->cursor.x + ident_offset);
        dyn_array_push(&e->whatever_buffer, &c);
        ident_offset++;
        ident_len++;
    }
    int ident_line_idx = 0;
    int ident_char_idx = 0;
    int ident_idx = 0;
    bool ident_found = false;

    const char *define = "define";
    int define_len = 6;
    int define_idx = 0;
    bool define_found = false;

    for (int i = 0; i < e->lines.length; i++) {
        if (ident_found) {
            break;
        }

        DynArray *current_line = dyn_array_get(&e->lines, i);
        for (int j = 0; current_line->length; j++) {
            char current_char = dyn_char_get(current_line, j);
            if (!define_found) {
                bool define_char_matches = current_char == define[define_idx];
                if (define_char_matches) {
                    define_idx++;
                }
                if (define_idx == define_len) {
                    define_found = true;
                }
            }
            if (is_valid_in_identifier(current_char)) {
                bool ident_char_matches = current_char == dyn_char_get(&e->whatever_buffer, ident_idx);
                if (ident_char_matches) {
                    ident_idx++;
                    int next_idx = j + 1;
                    char next = dyn_char_get(current_line, next_idx);
                    if ((ident_idx == ident_len && !is_valid_in_identifier(next)) || next_idx >= current_line->length) {
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
    DynArray *cursor_line = dyn_array_get(&e->lines, e->cursor.y);
    if (e->cursor.x > 0) {
        set_cursor_x(state, e->cursor.x - 1);
        char char_to_be_deleted = dyn_char_get(cursor_line, e->cursor.x);
        Editor_Coord action_coord = e->cursor;
        register_undo(state, EDITOR_ACTION_DELETE_CHAR, action_coord, &char_to_be_deleted);
        delete_editor_char(state, e->cursor);
    } else if (e->cursor.y > 0) {
        int new_y = e->cursor.y - 1;
        DynArray *new_y_line = dyn_array_get(&e->lines, new_y);
        int new_x = new_y_line->length;
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

    DynArray *cursor_line = dyn_array_get(&e->lines, e->cursor.y);

    switch (key) {
    case KEY_RIGHT:
        if (ctrl &&
            e->cursor.y < (e->lines.length - 1) &&
            e->cursor.x == cursor_line->length
        ) {
            set_target_y(state, e->cursor.y + 1);
            set_target_x(state, 0);
        } else if (e->cursor.x < cursor_line->length) {
            if (ctrl) {
                bool found_whitespace = false;
                int i = e->cursor.x;
                while (true) {
                    char c = dyn_char_get(cursor_line, i);
                    if (i == cursor_line->length) {
                        set_target_x(state, i);
                        break;
                    }
                    if (!found_whitespace && isspace(c)) {
                        found_whitespace = true;
                    } else if (found_whitespace && !isspace(c)) {
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
            set_target_x(state, cursor_line->length);
        } else if (e->cursor.x > 0) {
            if (ctrl) {
                int i = e->cursor.x;
                while (i > 0 && dyn_char_get(cursor_line, i) != ' ') {
                    i--;
                }
                bool found_word = false;
                while (true) {
                    char c = dyn_char_get(cursor_line, i);
                    if (i == 0) {
                        set_target_x(state, 0);
                        break;
                    }
                    if (!found_word && c != ' ') {
                        found_word = true;
                    } else if (found_word && c == ' ') {
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
            int line_idx = e->cursor.y;
            if (e->cursor.y < e->lines.length - 1) {
                line_idx++;
            }
            set_target_y(state, line_idx);
            snap_visual_vertical_offset_to_cursor(state);
        }
        break;
    case KEY_UP:
        {
            int line_idx = e->cursor.y;
            if (e->cursor.y > 0) {
                line_idx--;
            }
            set_target_y(state, line_idx);
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
    Editor *e = &state->editor;
    DynArray *cursor_line = dyn_array_get(&e->lines, e->cursor.y);
    int x;
    for (x = 0; dyn_char_get(cursor_line, x) == ' '; x++);
    set_cursor_x(state, x);
}

static bool auto_click(State *state, KeyboardKey key) {
    Editor *e = &state->editor;

    if (IsKeyPressed(key)) {
        e->autoclick_key = key;
        e->autoclick_down_time = 0.0f;
        return true;
    } else if (IsKeyDown(key) && e->autoclick_key == (int)key) {
        e->autoclick_down_time += state->delta_time;
        if (e->autoclick_down_time > AUTOCLICK_UPPER_THRESHOLD) {
            e->autoclick_down_time = AUTOCLICK_LOWER_THRESHOLD;
            return true;
        }
    } else if (e->autoclick_key == (int)key) {
        e->autoclick_key = KEY_NULL;
        e->autoclick_down_time = 0.0f;
    }
    return false;
}

#endif
