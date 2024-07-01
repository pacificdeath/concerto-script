#ifndef CONCERTO_SCRIPT_TYPES
#define CONCERTO_SCRIPT_TYPES

typedef struct Tone {
    int note;
    float frequency;
    float duration;
    uint16_t scale;
} Tone;

#endif