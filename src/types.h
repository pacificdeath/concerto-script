#ifndef CONCERTO_SCRIPT_TYPES
#define CONCERTO_SCRIPT_TYPES

#define OCTAVE_OFFSET 12.0f
#define MAX_OCTAVE 8
#define A4_OFFSET 48
#define A4_FREQ 440.0f
#define MAX_NOTE 50
#define MIN_NOTE -44
#define SILENCE (MAX_NOTE + 1)

typedef struct Tone {
    int start_note;
    int end_note;
    float start_frequency;
    int end_frequency;
    float duration;
    uint16_t scale;
} Tone;

#endif