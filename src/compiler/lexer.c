#include "string.h"

#include "../windows_wrapper.h"
#include "../main.h"

#define MAX_PAREN_NESTING 32

inline static int char_to_int(char c) {
    return c - 48;
}

inline static int digit_count(int n) {
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

inline static int str_to_int(char* string, int idx, Compiler_Error *error) {
    int num_chars_count = 0;
    char num_chars[4];
    while (is_numeric(string[idx + num_chars_count])) {
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

inline static Token *token_add(Compiler *result, Token_Type token_type) {
    int address = result->token_amount;
    Token* token = &(result->tokens[address]);
    token->address = address;
    token->type = token_type;
    token->line_number = result->line_number;
    token->char_index = result->char_idx;
    result->token_amount++;
    return token;
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
        if (is_numeric(line[char_idx])) {
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

static Compiler_Error lexer_run(Compiler *c) {
    c->token_amount = 0;
    c->tokens = (Token *)dyn_mem_alloc(sizeof(Token) * 4096);
    if (c->tokens == NULL) {
        thread_error();
    }

    int paren_nest_level = 0;
    int paren_open_addresses[MAX_PAREN_NESTING] = {0};
    for (int line_i = 0; line_i < c->data_len; line_i++) {
        char *line = c->data[line_i];
        c->line_number++;
        c->char_idx = 0;
        for (int *i = &(c->char_idx); line[*i] != '\0' && line[*i] != COMMENT_CHAR; *i += 1) {
            if (isspace(line[*i])) {
                continue;
            }
            switch (line[*i]) {
            case '(': {
                if (paren_nest_level >= MAX_PAREN_NESTING) {
                    return ERROR_NESTING_TOO_DEEP;
                }
                Token *token = token_add(c, TOKEN_PAREN_OPEN);
                token->value.int_number = -1;
                paren_open_addresses[paren_nest_level] = token->address;
                paren_nest_level += 1;
            } break;
            case ')': {
                paren_nest_level -= 1;
                if (paren_nest_level < 0) {
                    return ERROR_NO_MATCHING_OPENING_PAREN;
                }
                Token *token = token_add(c, TOKEN_PAREN_CLOSE);
                int paren_open_address = paren_open_addresses[paren_nest_level];
                token->value.int_number = paren_open_address;
                c->tokens[paren_open_address].value.int_number = token->address;
            } break;
            default: {
                if (try_get_note_token(c)) {
                    break;
                }
                if (try_get_play_or_wait_token(c)) {
                    break;
                }
                if (is_numeric(line[*i])) {
                    Compiler_Error str_to_int_error = NO_ERROR;
                    int number = str_to_int(line, *i, &str_to_int_error);
                    if (str_to_int_error != NO_ERROR) {
                        return str_to_int_error;;
                    }
                    Token *token = token_add(c, TOKEN_NUMBER);
                    token->value.int_number = number;
                    *i += (digit_count(number) - 1);
                    break;
                }
                if (!is_valid_in_identifier(line[*i])) {
                    return ERROR_SYNTAX_ERROR;
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
                    token_add(c, TOKEN_START);
                } else if (strcmp("sine", ident) == 0) {
                    token_add(c, TOKEN_SINE);
                } else if (strcmp("triangle", ident) == 0) {
                    token_add(c, TOKEN_TRIANGLE);
                } else if (strcmp("square", ident) == 0) {
                    token_add(c, TOKEN_SQUARE);
                } else if (strcmp("sawtooth", ident) == 0) {
                    token_add(c, TOKEN_SAWTOOTH);
                } else if (strcmp("semi", ident) == 0) {
                    token_add(c, TOKEN_SEMI);
                } else if (strcmp("play", ident) == 0) {
                    token_add(c, TOKEN_PLAY);
                } else if (strcmp("wait", ident) == 0) {
                    token_add(c, TOKEN_WAIT);
                } else if (strcmp("bpm", ident) == 0) {
                    token_add(c, TOKEN_BPM);
                } else if (strcmp("chord", ident) == 0) {
                    token_add(c, TOKEN_CHORD);
                } else if (strcmp("scale", ident) == 0) {
                    token_add(c, TOKEN_SCALE);
                } else if (strcmp("rise", ident) == 0) {
                    token_add(c, TOKEN_RISE);
                } else if (strcmp("fall", ident) == 0) {
                    token_add(c, TOKEN_FALL);
                } else if (strcmp("repeat", ident) == 0) {
                    token_add(c, TOKEN_REPEAT);
                } else if (strcmp("rounds", ident) == 0) {
                    token_add(c, TOKEN_ROUNDS);
                } else if (strcmp("forever", ident) == 0) {
                    token_add(c, TOKEN_FOREVER);
                } else if (strcmp("define", ident) == 0) {
                    token_add(c, TOKEN_DEFINE);
                } else {
                    Token *token = token_add(c, TOKEN_IDENTIFIER);
                    token->value.string = (char *)dyn_mem_alloc(sizeof(char) * (ident_length + 1));
                    if (token->value.string == NULL) {
                        thread_error();
                    }
                    strcpy(token->value.string, ident);
                }
                *i += (ident_length - 1);
            } break;
            }
        }
    }

    return NO_ERROR;
}
