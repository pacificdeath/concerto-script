#ifndef COMPILER_UTILS_C
#define COMPILER_UTILS_C

#include "../main.h"

#define MAX_NOTE_FROM_C0 96
#define CHROMATIC_SCALE (~0)
#define SILENT_CHORD ((Chord){0})
#define INVALID_CHORD ((Chord){ .size = -1 })

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

static Chord get_chord(int token_amount, Token *tokens, int *token_idx, Compiler_Error *error) {
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

static int get_scale(int token_amount, Token *tokens, int *token_idx, Compiler_Error *error) {
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

#endif