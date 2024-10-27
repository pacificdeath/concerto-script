#include "raylib.h"

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "windows_wrapper.h"
#include "compiler.c"
#include "console.c"
#include "editor.c"
#include "synthesizer.c"

#ifdef TEST_THREAD
    #include "test_thread.c"
#endif

#define OCTAVE_OFFSET 12.0f
#define MAX_OCTAVE 8
#define A4_OFFSET 48
#define A4_FREQ 440.0f
#define MAX_NOTE 50
#define MIN_NOTE -44
#define SILENCE (MAX_NOTE + 1)

int main (int argc, char **argv) {
    State *state = (State *)calloc(1, sizeof(State));

    state->window_width = 1500;
    state->window_height = 1000;

    editor_init(state);

    SetTraceLogLevel(
        #ifdef DEBUG
            LOG_DEBUG
        #else
            LOG_WARNING
        #endif
    );

    #ifdef TEST_THREAD
        test_thread();
        return 0;
    #endif

    if (argc < 2) {
        printf("You are a horrible person\n");
        exit(-1);
    }

    InitAudioDevice();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(state->window_width, state->window_height, "Concerto Script");

    SetExitKey(KEY_NULL);

    state->font = LoadFont("Consolas.ttf");

    SetTargetFPS(60);

    Synthesizer *synthesizer = NULL;
    Thread *synth_thread;

    char *filename = argv[1];

    bool is_playing = false;

    editor_load_file(state, filename);

    while (!WindowShouldClose()) {
        state->delta_time = GetFrameTime();

        editor_input(state);

        switch (state->state) {
        default: break;
        case STATE_EDITOR_FIND_TEXT: {
            char buffer[64 + EDITOR_FINDER_BUFFER_MAX];
            if (state->editor.finder_match_idx >= 0) {
                sprintf(
                    buffer,
                    "Find text:\n\"%s\"\nMatch %i of %i",
                    state->editor.finder_buffer,
                    state->editor.finder_match_idx + 1,
                    state->editor.finder_matches
                );
            } else if (state->editor.finder_buffer_length > 0) {
                sprintf(buffer, "Find text:\n\"%s\"\nFound %i matches", state->editor.finder_buffer, state->editor.finder_matches);
            } else {
                sprintf(buffer, "Find text:\n\"%s\"", state->editor.finder_buffer);
            }
            console_set_text(state, buffer);
        } break;
        case STATE_EDITOR_GO_TO_LINE: {
            char buffer[64 + EDITOR_GO_TO_LINE_BUFFER_MAX];
            sprintf(buffer, "Go to line:\n[%s]", state->editor.go_to_line_buffer);
            console_set_text(state, buffer);
        } break;
        case STATE_TRY_COMPILE: {
            editor_save_file(state, filename);
            state->compiler_result = compile(state);
            if (state->compiler_result->error_type != NO_ERROR) {
                console_set_text(state, state->compiler_result->error_message);
                compiler_result_free(state);
                state->state = STATE_COMPILATION_ERROR;
                continue;
            }
            int synthesizer_error;
            synthesizer = synthesizer_allocate(
                state->compiler_result->tones,
                state->compiler_result->tone_amount,
                &synthesizer_error
            );
            if (synthesizer_error) {
                exit(synthesizer_error);
            }
            synth_thread = thread_create(synthesizer_run, synthesizer);
            if (synth_thread == NULL) {
                printf("Failed creating thread");
                exit(1);
            }
            is_playing = true;
            state->state = STATE_WAITING_TO_PLAY;
        } break;
        case STATE_INTERRUPT: {
            if (is_playing) {
                state->current_sound = NULL;
                synthesizer_cancel(synthesizer);
                thread_join(synth_thread);
                synthesizer_free(synthesizer);
                compiler_result_free(state);
                is_playing = false;
            }
            editor_load_file(state, filename);
            state->state = STATE_EDITOR;
        }
        }

        if (is_playing && (!is_synthesizer_sound_playing(state->current_sound))) {
            state->current_sound = synthesizer_try_pop(synthesizer);
            if (state->current_sound != NULL) {
                PlaySound((state->current_sound->sound));
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        switch (state->state) {
        default: {

        } break;
        case STATE_EDITOR: {
            editor_render_state_write(state);
        } break;
        case STATE_EDITOR_FILE_EXPLORER: {
            editor_render_state_write(state);
            console_render(state);
        } break;
        case STATE_EDITOR_FIND_TEXT: {
            editor_render_state_write(state);
            console_render(state);
        } break;
        case STATE_EDITOR_GO_TO_LINE: {
            editor_render_state_write(state);
            console_render(state);
        } break;
        case STATE_TRY_COMPILE: {
        } break;
        case STATE_COMPILATION_ERROR: {
            editor_render_state_write(state);
            console_render(state);
        } break;
        case STATE_WAITING_TO_PLAY: {
            if (state->current_sound != NULL) {
                state->state = STATE_PLAY;
                break;
            }
            editor_render_state_wait_to_play(state);
        } break;
        case STATE_PLAY: {
            if (state->current_sound == NULL) {
                state->state = STATE_EDITOR;
                break;
            }
            editor_render_state_play(state);
        } break;
        }

        EndDrawing();
    }

    UnloadFont(state->font);
    CloseWindow();
    CloseAudioDevice();

    compiler_result_free(state);
    editor_free(state);
    free(state);
}
