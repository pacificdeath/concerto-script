#include "../main.h"
#include "editor_utils.c"

Big_State go_to_line(State *state) {
    Editor *e = &state->editor;

    if (IsKeyPressed(KEY_ESCAPE)) {
        e->go_to_line_buffer[0] = '\0';
        e->go_to_line_buffer_length = 0;
        return STATE_EDITOR;
    } else if (auto_click(state, KEY_BACKSPACE) && e->go_to_line_buffer_length > 0) {
        e->go_to_line_buffer_length--;
        e->go_to_line_buffer[e->go_to_line_buffer_length] = '\0';
    } else if (IsKeyPressed(KEY_ENTER)) {
        if (e->go_to_line_buffer_length == 0) {
            return state->state;
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
        e->go_to_line_buffer[0] = '\0';
        e->go_to_line_buffer_length = 0;
        return STATE_EDITOR;
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
    sprintf(buffer, "Go to line:\n[%s]", e->go_to_line_buffer);
    console_set_text(state, buffer);

    return state->state;
}