#include "raylib.h"
#include "../main.h"
#include "editor_utils.c"

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
            DrawTextCodepoint(e->font, line_number_str[j], position, line_height, EDITOR_LINENUMBER_COLOR);
        }

        for (int j = 0; e->lines[line_idx][j] != '\0'; j++) {
            char c = e->lines[line_idx][j];

            if (found_word) {
                switch (c) {
                default: break;
                case COMMENT_CHAR:
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
                            color = EDITOR_KEYWORD_COLOR;
                        } else {
                            color = EDITOR_NORMAL_COLOR;
                        }
                    }
                } else {
                    switch (c) {
                    case COMMENT_CHAR:
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

            DrawTextCodepoint(e->font, c, position, line_height, color);
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
    if (tone->token_idx % 2 == 0) {
        color.r = 255;
        color.b = 0;
    } else {
        color.r = 0;
        color.b = 255;
    }
    DrawRectangleLinesEx(rec, 5, color);
}
