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

void compiler_thread(void *data) {
    State *state = (State *)data;
    do {
        while (!can_compile(&state->compiler)) {
            if (has_flag(state->compiler.flags, COMPILER_FLAG_CANCELLED)) {
                return;
            }
        }
        compile(state);
        synthesizer_back_buffer_generate_data(&state->synthesizer, &state->compiler);
    } while (has_flag(state->compiler.flags, COMPILER_FLAG_IN_PROCESS));
}

int main(int argc, char **argv) {
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
    SetTargetFPS(60);

    state->font = LoadFont("Consolas.ttf");

    char *filename = (argc > 1) ? argv[1] : NULL;
    editor_load_file(state, filename);

    compiler_init(&state->compiler);
    synthesizer_init(&state->synthesizer);

    bool is_playing = false;

    while (!WindowShouldClose()) {
        state->keyboard_layout = get_keyboard_layout();
        state->delta_time = GetFrameTime();

        editor_input(state);

        switch (state->state) {
        default: break;
        case STATE_TRY_COMPILE: {
            compile(state);
            if (state->compiler.error_type != NO_ERROR) {
                console_set_text(state, state->compiler.error_message);
                compiler_reset(&state->compiler);
                state->state = STATE_COMPILATION_ERROR;
                continue;
            }
            synthesizer_back_buffer_generate_data(&state->synthesizer, &state->compiler);
            synthesizer_swap_sound_buffers(&state->synthesizer);
            compiler_output_handled(&state->compiler);

            if (has_flag(state->compiler.flags, COMPILER_FLAG_IN_PROCESS)) {
                state->compiler.thread = thread_create(compiler_thread, state);
                if (state->compiler.thread == NULL) {
                    exit(1);
                }
            }

            is_playing = true;
            state->state = STATE_WAITING_TO_PLAY;
        } break;
        case STATE_WAITING_TO_PLAY: {
            if (state->current_sound != NULL) {
                state->state = STATE_PLAY;
                break;
            }
        } break;
        case STATE_PLAY: {
            if (state->current_sound == NULL) {
                compiler_output_handled(&state->compiler);
                bool synthesizer_has_more_sounds = synthesizer_swap_sound_buffers(&state->synthesizer);
                if (!synthesizer_has_more_sounds) {
                    state->state = STATE_EDITOR;
                    synthesizer_reset(&state->synthesizer);
                    compiler_reset(&state->compiler);
                    is_playing = false;
                }
            }
        } break;
        case STATE_INTERRUPT: {
            if (is_playing) {
                synthesizer_reset(&state->synthesizer);
                compiler_reset(&state->compiler);
                is_playing = false;
            }
            state->state = STATE_EDITOR;
        }
        }

        if (is_playing && !is_synthesizer_sound_playing(state->current_sound)) {
            state->current_sound = synthesizer_front_buffer_try_get_sound(&state->synthesizer);
            if (state->current_sound != NULL) {
                PlaySound(state->current_sound->sound);
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
            editor_render_state_wait_to_play(state);
        } break;
        case STATE_PLAY: {
            editor_render_state_play(state);
        } break;
        }

        EndDrawing();
    }

    UnloadFont(state->font);
    CloseWindow();
    CloseAudioDevice();

    compiler_free(&state->compiler);
    editor_free(state);
    free(state);
}
