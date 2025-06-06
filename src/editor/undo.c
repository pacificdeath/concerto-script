#include "../main.h"
#include "editor_utils.c"

static void register_undo(State *state, Editor_Action_Type type, Editor_Coord coord, void *data) {
    Editor *e = &state->editor;

    Editor_Action *action = &(e->undo_buffer[e->undo_buffer_end]);

    bool wrap = e->undo_buffer_size == EDITOR_UNDO_BUFFER_MAX;
    if (wrap) {
        bool cleanup_needed = action->type == EDITOR_ACTION_DELETE_STRING;
        if (cleanup_needed) {
            dyn_array_release(&action->data.string);
        }
    } else {
        e->undo_buffer_size++;
    }

    action->type = type;
    action->coord = coord;
    switch (type) {
    default: break;
    case EDITOR_ACTION_DELETE_CHAR: {
        action->data.character = *(char *)data;
    } break;
    case EDITOR_ACTION_ADD_STRING: {
        action->data.extra_coord = *(Editor_Coord *)data;
    } break;
    case EDITOR_ACTION_DELETE_STRING: {
        dyn_array_alloc(&action->data.string, sizeof(char));
        Editor_Selection_Data *selection_data = (Editor_Selection_Data *)data;
        copy_editor_string(state, &action->data.string, selection_data->start, selection_data->end);
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
    case EDITOR_ACTION_ADD_CHAR: {
        delete_editor_char(state, action->coord);
        set_cursor_y(state, action->coord.y);
        set_cursor_x(state, action->coord.x);
    } break;
    case EDITOR_ACTION_DELETE_CHAR: {
        add_editor_char(state, action->data.character, action->coord);
        set_cursor_y(state, action->coord.y);
        set_cursor_x(state, action->coord.x + 1);
    } break;
    case EDITOR_ACTION_ADD_LINE: {
        int prev_line_idx = (action->coord.y - 1);
        DynArray *prev_line = dyn_array_get(&e->lines, prev_line_idx);
        delete_editor_line(state, action->coord.y);
        set_cursor_y(state, prev_line_idx);
        set_cursor_x(state, prev_line->length);
    } break;
    case EDITOR_ACTION_DELETE_LINE: {
        add_editor_line(state, action->coord);
        set_cursor_y(state, action->coord.y + 1);
        set_cursor_x(state, 0);
    } break;
    case EDITOR_ACTION_ADD_STRING: {
        Editor_Coord start = action->coord;
        Editor_Coord end = action->data.extra_coord;
        delete_editor_string(state, start, end);
        set_cursor_y(state, start.y);
        set_cursor_x(state, start.x);
    } break;
    case EDITOR_ACTION_DELETE_STRING: {
        Editor_Coord string_end = add_editor_string(state, &action->data.string, action->coord);
        set_cursor_y(state, string_end.y);
        set_cursor_x(state, string_end.x);
        dyn_array_release(&action->data.string);
    } break;
    }
}
