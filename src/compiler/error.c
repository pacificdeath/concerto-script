#include "../main.h"

static void populate_error_message(Compiler *compiler, int line_number, int char_idx) {
    compiler->error_message[0] = '\0';
    char str_buffer[128];

    switch (compiler->error_type) {
    case NO_ERROR:
        sprintf(str_buffer, "No error");
        break;
    case ERROR_SYNTAX_ERROR:
        sprintf(str_buffer, "This is wrong");
        break;
    case ERROR_INVALID_SEMI:
        sprintf(str_buffer, "\"SEMI\" must be followed\nby one of these:\nRISE, FALL");
        break;
    case ERROR_UNKNOWN_IDENTIFIER:
        sprintf(str_buffer, "This thing is not at all known");
        break;
    case ERROR_EXPECTED_IDENTIFIER:
        sprintf(str_buffer, "Expected an identifier after this");
        break;
    case ERROR_EXPECTED_PAREN_OPEN:
        sprintf(str_buffer, "Expected a \'(\' after this");
        break;
    case ERROR_NO_SCALE_IDENTIFIER:
        sprintf(str_buffer, "You should come up with a\nname for the scale here");
        break;
    case ERROR_EXPECTED_NUMBER:
        sprintf(str_buffer, "There should be a number after this");
        break;
    case ERROR_NUMBER_TOO_BIG:
        sprintf(str_buffer, "This number is way too large");
        break;
    case ERROR_NO_MATCHING_OPENING_PAREN:
        sprintf(str_buffer, "This has no matching \'(\'");
        break;
    case ERROR_NO_MATCHING_CLOSING_PAREN:
        sprintf(str_buffer, "This has no matching \')\'");
        break;
    case ERROR_MULTIPLE_DEFINITIONS:
        sprintf(str_buffer, "This has already been defined");
        break;
    case ERROR_NESTING_TOO_DEEP:
        sprintf(str_buffer, "Nesting this many \"()\" is too much");
        break;
    case ERROR_CHORD_CAN_ONLY_CONTAIN_NOTES:
        sprintf(str_buffer, "Chords may only contain\nnotes, not this thing");
        break;
    case ERROR_CHORD_CAN_NOT_BE_EMPTY:
        sprintf(str_buffer, "Chords can not be completely\nempty like that");
        break;
    case ERROR_CHORD_TOO_MANY_NOTES:
        sprintf(str_buffer, "Chords can not have this many\nnotes for some reason");
        break;
    case ERROR_SCALE_CAN_ONLY_CONTAIN_NOTES:
        sprintf(str_buffer, "Scales may only contain\nnotes, not this thing");
        break;
    case ERROR_SCALE_CAN_NOT_BE_EMPTY:
        sprintf(str_buffer, "Scales can not be completely\nempty like that");
        break;
    default:
        sprintf(str_buffer, "Unknown error");
        break;
    }
    strcat(compiler->error_message, str_buffer);

    int visual_line_number = line_number + 1;
    sprintf(str_buffer, "\nOn line %d:\n", visual_line_number);
    strcat(compiler->error_message, str_buffer);

    int slice_max_right = 20;
    int slice_start = char_idx > slice_max_right ? char_idx - slice_max_right : 0;
    int slice_padding = 10;
    int slice_end = (slice_start > 0 ? char_idx : slice_max_right) + slice_padding;
    int slice_len = slice_end - slice_start + 1;
    char slice[slice_len];
    {
        int i;
        for (i = 0; i < slice_len; i++) {
            DynArray *line = dyn_array_get(compiler->data, line_number);
            char c = dyn_char_get(line, slice_start + i);
            slice[i] = c;
        }
        slice[i] = '\0';
    }
    int pre_truncation_offset = 0;
    if (slice_start > 0) {
        strcat(compiler->error_message, "...");
        pre_truncation_offset = 3;
    }
    strcat(compiler->error_message, slice);
    if (slice_start + slice_len < compiler->data->length) {
        strcat(compiler->error_message, "...");
    }
    strcat(compiler->error_message, "\n");
    {
        int i;
        for (i = 0; i < pre_truncation_offset + char_idx - slice_start; i++) {
            str_buffer[i] = ' ';
        }
        str_buffer[i] = '\0';
    }
    strcat(compiler->error_message, str_buffer);
    str_buffer[0] = '^';
    {
        int i;
        for (i = 1; char_idx + i <= slice_end && is_valid_in_identifier(slice[char_idx + i - slice_start]); i += 1) {
            str_buffer[i] = '^';
        }
        str_buffer[i] = '\0';
    }
    strcat(compiler->error_message, str_buffer);
    return;
}

static void handle_no_sound_error(Compiler *c) {
    if (c->tone_amount == 0) {
        c->error_type = ERROR_NO_SOUND;
        sprintf(c->error_message, "This produces no sound or silence");
    }
}
