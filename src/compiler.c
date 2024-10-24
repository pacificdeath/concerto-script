#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "raylib.h"
#include "main.h"

#ifdef DEBUG
#include <time.h>
#endif

/*
    TODO: convert input file into lowercase
    TODO: other waveforms
    TODO:   handle case when some ridiculous person is trying to generate more tokens
            or tones than what is allowed by this thing
*/

int is_valid_in_identifier (char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

static bool peek_token(Compiler_Result *result, int index, int offset, Token **token) {
    int actual = index + offset;
    if (actual == 0 || actual >= result->token_amount) {
        return false;
    }
    *token = &(result->tokens[index + offset]);
    return true;
}

static void populate_error_message(Compiler_Result *result, int line_number, int char_idx) {
    Compiler_Error *error = &(result->error);
    error->message[0] = '\0';
    char str_buffer[128];

    switch (error->type) {
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
    case ERROR_NESTING_TOO_DEEP:
        sprintf(str_buffer, "Nesting this many \"()\" is too much");
        break;
    case ERROR_SCALE_CAN_ONLY_HAVE_NOTES:
        sprintf(str_buffer, "A scale may only contain\nnotes, not this thing");
        break;
    case ERROR_SCALE_CAN_NOT_BE_EMPTY:
        sprintf(str_buffer, "A scale can not be completely empty");
        break;
    case ERROR_INTERNAL:
        sprintf(str_buffer, "Internal error");
        break;
    default:
        sprintf(str_buffer, "Unknown error");
        break;
    }
    strcat(error->message, str_buffer);
    int slice_max_right = 20;
    int slice_start = char_idx > slice_max_right ? char_idx - slice_max_right : 0;
    int slice_padding = 10;
    int slice_end = (slice_start > 0 ? char_idx : slice_max_right) + slice_padding;
    int slice_len = slice_end - slice_start + 1;
    char slice[slice_len];
    {
        int i;
        for (i = 0; i < slice_len; i += 1) {
            slice[i] = result->data[0][slice_start + i];
        }
        slice[i] = '\0';
    }
    sprintf(str_buffer, "\nOn line %d:\n%s", line_number, slice);
    strcat(error->message, str_buffer);
    if (slice_start + slice_len < strlen(result->data[0])) {
        strcat(error->message, "...");
    }
    strcat(error->message, "\n");
    {
        int i;
        for (i = 0; i < char_idx - slice_start; i++) {
            str_buffer[i] = ' ';
        }
        str_buffer[i] = '\0';
    }
    strcat(error->message, str_buffer);
    str_buffer[0] = '^';
    {
        int i;
        for (i = 1; char_idx + i <= slice_end && is_valid_in_identifier(slice[char_idx + i - slice_start]); i += 1) {
            str_buffer[i] = '^';
        }
        str_buffer[i] = '\n';
        str_buffer[i + 1] = '\0';
    }
    strcat(error->message, str_buffer);
    return;
}

static Compiler_Result *lexer_error(Compiler_Result *result, Compiler_Error_Type error_type) {
    result->error.type = error_type;
    populate_error_message(result, result->line_number, result->char_idx);
    return result;
}

static Compiler_Result *parser_error(Compiler_Result *result, Compiler_Error_Type error_type, int token_idx) {
    result->error.type = error_type;
    populate_error_message(result, result->tokens[token_idx].line_number, result->tokens[token_idx].char_index);
    return result;
}

static Token *token_add(Compiler_Result *result, Token_Type token_type) {
    int address = result->token_amount;
    Token* token = &(result->tokens[address]);
    token->address = address;
    token->type = token_type;
    token->line_number = result->line_number;
    token->char_index = result->char_idx;
    (result->token_amount)++;
    return token;
}

static float note_to_frequency (int semi_offset) {
    return A4_FREQ * powf(2.0f, (float)semi_offset / OCTAVE_OFFSET);
}

static void tone_add(Tone_Add_Data *data) {
    float start_frequency = note_to_frequency(data->start_note);
    float end_frequency = note_to_frequency(data->end_note);
    Tone* tone = &(data->result->tones[data->result->tone_amount]);
    tone->idx = data->idx;
    tone->line_idx = data->token->line_number;
    tone->char_idx = data->token->char_index;
    switch (data->token->type) {
    default: tone->char_count = 1; break;
    case PLAY:
    case WAIT: tone->char_count = 4; break;
    }
    tone->start_note = data->start_note;
    tone->start_frequency = start_frequency;
    tone->end_note = data->end_note;
    tone->end_frequency = end_frequency;
    tone->duration = data->duration;
    unsigned int unsigned_note_from_c0 = (data->start_note + 57);
    tone->octave = unsigned_note_from_c0 / 12;
    (data->result->tone_amount)++;
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

static bool try_get_note (char *str, int *str_ptr, int *result_note) {
    int i = *str_ptr;
    int note = SILENCE;
    switch (str[i]) {
    case 'C': note = -9; break;
    case 'D': note = -7; break;
    case 'E': note = -5; break;
    case 'F': note = -4; break;
    case 'G': note = -2; break;
    case 'A': note = 0; break;
    case 'B': note = 2; break;
    default: return false;
    }
    i++;
    int octave = 0;
    bool note_is_valid_identifier = true;
    if (!isspace(str[i])) {
        switch (str[i]) {
        case '#':
            note++;
            i++;
            note_is_valid_identifier = false;
            break;
        case 'B':
            note--;
            i++;
            break;
        default:
            break;
        }
    }
    if (!isspace(str[i])) {
        if (str[i] >= '0' && str[i] <= '8') {
            octave = (char_to_int(str[i]) * OCTAVE_OFFSET) - A4_OFFSET;
            i++;
        }
        if (note_is_valid_identifier && !isspace(str[i]) && is_valid_in_identifier(str[i])) {
            return false;
        }
    }
    (*str_ptr) = i - 1;
    *result_note = octave + note;
    return true;
}

static int get_scale(int token_amount, Token *tokens, int *token_ptr, Compiler_Error_Type *error) {
    int scale = 0;
    while (*token_ptr < token_amount && tokens[*token_ptr].type != PAREN_CLOSE) {
        if (tokens[*token_ptr].type != NOTE) {
            (*error) = ERROR_SCALE_CAN_ONLY_HAVE_NOTES;
            return -1;
        }
        scale |= 1 << (tokens[*token_ptr].value.number + 96) % 12;
        (*token_ptr)++;
    }
    if (scale == 0) {
        (*error) = ERROR_SCALE_CAN_NOT_BE_EMPTY;
        return -1;
    }
    return scale;
}

static int parse_optional_scale_offset(Optional_Scale_Offset_Data data) {
    Token *peek_token_ptr;
    int offset = 1; // default offset
    if (peek_token(data.result, (*data.token_idx), 1, &peek_token_ptr) && peek_token_ptr->type == NUMBER) {
        (*data.token_idx) += 1;
        offset = peek_token_ptr->value.number;
    }
    int note = data.current_note;
    for (int j = 0; j < offset; j += 1) {
        note += data.direction;
        while (true) {
            int bit_note = 1 << ((note + 96) % 12);
            if ((bit_note & data.current_scale) == bit_note) {
                break;
            }
            note += data.direction;
        }
    }
    return note;
}

Compiler_Result *compile(State *state) {
    #ifdef DEBUG
        printf("Compiling:\n");
    #endif

    int data_len = state->editor.line_count;
    char *data[state->editor.line_count];
    for (int i = 0; i < data_len; i++) {
        data[i] = state->editor.lines[i];
    }

    Compiler_Result *result = (Compiler_Result *)malloc(sizeof(Compiler_Result));
    result->data = data;
    result->data_len = data_len;
    result->tokens = NULL;
    result->tones = NULL;
    result->line_number = -1;
    result->char_idx = 0;
    result->error.type = NO_ERROR;

    result->token_amount = 0;
    result->tokens = (Token *)malloc(sizeof(Token) * 4096);
    if (result->tokens == NULL) {
        printf("malloc failed for tokens\n");
        result->error.type = ERROR_INTERNAL;
        return result;
    }

    result->tone_amount = 0;
    result->tones = NULL;

    // start lexer
    {
        #ifdef DEBUG
        clock_t lexer_time;
        lexer_time = clock();
        #endif
        int paren_nest_level = 0;
        int paren_open_addresses[COMPILER_MAX_PAREN_NESTING] = {0};
        for (int line_i = 0; line_i < data_len; line_i++) {
            char *line = data[line_i];
            result->line_number++;
            result->char_idx = 0;
            for (int *i = &(result->char_idx); line[*i] != '\0' && line[*i] != '!'; *i += 1) {
                if (isspace(line[*i])) {
                    continue;
                }
                switch (line[*i]) {
                case '(':
                    {
                        if (paren_nest_level >= COMPILER_MAX_PAREN_NESTING) {
                            return lexer_error(result, ERROR_NESTING_TOO_DEEP);
                        }
                        Token *token = token_add(result, PAREN_OPEN);
                        token->value.number = -1;
                        paren_open_addresses[paren_nest_level] = token->address;
                        paren_nest_level += 1;
                    }
                    break;
                case ')':
                    {
                        paren_nest_level -= 1;
                        if (paren_nest_level < 0) {
                            return lexer_error(result, ERROR_NO_MATCHING_OPENING_PAREN);
                        }
                        Token *token = token_add(result, PAREN_CLOSE);
                        int paren_open_address = paren_open_addresses[paren_nest_level];
                        token->value.number = paren_open_address;
                        result->tokens[paren_open_address].value.number = token->address;
                    }
                    break;
                default:
                    {
                        int note;
                        if (try_get_note(line, i, &note)) {
                            Token *token = token_add(result, NOTE);
                            token->value.number = note;
                            break;
                        }
                    }
                    if (isdigit(line[*i])) {
                        Compiler_Error_Type str_to_int_error = NO_ERROR;
                        int number = str_to_int(line, *i, &str_to_int_error);
                        if (str_to_int_error != NO_ERROR) {
                            return lexer_error(result, str_to_int_error);
                        }
                        Token *token = token_add(result, NUMBER);
                        token->value.number = number;
                        *i += (digit_count(number) - 1);
                        break;
                    }
                    if (!is_valid_in_identifier(line[*i])) {
                        return lexer_error(result, ERROR_SYNTAX_ERROR);
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
                    if (strcmp("SEMI", ident) == 0) {
                        token_add(result, SEMI);
                    } else if (strcmp("PLAY", ident) == 0) {
                        token_add(result, PLAY);
                    } else if (strcmp("WAIT", ident) == 0) {
                        token_add(result, WAIT);
                    } else if (strcmp("BPM", ident) == 0) {
                        token_add(result, BPM);
                    } else if (strcmp("DURATION", ident) == 0) {
                        token_add(result, DURATION);
                    } else if (strcmp("SCALE", ident) == 0) {
                        token_add(result, SCALE);
                    } else if (strcmp("RISE", ident) == 0) {
                        token_add(result, RISE);
                    } else if (strcmp("FALL", ident) == 0) {
                        token_add(result, FALL);
                    } else if (strcmp("REPEAT", ident) == 0) {
                        token_add(result, REPEAT);
                    } else if (strcmp("ROUNDS", ident) == 0) {
                        token_add(result, ROUNDS);
                    } else if (strcmp("DEFINE", ident) == 0) {
                        token_add(result, DEFINE);
                    } else {
                        Token *token = token_add(result, IDENTIFIER);
                        token->value.string = (char *)malloc(sizeof(char) * (ident_length + 1));
                        strcpy(token->value.string, ident);
                    }
                    *i += (ident_length - 1);
                    break;
                }
            }
        }
        #ifdef DEBUG
            lexer_time = clock() - lexer_time;
            printf(" * Lexer time: %f sec\n", ((float)lexer_time) / CLOCKS_PER_SEC);
        #endif
        #ifdef DEBUG_LIST_TOKENS
            print_tokens(result->token_amount, result->tokens);
        #endif
    }
    // end lexer

    for (int i = 0; i < result->token_amount; i += 1) {
        switch (result->tokens[i].type) {
        case PAREN_OPEN:
            if (result->tokens[i].value.number < 0) {
                return parser_error(result, ERROR_NO_MATCHING_CLOSING_PAREN, i);
            }
            break;
        default:
            break;
        }
    }

    Token_Variable variables[COMPILER_VARIABLE_MAX_COUNT] = {0};
    int variable_count = 0;

    result->tones = (Tone *)malloc(sizeof(Tone) * 4096);
    if (result->tones == NULL) {
        printf("malloc failed for tones\n");
        result->error.type = ERROR_INTERNAL;
        return result;
    }

    uint32_t tone_idx = 0;

    // start parser
    {
        #ifdef DEBUG
        clock_t parser_time;
        parser_time = clock();
        #endif
        Token* tokens = result->tokens;
        int nest_idx = -1;
        Repetition nest_repetitions[16];
        for (int i = 0; i < 16; i++) {
            nest_repetitions[i].target = -1;
            nest_repetitions[i].round = 0;
        }
        int bpm = 125;
        int i_return_positions[32];
        int i_return_idx = 0;
        int note = 0;
        float duration_sec = 240 / bpm;

        // TODO: scale / scale / define must never be nested
        int scale = ~0;
        Token *peek_token_ptr;
        bool semi_flag = false;
        for (int i = 0; i < result->token_amount; i++) {
            switch (tokens[i].type) {
            case SEMI: {
                if (!peek_token(result, i, 1, &peek_token_ptr)) {
                    return parser_error(result, ERROR_INVALID_SEMI, i);
                }
                switch (peek_token_ptr->type) {
                case RISE:
                case FALL:
                    break;
                default:
                    return parser_error(result, ERROR_INVALID_SEMI, i);
                    break;
                }
                semi_flag = true;
            } break;
            case BPM:
                i += 1;
                if (i < result->token_amount && tokens[i].type == NUMBER) {
                    bpm = tokens[i].value.number;
                } else {
                    return parser_error(result, ERROR_EXPECTED_NUMBER, i);
                }
                break;
            case PLAY: {
                float bpm_play_duration = duration_sec * 240 / bpm;
                Tone_Add_Data tone_add_data = {
                    .idx = tone_idx++,
                    .result = result,
                    .token = &tokens[i],
                    .start_note = note,
                    .end_note = note,
                    .duration = bpm_play_duration,
                };
                tone_add(&tone_add_data);
            } break;
            case WAIT: {
                float bpm_wait_duration = duration_sec * 240 / bpm;
                Tone_Add_Data tone_add_data = {
                    .idx = tone_idx++,
                    .result = result,
                    .token = &tokens[i],
                    .start_note = SILENCE,
                    .end_note = SILENCE,
                    .duration = bpm_wait_duration,
                };
                tone_add(&tone_add_data);
            } break;
            case DURATION:
                i += 1;
                if (tokens[i].type != NUMBER) {
                    return parser_error(result, ERROR_EXPECTED_NUMBER, i);
                }
                duration_sec = 1.0f / (float)tokens[i].value.number;
                break;
            case NOTE:
                note = tokens[i].value.number;
                break;
            case RISE:
            case FALL: {
                Optional_Scale_Offset_Data data = {
                    .result = result,
                    .token_idx = &i,
                    .current_note = note,
                    .current_scale = semi_flag ? ~0 : scale,
                    .direction = (tokens[i].type == RISE) ? DIRECTION_RISE : DIRECTION_FALL,
                };
                semi_flag = false;
                note = parse_optional_scale_offset(data);
            } break;
            case SCALE:
                if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != PAREN_OPEN) {
                    return parser_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                }
                i += 2;
                Compiler_Error_Type scale_error = NO_ERROR;
                scale = get_scale(result->token_amount, tokens, &i, &scale_error);
                if (scale_error != NO_ERROR) {
                    return parser_error(result, scale_error, i);
                }
                break;
            case REPEAT:
                if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != NUMBER) {
                    return parser_error(result, ERROR_EXPECTED_NUMBER, i);
                }
                i++;
                int repeat_amount = tokens[i].value.number;
                nest_idx += 1;
                nest_repetitions[nest_idx].target = repeat_amount;
                nest_repetitions[nest_idx].round = 0;
                if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != PAREN_OPEN) {
                    return parser_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                }
                i++;
                break;
            case ROUNDS: {
                int rounds[16] = {0};
                int amount;
                for (
                    amount = 0;
                    peek_token(result, i + amount, 1, &peek_token_ptr) && peek_token_ptr->type == NUMBER;
                    amount++
                ) {
                    rounds[amount] = peek_token_ptr->value.number;
                }
                if (amount == 0) {
                    return parser_error(result, ERROR_EXPECTED_NUMBER, i);
                }
                i += amount;
                if (!peek_token(result, i, 1, &peek_token_ptr) || peek_token_ptr->type != PAREN_OPEN) {
                    return parser_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                }
                i++;
                bool special_round = false;
                for (int j = 0; j < amount; j++) {
                    if ((rounds[j] - 1) == nest_repetitions[nest_idx].round) {
                        special_round = true;
                        break;
                    }
                }
                if (!special_round) {
                    int paren_close_location = tokens[i].value.number;
                    i = paren_close_location;
                }
            } break;
            case DEFINE: {
                if (!peek_token(result, i, 1 , &peek_token_ptr) || peek_token_ptr->type != IDENTIFIER) {
                    return parser_error(result, ERROR_EXPECTED_IDENTIFIER, i);
                }
                i++;
                bool new_variable = true;
                int variable_idx = variable_count;
                for (int j = 0; j < variable_count; j++) {
                    if (strcmp(tokens[i].value.string, variables[j].ident) == 0) {
                        new_variable = false;
                        variable_idx = j;
                        break;
                    }
                }
                variables[variable_idx].ident = tokens[i].value.string;
                if (peek_token(result, i, 1, &peek_token_ptr) && peek_token_ptr->type == PAREN_OPEN) {
                    int paren_close_location = peek_token_ptr->value.number;
                    i = paren_close_location;
                    variables[variable_idx].address = peek_token_ptr->address;
                } else {
                    return parser_error(result, ERROR_EXPECTED_PAREN_OPEN, i);
                }
                if (new_variable) {
                    variable_count++;
                }
            } break;
            case IDENTIFIER:
                bool known_identifier = false;
                for (int j = 0; j < variable_count; j++) {
                    if (strcmp(tokens[i].value.string, variables[j].ident) == 0) {
                        known_identifier = true;
                        i_return_positions[i_return_idx] = i;
                        i_return_idx += 1;
                        i = variables[j].address;
                        break;
                    }
                }
                if (!known_identifier) {
                    return parser_error(result, ERROR_UNKNOWN_IDENTIFIER, i);
                }
                break;
            case PAREN_CLOSE:
                int paren_return_location = tokens[i].value.number;
                if (paren_return_location > 1 && tokens[paren_return_location - 2].type == REPEAT) {
                    nest_repetitions[nest_idx].round++;
                    if (nest_repetitions[nest_idx].round < nest_repetitions[nest_idx].target) {
                        int paren_open_idx = tokens[i].value.number;
                        i = paren_open_idx;
                    } else {
                        nest_repetitions[nest_idx].target = -1;
                        nest_repetitions[nest_idx].round = 0;
                        nest_idx -= 1;
                    }
                } else if (paren_return_location > 1 && tokens[paren_return_location - 2].type == DEFINE) {
                    i_return_idx--;
                    i = i_return_positions[i_return_idx];
                }
                break;
            default:
                return parser_error(result, ERROR_SYNTAX_ERROR, i);
            }
        }
        #ifdef DEBUG_LIST_FREQUENCIES
            print_tones(result->tone_amount, result->tones);
        #endif
        #ifdef DEBUG
            parser_time = clock() - parser_time;
            double total_parser_time = ((double)parser_time) / CLOCKS_PER_SEC;
            printf(" * Parser time: %f sec\n", total_parser_time);
        #endif
    }
    // end parser

    if (result->tone_amount == 0) {
        result->error.type = ERROR_NO_SOUND;
        strcpy(result->error.message, "This produces no sound or silence");
        return result;
    }

    #ifdef DEBUG
        printf("Compiling Completed\n");
    #endif
    return result;
}

void free_compiler_result(Compiler_Result *compiler_result) {
    for (int i = 0; i < compiler_result->token_amount; i += 1) {
        switch (compiler_result->tokens[i].type) {
        case IDENTIFIER:
            free(compiler_result->tokens[i].value.string);
            break;
        default:
            break;
        }
    }
    if (compiler_result->tokens != NULL) { free(compiler_result->tokens); }
    if (compiler_result->tones != NULL) { free(compiler_result->tones); }
    free(compiler_result);
}

#ifdef DEBUG_LIST_TOKENS
    static void print_tokens (int token_amount, Token *tokens) {
        for (int i = 0; i < token_amount; i++) {
            char *token_type;
            switch (tokens[i].type) {
            case BPM:           token_type = "BPM"; break;
            case NUMBER:        token_type = "Number"; break;
            case PLAY:          token_type = "Play"; break;
            case WAIT:          token_type = "Wait"; break;
            case NOTE:          token_type = "Note"; break;
            case RISE:          token_type = "Rise"; break;
            case FALL:          token_type = "Fall"; break;
            case RISE_SEMI:     token_type = "Semi Rise"; break;
            case FALL_SEMI:     token_type = "Semi Fall"; break;
            case DURATION:      token_type = "Duration"; break;
            case SCALE:         token_type = "Scale"; break;
            case REPEAT:        token_type = "Repeat"; break;
            case DEFINE:        token_type = "Define"; break;
            case PAREN_OPEN:    token_type = "Paren Open"; break;
            case PAREN_CLOSE:   token_type = "Paren Close"; break;
            case SLASH:         token_type = "Slash"; break;
            default:            token_type = "Unknown"; break;
            }
            printf("token[%04i]  %-16s", i, token_type);
            switch (tokens[i].type) {
            case NOTE:
            case NUMBER:
                printf("%d", tokens[i].value.number);
                break;
            case SCALE: {
                    int location = tokens[i].value.number;
                    printf("token[%04i]", location);
                }
                break;
            case IDENTIFIER: {
                    printf("%s", tokens[i].value.string);
                }
                break;
            case PAREN_OPEN:
            case PAREN_CLOSE: {
                    int match_location = tokens[i].value.number;
                    printf("token[%04i]", match_location);
                }
                break;
            default:
                printf("----");
                break;
            }
            printf("\n");
        }
    }
#endif
