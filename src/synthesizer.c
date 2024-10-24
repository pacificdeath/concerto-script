#include "raylib.h"
#include "main.h"
#include "windows_wrapper.h"

bool is_synthesizer_sound_playing(Synthesizer_Sound *s) {
    return s != NULL && IsSoundPlaying((s->sound));
}

Synthesizer *synthesizer_allocate(Tone *tones, int tone_amount, int *error) {
    Synthesizer *synthesizer = (Synthesizer *)malloc(sizeof(Synthesizer));
    if (synthesizer == NULL) {
        printf("malloc failed for synthesizer\n");
        (*error) = 1;
        return synthesizer;
    }
    synthesizer->sounds = malloc(sizeof(Synthesizer_Sound) * tone_amount);
    if (synthesizer->sounds == NULL) {
        printf("malloc failed for tones\n");
        (*error) = 2;
        return synthesizer;
    }
    for (int i = 0; i < tone_amount; i += 1) {
        synthesizer->sounds[i].tone = tones[i];
    }
    synthesizer->current_sound_idx = -1;
    synthesizer->sound_count = 0;
    synthesizer->sound_capacity = tone_amount;
    synthesizer->should_cancel = false;
    synthesizer->mutex = mutex_create();
    (*error) = 0;
    return synthesizer;
}

Synthesizer_Sound *synthesizer_try_pop(Synthesizer *synthesizer) {
    mutex_lock(synthesizer->mutex);
        if ((synthesizer->current_sound_idx) >= (synthesizer->sound_count - 1)) {
            mutex_unlock(synthesizer->mutex);
            return NULL;
        }
        synthesizer->current_sound_idx += 1;
    mutex_unlock(synthesizer->mutex);
    return &(synthesizer->sounds[synthesizer->current_sound_idx]);
}

void synthesizer_cancel(Synthesizer *synthesizer) {
    mutex_lock(synthesizer->mutex);
        synthesizer->should_cancel = true;
    mutex_unlock(synthesizer->mutex);
}

void synthesizer_free(Synthesizer *synthesizer) {
    mutex_lock(synthesizer->mutex);
    for (int i = 0; i < synthesizer->sound_count; i += 1) {
        UnloadSound((synthesizer->sounds[i].sound));
        free(synthesizer->sounds[i].raw_data);
    }
    free(synthesizer->sounds);
    mutex_destroy(synthesizer->mutex);
    free(synthesizer);
}

void synthesizer_run(void *data) {
    Synthesizer *synthesizer = (Synthesizer *)data;
    const int sample_rate = 44100;
    const int channels = 2;
    const int sample_size = 16; // 16 bits per sample
    int bytes_per_sample = sample_size / 8;
    for (int i = 0; i < synthesizer->sound_capacity; i += 1) {
        int frame_count = synthesizer->sounds[i].tone.duration * sample_rate;
        int data_size = frame_count * channels * bytes_per_sample;
        int16_t *audio_data = (int16_t *)malloc(data_size);
        if (audio_data == NULL) {
            printf("malloc failed for audio_data");
            return;
        }
        if (synthesizer->sounds[i].tone.start_note == SILENCE) {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int k = 0; k < channels; k++) {
                    audio_data[frame * channels + k] = 0;
                }
            }
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                float t = (float)frame / (float)(frame_count - 1); // Normalized time (0.0 to 1.0)
                float current_frequency = (1.0f - t) * synthesizer->sounds[i].tone.start_frequency + t * synthesizer->sounds[i].tone.end_frequency;
                // float sample = sinf(2.0f * PI * current_frequency * frame / sample_rate);
                float x = current_frequency * frame / sample_rate;
                float sample = 4.0f * fabs(x - floor(x + 0.75f) + 0.25f) - 1.0f;
                float envelope = 0.5f * (1.0f - cosf(2.0f * PI * frame / (frame_count - 1)));
                sample *= envelope;
                for (int k = 0; k < channels; k++) {
                    audio_data[frame * channels + k] = (int16_t)(sample * 32767); // 32767 is the max value for 16-bit audio
                }
            }
        }

        Wave wave;
        wave.frameCount = frame_count;
        wave.sampleRate = sample_rate;
        wave.sampleSize = sample_size;
        wave.channels = channels;
        wave.data = audio_data;

        mutex_lock(synthesizer->mutex);
            synthesizer->sounds[i].raw_data = audio_data;
            synthesizer->sounds[synthesizer->sound_count].sound = LoadSoundFromWave(wave);
            synthesizer->sound_count += 1;
        mutex_unlock(synthesizer->mutex);

        if (synthesizer->should_cancel) {
            return;
        }
    }
}

#ifdef DEBUG_LIST_FREQUENCIES
    static void print_tones(int tone_amount, Tone *tones) {
        printf("TONE AMOUNT: %d\n", tone_amount);
        for (int i = 0; i < tone_amount; i++) {
            if (tones[i].note == SILENCE) {
                printf("silent  %.2fsec\n",
                    tones[i].duration
                );
            } else {
                printf("%-6d  %.2fsec  %.2fHz\n",
                    tones[i].note,
                    tones[i].duration,
                    tones[i].frequency
                );
            }
        }
    }
#endif
