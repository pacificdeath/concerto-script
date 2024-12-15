#include "../main.h"
#include "compiler_utils.c"

inline static Compiler_Error_Address validator_error(Compiler_Error error, int token_idx) {
    return (Compiler_Error_Address) {
        .type = error,
        .token_idx = token_idx,
    };
}

static Compiler_Error_Address validator_run(Compiler *c) {
    Token* tokens = c->tokens;

    for (int i = 0; i < c->token_amount; i += 1) {
        switch (c->tokens[i].type) {
        case TOKEN_PAREN_OPEN:
            if (c->tokens[i].value.int_number < 0) {
                return validator_error(ERROR_NO_MATCHING_CLOSING_PAREN, i);
            }
            break;
        default:
            break;
        }
    }

    c->variable_count = 0;

    Token *peek_token_ptr;

    for (int i = 0; i < c->token_amount; i++) {
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
            if (!peek_token(c, i, 1, &peek_token_ptr)) {
                validator_error(ERROR_INVALID_SEMI, i);
            }
            switch (peek_token_ptr->type) {
            case TOKEN_RISE:
            case TOKEN_FALL:
                break;
            default:
                validator_error(ERROR_INVALID_SEMI, i);
            }
        } break;
        case TOKEN_RISE:
        case TOKEN_FALL: {
            if (peek_token(c, i, 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER) {
                i++;
            }
        } break;
        case TOKEN_BPM: {
            i += 1;
            if (i >= c->token_amount || tokens[i].type != TOKEN_NUMBER) {
                validator_error(ERROR_EXPECTED_NUMBER, i);
            }
        } break;
        case TOKEN_CHORD: {
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i += 2;
            Compiler_Error chord_error = NO_ERROR;
            Chord chord = get_chord(c->token_amount, tokens, &i, &chord_error);
            if (chord_error != NO_ERROR) {
                validator_error(chord_error, i);
            }
        } break;
        case TOKEN_SCALE: {
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i += 2;
            Compiler_Error scale_error = NO_ERROR;
            int scale = get_scale(c->token_amount, tokens, &i, &scale_error);
            if (scale_error != NO_ERROR) {
                validator_error(scale_error, i);
            }
        } break;
        case TOKEN_REPEAT: {
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_NUMBER) {
                validator_error(ERROR_EXPECTED_NUMBER, i);
            }
            i++;
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i++;
        } break;
        case TOKEN_FOREVER: {
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i++;
        } break;
        case TOKEN_ROUNDS: {
            int amount;
            for (
                amount = 0;
                peek_token(c, i + amount, 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER;
                amount++
            );
            if (amount == 0) {
                validator_error(ERROR_EXPECTED_NUMBER, i);
            }
            tokens[i].value.int_number = amount;
            i += amount;
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i++;
        } break;
        case TOKEN_DEFINE: {
            if (!peek_token(c, i, 1 , &peek_token_ptr) || peek_token_ptr->type != TOKEN_IDENTIFIER) {
                validator_error(ERROR_EXPECTED_IDENTIFIER, i);
            }
            i++;
            bool new_variable = true;
            int variable_idx = c->variable_count;
            for (int j = 0; j < c->variable_count; j++) {
                if (strcmp(tokens[i].value.string, c->variables[j].ident) == 0) {
                    validator_error(ERROR_MULTIPLE_DEFINITIONS, i);
                }
            }
            c->variables[variable_idx].ident = tokens[i].value.string;
            c->variable_count++;
            if (!peek_token(c, i, 1, &peek_token_ptr) || peek_token_ptr->type != TOKEN_PAREN_OPEN) {
                validator_error(ERROR_EXPECTED_PAREN_OPEN, i);
            }
            i++;
            c->variables[variable_idx].address = tokens[i].address;
        } break;
        case TOKEN_IDENTIFIER: {
            bool known_identifier = false;
            for (int j = 0; j < c->variable_count; j++) {
                if (strcmp(tokens[i].value.string, c->variables[j].ident) == 0) {
                    known_identifier = true;
                    break;
                }
            }
            if (!known_identifier) {
                validator_error(ERROR_UNKNOWN_IDENTIFIER, i);
            }
        } break;
        default: {
            validator_error(ERROR_SYNTAX_ERROR, i);
        } break;
        }
    }
}
