#include "../main.h"
#include "compiler_utils.c"

inline static float note_to_frequency(int semi_offset) {
    return A4_FREQ * powf(2.0f, (float)semi_offset / (float)OCTAVE);
}

static void parser_init(Parser *parser) {
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

static void parser_run(Compiler *compiler) {
    Parser *parser = &compiler->parser;
    Token* tokens = compiler->tokens;

    compiler->tone_amount = 0;

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
                compiler->parser.token_idx = (i + 1);
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
            int direction = (tokens[i].type == TOKEN_RISE) ? 1 : -1;
            int scale = semi_flag ? CHROMATIC_SCALE : parser->current_scale;
            semi_flag = false;
            Token *peek_token_ptr;
            int offset = 1; // default offset
            if (peek_token(compiler, i, 1, &peek_token_ptr) && peek_token_ptr->type == TOKEN_NUMBER) {
                i++;
                offset = peek_token_ptr->value.int_number;
            }
            Chord new_chord = {0};
            for (int i = 0; i < parser->current_chord.size; i++) {
                for (int j = 0; j < offset; j += 1) {
                    parser->current_chord.notes[i] += direction;
                    while (true) {
                        int note_flag = get_note_flag(parser->current_chord.notes[i]);
                        if (has_flag(scale, note_flag)) {
                            break;
                        }
                        parser->current_chord.notes[i] += direction;
                    }
                }
            }
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
            if (paren_return_address < 1) {
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

    compiler->flags &= ~COMPILER_FLAG_IN_PROCESS;
}
