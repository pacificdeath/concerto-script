#include "../main.h"
#include "editor_utils.c"

static void copy_to_clipboard(State *state) {
    Editor *e = &state->editor;
    if (e->clipboard.data != NULL) {
        dyn_array_release(&e->clipboard);
    }
    dyn_array_alloc(&e->clipboard, sizeof(char));
    Editor_Selection_Data selection_data = get_cursor_selection_data(state);
    copy_editor_string(state, &e->clipboard, selection_data.start, selection_data.end);
}

static void cut_to_clipboard(State *state) {
    copy_to_clipboard(state);
    cursor_delete_selection(state);
}

static void paste_clipboard(State *state) {
    Editor *e = &state->editor;
    if (e->clipboard.data == NULL) {
        return;
    }
    cursor_delete_selection(state);
    Editor_Coord start_coord = e->cursor;
    Editor_Coord end_coord = add_editor_string(state, &e->clipboard, e->cursor);
    register_undo(state, EDITOR_ACTION_ADD_STRING, start_coord, &end_coord);
    set_cursor_y(state, end_coord.y);
    set_cursor_x(state, end_coord.x);
}
