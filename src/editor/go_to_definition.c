#include "../main.h"
#include "./editor_utils.c"

static void go_to_definition(State *state) {
    Editor *e = &state->editor;

    dyn_array_clear(&e->whatever_buffer);

    int ident_start = e->cursor.x;
    DynArray *cursor_line = dyn_array_get(&e->lines, e->cursor.y);
    while (ident_start > 0 && is_valid_in_identifier(dyn_char_get(cursor_line, ident_start - 1))) {
        ident_start--;
    }
    int ident_len = 0;
    while (is_valid_in_identifier(dyn_char_get(cursor_line, ident_start + ident_len))) {
        char c = dyn_char_get(cursor_line, ident_start + ident_len);
        dyn_array_push(&e->whatever_buffer, &c);
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
        for (int j = 0; j < current_line->length; j++) {
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

