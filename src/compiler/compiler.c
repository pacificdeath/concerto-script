#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "../main.h"
#include "../windows_wrapper.h"

#include "error.c"
#include "lexer.c"
#include "validator.c"
#include "parser.c"

void compiler_init(Compiler *c) {
    c->mutex = mutex_create();
}

void compiler_start(Compiler *c, DynArray *data) {
    mutex_lock(c->mutex);
        c->data = data;
        if (c->data == NULL) {
            thread_error();
        }

        c->tokens = NULL;
        c->line_number = -1;
        c->char_idx = 0;
        c->error_type = NO_ERROR;

        Compiler_Error lexer_error = lexer_run(c);
        if (lexer_error != NO_ERROR) {
            c->error_type = lexer_error;
            populate_error_message(c, c->line_number, c->char_idx);
            return;
        }

        Compiler_Error_Address validator_error = validator_run(c);
        if (validator_error.type != NO_ERROR) {
            c->error_type = validator_error.type;
            populate_error_message(
                c,
                c->tokens[validator_error.token_idx].line_number,
                c->tokens[validator_error.token_idx].char_index
            );
            return;
        }

        c->flags |= COMPILER_FLAG_IN_PROCESS;
        parser_init(&c->parser);
        parser_run(c);
        handle_no_sound_error(c);

    mutex_unlock(c->mutex);
}

bool compiler_can_continue(Compiler *c) {
    bool x = true;
    mutex_lock(c->mutex);
        if (has_flag(c->flags, COMPILER_FLAG_OUTPUT_AVAILABLE)) {
            if (has_flag(c->flags, COMPILER_FLAG_OUTPUT_HANDLED)) {
                c->flags &= ~(COMPILER_FLAG_OUTPUT_AVAILABLE | COMPILER_FLAG_OUTPUT_HANDLED);
            } else {
                x = false;
            }
        }
    mutex_unlock(c->mutex);
    return x;
}

void compiler_continue(Compiler *c) {
    mutex_lock(c->mutex);
        parser_run(c);
        handle_no_sound_error(c);
    mutex_unlock(c->mutex);
}

void compiler_output_handled(Compiler *c) {
    mutex_lock(c->mutex);
        c->flags |= COMPILER_FLAG_OUTPUT_HANDLED;
    mutex_unlock(c->mutex);
}

void compiler_reset(Compiler *c) {
    mutex_lock(c->mutex);
        c->flags |= COMPILER_FLAG_CANCELLED;
        for (int i = 0; i < c->token_amount; i += 1) {
            switch (c->tokens[i].type) {
            case TOKEN_IDENTIFIER:
                dyn_mem_release(c->tokens[i].value.string);
                c->tokens[i].value.string = NULL;
                break;
            default:
                break;
            }
        }
        if (c->tokens != NULL) {
            dyn_mem_release(c->tokens);
            c->tokens = NULL;
        }
        *c = (Compiler){0};
    mutex_unlock(c->mutex);
}

void compiler_free(Compiler *c) {
    compiler_reset(c);
    mutex_destroy(c->mutex);
}
