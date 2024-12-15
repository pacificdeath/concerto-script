#include "../main.h"
#include "editor_utils.c"

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
