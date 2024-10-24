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

    state->font = LoadFont("Consolas.ttf");

    SetTargetFPS(60);

    bool console_active = false;

    Synthesizer *synthesizer = NULL;
    Thread *synth_thread;

    char *filename = argv[1];

    bool is_playing = false;

    editor_load_file(state, filename);

    while (!WindowShouldClose()) {
        state->delta_time = GetFrameTime();
        if (console_active) {
            if (IsKeyPressed(KEY_ENTER)) {
                console_active = false;
            }
        } else {
            editor_input(state);

            if (is_playing && (!is_synthesizer_sound_playing(state->current_sound) || state->current_sound == NULL)) {
                state->current_sound = synthesizer_try_pop(synthesizer);
                if (state->current_sound != NULL) {
                    PlaySound((state->current_sound->sound));
                }
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (console_active) {
            editor_render_state_write(state);
            console_render(state);
        } else {
            switch (state->editor.state) {
            case EDITOR_STATE_WRITE: {
                editor_render_state_write(state);
            } break;
            case EDITOR_STATE_READY_TO_PLAY: {
                editor_save_file(state, filename);
                state->compiler_result = compile(state);
                if (state->compiler_result->error.type != NO_ERROR) {
                    console_print_str(state, state->compiler_result->error.message);
                    free_compiler_result(state->compiler_result);
                    state->editor.state = EDITOR_STATE_WRITE;
                    console_active = true;
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
                state->editor.state = EDITOR_STATE_WAITING_TO_PLAY;
            } break;
            case EDITOR_STATE_WAITING_TO_PLAY: {
                if (state->current_sound != NULL) {
                    state->editor.state = EDITOR_STATE_PLAY;
                    break;
                }
                editor_render_state_wait_to_play(state);
            } break;
            case EDITOR_STATE_PLAY: {
                if (state->current_sound == NULL) {
                    state->editor.state = EDITOR_STATE_WRITE;
                    break;
                }
                editor_render_state_play(state);
            } break;
            case EDITOR_STATE_INTERRUPT: {
                if (is_playing) {
                    state->current_sound = NULL;
                    synthesizer_cancel(synthesizer);
                    thread_join(synth_thread);
                    synthesizer_free(synthesizer);
                    free_compiler_result(state->compiler_result);
                    is_playing = false;
                }
                editor_load_file(state, filename);
                state->editor.state = EDITOR_STATE_WRITE;
            }
            }
        }

        EndDrawing();
    }


    UnloadFont(state->font);
    CloseWindow();
    CloseAudioDevice();
    if (state->compiler_result != NULL) {
        free_compiler_result(state->compiler_result);
    }
    free(state);
}
