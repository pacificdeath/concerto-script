#include "../main.h"
#include "editor_utils.c"

static void render_cursor(Editor_Cursor_Render_Data data, Color color) {
    Vector2 start = {
        .x = data.line_number_offset + (data.x * data.char_width),
        .y = (data.y * data.line_height)
    };
    Vector2 end = {
        .x = start.x,
        .y = start.y + data.line_height
    };
    color.a = data.alpha;
    DrawLineEx(start, end, EDITOR_CURSOR_WIDTH, color);
}

static void render_cursor_line(State *state) {
    Editor *e = &state->editor;

    Editor_Coord wrapped_coord = editor_wrapped_coord(state, e->cursor);
    int line_idx = e->cursor.y - e->visual_vertical_offset;

    ASSERT(line_idx >= 0);
    ASSERT(line_idx < e->wrap_line_count);

    int base_wrapped_y = e->wrap_lines[line_idx].visual_idx;
    int wrap_amount = e->wrap_lines[line_idx].wrap_amount;

    Rectangle rec = {
        .x = 0,
        .y = base_wrapped_y * editor_line_height(state),
        .width = GetScreenWidth(),
        .height = (1 + wrap_amount) * editor_line_height(state),
    };

    DrawRectangleRec(rec, e->theme.cursor_line);
}

static void render_selection(State *state, Editor_Selection_Render_Data data) {
    Editor *e = &state->editor;

    Editor_Coord wrapped_selection_start = editor_wrapped_coord(state, (Editor_Coord){ data.line, data.start_x });
    Editor_Coord wrapped_selection_end = editor_wrapped_coord(state, (Editor_Coord){ data.line, data.end_x });

    for (int i = wrapped_selection_start.y; i <= wrapped_selection_end.y; i++) {

        float start = (i == wrapped_selection_start.y) ? wrapped_selection_start.x : 0;
        float end = (i == wrapped_selection_end.y) ? wrapped_selection_end.x : e->wrap_idx;

        Rectangle rec = {
            .x = data.line_number_offset + (start * data.char_width),
            .y = i * data.line_height,
            .width = (end - start) * data.char_width,
            .height = data.line_height,
        };

        DrawRectangleRec(rec, e->theme.selection);
    }
}

static Token_Type try_get_play_or_wait_token_type_at_coord(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, coord.y);
    int char_idx = coord.x;

    Token_Type token_type = TOKEN_NONE;

    if (
        dyn_char_get(line, char_idx) == 'p' &&
        dyn_char_get(line, char_idx + 1) == 'l' &&
        dyn_char_get(line, char_idx + 2) == 'a' &&
        dyn_char_get(line, char_idx + 3) == 'y'
    ) {
        token_type = TOKEN_PLAY;
    }

    if (token_type == TOKEN_NONE &&
        dyn_char_get(line, char_idx) == 'w' &&
        dyn_char_get(line, char_idx + 1) == 'a' &&
        dyn_char_get(line, char_idx + 2) == 'i' &&
        dyn_char_get(line, char_idx + 3) == 't'
    ) {
        token_type = TOKEN_WAIT;
    }

    if (token_type == TOKEN_NONE) {
        return TOKEN_NONE;
    }

    char_idx += 4;

    switch (dyn_char_get(line, char_idx)) {
    default: break;
    case '1': {
        if (dyn_char_get(line, char_idx + 1) == '6') char_idx += 2; else char_idx++; break;
    } break;
    case '2':
    case '4':
    case '8': char_idx++; break;
    case '3': if (dyn_char_get(line, char_idx + 1) == '2') char_idx += 2; else return TOKEN_NONE; break;
    case '6': if (dyn_char_get(line, char_idx + 1) == '4') char_idx += 2; else return TOKEN_NONE; break;
    }

    switch (dyn_char_get(line, char_idx)) {
    default: {
        if (is_valid_in_identifier(dyn_char_get(line, char_idx))) {
            return false;
        }
    } break;
    case 'd': {
        if (
            dyn_char_get(line, char_idx + 1) != 'o' ||
            dyn_char_get(line, char_idx + 2) != 't'
        ) {
            return false;
        }
        char_idx += 3;
        if (is_numeric(dyn_char_get(line, char_idx))) {
            char_idx++;
        } else if (is_valid_in_identifier(dyn_char_get(line, char_idx))) {
            return false;
        }
    } break;
    case 't': {
        if (
            dyn_char_get(line, char_idx + 1) != 'r' ||
            dyn_char_get(line, char_idx + 2) != 'i' ||
            dyn_char_get(line, char_idx + 3) != 'p' ||
            dyn_char_get(line, char_idx + 4) != 'l' ||
            dyn_char_get(line, char_idx + 5) != 'e' ||
            dyn_char_get(line, char_idx + 6) != 't'
        ) {
            return false;
        }
        char_idx += 7;
    } break;
    }

    if (is_valid_in_identifier(dyn_char_get(line, char_idx))) {
        return TOKEN_NONE;
    }

    return token_type;
}

static bool is_note_at_coord(State *state, Editor_Coord coord) {
    Editor *e = &state->editor;
    DynArray *line = dyn_array_get(&e->lines, coord.y);
    char c = dyn_char_get(line, coord.x);

    bool first_char_is_valid =
        (c >= 'a' && c <= 'g') ||
        (c >= 'A' && c <= 'G');

    if (!first_char_is_valid) {
        return false;
    }

    bool has_explicit_octave = false;
    for (int offset = 1; offset < 4; offset++) {
        int offset_char_idx = coord.x + offset;
        if (offset_char_idx >= line->length) {
            return true;
        }
        char offset_char = dyn_char_get(line, offset_char_idx);
        switch (offset_char) {
        case COMMENT_CHAR: return true;
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
            } else if (is_alphabetic(offset_char) || offset_char == '_') {
                return false;
            }
        } break;
        case ' ': {
            return true;
        }
        }
    }

    return true;
}

static void editor_render_base(State *state, float line_height, float char_width, int line_number_padding) {
    Editor *e = &state->editor;

    int window_line_count = editor_window_line_count(state);
    int line_max_chars = editor_line_max_chars(state);

    for (int i = 0; i < e->wrap_line_count; i++) {
        int line_idx = e->wrap_lines[i].logical_idx;
        int visual_idx = e->wrap_lines[i].visual_idx;
        int wrap_amount = e->wrap_lines[i].wrap_amount;

        if (line_idx >= e->lines.length) {
            break;
        }

        DynArray *line = dyn_array_get(&e->lines, line_idx);

        bool rest_is_comment = false;
        bool found_word = false;
        Color color = state->editor.theme.fg;

        char line_number_str[16];
        snprintf(line_number_str, sizeof(line_number_str), "%4d", line_idx + 1);
        int line_number_str_len = strlen(line_number_str);

        for (int j = 0; j < line_number_str_len; j++) {
            Vector2 position = { j * char_width, visual_idx * line_height };
            DrawTextCodepoint(e->font, line_number_str[j], position, line_height, state->editor.theme.linenumber);
        }

        const int current_word_max_length = 32;
        char current_word[current_word_max_length];

        for (int j = 0; j < line->length; j++) {
            char c = dyn_char_get(line, j);

            if (found_word) {
                switch (c) {
                default: break;
                case COMMENT_CHAR:
                case '(':
                case ')':
                case ' ':
                {
                    found_word = false;
                    color = state->editor.theme.fg;
                } break;
                }
            }

            Editor_Coord coord = {
                .y = line_idx,
                .x = j,
            };

            Editor_Coord visual_coord = editor_wrapped_coord(state, coord);

            if (!rest_is_comment && !found_word) {
                if (is_alphabetic(c)) {
                    found_word = true;

                    Token_Type play_or_wait = try_get_play_or_wait_token_type_at_coord(state, coord);
                    if (play_or_wait != TOKEN_NONE) {
                        if (play_or_wait == TOKEN_PLAY) {
                            color = state->editor.theme.play;
                        } else {
                            color = state->editor.theme.wait;
                        }
                    } else if (is_note_at_coord(state, coord)) {
                        color = state->editor.theme.note;
                    } else {
                        int char_offset = 0;
                        while (true) {
                            if (char_offset >= current_word_max_length) {
                                break;
                            }
                            if ((j + char_offset) >= line->length) {
                                break;
                            }
                            if (!is_valid_in_identifier(dyn_char_get(line, j + char_offset))) {
                                break;
                            }

                            char *c = dyn_array_get(line, j + char_offset);
                            current_word[char_offset] = *c;

                            char_offset++;
                        }
                        current_word[char_offset] = '\0';

                        if (strcmp(current_word, "play") == 0) {
                            color = state->editor.theme.play;
                        } else if (strcmp(current_word, "wait") == 0) {
                            color = state->editor.theme.wait;
                        } else if (
                            strcmp(current_word, "start") == 0 ||
                            strcmp(current_word, "sine") == 0 ||
                            strcmp(current_word, "triangle") == 0 ||
                            strcmp(current_word, "square") == 0 ||
                            strcmp(current_word, "sawtooth") == 0 ||
                            strcmp(current_word, "define") == 0 ||
                            strcmp(current_word, "repeat") == 0 ||
                            strcmp(current_word, "rounds") == 0 ||
                            strcmp(current_word, "forever") == 0 ||
                            strcmp(current_word, "semi") == 0 ||
                            strcmp(current_word, "bpm") == 0 ||
                            strcmp(current_word, "chord") == 0 ||
                            strcmp(current_word, "scale") == 0 ||
                            strcmp(current_word, "rise") == 0 ||
                            strcmp(current_word, "fall") == 0) {
                            color = state->editor.theme.keyword;
                        } else {
                            color = state->editor.theme.fg;
                        }
                    }
                } else {
                    switch (c) {
                    case COMMENT_CHAR:
                        rest_is_comment = true;
                        color = state->editor.theme.comment;
                        break;
                    case '(':
                    case ')':
                        color = state->editor.theme.paren;
                        break;
                    case ' ':
                        color = state->editor.theme.space;
                        c = '-';
                        break;
                    default:
                        color = state->editor.theme.fg;
                        break;
                    }
                }
            }

            Vector2 position = {
                line_number_padding + (visual_coord.x * char_width),
                (visual_coord.y * line_height)
            };

            DrawTextCodepoint(e->font, c, position, line_height, color);
        }
    }
}

static void editor_render_state_write(State *state) {
    Editor *e = &state->editor;

    float line_height = editor_line_height(state);
    float char_width = editor_char_width(line_height);
    float line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    bool is_cursor_visible = is_line_visible(state, e->cursor.y);

    if (is_cursor_visible) {
        render_cursor_line(state);
    }

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
            render_selection(state, selection_render_data);
        } else {
            DynArray *line = dyn_array_get(&e->lines, selection_render_data.line);
            selection_render_data.start_x = selection_data.start.x;
            selection_render_data.end_x = line->length;
            render_selection(state, selection_render_data);

            selection_render_data.start_x = 0;
            for (int i = selection_data.start.y + 1; i < selection_data.end.y; i++) {
                DynArray *other_line = dyn_array_get(&e->lines, i);
                selection_render_data.line = i;
                selection_render_data.end_x = other_line->length;
                render_selection(state, selection_render_data);
            }

            selection_render_data.line = selection_data.end.y;
            selection_render_data.start_x = 0;
            selection_render_data.end_x = selection_data.end.x;
            render_selection(state, selection_render_data);
        }
    }

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (is_cursor_visible) {
        e->cursor_anim_time = e->cursor_anim_time + (state->delta_time * EDITOR_CURSOR_ANIMATION_SPEED);
        float pi2 = PI * 2.0f;
        if (e->cursor_anim_time > pi2) {
            e->cursor_anim_time -= pi2;
        }
        Editor_Coord wrapped_cursor = editor_wrapped_coord(state, e->cursor);
        float alpha = (cos(e->cursor_anim_time) + 1.0) * 127.5f;
        Editor_Cursor_Render_Data cursor_data = {
            .y = wrapped_cursor.y,
            .line_number_offset = line_number_padding,
            .visual_vertical_offset = e->visual_vertical_offset,
            .x = wrapped_cursor.x,
            .line_height = line_height,
            .char_width = char_width,
            .alpha = alpha
        };
        render_cursor(cursor_data, e->theme.cursor);
    }
}

void editor_render_state_wait_to_play(State *state) {
    float line_height = editor_line_height(state);
    float char_width = editor_char_width(line_height);

    float line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    editor_render_base(state, line_height, char_width, line_number_padding);
}

void editor_render_state_play(State *state) {
    Editor *e = &state->editor;

    float line_height = editor_line_height(state);
    float char_width = editor_char_width(line_height);

    float line_number_padding = char_width * EDITOR_LINE_NUMBER_PADDING;

    editor_render_base(state, line_height, char_width, line_number_padding);

    if (state->current_sound == NULL) {
        return;
    }

    int middle_line = e->wrap_line_count / 2;

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

    Color color = e->theme.play_cursor;

    color.a = CLAMP(
        ((state->current_sound->tone.duration - state->sound_time) 
        / (float)state->current_sound->tone.duration) * 255, 
        0, 255);

    DrawRectangleLinesEx(rec, 5, color);
}

