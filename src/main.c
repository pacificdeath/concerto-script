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

    state->console_highlight_idx = -1;

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

    InitAudioDevice();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(state->window_width, state->window_height, "Concerto Script");

    SetExitKey(KEY_NULL);

    state->font = LoadFont("Consolas.ttf");

    SetTargetFPS(60);

    Synthesizer *synthesizer = NULL;
    Thread *synth_thread;

    char *filename = (argc > 1) ? argv[1] : NULL;

    bool is_playing = false;

    editor_load_file(state, filename);

    while (!WindowShouldClose()) {
        state->keyboard_layout = get_keyboard_layout();
        state->delta_time = GetFrameTime();

        editor_input(state);

        switch (state->state) {
        default: break;
        case STATE_TRY_COMPILE: {
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
        ClearBackground(EDITOR_BG_COLOR);

        switch (state->state) {
        default: {

        } break;
        case STATE_EDITOR: {
            editor_render_state_write(state);
        } break;
        case STATE_EDITOR_SAVE_FILE:
        case STATE_EDITOR_SAVE_FILE_ERROR:
        case STATE_EDITOR_FILE_EXPLORER:
        case STATE_EDITOR_FIND_TEXT:
        case STATE_EDITOR_GO_TO_LINE:
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
