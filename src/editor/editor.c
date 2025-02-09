#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "../main.h"
#include "editor_utils.c"
#include "clipboard.c"
#include "undo.c"
#include "file_explorer.c"
#include "finder.c"
#include "go_to_line.c"
#include "editor_renderer.c"
#include "console.c"

void editor_init(State *state, char *filename) {
    Editor *e = &state->editor;
    e->font = LoadFont("Consolas.ttf");
    e->line_count = 1;
    e->size_multiplier = 1.0f;
    e->autoclick_key = KEY_NULL;
    e->finder_match_idx = -1;
    e->console_highlight_idx = -1;
    editor_load_file(state, filename);
}

Big_State editor_input(State *state) {
    Editor *e = &state->editor;

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (ctrl) {
        const float min_size = 0.5f;
        const float max_size = 2.0f;
        const float incr = 0.1f;

        if (IsKeyPressed(KEY_KP_ADD)) {
            e->size_multiplier += incr;
            if (e->size_multiplier > max_size) {
                e->size_multiplier = max_size;
            }
            resize_window(state, e->size_multiplier);
        } else if (IsKeyPressed(KEY_KP_SUBTRACT)) {
            e->size_multiplier -= incr;
            if (e->size_multiplier < min_size) {
                e->size_multiplier = min_size;
            }
            resize_window(state, e->size_multiplier);
        }
    }

    switch (state->state) {
    default:
        break;
    case STATE_EDITOR: {
        if (ctrl && IsKeyPressed(KEY_O)) {
            update_filename_buffer(state);
            e->console_highlight_idx = 1;
            return STATE_EDITOR_FILE_EXPLORER;
        }
        if (ctrl && IsKeyPressed(KEY_S)) {
            char buffer[EDITOR_FILENAME_MAX_LENGTH];
            if (editor_save_file(state)) {
                sprintf(buffer, "\"%s\" saved", e->current_file);
                console_set_text(state, buffer);
                return STATE_EDITOR_SAVE_FILE;
            } else {
                sprintf(buffer, "Saving failed for file:\n\"%s\"", e->current_file);
                console_set_text(state, buffer);
                return STATE_EDITOR_SAVE_FILE_ERROR;
            }
        }
        if (ctrl && IsKeyPressed(KEY_P)) {
            return STATE_TRY_COMPILE;
        }
        if (ctrl && auto_click(state, KEY_Z)) {
            undo(state);
            break;
        }
        if (ctrl && IsKeyPressed(KEY_F)) {
            console_set_text(state, "");
            return STATE_EDITOR_FIND_TEXT;
        }
        if (ctrl && IsKeyPressed(KEY_G)) {
            console_set_text(state, "");
            return STATE_EDITOR_GO_TO_LINE;
        }
        if (ctrl && IsKeyPressed(KEY_D)) {
            go_to_definition(state);
            break;
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
                break;
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
            break;
        }
        if (ctrl && IsKeyPressed(KEY_C)) {
            copy_to_clipboard(state);
            break;
        }
        if (ctrl && IsKeyPressed(KEY_X)) {
            cut_to_clipboard(state);
            break;
        }
        if (ctrl && IsKeyPressed(KEY_V)) {
            paste_clipboard(state);
            break;
        }
        for (KeyboardKey key = 32; key < 127; key++) {
            if (IsKeyPressed(key) && e->cursor.x < EDITOR_LINE_MAX_LENGTH - 2 && e->cursor.x < EDITOR_LINE_MAX_LENGTH - 1) {
                char c = keyboard_key_to_char(state, key, shift);
                cursor_add_char(state, c);
                break;
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
                break;
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
            break;
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
    } break;
    case STATE_EDITOR_SAVE_FILE:
    case STATE_EDITOR_SAVE_FILE_ERROR: {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            return STATE_EDITOR;
        }
    } break;
    case STATE_EDITOR_FILE_EXPLORER: return file_explorer(state, shift);
    case STATE_EDITOR_FIND_TEXT: return find_text(state, shift);
    case STATE_EDITOR_GO_TO_LINE: return go_to_line(state);
    case STATE_COMPILATION_ERROR: {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            return STATE_EDITOR;
        }
    } break;
    case STATE_PLAY: {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            return STATE_INTERRUPT;
        }
    } break;
    }

    return state->state;
}

void editor_error_display(State *state, char *error_message) {
    console_set_text(state, error_message);
}

void editor_render(State *state) {
    switch (state->state) {
    default: {
    } break;
    case STATE_EDITOR: {
        editor_render_state_write(state);
    } break;
    case STATE_EDITOR_SAVE_FILE:
    case STATE_EDITOR_SAVE_FILE_ERROR:
    case STATE_EDITOR_FILE_EXPLORER:
    case STATE_EDITOR_FIND_TEXT:
    case STATE_EDITOR_GO_TO_LINE:
    case STATE_COMPILATION_ERROR: {
        editor_render_state_write(state);
        console_render(state);
    } break;
    case STATE_WAITING_TO_PLAY: {
        editor_render_state_wait_to_play(state);
    } break;
    case STATE_PLAY: {
        editor_render_state_play(state);
    } break;
    }
}

void editor_free(State *state) {
    Editor *e = &state->editor;
    UnloadFont(e->font);
    if (e->clipboard != NULL) {
        dyn_mem_release(e->clipboard);
    }
    for (int i = 0; i < EDITOR_UNDO_BUFFER_MAX; i++) {
        Editor_Action *action = &(e->undo_buffer[i]);
        bool is_type_delete_selection = action->type == EDITOR_ACTION_DELETE_STRING;
        bool is_allocated = action->string != NULL;
        if (is_type_delete_selection && is_allocated) {
            dyn_mem_release(action->string);
        }
    }
}