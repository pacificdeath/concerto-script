#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "main.h"

static bool is_alphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_numeric(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_uppercase(char c) {
    return (c >= 'A' && c <= 'Z');
}

char keyboard_key_to_char(State *state, KeyboardKey key, bool shift) {
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
    char cut_line[EDITOR_LINE_MAX_LENGTH];
    {
        int i;
        for (i = 0; e->lines[coord.y][coord.x + i] != '\0'; i++) {
            cut_line[i] = e->lines[coord.y][coord.x + i];
        }
        cut_line[i] = '\0';
    }
    e->lines[coord.y][coord.x] = '\0';
    for (int i = e->line_count - 1; i > (coord.y + 1); i--) {
        strcpy(e->lines[i], e->lines[i - 1]);
    }
    coord.y++;
    strcpy(e->lines[coord.y], cut_line);
}

static void delete_editor_line(State *state, int line_idx) {
    Editor *e = &state->editor;
    int line_len = TextLength(e->lines[line_idx]);
    int prev_line_len = TextLength(e->lines[line_idx - 1]);
    if ((line_len + prev_line_len) < EDITOR_LINE_MAX_LENGTH - 1) {
        strcat(e->lines[line_idx - 1], e->lines[line_idx]);
        for (int i = line_idx; i < e->line_count; i++) {
            strcpy(e->lines[i], e->lines[i + 1]);
        }
        e->line_count--;
    } else {
        //TODO
    }
}

static void add_editor_char(State *state, char character, Editor_Coord coord) {
    Editor *e = &state->editor;
    if (TextLength(e->lines[coord.y]) > EDITOR_LINE_MAX_LENGTH - 2) {
        return;
    }
    char next = e->lines[coord.y][coord.x];
    e->lines[coord.y][coord.x] = character;
    coord.x++;
    while (true) {
        char tmp = e->lines[coord.y][coord.x];
        e->lines[coord.y][coord.x] = next;
        next = tmp;
        if (e->lines[coord.y][coord.x] == '\0') {
            break;
        }
        coord.x++;
    }
    e->lines[coord.y][coord.x + 1] = '\0';
}

static void delete_editor_char(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    int i;
    for (i = coord.x; e->lines[coord.y][i] != '\0'; i++) {
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
    Editor *e = &state->editor;
    char buffer[2048];
    if (start.y < end.y) {
        e->lines[start.y][start.x] = '\0';
        int i;
        for (i = 0; i < strlen(e->lines[end.y]) - end.x; i++) {
            buffer[i] = e->lines[end.y][i + end.x];
        }
        buffer[i] = '\0';
        strcat(e->lines[start.y], buffer);
        int delete_amount = end.y - start.y;
        for (i = start.y + 1; i < e->line_count; i++) {
            int copy_line = i + delete_amount;
            if (copy_line >= e->line_count) {
                e->lines[i][0] = '\0';
            } else {
                strcpy(e->lines[i], e->lines[copy_line]);
            }
        }
        e->line_count -= delete_amount;
    } else {
        int i;
        for (i = 0; i < strlen(e->lines[start.y]) - end.x; i++) {
            buffer[i] = e->lines[start.y][i + end.x];
        }
        buffer[i] = '\0';
        e->lines[start.y][start.x] = '\0';
        strcat(e->lines[start.y], buffer);
    }
}

static char *copy_editor_string(State *state, Editor_Coord start, Editor_Coord end) {
    Editor *e = &state->editor;
    char *string = (char *)malloc(sizeof(char) * e->line_count * EDITOR_LINE_CAPACITY);
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
    if (e->cursor.y > (e->visual_vertical_offset + EDITOR_MAX_VISUAL_LINES - 1)) {
        e->visual_vertical_offset = e->cursor.y - EDITOR_MAX_VISUAL_LINES + 1;
    } else if (e->cursor.y < e->visual_vertical_offset) {
        e->visual_vertical_offset = e->cursor.y;
    }
}

static void center_visual_vertical_offset_around_cursor(State *state) {
    Editor *e = &state->editor;
    int middle_line = EDITOR_MAX_VISUAL_LINES / 2;
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

static void register_undo(State *state, Editor_Action_Type type, Editor_Coord coord, void *data) {
    Editor *e = &state->editor;

    Editor_Action *action = &(e->undo_buffer[e->undo_buffer_end]);

    bool wrap = e->undo_buffer_size == EDITOR_UNDO_BUFFER_MAX;
    if (wrap) {
        bool cleanup_needed = action->type == EDITOR_ACTION_DELETE_STRING;
        if (cleanup_needed) {
            free(action->string);
        }
    } else {
        e->undo_buffer_size++;
    }

    action->type = type;
    action->coord = coord;
    switch (type) {
    default: break;
    case EDITOR_ACTION_DELETE_CHAR: {
        action->character = *(char *)data;
    } break;
    case EDITOR_ACTION_ADD_STRING: {
        action->extra_coord = *(Editor_Coord *)data;
    } break;
    case EDITOR_ACTION_DELETE_STRING: {
        Editor_Selection_Data *selection_data = (Editor_Selection_Data *)data;
        action->string = copy_editor_string(state, selection_data->start, selection_data->end);
    } break;
    }

    e->undo_buffer_end = (e->undo_buffer_end + 1) % EDITOR_UNDO_BUFFER_MAX;
    if (e->undo_buffer_end == e->undo_buffer_start) {
        e->undo_buffer_start = e->undo_buffer_end;
    }
}

static void undo(State *state) {
    Editor *e = &state->editor;
    if (e->undo_buffer_size == 0) {
        return;
    }
    e->undo_buffer_end = (e->undo_buffer_end + EDITOR_UNDO_BUFFER_MAX - 1) % EDITOR_UNDO_BUFFER_MAX;
    e->undo_buffer_size--;
    Editor_Action *action = &(e->undo_buffer[e->undo_buffer_end]);
    switch (action->type) {
    case EDITOR_ACTION_ADD_LINE: {
        delete_editor_line(state, action->coord.y);
        action->coord.y--;
        action->coord.x = TextLength(e->lines[action->coord.y]);
        set_cursor_y(state, action->coord.y);
        set_cursor_x(state, action->coord.x);
    } break;
    case EDITOR_ACTION_ADD_CHAR: {
        char character = e->lines[action->coord.y][action->coord.x];
        delete_editor_char(state, action->coord);
        set_cursor_y(state, action->coord.y);
        set_cursor_x(state, action->coord.x);
    } break;
    case EDITOR_ACTION_DELETE_LINE: {
        add_editor_line(state, action->coord);
        set_cursor_y(state, action->coord.y + 1);
        set_cursor_x(state, 0);
    } break;
    case EDITOR_ACTION_DELETE_CHAR: {
        add_editor_char(state, action->character, action->coord);
        set_cursor_y(state, action->coord.y);
        set_cursor_x(state, action->coord.x + 1);
    } break;
    case EDITOR_ACTION_ADD_STRING: {
        Editor_Coord start = action->coord;
        Editor_Coord end = action->extra_coord;
        delete_editor_string(state, start, end);
        set_cursor_y(state, start.y);
        set_cursor_x(state, start.x);
    } break;
    case EDITOR_ACTION_DELETE_STRING: {
        Editor_Coord string_end = add_editor_string(state, action->string, action->coord);
        set_cursor_y(state, string_end.y);
        set_cursor_x(state, string_end.x);
        free(action->string);
    } break;
    }
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

void editor_init(State *state) {
    Editor *e = &state->editor;
    e->line_count = 1;
    e->autoclick_key = KEY_NULL;
    state->editor.finder_match_idx = -1;
}

void editor_free(State *state) {
    Editor *e = &state->editor;
    if (e->clipboard != NULL) {
        free(e->clipboard);
    }
    for (int i = 0; i < EDITOR_UNDO_BUFFER_MAX; i++) {
        Editor_Action *action = &(e->undo_buffer[i]);
        bool is_type_delete_selection = action->type == EDITOR_ACTION_DELETE_STRING;
        bool is_allocated = action->string != NULL;
        if (is_type_delete_selection && is_allocated) {
            free(action->string);
        }
    }
}

void editor_load_file(State *state, char *filename) {
    FILE *file;
    char line[EDITOR_LINE_MAX_LENGTH];
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
    }
    if (i == 0) {
        state->editor.lines[i][0] = '\0';
        state->editor.line_count = 1;
    } else {
        state->editor.line_count = i;
    }
    strcpy(state->editor.current_file, filename);
    fclose(file);
    set_cursor_y(state, 0);
    set_cursor_x(state, 0);
    snap_visual_vertical_offset_to_cursor(state);
}

bool editor_save_file(State *state) {
    FILE *file;
    file = fopen(state->editor.current_file, "w");
    if (file == NULL) {
        return false;
    }
    for (int i = 0; i < state->editor.line_count; i++) {
        int len = strlen(state->editor.lines[i]);
        state->editor.lines[i][len] = '\n';
        fwrite(state->editor.lines[i], sizeof(char), len, file);
        state->editor.lines[i][len] = '\0';
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
    return true;
}

static void editor_set_cursor_x_first_non_whitespace(State *state) {
    int x;
    for (x = 0; state->editor.lines[state->editor.cursor.y][x] == ' '; x++);
    set_cursor_x(state, x);
}

static void copy_to_clipboard(State *state) {
    Editor *e = &state->editor;
    if (e->clipboard != NULL) {
        free(e->clipboard);
        e->clipboard = NULL;
    }
    Editor_Selection_Data selection_data = get_cursor_selection_data(state);
    e->clipboard = copy_editor_string(state, selection_data.start, selection_data.end);
    e->clipboard_line_count = 1;
    for (int i = 0; e->clipboard[i] != '\0'; i++) {
        if (e->clipboard[i] == '\n') {
            e->clipboard_line_count++;
        }
    }
}

static void cut_to_clipboard(State *state) {
    copy_to_clipboard(state);
    cursor_delete_selection(state);
}

static void paste_clipboard(State *state) {
    Editor *e = &state->editor;
    if (e->clipboard == NULL) {
        return;
    }
    if ((e->line_count + e->clipboard_line_count) > EDITOR_LINE_CAPACITY) {
        return;
    }
    Editor_Selection_Data data = get_cursor_selection_data(state);
    cursor_delete_selection(state);
    Editor_Coord start_coord = e->cursor;
    Editor_Coord end_coord = add_editor_string(state, e->clipboard, e->cursor);
    register_undo(state, EDITOR_ACTION_ADD_STRING, start_coord, &end_coord);
    set_cursor_y(state, end_coord.y);
    set_cursor_x(state, end_coord.x);
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

void update_filename_buffer(State *state) {
    Editor *e = &state->editor;
    char filter[64];
    sprintf(filter, "programs\\%s*.*", e->file_search_buffer);
    int file_amount = list_files(filter, e->filename_buffer, EDITOR_FILENAMES_MAX_AMOUNT);
    char console_text[EDITOR_FILENAMES_MAX_AMOUNT * EDITOR_FILENAME_MAX_LENGTH];
    sprintf(console_text, "%s\n%s", e->file_search_buffer, e->filename_buffer);
    console_set_text(state, console_text);
    state->console_highlight_idx = 1;
}

void finder_update_matches(State *state) {
    Editor *e = &state->editor;

    if (e->finder_buffer_length == 0) {
        e->finder_matches = 0;
    }
    int matches = 0;
    for (int i = 0; i < e->line_count; i++) {
        int s_idx = 0;
        for (int j = 0; e->lines[i][j] != '\0'; j++) {
            if (e->lines[i][j] == e->finder_buffer[s_idx]) {
                s_idx++;
                if (s_idx == e->finder_buffer_length) {
                    matches++;
                    s_idx = 0;
                    break;
                }
            } else {
                s_idx = 0;
            }
        }
    }
    e->finder_matches = matches;

    if (matches > 0) {
        char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
        sprintf(buffer, "Find text:\n\"%s\"\nFound %i matches", state->editor.finder_buffer, state->editor.finder_matches);
        console_set_text(state, buffer);
    } else {
        char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
        sprintf(buffer, "Find text:\n\"%s\"", state->editor.finder_buffer);
        console_set_text(state, buffer);
    }
}

void editor_input(State *state) {
    Editor *e = &state->editor;

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    switch (state->state) {
    default: return;
    case STATE_EDITOR: {
        if (ctrl && IsKeyPressed(KEY_O)) {
            update_filename_buffer(state);
            console_set_text(state, "");
            state->console_highlight_idx = 1;
            state->state = STATE_EDITOR_FILE_EXPLORER;
            return;
        }
        if (ctrl && IsKeyPressed(KEY_S)) {
            char buffer[FILENAME_MAX];
            if (editor_save_file(state)) {
                sprintf(buffer, "\"%s\" saved", e->current_file);
                console_set_text(state, buffer);
                state->state = STATE_EDITOR_SAVE_FILE;
            } else {
                sprintf(buffer, "Saving failed for file:\n\"%s\"", e->current_file);
                console_set_text(state, buffer);
                state->state = STATE_EDITOR_SAVE_FILE_ERROR;
            }
            return;
        }
        if (ctrl && IsKeyPressed(KEY_P)) {
            state->state = STATE_TRY_COMPILE;
            return;
        }
        if (ctrl && auto_click(state, KEY_Z)) {
            undo(state);
            return;
        }
        if (ctrl && IsKeyPressed(KEY_F)) {
            console_set_text(state, "");
            state->state = STATE_EDITOR_FIND_TEXT;
            return;
        }
        if (ctrl && IsKeyPressed(KEY_G)) {
            console_set_text(state, "");
            state->state = STATE_EDITOR_GO_TO_LINE;
            return;
        }
        if (ctrl && IsKeyPressed(KEY_D)) {
            go_to_definition(state);
            return;
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
            if (mouse_pos.x < 0 ||
                mouse_pos.y < 0 ||
                mouse_pos.x > state->window_width ||
                mouse_pos.x > state->window_height) {
                return;
            }
            int line_height = editor_line_height(state);
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
                set_cursor_y(state, requested_line);
                set_cursor_x(state, requested_char);
            } else {
                set_cursor_selection_y(state, requested_line);
                set_cursor_selection_x(state, requested_char);
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
        for (KeyboardKey key = 32; key < 127; key++) {
            if (IsKeyPressed(key) && e->cursor.x < EDITOR_LINE_MAX_LENGTH - 2 && e->cursor.x < EDITOR_LINE_MAX_LENGTH - 1) {
                char c = keyboard_key_to_char(state, key, shift);
                cursor_add_char(state, c);
                return;
            } else {
                // TODO: this is currently a silent failure and it should not at all be that
            }
        }
        if (IsKeyPressed(KEY_TAB)) {
            bool selection_active = cursor_selection_active(state);
            int start_line;
            int end_line;
            if (e->cursor.y < e->selection_y) {
                start_line = e->cursor.y;
                end_line = e->selection_y;
            } else {
                start_line = e->selection_y;
                end_line = e->cursor.y;
            }
            bool bad = false;
            int spaces_len = 1 + (end_line - start_line);
            int spaces[spaces_len];
            for (int i = 0; i < spaces_len; i++) {
                if (selection_active || shift) {
                    set_cursor_y(state, start_line + i);
                    editor_set_cursor_x_first_non_whitespace(state);
                }
                int line_len = strlen(e->lines[start_line + i]);
                if (shift) {
                    if (e->cursor.x == 0) {
                        spaces[i] = 0;
                        continue;
                    }
                    for (
                        spaces[i] = 1;
                        spaces[i] < line_len - 1
                            && e->lines[start_line + i][spaces[i]] == ' '
                            && (e->cursor.x - spaces[i]) % 4 != 0;
                        spaces[i]++
                    );
                } else {
                    for (spaces[i] = 1; (e->cursor.x + spaces[i]) % 4 != 0; spaces[i]++) {
                        if (line_len + spaces[i] >= EDITOR_LINE_MAX_LENGTH - 2) {
                            bad = true;
                            break;
                        }
                    }
                }
            }
            if (bad) {
                return;
            }
            if (shift) {
                for (int i = 0; i < spaces_len; i++) {
                    int line_idx = start_line + i;
                    int line_len = strlen(e->lines[line_idx]);
                    for (int j = spaces[i]; j < line_len; j++) {
                        e->lines[line_idx][j - spaces[i]] = e->lines[line_idx][j];
                    }
                    e->lines[line_idx][line_len - spaces[i]] = '\0';
                    set_cursor_x(state, e->cursor.x - spaces[i]);
                }
            } else {
                if (selection_active) {
                    for (int i = 0; i < spaces_len; i++) {
                        set_cursor_y(state, start_line + i);
                        set_cursor_x(state, 0);
                        for (int j = 0; j < spaces[i]; j++) {
                            cursor_add_char(state, ' ');
                        }
                    }
                } else {
                    for (int j = 0; j < spaces[0]; j++) {
                        cursor_add_char(state, ' ');
                    }
                }
            }
            if (selection_active) {
                e->cursor.y = start_line;
                e->cursor.x = 0;
                e->selection_y = end_line;
                e->selection_x = strlen(e->lines[end_line]);
            }
            return;
        }
        int auto_clickable_keys_amount = 6;
        KeyboardKey auto_clickable_keys[6] = {
            KEY_RIGHT,
            KEY_LEFT,
            KEY_DOWN,
            KEY_UP,
            KEY_BACKSPACE,
            KEY_ENTER
        };
        for (int i = 0; i < auto_clickable_keys_amount; i += 1) {
            int key = auto_clickable_keys[i];
            if (auto_click(state, key)) {
                trigger_key(state, key, ctrl, shift);
            }
        }
    } return;
    case STATE_EDITOR_SAVE_FILE:
    case STATE_EDITOR_SAVE_FILE_ERROR: {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            state->state = STATE_EDITOR;
        }
    } break;
    case STATE_EDITOR_FILE_EXPLORER: {
        if (IsKeyPressed(KEY_ESCAPE)) {
            e->file_search_buffer[0] = '\0';
            e->file_search_buffer_length = 0;
            e->file_cursor = 0;
            state->console_highlight_idx = -1;
            state->state = STATE_EDITOR;
        } else if (IsKeyPressed(KEY_DOWN)) {
            if (state->console_highlight_idx < (state->console_line_count - 1)) {
                state->console_highlight_idx++;
            }
        } else if (IsKeyPressed(KEY_UP)) {
            if (state->console_highlight_idx > 1) {
                state->console_highlight_idx--;
            }
        } else if (auto_click(state, KEY_BACKSPACE) && e->file_search_buffer_length > 0) {
            e->file_search_buffer_length--;
            e->file_search_buffer[e->file_search_buffer_length] = '\0';
            update_filename_buffer(state);
            return;
        } else if (IsKeyPressed(KEY_ENTER)) {
            char filename[128];
            console_get_highlighted_text(state, filename);
            char filepath[128];
            sprintf(filepath, "programs\\%s", filename);
            editor_load_file(state, filepath);
            e->file_search_buffer[0] = '\0';
            e->file_search_buffer_length = 0;
            e->file_cursor = 0;
            state->console_highlight_idx = -1;
            state->state = STATE_EDITOR;
        } else {
            for (KeyboardKey key = 32; key < 127; key++) {
                if (IsKeyPressed(key) && e->file_search_buffer_length < EDITOR_FILE_SEARCH_BUFFER_MAX - 1) {
                    char c = keyboard_key_to_char(state, key, shift);
                    e->file_search_buffer[e->file_search_buffer_length] = c;
                    e->file_search_buffer_length++;
                    e->file_search_buffer[e->file_search_buffer_length] = '\0';
                    update_filename_buffer(state);
                    return;
                }
            }
        }
    } return;
    case STATE_EDITOR_FIND_TEXT: {
        if (IsKeyPressed(KEY_ESCAPE)) {
            e->finder_buffer[0] = '\0';
            e->finder_buffer_length = 0;
            e->finder_match_idx = -1;
            e->finder_matches = 0;
            state->state = STATE_EDITOR;
        } else if (auto_click(state, KEY_BACKSPACE) && e->finder_buffer_length > 0) {
            e->finder_match_idx = -1;
            e->finder_buffer_length--;
            e->finder_buffer[e->finder_buffer_length] = '\0';
            finder_update_matches(state);
        } else if (IsKeyPressed(KEY_ENTER)) {
            if (e->finder_buffer_length == 0 || e->finder_matches == 0) {
                char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
                sprintf(buffer, "Find text:\n\"%s\"\n0 matches", state->editor.finder_buffer);
                console_set_text(state, buffer);
                return;
            }
            if (e->finder_match_idx == -1) {
                e->finder_match_idx = 0;
            } else if (shift) {
                e->finder_match_idx = (e->finder_match_idx + e->finder_matches - 1) % e->finder_matches;
            } else {
                e->finder_match_idx = (e->finder_match_idx + 1) % e->finder_matches;
            }
            int match_idx = 0;
            for (int i = 0; i < e->line_count; i++) {
                int s_idx = 0;
                for (int j = 0; e->lines[i][j] != '\0'; j++) {
                    if (e->lines[i][j] == e->finder_buffer[s_idx]) {
                        s_idx++;
                        if (s_idx == e->finder_buffer_length) {
                            if (match_idx == e->finder_match_idx) {
                                set_cursor_y(state, i);
                                int match_end = j + 1;
                                set_cursor_x(state, match_end - e->finder_buffer_length);
                                set_cursor_selection_x(state, match_end);
                                center_visual_vertical_offset_around_cursor(state);
                                char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
                                sprintf(
                                    buffer,
                                    "Find text:\n\"%s\"\nMatch %i of %i",
                                    state->editor.finder_buffer,
                                    state->editor.finder_match_idx + 1,
                                    state->editor.finder_matches
                                );
                                console_set_text(state, buffer);
                                return;
                            }
                            match_idx++;
                        }
                    } else {
                        s_idx = 0;
                    }
                }
            }
        } else {
            for (KeyboardKey key = 32; key < 127; key++) {
                if (IsKeyPressed(key) && e->finder_buffer_length < EDITOR_FINDER_BUFFER_MAX - 1) {
                    e->finder_match_idx = -1;
                    char c = keyboard_key_to_char(state, key, shift);
                    e->finder_buffer[e->finder_buffer_length] = c;
                    e->finder_buffer_length++;
                    e->finder_buffer[e->finder_buffer_length] = '\0';
                    finder_update_matches(state);
                    return;
                }
            }
        }
        if (e->finder_buffer_length == 0) {
            console_set_text(state, "Find text:");
        }
    } return;
    case STATE_EDITOR_GO_TO_LINE: {
        if (IsKeyPressed(KEY_ESCAPE)) {
            e->go_to_line_buffer[0] = '\0';
            e->go_to_line_buffer_length = 0;
            state->state = STATE_EDITOR;
            return;
        } else if (auto_click(state, KEY_BACKSPACE) && e->go_to_line_buffer_length > 0) {
            e->go_to_line_buffer_length--;
            e->go_to_line_buffer[e->go_to_line_buffer_length] = '\0';
        } else if (IsKeyPressed(KEY_ENTER)) {
            if (e->go_to_line_buffer_length == 0) {
                return;
            }
            int line = TextToInteger(e->go_to_line_buffer) - 1;
            if (line >= e->line_count) {
                line = e->line_count - 1;
            } else if (line < 0) {
                line = 0;
            }
            set_cursor_y(state, line);
            set_cursor_x(state, 0);
            center_visual_vertical_offset_around_cursor(state);
            state->state = STATE_EDITOR;
            e->go_to_line_buffer[0] = '\0';
            e->go_to_line_buffer_length = 0;
        } else {
            for (KeyboardKey key = KEY_ZERO; key <= KEY_NINE; key++) {
                if (IsKeyPressed(key) && e->go_to_line_buffer_length < (EDITOR_GO_TO_LINE_BUFFER_MAX - 1)) {
                    e->go_to_line_buffer[e->go_to_line_buffer_length] = key;
                    e->go_to_line_buffer_length++;
                    e->go_to_line_buffer[e->go_to_line_buffer_length] = '\0';
                    break;
                }
            }
        }
        char buffer[64 + EDITOR_GO_TO_LINE_BUFFER_MAX];
        sprintf(buffer, "Go to line:\n[%s]", state->editor.go_to_line_buffer);
        console_set_text(state, buffer);
    } return;
    case STATE_COMPILATION_ERROR: {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            state->state = STATE_EDITOR;
        }
    } return;
    case STATE_PLAY: {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            state->state = STATE_INTERRUPT;
        }
    } return;
    }
}

static void render_cursor(Editor_Cursor_Render_Data data) {
    Vector2 start = {
        .x = data.line_number_offset + (data.x * data.char_width),
        .y = ((data.y - data.visual_vertical_offset) * data.line_height)
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

static Token_Type try_get_play_or_wait_token_type_at_coord(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    char *line = e->lines[coord.y];
    int char_idx = coord.x;

    Token_Type token_type = TOKEN_NONE;

    if (
        line[char_idx] == 'p' &&
        line[char_idx + 1] == 'l' &&
        line[char_idx + 2] == 'a' &&
        line[char_idx + 3] == 'y'
    ) {
        token_type = TOKEN_PLAY;
    }

    if (token_type == TOKEN_NONE &&
        line[char_idx] == 'w' &&
        line[char_idx + 1] == 'a' &&
        line[char_idx + 2] == 'i' &&
        line[char_idx + 3] == 't'
    ) {
        token_type = TOKEN_WAIT;
    }

    if (token_type == TOKEN_NONE) {
        return TOKEN_NONE;
    }

    char_idx += 4;

    switch (line[char_idx]) {
    default: break;
    case '1': {
        if (line[char_idx + 1] == '6') char_idx += 2; else char_idx++; break;
    } break;
    case '2':
    case '4':
    case '8': char_idx++; break;
    case '3': if (line[char_idx + 1] == '2') char_idx += 2; else return TOKEN_NONE; break;
    case '6': if (line[char_idx + 1] == '4') char_idx += 2; else return TOKEN_NONE; break;
    }

    switch (line[char_idx]) {
    default: {
        if (is_valid_in_identifier(line[char_idx])) {
            return false;
        }
    } break;
    case 'd': {
        if (
            line[char_idx + 1] != 'o' ||
            line[char_idx + 2] != 't'
        ) {
            return false;
        }
        char_idx += 3;
        if (is_numeric(line[char_idx])) {
            char_idx++;
        } else if (is_valid_in_identifier(line[char_idx])) {
            return false;
        }
    } break;
    case 't': {
        if (
            line[char_idx + 1] != 'r' ||
            line[char_idx + 2] != 'i' ||
            line[char_idx + 3] != 'p' ||
            line[char_idx + 4] != 'l' ||
            line[char_idx + 5] != 'e' ||
            line[char_idx + 6] != 't'
        ) {
            return false;
        }
        char_idx += 7;
    } break;
    }

    if (is_valid_in_identifier(line[char_idx])) {
        return TOKEN_NONE;
    }

    return token_type;
}

static bool is_note_at_coord(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    char c = e->lines[coord.y][coord.x];

    bool first_char_is_valid = 
        (c >= 'a' && c <= 'g') ||
        (c >= 'A' && c <= 'G');

    if (!first_char_is_valid) {
        return false;
    }

    bool has_explicit_octave = false;
    for (int offset = 1; offset < 4; offset++) {
        char offset_char = e->lines[coord.y][coord.x + offset];
        switch (offset_char) {
        case '#':
        case 'b':
        case 'B': {
            // accidental must as of now precede explicit octave
            if (has_explicit_octave) {
                return false;
            }
        } break;
        default: {
            if (offset_char >= '0' && offset_char <= '8') {
                has_explicit_octave = true;
            } else if (is_alphabetic(offset_char)) {
                return false;
            }
        } break;
        case ' ':
        case '\0': {
            return true;
        }
        }
    }

    return true;
}

static void editor_render_base(State *state, float line_height, float char_width, int line_number_padding) {
    Editor *e = &state->editor;
    char line_number_str[EDITOR_LINE_NUMBER_PADDING];

    char current_word[EDITOR_LINE_MAX_LENGTH];

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

            if (found_word) {
                switch (c) {
                default: break;
                case '(':
                case ')':
                case ' ':
                case '\0':
                {
                    found_word = false;
                    color = EDITOR_NORMAL_COLOR;
                } break;
                }
            }

            if (!rest_is_comment && !found_word) {
                if (is_alphabetic(c)) {
                    found_word = true;

                    Editor_Coord coord = { .y = line_idx, .x = j };

                    Token_Type play_or_wait = try_get_play_or_wait_token_type_at_coord(state, coord);
                    if (play_or_wait != TOKEN_NONE) {
                        if (play_or_wait == TOKEN_PLAY) {
                            color = EDITOR_PLAY_COLOR;
                        } else {
                            color = EDITOR_WAIT_COLOR;
                        }
                    } else if (is_note_at_coord(state, coord)) {
                        color = EDITOR_NOTE_COLOR;
                    } else {
                        int char_offset;
                        for (
                            char_offset = 0;
                            !isspace(e->lines[line_idx][j + char_offset]) &&
                            is_valid_in_identifier(e->lines[line_idx][j + char_offset]);
                            char_offset += 1
                        ) {
                            current_word[char_offset] = e->lines[line_idx][j + char_offset];
                        }
                        current_word[char_offset] = '\0';

                        if (strcmp(current_word, "play") == 0) {
                            color = EDITOR_PLAY_COLOR;
                        } else if (strcmp(current_word, "wait") == 0) {
                            color = EDITOR_WAIT_COLOR;
                        } else if (
                            strcmp(current_word, "start") == 0 ||
                            strcmp(current_word, "define") == 0 ||
                            strcmp(current_word, "repeat") == 0 ||
                            strcmp(current_word, "rounds") == 0 ||
                            strcmp(current_word, "semi") == 0 ||
                            strcmp(current_word, "bpm") == 0 ||
                            strcmp(current_word, "scale") == 0 ||
                            strcmp(current_word, "rise") == 0 ||
                            strcmp(current_word, "fall") == 0) {
                            color = EDITOR_KEYWORD_COLOR;
                        } else {
                            color = EDITOR_NORMAL_COLOR;
                        }
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

    int line_height = editor_line_height(state);
    int char_width = editor_char_width(line_height);

    int line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (cursor_selection_active(state)) {
        Editor_Selection_Data selection_data = get_cursor_selection_data(state);
        Editor_Selection_Render_Data selection_render_data = {
            .line = selection_data.start.y,
            .line_number_offset = line_number_padding,
            .visual_vertical_offset = e->visual_vertical_offset,
            .start_x = selection_data.start.x,
            .end_x = 0, // might differ in first selection render
            .line_height = line_height,
            .char_width = char_width
        };
        if (selection_data.start.y == selection_data.end.y) {
            selection_render_data.start_x = selection_data.start.x;
            selection_render_data.end_x = selection_data.end.x;
            render_selection(selection_render_data);
        } else {
            selection_render_data.start_x = selection_data.start.x;
            selection_render_data.end_x = strlen(e->lines[selection_render_data.line]);
            render_selection(selection_render_data);

            selection_render_data.start_x = 0;
            for (int i = selection_data.start.y + 1; i < selection_data.end.y; i++) {
                selection_render_data.line = i;
                selection_render_data.end_x = strlen(e->lines[i]);
                render_selection(selection_render_data);
            }

            selection_render_data.line = selection_data.end.y;
            selection_render_data.start_x = 0;
            selection_render_data.end_x = selection_data.end.x;
            render_selection(selection_render_data);
        }
    }
    e->cursor_anim_time = e->cursor_anim_time + (state->delta_time * EDITOR_CURSOR_ANIMATION_SPEED);
    float pi2 = PI * 2.0f;
    if (e->cursor_anim_time > pi2) {
        e->cursor_anim_time -= pi2;
    }
    float alpha = (cos(e->cursor_anim_time) + 1.0) * 127.5f;
    Editor_Cursor_Render_Data cursor_data = {
        .y = e->cursor.y,
        .line_number_offset = line_number_padding,
        .visual_vertical_offset = e->visual_vertical_offset,
        .x = e->cursor.x,
        .line_height = line_height,
        .char_width = char_width,
        .alpha = alpha
    };
    render_cursor(cursor_data);
}

void editor_render_state_wait_to_play(State *state) {
    int line_height = editor_line_height(state);
    int char_width = editor_char_width(line_height);

    int line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    editor_render_base(state, line_height, char_width, line_number_padding);
}

void editor_render_state_play(State *state) {
    Editor *e = &state->editor;

    int line_height = editor_line_height(state);
    int char_width = editor_char_width(line_height);

    int line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (state->current_sound == NULL) {
        return;
    }

    int middle_line = EDITOR_MAX_VISUAL_LINES / 2;

    float rec_line_size = 5.0f;
    Rectangle rec;

    Tone *tone = &state->current_sound->tone;

    rec.x = -(rec_line_size) + line_number_padding + (tone->char_idx * char_width);

    set_cursor_x(state, tone->char_idx);
    if (tone->line_idx < middle_line) {
        set_cursor_y(state, tone->line_idx);

        e->visual_vertical_offset = 0;
        rec.y = -(rec_line_size) + (tone->line_idx * line_height);
    } else {
        set_cursor_y(state, e->visual_vertical_offset + middle_line);

        e->visual_vertical_offset = tone->line_idx - middle_line;
        rec.y = -(rec_line_size) + (middle_line * line_height);
    }

    rec.width = (2 * rec_line_size) + char_width * tone->char_count;

    rec.height = (2 * rec_line_size) + line_height;

    Color color = { .g = 0, .a = 255 };
    if (tone->idx % 2 == 0) {
        color.r = 255;
        color.b = 0;
    } else {
        color.r = 0;
        color.b = 255;
    }
    DrawRectangleLinesEx(rec, 5, color);
}

