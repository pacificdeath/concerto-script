#include "raylib.h"

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "bird_windows.h"
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

typedef struct {
    int tex;
    float anim_time;
} Bird;

int main (int argc, char **argv) {
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

    const int window_width = 1200;
    const int window_height = 900;
    const int bird_amount = 12;
    const int bird_row_amount = bird_amount / 2;
    const float bird_width = (window_width / bird_row_amount);
    InitWindow(window_width, window_height, "Bird");
    // Textures MUST be loaded after Window initialization (OpenGL context is required)
    int bird_tex_amount = 4;
    int bird_tex_px = 32;
    float bird_scale = (window_width / bird_tex_px) / bird_row_amount;
    Texture2D bird_textures[bird_tex_amount];
    bird_textures[0] = LoadTexture("textures/bird0.png");
    bird_textures[1] = LoadTexture("textures/bird1.png");
    bird_textures[2] = LoadTexture("textures/bird2.png");
    bird_textures[3] = LoadTexture("textures/bird3.png");
    Texture2D birdsing_textures[bird_tex_amount];
    birdsing_textures[0] = LoadTexture("textures/birdsing0.png");
    birdsing_textures[1] = LoadTexture("textures/birdsing1.png");
    birdsing_textures[2] = LoadTexture("textures/birdsing2.png");
    birdsing_textures[3] = LoadTexture("textures/birdsing3.png");
    Texture2D nonscalebird_textures[bird_tex_amount];
    nonscalebird_textures[0] = LoadTexture("textures/nonscalebird0.png");
    nonscalebird_textures[1] = LoadTexture("textures/nonscalebird1.png");
    nonscalebird_textures[2] = LoadTexture("textures/nonscalebird2.png");
    nonscalebird_textures[3] = LoadTexture("textures/nonscalebird3.png");
    Texture2D nonscalebirdsing_textures[bird_tex_amount];
    nonscalebirdsing_textures[0] = LoadTexture("textures/nonscalebirdsing0.png");
    nonscalebirdsing_textures[1] = LoadTexture("textures/nonscalebirdsing1.png");
    nonscalebirdsing_textures[2] = LoadTexture("textures/nonscalebirdsing2.png");
    nonscalebirdsing_textures[3] = LoadTexture("textures/nonscalebirdsing3.png");
    Texture2D unusedbird_textures[bird_tex_amount];
    unusedbird_textures[0] = LoadTexture("textures/unusedbird0.png");
    unusedbird_textures[1] = LoadTexture("textures/unusedbird1.png");
    unusedbird_textures[2] = LoadTexture("textures/unusedbird2.png");
    unusedbird_textures[3] = LoadTexture("textures/unusedbird3.png");
    float bird_anim_rate = 0.1;
    Bird birds[bird_amount];
    for (int i = 0; i < bird_amount; i++) {
        birds[i].tex = 0;
        birds[i].anim_time = (bird_anim_rate / bird_amount) * i;
    }

    SetTargetFPS(60);

    Synthesizer_Sound *current_sound = NULL;

    Console *console = console_allocate();
    bool console_active = false;

    Editor *editor = editor_allocate();
    bool editor_active = false;

    bool birds_active = false;

    Compiler_Result *compiler_result = NULL;

    Synthesizer *synthesizer = NULL;
    Thread *synth_thread;

    Font font = LoadFont("Consolas.ttf");

    char *filename = argv[1];

    uint16_t used_notes = ~0;
    uint16_t current_scale = ~0;

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();
        if (birds_active && (!is_synthesizer_sound_playing(current_sound) || current_sound == NULL)) {
            current_sound = synthesizer_try_pop(synthesizer);
            if (current_sound != NULL) {
                PlaySound((current_sound->sound));
            }
        }
        if (console_active) {
            if (IsKeyPressed(KEY_ENTER)) {
                console_active = false;
            }
        } else if (editor_active && !console_active) {
            editor_input(editor, window_width, window_height, delta_time);
            if (editor->command != EDITOR_COMMAND_NONE) {
                char *data[editor->line_count];
                for (int i = 0; i < editor->line_count; i++) {
                    data[i] = editor->lines[i];
                }
                switch (editor->command) {
                case EDITOR_COMMAND_PLAY:
                    editor->command = EDITOR_COMMAND_NONE;
                    editor_save_file(editor, filename);
                    compiler_result = compile(data, editor->line_count);
                    if (compiler_result->error.type != NO_ERROR) {
                        console_print_str(console, compiler_result->error.message);
                        free_compiler_result(compiler_result);
                        console_active = true;
                        continue;
                    }
                    int synthesizer_error;
                    synthesizer = synthesizer_allocate(
                        compiler_result->tones,
                        compiler_result->tone_amount,
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
                    birds_active = true;
                    used_notes = compiler_result->used_notes;
                    editor_active = false;
                    break;
                default:
                    break;
                }
            }
        } else {
            if (IsKeyPressed(KEY_E)) {
                if (birds_active) {
                    current_sound = NULL;
                    synthesizer_cancel(synthesizer);
                    thread_join(synth_thread);
                    synthesizer_free(synthesizer);
                    free_compiler_result(compiler_result);
                    birds_active = false;
                }
                editor_load_file(editor, filename);
                editor_active = true;
            }
        }
        BeginDrawing();
        ClearBackground(SKYBLUE);
        for (int bird_idx = 0; bird_idx < bird_amount; bird_idx++) {
            Bird *bird = &birds[bird_idx];
            bird->anim_time += delta_time;
            if (bird->anim_time >= bird_anim_rate) {
                bird->anim_time = 0.0f;
                bird->tex = (bird->tex + 1) % bird_tex_amount;
            }
            int c_offset_from_a = 3;
            int offset_to_avoid_negatives = 96;
            int bird_note = (bird_idx + c_offset_from_a) % 12;
            bool unused = (used_notes & (1 << bird_note)) == 0;
            bool is_singing;
            if (!unused && birds_active && is_synthesizer_sound_playing(current_sound)) {
                current_scale = current_sound->tone.scale;
                if (current_sound->tone.start_note == SILENCE) {
                    is_singing = false;
                } else {
                    is_singing = (
                        current_sound->tone.start_note
                        + offset_to_avoid_negatives
                        - c_offset_from_a
                    ) % 12 == bird_idx;
                }
            } else {
                is_singing = false;
            }
            bool in_scale = (current_scale & (1 << bird_note)) != 0;
            int y_offset = (window_height / 3);
            if (bird_idx >= bird_row_amount) {
                y_offset *= 2;
            }
            if (is_singing) {
                y_offset -= bird_tex_px;
            }
            float x = (bird_width / 2.0f) + (bird_width * (bird_idx % bird_row_amount));
            float y = sin((GetTime() + x) * 0.5f) * 10.0f + y_offset;
            x -= bird_width / 4.0f;
            y -= (window_height / 3.0f) / 4.0f;
            Vector2 position = { x, y };
            float rotation = 0.0f;
            Texture2D *texture;
            if (is_singing) {
                if (in_scale) {
                    texture = &birdsing_textures[bird->tex];
                } else {
                    texture = &nonscalebirdsing_textures[bird->tex];
                }
            } else if (!unused) {
                if (in_scale) {
                    texture = &bird_textures[bird->tex];
                } else {
                    texture = &nonscalebird_textures[bird->tex];
                }
            } else {
                texture = &unusedbird_textures[bird->tex];
            }
            DrawTextureEx(*texture, position, rotation, bird_scale, WHITE);
        }

        if (editor_active) {
            editor_render(editor, window_width, window_height, &font, delta_time);
        }

        if (console_active) {
            console_render(console, window_width, window_height, &font);
        }

        EndDrawing();
    }
    for (int i = 0; i < bird_tex_amount; i++) {
        UnloadTexture(bird_textures[i]);
        UnloadTexture(birdsing_textures[i]);
    }
    UnloadFont(font);
    CloseWindow();
    CloseAudioDevice();
    if (compiler_result != NULL) {
        free_compiler_result(compiler_result);
    }
    console_free(console);
}
