#include "../main.h"
#include "editor_utils.c"

void finder_update_matches(State *state) {
    Editor *e = &state->editor;

    if (e->finder_buffer_length == 0) {
        e->finder_matches = 0;
    }
    int matches = 0;
    for (int i = 0; i < e->lines.length; i++) {
        DynArray *line = dyn_array_get(&e->lines, i);
        int s_idx = 0;
        for (int j = 0; dyn_char_get(line, j) != '\0'; j++) {
            if (dyn_char_get(line, j) == e->finder_buffer[s_idx]) {
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

Big_State find_text(State *state, bool shift) {
    Editor *e = &state->editor;

    if (IsKeyPressed(KEY_ESCAPE)) {
        e->finder_buffer[0] = '\0';
        e->finder_buffer_length = 0;
        e->finder_match_idx = -1;
        e->finder_matches = 0;
        return STATE_EDITOR;
    } else if (auto_click(state, KEY_BACKSPACE) && e->finder_buffer_length > 0) {
        e->finder_match_idx = -1;
        e->finder_buffer_length--;
        e->finder_buffer[e->finder_buffer_length] = '\0';
        finder_update_matches(state);
    } else if (IsKeyPressed(KEY_ENTER)) {
        if (e->finder_buffer_length == 0 || e->finder_matches == 0) {
            char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
            sprintf(buffer, "Find text:\n\"%s\"\n0 matches", e->finder_buffer);
            console_set_text(state, buffer);
            return state->state;
        }
        if (e->finder_match_idx == -1) {
            e->finder_match_idx = 0;
        } else if (shift) {
            e->finder_match_idx = (e->finder_match_idx + e->finder_matches - 1) % e->finder_matches;
        } else {
            e->finder_match_idx = (e->finder_match_idx + 1) % e->finder_matches;
        }
        int match_idx = 0;
        for (int i = 0; i < e->lines.length; i++) {
            DynArray *line = dyn_array_get(&e->lines, i);
            int s_idx = 0;
            for (int j = 0; j < line->length; j++) {
                if (dyn_char_get(line, j) == e->finder_buffer[s_idx]) {
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
                                e->finder_buffer,
                                e->finder_match_idx + 1,
                                e->finder_matches
                            );
                            console_set_text(state, buffer);
                            return state->state;
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
                return state->state;
            }
        }
    }

    if (e->finder_buffer_length == 0) {
        console_set_text(state, "Find text:");
    }

    return state->state;
}
