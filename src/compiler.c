#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "raylib.h"
#include "main.h"
#include "windows_wrapper.h"

#define CHROMATIC_SCALE (~0)
#define SILENT_CHORD ((Chord){0})
#define INVALID_CHORD ((Chord){ .size = -1 })
#define MAX_PAREN_NESTING 32

static int get_note_flag(int note) {
    return 1 << (note + MAX_NOTE_FROM_C0) % OCTAVE;
}

static bool peek_token(Compiler *result, int index, int offset, Token **token_ptr) {
    int actual = index + offset;
    if (actual == 0 || actual >= result->token_amount) {
        return false;
    }
    *token_ptr = &(result->tokens[index + offset]);
    return true;
}

static void populate_error_message(Compiler *result, int line_number, int char_idx) {
    result->error_message[0] = '\0';
    char str_buffer[128];

    switch (result->error_type) {
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
    strcat(result->error_message, str_buffer);

    int visual_line_number = line_number + 1;
    sprintf(str_buffer, "\nOn line %d:\n", visual_line_number);
    strcat(result->error_message, str_buffer);

    int slice_max_right = 20;
    int slice_start = char_idx > slice_max_right ? char_idx - slice_max_right : 0;
    int slice_padding = 10;
    int slice_end = (slice_start > 0 ? char_idx : slice_max_right) + slice_padding;
    int slice_len = slice_end - slice_start + 1;
    char slice[slice_len];
    {
        int i;
        for (i = 0; i < slice_len; i++) {
            slice[i] = result->data[line_number][slice_start + i];
        }
        slice[i] = '\0';
    }
    int pre_truncation_offset = 0;
    if (slice_start > 0) {
        strcat(result->error_message, "...");
        pre_truncation_offset = 3;
    }
    strcat(result->error_message, slice);
    if (slice_start + slice_len < strlen(result->data[0])) {
        strcat(result->error_message, "...");
    }
    strcat(result->error_message, "\n");
    {
        int i;
        for (i = 0; i < pre_truncation_offset + char_idx - slice_start; i++) {
            str_buffer[i] = ' ';
        }
        str_buffer[i] = '\0';
    }
    strcat(result->error_message, str_buffer);
    str_buffer[0] = '^';
    {
        int i;
        for (i = 1; char_idx + i <= slice_end && is_valid_in_identifier(slice[char_idx + i - slice_start]); i += 1) {
            str_buffer[i] = '^';
        }
        str_buffer[i] = '\0';
    }
    strcat(result->error_message, str_buffer);
    return;
}

static void *lexer_error(Compiler *result, Compiler_Error_Type error_type) {
    result->error_type = error_type;
    populate_error_message(result, result->line_number, result->char_idx);
}

static void *validator_error(Compiler *result, Compiler_Error_Type error_type, int token_idx) {
    result->error_type = error_type;
    populate_error_message(result, result->tokens[token_idx].line_number, result->tokens[token_idx].char_index);
}

static Token *token_add(Compiler *result, Token_Type token_type) {
    int address = result->token_amount;
    Token* token = &(result->tokens[address]);
    token->address = address;
    token->type = token_type;
    token->line_number = result->line_number;
    token->char_index = result->char_idx;
    (result->token_amount)++;
    return token;
}

static float note_to_frequency(int semi_offset) {
    return A4_FREQ * powf(2.0f, (float)semi_offset / (float)OCTAVE);
}

static void tone_add(Tone_Add_Data *data) {
}

static int char_to_int(char c) {
    return c - 48;
}

static int str_to_int(char* string, int idx, Compiler_Error_Type *error) {
    int num_chars_count = 0;
    char num_chars[4];
    while (isdigit(string[idx + num_chars_count])) {
        num_chars[num_chars_count] = string[idx + num_chars_count];
        num_chars_count++;
        if (num_chars_count > 4) {
            (*error) = ERROR_NUMBER_TOO_BIG;
            return 0;
        }
    }
    int number = 0;
    int multiplier = 1;
    for (int j = num_chars_count - 1; j >= 0; j--) {
        number += char_to_int(num_chars[j]) * multiplier;
        multiplier *= 10;
    }
    return number;
}

static int digit_count (int n) {
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    return -1;
}

static bool try_get_note_token(Compiler *result) {
    char *line = result->data[result->line_number];
    int char_idx = result->char_idx;

    int note = SILENCE;

    switch (line[char_idx]) {
    case 'c':
    case 'C': note = -9; break;
    case 'd':
    case 'D': note = -7; break;
    case 'e':
    case 'E': note = -5; break;
    case 'f':
    case 'F': note = -4; break;
    case 'g':
    case 'G': note = -2; break;
    case 'a':
    case 'A': note = 0; break;
    case 'b':
    case 'B': note = 2; break;
    default: return false;
    }
    char_idx++;

    int octave = 0;
    bool note_is_valid_identifier = true;
    if (!isspace(line[char_idx])) {
        switch (line[char_idx]) {
        case '#':
            note++;
            char_idx++;
            note_is_valid_identifier = false;
            break;
        case 'b':
        case 'B':
            note--;
            char_idx++;
            break;
        default:
            break;
        }
    }

    if (!isspace(line[char_idx])) {
        if (line[char_idx] >= '0' && line[char_idx] <= '8') {
            octave = (char_to_int(line[char_idx]) * (float)OCTAVE) - A4_OFFSET;
            char_idx++;
        }
        if (note_is_valid_identifier && !isspace(line[char_idx]) && is_valid_in_identifier(line[char_idx])) {
            return false;
        }
    }

    Token *token = token_add(result, TOKEN_NOTE);
    token->value.int_number = octave + note,
    result->char_idx = char_idx - 1;

    return true;
}

static bool try_get_play_or_wait_token(Compiler *result) {
    char *line = result->data[result->line_number];
    int char_idx = result->char_idx;

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
        return false;
    }

    char_idx += 4;

    float duration_divisor;

    switch (line[char_idx]) {
    default: {
        duration_divisor = 0.25f;
    } break;
    case '1': {
        if (line[char_idx + 1] == '6') {
            char_idx += 2;
            duration_divisor = 0.0625f;
        } else {
            char_idx++;
            duration_divisor = 1.0f;
        }
    } break;
    case '2': {
        char_idx++;
        duration_divisor = 0.5f;
    } break;
    case '4': {
        char_idx++;
        duration_divisor = 0.25f;
    } break;
    case '8': {
        char_idx++;
        duration_divisor = 0.125f;
    } break;
    case '3': {
        if (line[char_idx + 1] == '2') {
            char_idx += 2;
            duration_divisor = 0.03125f;
        } else {
            return false;
        }
    } break;
    case '6': {
        if (line[char_idx + 1] == '4') {
            char_idx += 2;
            duration_divisor = 0.015625f;
        } else {
            return false;
        }
    } break;
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
        int dot_amount = 1;
        if (isdigit(line[char_idx])) {
            dot_amount = char_to_int(line[char_idx]);
            char_idx++;
        } else if (is_valid_in_identifier(line[char_idx])) {
            return false;
        }
        for (int i = 0; i < dot_amount; i++) {
            duration_divisor += (duration_divisor * 0.5f);
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
        duration_divisor = (duration_divisor * 2.0f) / 3.0f;
    } break;
    }

    Token *token = token_add(result, token_type);
    token->value.play_or_wait.duration = duration_divisor;
    token->value.play_or_wait.char_count = char_idx - result->char_idx;
    result->char_idx = char_idx - 1;

    return true;
}

static Chord get_chord(int token_amount, Token *tokens, int *token_idx, Compiler_Error_Type *error) {
    Chord chord = {0};
    while (*token_idx < token_amount && tokens[*token_idx].type != TOKEN_PAREN_CLOSE) {
        if (tokens[*token_idx].type != TOKEN_NOTE) {
            (*error) = ERROR_CHORD_CAN_ONLY_CONTAIN_NOTES;
            return INVALID_CHORD;
        }
        if (chord.size >= OCTAVE) {
            (*error) = ERROR_CHORD_TOO_MANY_NOTES;
            return INVALID_CHORD;
        }
        chord.notes[chord.size] = tokens[*token_idx].value.int_number;
        chord.size++;
        (*token_idx)++;
    }
    if (chord.size == 0) {
        (*error) = ERROR_CHORD_CAN_NOT_BE_EMPTY;
        return INVALID_CHORD;
    }
    return chord;
}

static int get_scale(int token_amount, Token *tokens, int *token_idx, Compiler_Error_Type *error) {
    int scale_flags = 0;
    while (*token_idx < token_amount && tokens[*token_idx].type != TOKEN_PAREN_CLOSE) {
        if (tokens[*token_idx].type != TOKEN_NOTE) {
            (*error) = ERROR_SCALE_CAN_ONLY_CONTAIN_NOTES;
            return -1;
        }
        scale_flags |= get_note_flag(tokens[*token_idx].value.int_number);
        (*token_idx)++;
    }
    if (scale_flags == 0) {
        (*error) = ERROR_SCALE_CAN_NOT_BE_EMPTY;
        return -1;
    }
    return scale_flags;
}

static Chord parse_optional_scale_offset(Optional_Scale_Offset_Data *data) {
    Token *peek_token_ptr;
    int offset = 1; // default offset
    if (peek_token(data->result, (*data->token_idx), 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER) {
        (*data->token_idx) += 1;
        offset = peek_token_ptr->value.int_number;
    }
    Chord new_chord = {0};
    new_chord.size = data->chord.size;
    for (int i = 0; i < data->chord.size; i++) {
        int note = data->chord.notes[i];
        for (int j = 0; j < offset; j += 1) {
            note += data->direction;
            while (true) {
                int note_flag = get_note_flag(note);
                if (has_flag(data->scale, note_flag)) {
                    break;
                }
                note += data->direction;
            }
        }
        new_chord.notes[i] = note;
    }
    return new_chord;
}

static void run_lexer(Compiler *result) {
    result->token_amount = 0;
    result->tokens = (Token *)malloc(sizeof(Token) * 4096);
    if (result->tokens == NULL) {
        thread_error();
    }

    int paren_nest_level = 0;
    int paren_open_addresses[MAX_PAREN_NESTING] = {0};
    for (int line_i = 0; line_i < result->data_len; line_i++) {
        char *line = result->data[line_i];
        result->line_number++;
        result->char_idx = 0;
        for (int *i = &(result->char_idx); line[*i] != '\0' && line[*i] != COMMENT_CHAR; *i += 1) {
            if (isspace(line[*i])) {
                continue;
            }
            switch (line[*i]) {
            case '(':
                {
                    if (paren_nest_level >= MAX_PAREN_NESTING) {
                        lexer_error(result, ERROR_NESTING_TOO_DEEP);
                        return;
                    }
                    Token *token = token_add(result, TOKEN_PAREN_OPEN);
                    token->value.int_number = -1;
                    paren_open_addresses[paren_nest_level] = token->address;
                    paren_nest_level += 1;
                }
                break;
            case ')':
                {
                    paren_nest_level -= 1;
                    if (paren_nest_level < 0) {
                        lexer_error(result, ERROR_NO_MATCHING_OPENING_PAREN);
                        return;
                    }
                    Token *token = token_add(result, TOKEN_PAREN_CLOSE);
                    int paren_open_address = paren_open_addresses[paren_nest_level];
                    token->value.int_number = paren_open_address;
                    result->tokens[paren_open_address].value.int_number = token->address;
                }
                break;
            default:
                if (try_get_note_token(result)) {
                    break;
                }
                if (try_get_play_or_wait_token(result)) {
                    break;
                }
                if (isdigit(line[*i])) {
                    Compiler_Error_Type str_to_int_error = NO_ERROR;
                    int number = str_to_int(line, *i, &str_to_int_error);
                    if (str_to_int_error != NO_ERROR) {
                        lexer_error(result, str_to_int_error);
                        return;
                    }
                    Token *token = token_add(result, TOKEN_NUMBER);
                    token->value.int_number = number;
                    *i += (digit_count(number) - 1);
                    break;
                }
                if (!is_valid_in_identifier(line[*i])) {
                    lexer_error(result, ERROR_SYNTAX_ERROR);
                    return;
                }
                int ident_length = 0;
                for (int j = *i; is_valid_in_identifier(line[j]); j++) {
                    ident_length++;
                }
                char ident[1024];
                for (int j = 0; j < ident_length; j++) {
                    ident[j] = line[*i + j];
                }
                ident[ident_length] = '\0';
                if (strcmp("start", ident) == 0) {
                    token_add(result, TOKEN_START);
                } else if (strcmp("sine", ident) == 0) {
                    token_add(result, TOKEN_SINE);
                } else if (strcmp("triangle", ident) == 0) {
                    token_add(result, TOKEN_TRIANGLE);
                } else if (strcmp("square", ident) == 0) {
                    token_add(result, TOKEN_SQUARE);
                } else if (strcmp("sawtooth", ident) == 0) {
                    token_add(result, TOKEN_SAWTOOTH);
                } else if (strcmp("semi", ident) == 0) {
                    token_add(result, TOKEN_SEMI);
                } else if (strcmp("play", ident) == 0) {
                    token_add(result, TOKEN_PLAY);
                } else if (strcmp("wait", ident) == 0) {
                    token_add(result, TOKEN_WAIT);
                } else if (strcmp("bpm", ident) == 0) {
                    token_add(result, TOKEN_BPM);
                } else if (strcmp("chord", ident) == 0) {
                    token_add(result, TOKEN_CHORD);
                } else if (strcmp("scale", ident) == 0) {
                    token_add(result, TOKEN_SCALE);
                } else if (strcmp("rise", ident) == 0) {
                    token_add(result, TOKEN_RISE);
                } else if (strcmp("fall", ident) == 0) {
                    token_add(result, TOKEN_FALL);
                } else if (strcmp("repeat", ident) == 0) {
                    token_add(result, TOKEN_REPEAT);
                } else if (strcmp("rounds", ident) == 0) {
                    token_add(result, TOKEN_ROUNDS);
                } else if (strcmp("forever", ident) == 0) {
                    token_add(result, TOKEN_FOREVER);
                } else if (strcmp("define", ident) == 0) {
                    token_add(result, TOKEN_DEFINE);
                } else {
                    Token *token = token_add(result, TOKEN_IDENTIFIER);
                    token->value.string = (char *)malloc(sizeof(char) * (ident_length + 1));
                    if (token->value.string == NULL) {
                        thread_error();
                    }
                    strcpy(token->value.string, ident);
                }
                *i += (ident_length - 1);
                break;
            }
        }
    }
}

static void run_validator(Compiler *result) {
    Token* tokens = result->tokens;

    for (int i = 0; i < result->token_amount; i += 1) {
        switch (result->tokens[i].type) {
        case TOKEN_PAREN_OPEN:
            if (result->tokens[i].value.int_number < 0) {
                validator_error(result, ERROR_NO_MATCHING_CLOSING_PAREN, i);
                return;
            }
            break;
        default:
            break;
        }
    }

    result->variable_count = 0;

    Token *peek_token_ptr;

    for (int i = 0; i < result->token_amount; i++) {
        switch (tokens[i].type) {
        case TOKEN_START:
        case TOKEN_SINE:
        case TOKEN_TRIANGLE:
        case TOKEN_SQUARE:
        case TOKEN_SAWTOOTH:
        case TOKEN_PLAY:
        case TOKEN_WAIT:
        case TOKEN_NOTE:
        case TOKEN_PAREN_CLOSE: {
            // these are valid in isolation
        } break;
        case TOKEN_SEMI: {
            if (!peek_token(result, i, 1, &peek_token_ptr)) {
                validator_error(result, ERROR_INVALID_SEMI, i);
                return;
            }
            switch (peek_token_ptr->type) {
            case TOKEN_RISE:
            case TOKEN_FALL:
                break;
            default:
                validator_error(result, ERROR_INVALID_SEMI, i);
                return;
            }
        } break;
        case TOKEN_RISE:
        case TOKEN_FALL: {
            if (peek_token(result, i, 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER) {
                i++;
            }
        } break;
        case TOKEN_BPM: {
            i += 1;
            if (i >= result->token_amount || tokens[i].type != TOKEN_NUMBER) {
                validator_error(result, ERROR_EXPECTED_NUMBER, i);
                return;
            }
        } break;
        case TOKEN_CHORD: {
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i += 2;
            Compiler_Error_Type chord_error = NO_ERROR;
            Chord chord = get_chord(result->token_amount, tokens, &i, &chord_error);
            if (chord_error != NO_ERROR) {
                validator_error(result, chord_error, i);
                return;
            }
        } break;
        case TOKEN_SCALE: {
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i += 2;
            Compiler_Error_Type scale_error = NO_ERROR;
            int scale = get_scale(result->token_amount, tokens, &i, &scale_error);
            if (scale_error != NO_ERROR) {
                validator_error(result, scale_error, i);
                return;
            }
        } break;
        case TOKEN_REPEAT: {
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_NUMBER) {
                validator_error(result, ERROR_EXPECTED_NUMBER, i);
                return;
            }
            i++;
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i++;
        } break;
        case TOKEN_FOREVER: {
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i++;
        } break;
        case TOKEN_ROUNDS: {
            int amount;
            for (
                amount = 0;
                peek_token(result, i + amount, 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER;
                amount++
            );
            if (amount == 0) {
                validator_error(result, ERROR_EXPECTED_NUMBER, i);
                return;
            }
            tokens[i].value.int_number = amount;
            i += amount;
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i++;
        } break;
        case TOKEN_DEFINE: {
            if (!peek_token(result, i, 1 , &peek_token_ptr) || peek_token_ptr->type != TOKEN_IDENTIFIER) {
                validator_error(result, ERROR_EXPECTED_IDENTIFIER, i);
                return;
            }
            i++;
            bool new_variable = true;
            int variable_idx = result->variable_count;
            for (int j = 0; j < result->variable_count; j++) {
                if (strcmp(tokens[i].value.string, result->variables[j].ident) == 0) {
                    validator_error(result, ERROR_MULTIPLE_DEFINITIONS, i);
                    return;
                }
            }
            result->variables[variable_idx].ident = tokens[i].value.string;
            result->variable_count++;
            if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                return;
            }
            i++;
            result->variables[variable_idx].address = tokens[i].address;
        } break;
        case TOKEN_IDENTIFIER: {
            bool known_identifier = false;
            for (int j = 0; j < result->variable_count; j++) {
                if (strcmp(tokens[i].value.string, result->variables[j].ident) == 0) {
                    known_identifier = true;
                    break;
                }
            }
            if (!known_identifier) {
                validator_error(result, ERROR_UNKNOWN_IDENTIFIER, i);
                return;
            }
        } break;
        default: {
            validator_error(result, ERROR_SYNTAX_ERROR, i);
            return;
        } break;
        }
    }
}

static void run_parser(Compiler *compiler) {
    Parser *parser = &compiler->parser;
    Token* tokens = compiler->tokens;

    compiler->tone_amount = 0;

    if (!has_flag(compiler->flags, COMPILER_FLAG_IN_PROCESS)) {
        compiler->flags |= COMPILER_FLAG_IN_PROCESS;

        parser->token_idx = 0;
        parser->nest_idx = -1;
        for (int i = 0; i < 16; i++) {
            parser->nest_repetitions[i].target = -1;
            parser->nest_repetitions[i].round = 0;
        }
        parser->current_bpm = 125;
        parser->current_waveform = WAVEFORM_SINE;
        parser->token_ptr_return_idx = 0;
        parser->current_chord = SILENT_CHORD;
        parser->current_chord.size = 1;
        parser->current_scale = ~0;
    }

    uint32_t tone_idx = 0;
    bool semi_flag = false;

    for (int i = parser->token_idx; i < compiler->token_amount; i++) {
        switch (tokens[i].type) {
        case TOKEN_PLAY:
        case TOKEN_WAIT: {
            Tone* tone = &compiler->tones[compiler->tone_amount];
            tone->waveform = tokens[i].type == TOKEN_PLAY
                ? parser->current_waveform
                : WAVEFORM_NONE;
            tone->token_idx = tone_idx++;
            tone->line_idx = tokens[i].line_number;
            tone->char_idx = tokens[i].char_index;
            tone->char_count = tokens[i].value.play_or_wait.char_count;
            tone->chord = tokens[i].type == TOKEN_PLAY
                ? parser->current_chord
                : INVALID_CHORD;
            for (int i = 0; i < parser->current_chord.size; i++) {
                tone->chord.frequencies[i] = note_to_frequency(parser->current_chord.notes[i]);
            }
            tone->duration = tokens[i].value.play_or_wait.duration * 240.0f / parser->current_bpm;
            compiler->tone_amount++;
            if (compiler->tone_amount == SYNTHESIZER_TONE_CAPACITY) {
                compiler->flags |= COMPILER_FLAG_OUTPUT_AVAILABLE;
                parser->token_idx = (i + 1);
                if (compiler->parser.token_idx == (compiler->token_amount - 1)) {
                    compiler->flags &= ~COMPILER_FLAG_IN_PROCESS;
                    return;
                }
                return;
            }
        } break;
        case TOKEN_START: {
            int old_idx = tone_idx + 1;
            int new_idx = 0;
            while (old_idx < compiler->tone_amount) {
                compiler->tones[new_idx] = compiler->tones[old_idx];
                old_idx++;
                new_idx++;
            }
            compiler->tone_amount = new_idx;
        } break;
        case TOKEN_SINE:        { parser->current_waveform = WAVEFORM_SINE; } break;
        case TOKEN_TRIANGLE:    { parser->current_waveform = WAVEFORM_TRIANGLE; } break;
        case TOKEN_SQUARE:      { parser->current_waveform = WAVEFORM_SQUARE; } break;
        case TOKEN_SAWTOOTH:    { parser->current_waveform = WAVEFORM_SAWTOOTH; } break;
        case TOKEN_SEMI: {
            semi_flag = true;
        } break;
        case TOKEN_BPM: {
            i += 1;
            parser->current_bpm = tokens[i].value.int_number;
        } break;
        case TOKEN_NOTE: {
            parser->current_chord.size = 1;
            parser->current_chord.notes[0] = tokens[i].value.int_number;
        } break;
        case TOKEN_RISE:
        case TOKEN_FALL: {
            Optional_Scale_Offset_Data data = {
                .result = compiler,
                .token_idx = &i,
                .chord = parser->current_chord,
                .scale = semi_flag ? CHROMATIC_SCALE : parser->current_scale,
                .direction = (tokens[i].type == TOKEN_RISE) ? DIRECTION_RISE : DIRECTION_FALL,
            };
            semi_flag = false;
            parser->current_chord = parse_optional_scale_offset(&data);
        } break;
        case TOKEN_CHORD: {
            i += 2;
            parser->current_chord = get_chord(compiler->token_amount, tokens, &i, NULL);
        } break;
        case TOKEN_SCALE: {
            i += 2;
            parser->current_scale = get_scale(compiler->token_amount, tokens, &i, NULL);
        } break;
        case TOKEN_REPEAT: {
            i++;
            int repeat_amount = tokens[i].value.int_number;
            parser->nest_idx += 1;
            parser->nest_repetitions[parser->nest_idx].target = repeat_amount;
            parser->nest_repetitions[parser->nest_idx].round = 0;
            i++;
        } break;
        case TOKEN_FOREVER: {
            i++;
        } break;
        case TOKEN_ROUNDS: {
            int rounds[16] = {0};
            int amount = tokens[i].value.int_number;
            for (int j = 0; j < amount; j++) {
                rounds[j] = tokens[i + j + 1].value.int_number;
            }
            i += amount;
            i++;
            bool special_round = false;
            for (int j = 0; j < amount; j++) {
                if ((rounds[j] - 1) == parser->nest_repetitions[parser->nest_idx].round) {
                    special_round = true;
                    break;
                }
            }
            if (!special_round) {
                int paren_close_address = tokens[i].value.int_number;
                i = paren_close_address;
            }
        } break;
        case TOKEN_DEFINE: {
            i += 2;
            int paren_close_address = tokens[i].value.int_number;
            i = paren_close_address;
        } break;
        case TOKEN_IDENTIFIER: {
            for (int j = 0; j < compiler->variable_count; j++) {
                if (strcmp(tokens[i].value.string, compiler->variables[j].ident) == 0) {
                    parser->token_ptr_return_positions[parser->token_ptr_return_idx] = i;
                    parser->token_ptr_return_idx += 1;
                    i = compiler->variables[j].address;
                    break;
                }
            }
        } break;
        case TOKEN_PAREN_CLOSE: {
            int paren_return_address = tokens[i].value.int_number;
            if (paren_return_address <= 1) {
                break;
            }
            if (TOKEN_REPEAT == tokens[paren_return_address - 2].type) {
                Repetition *rep = &parser->nest_repetitions[parser->nest_idx];
                rep->round++;
                if (rep->round < rep->target) {
                    int paren_open_idx = tokens[i].value.int_number;
                    i = paren_open_idx;
                } else {
                    rep->target = -1;
                    rep->round = 0;
                    parser->nest_idx -= 1;
                }
            } else if (TOKEN_FOREVER == tokens[paren_return_address - 1].type) {
                i = paren_return_address;
            } else if (TOKEN_DEFINE == tokens[paren_return_address - 2].type) {
                parser->token_ptr_return_idx--;
                i = parser->token_ptr_return_positions[parser->token_ptr_return_idx];
            }
        } break;
        }
    }

    parser->token_idx = (compiler->token_amount - 1);
    compiler->flags &= ~COMPILER_FLAG_IN_PROCESS;
}

void compiler_init(Compiler *compiler) {
    compiler->mutex = mutex_create();
}

void compiler_output_handled(Compiler *compiler) {
    mutex_lock(compiler->mutex);
        compiler->flags |= COMPILER_FLAG_OUTPUT_HANDLED;
    mutex_unlock(compiler->mutex);
}

void compiler_reset(Compiler *compiler) {
    mutex_lock(compiler->mutex);
        compiler->flags |= COMPILER_FLAG_CANCELLED;
        for (int i = 0; i < compiler->token_amount; i += 1) {
            switch (compiler->tokens[i].type) {
            case TOKEN_IDENTIFIER:
                free(compiler->tokens[i].value.string);
                compiler->tokens[i].value.string = NULL;
                break;
            default:
                break;
            }
        }
        if (compiler->data != NULL) {
            free(compiler->data);
            compiler->data = NULL;
        }
        if (compiler->tokens != NULL) {
            free(compiler->tokens);
            compiler->tokens = NULL;
        }
        *compiler = (Compiler){0};
    mutex_unlock(compiler->mutex);
}

bool can_compile(Compiler *compiler) {
    bool x = true;
    mutex_lock(compiler->mutex);
        if (has_flag(compiler->flags, COMPILER_FLAG_OUTPUT_AVAILABLE)) {
            if (has_flag(compiler->flags, COMPILER_FLAG_OUTPUT_HANDLED)) {
                compiler->flags &= ~(COMPILER_FLAG_OUTPUT_AVAILABLE | COMPILER_FLAG_OUTPUT_HANDLED);
            } else {
                x = false;
            }
        }
    mutex_unlock(compiler->mutex);
    return x;
}

void compile(State *state) {
    Compiler *compiler = &state->compiler;

    if (compiler->parser.token_idx == compiler->token_amount - 1) {
        compiler->flags &= ~COMPILER_FLAG_IN_PROCESS;
        return;
    }

    mutex_lock(compiler->mutex);

    if (!has_flag(compiler->flags, COMPILER_FLAG_IN_PROCESS)) {
        compiler->data_len = state->editor.line_count;
        compiler->data = (char**)malloc(sizeof(char *) * state->editor.line_count);
        if (compiler->data == NULL) {
            thread_error();
        }

        for (int i = 0; i < compiler->data_len; i++) {
            compiler->data[i] = state->editor.lines[i];
        }

        compiler->tokens = NULL;
        compiler->line_number = -1;
        compiler->char_idx = 0;
        compiler->error_type = NO_ERROR;

        run_lexer(compiler);
        if (compiler->error_type != NO_ERROR) {
            goto compile_end;
        }

        run_validator(compiler);
        if (compiler->error_type != NO_ERROR) {
            goto compile_end;
        }
    }

    run_parser(compiler);

    if (compiler->tone_amount == 0) {
        compiler->error_type = ERROR_NO_SOUND;
        sprintf(compiler->error_message, "This produces no sound or silence");
        goto compile_end;
    }

    compile_end:
    mutex_unlock(compiler->mutex);
}

void compiler_free(Compiler *compiler) {
    compiler_reset(compiler);
    mutex_destroy(compiler->mutex);
}
