#include "raylib.h"
#include "main.h"
#include "windows_wrapper.h"

inline static void sound_buffer_free(Sound_Buffer *buffer) {
    for (int i = 0; i < buffer->sound_count; i++) {
        UnloadSound(buffer->sounds[i].sound);
        dyn_mem_release(buffer->sounds[i].raw_data);
    }
    buffer->sound_count = 0;
}

bool is_synthesizer_sound_playing(Synthesizer_Sound *s) {
    if (s == NULL) {
        return false;
    }

    return IsSoundPlaying(s->sound);
}

void synthesizer_init(Synthesizer *synthesizer) {
    synthesizer->current_sound_idx = -1;
    for (int i = 0; i < 2; i++) {
        synthesizer->buffers[i].mutex = mutex_create();
    }
    synthesizer->front_buffer = &synthesizer->buffers[0];
    synthesizer->back_buffer = &synthesizer->buffers[1];
}

void synthesizer_cancel(Synthesizer *synthesizer) {
    synthesizer->flags |= SYNTHESIZER_FLAG_SHOULD_CANCEL;
}

void synthesizer_reset(Synthesizer *synthesizer) {
    synthesizer->flags = SYNTHESIZER_FLAG_NONE;
    for (int i = 0; i < 2; i++) {
        Sound_Buffer *b = &synthesizer->buffers[i];
        mutex_lock(b->mutex);
            sound_buffer_free(b);
        mutex_unlock(b->mutex);
    }
    synthesizer->current_sound_idx = -1;
}

void synthesizer_back_buffer_generate_data(Synthesizer *synthesizer, Compiler *compiler) {
    Sound_Buffer *back_buffer = synthesizer->back_buffer;

    mutex_lock(back_buffer->mutex);
        for (int i = 0; i < compiler->tone_amount; i++) {
            back_buffer->sounds[i].tone = compiler->tones[i];
            back_buffer->sound_count++;
        }
    mutex_unlock(back_buffer->mutex);

    if (back_buffer->sound_count == 0) {
        synthesizer->flags |= SYNTHESIZER_FLAG_COMPLETED;
        return;
    }

    back_buffer->sound_count = 0;

    const int sample_rate = 44100;
    const int channels = 2;
    const int sample_size = 16; // 16 bits per sample
    int bytes_per_sample = sample_size / 8;

    for (int i = 0; i < SYNTHESIZER_TONE_CAPACITY; i++) {
        int frame_count = back_buffer->sounds[i].tone.duration * sample_rate;
        int data_size = frame_count * channels * bytes_per_sample;
        int16_t *audio_data = (int16_t *)dyn_mem_alloc(data_size);
        if (audio_data == NULL) {
            thread_error();
        }
        bool is_chord_silent = (back_buffer->sounds[i].tone.chord.size <= 0);
        if (is_chord_silent) {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int k = 0; k < channels; k++) {
                    audio_data[frame * channels + k] = 0;
                }
            }
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                float t = (float)frame / (float)(frame_count - 1); // Normalized time (0.0 to 1.0)

                Chord *chord = &back_buffer->sounds[i].tone.chord;
                float samples[chord->size];
                switch (back_buffer->sounds[i].tone.waveform) {
                case WAVEFORM_SINE: {
                    for (int j = 0; j < chord->size; j++) {
                        float x = chord->frequencies[j] * frame / sample_rate;
                        samples[j] = sinf(2.0f * PI * x);
                    }
                } break;
                case WAVEFORM_TRIANGLE: {
                    for (int j = 0; j < chord->size; j++) {
                        float x = chord->frequencies[j] * frame / sample_rate;
                        samples[j] = 4.0f * fabsf(x - floorf(x + 0.75f) + 0.25f) - 1.0f;
                    }
                } break;
                case WAVEFORM_SQUARE: {
                    for (int j = 0; j < chord->size; j++) {
                        float x = chord->frequencies[j] * frame / sample_rate;
                        samples[j] = 4.0f * floorf(x) - 2.0f * floorf(2.0f * x) + 1.0f;
                    }
                } break;
                case WAVEFORM_SAWTOOTH: {
                    for (int j = 0; j < chord->size; j++) {
                        float x = chord->frequencies[j] * frame / sample_rate;
                        samples[j] = 2.0f * (x - floorf(x + 0.5f));
                    }
                } break;
                }

                float envelope; // Default to full volume
                if (frame < SYNTHESIZER_FADE_FRAMES) {
                    // Fade-in
                    envelope = (float)frame / (float)SYNTHESIZER_FADE_FRAMES;
                } else if (frame >= frame_count - SYNTHESIZER_FADE_FRAMES) {
                    // Fade-out
                    envelope = (float)(frame_count - frame) / (float)SYNTHESIZER_FADE_FRAMES;
                } else {
                    envelope = 1.0f;
                }

                float combined_sample = 0;
                for (int j = 0; j < chord->size; j++) {
                    combined_sample += (samples[j] * envelope);
                }
                combined_sample /= chord->size;

                for (int k = 0; k < channels; k++) {
                    audio_data[frame * channels + k] = (int16_t)(combined_sample * 32767); // 32767 is the max value for 16-bit audio
                }
            }
        }

        Wave wave;
        wave.frameCount = frame_count;
        wave.sampleRate = sample_rate;
        wave.sampleSize = sample_size;
        wave.channels = channels;
        wave.data = audio_data;

        mutex_lock(back_buffer->mutex);
            back_buffer->sounds[i].raw_data = audio_data;
            back_buffer->sounds[i].sound = LoadSoundFromWave(wave);
            back_buffer->sound_count++;
            if (i >= (compiler->tone_amount - 1)) {
                synthesizer->flags |= SYNTHESIZER_FLAG_SOUND_BUFFER_SWAP_REQUIRED;
                mutex_unlock(back_buffer->mutex);
                break;
            }
        mutex_unlock(back_buffer->mutex);

        if (has_flag(synthesizer->flags, SYNTHESIZER_FLAG_SHOULD_CANCEL)) {
            for (int i = 0; i < 2; i++) {
                Sound_Buffer *b = &synthesizer->buffers[i];
                mutex_lock(b->mutex);
                    sound_buffer_free(b);
                mutex_unlock(b->mutex);
            }
            break;
        }
    }
}

Synthesizer_Sound *synthesizer_front_buffer_try_get_sound(Synthesizer *synthesizer) {
    Sound_Buffer *front_buffer = synthesizer->front_buffer;
    mutex_lock(front_buffer->mutex);
        if (synthesizer->current_sound_idx >= (front_buffer->sound_count - 1)) {
            mutex_unlock(front_buffer->mutex);
            return NULL;
        }
        synthesizer->current_sound_idx++;
    mutex_unlock(front_buffer->mutex);
    return &(front_buffer->sounds[synthesizer->current_sound_idx]);
}

bool synthesizer_swap_sound_buffers(Synthesizer *synthesizer) {
    mutex_lock(synthesizer->front_buffer->mutex);
    mutex_lock(synthesizer->back_buffer->mutex);

    Sound_Buffer *temp_buffer = synthesizer->front_buffer;
    synthesizer->front_buffer = synthesizer->back_buffer;
    synthesizer->back_buffer = temp_buffer;

    sound_buffer_free(synthesizer->back_buffer);

    bool has_sounds = synthesizer->front_buffer->sound_count > 0;

    mutex_unlock(synthesizer->front_buffer->mutex);
    mutex_unlock(synthesizer->back_buffer->mutex);

    synthesizer->current_sound_idx = -1;

    return has_sounds;
}
