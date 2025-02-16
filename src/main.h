#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <stdint.h>

#include "../raylib-5.0_win64_mingw-w64/include/raylib.h"
#include "windows_wrapper.h"

#define WINDOW_BASE_WIDTH 1500
#define WINDOW_BASE_HEIGHT 1000

#define THEMES_DIRECTORY "themes"
#define THEME_DEFAULT THEMES_DIRECTORY"/default"
#define PROGRAMS_DIRECTORY "programs"

#define OCTAVE 12
#define MAX_OCTAVE 8
#define A4_OFFSET 48
#define A4_FREQ 440.0f
#define MAX_NOTE 50
#define MIN_NOTE -44
#define SILENCE (MAX_NOTE + 1)
#define COMMENT_CHAR '!'

#define EDITOR_MAX_VISUAL_LINES 30
#define EDITOR_LINE_CAPACITY 256
#define EDITOR_LINE_MAX_LENGTH 64
#define EDITOR_LINE_NUMBER_PADDING 5
#define AUTO_CLICKABLE_KEYS_AMOUNT 6
#define AUTOCLICK_LOWER_THRESHOLD 0.3F
#define AUTOCLICK_UPPER_THRESHOLD 0.33F
#define EDITOR_CURSOR_WIDTH 3
#define EDITOR_CURSOR_ANIMATION_SPEED 10.0f
#define EDITOR_SCROLL_MULTIPLIER 3
#define EDITOR_FILE_SEARCH_BUFFER_MAX 32
#define EDITOR_FILENAME_MAX_LENGTH 128
#define EDITOR_FILENAMES_MAX_AMOUNT 10
#define EDITOR_FINDER_BUFFER_MAX 32
#define EDITOR_GO_TO_LINE_BUFFER_MAX 5
#define EDITOR_UNDO_BUFFER_MAX 64

#define CONSOLE_LINE_CAPACITY 32
#define CONSOLE_LINE_MAX_LENGTH 255

#define VARIABLE_MAX_COUNT 255

#define SYNTHESIZER_FADE_FRAMES 500
#define SYNTHESIZER_TONE_CAPACITY 8

typedef enum Waveform {
    WAVEFORM_NONE,
    WAVEFORM_SINE,
    WAVEFORM_TRIANGLE,
    WAVEFORM_SQUARE,
    WAVEFORM_SAWTOOTH,
} Waveform;

typedef struct Chord {
    int8_t size;
    int8_t notes[OCTAVE];
    float frequencies[OCTAVE];
} Chord;

typedef struct Tone {
    Waveform waveform;
    uint32_t token_idx;
    uint16_t line_idx;
    uint16_t char_idx;
    uint16_t char_count;
    Chord chord;
    float duration;
} Tone;

typedef enum Editor_Theme_Status {
    EDITOR_THEME_OK,
    EDITOR_THEME_ERROR,
    EDITOR_THEME_NO_CHANGES,
} Editor_Theme_Status;

typedef struct Editor_Theme {
    char filepath[EDITOR_FILENAME_MAX_LENGTH];
    long file_mod_time;
    Color bg;
    Color fg;
    Color play;
    Color wait;
    Color keyword;
    Color note;
    Color paren;
    Color space;
    Color comment;
    Color linenumber;
    Color cursor;
    Color selection;
    Color console_bg;
    Color console_foreground;
    Color console_foreground_error;
    Color console_highlight;
    Color play_cursor;
} Editor_Theme;

typedef enum Editor_Action_Type {
    EDITOR_ACTION_ADD_LINE,
    EDITOR_ACTION_ADD_CHAR,
    EDITOR_ACTION_DELETE_CHAR,
    EDITOR_ACTION_DELETE_LINE,
    EDITOR_ACTION_DELETE_STRING,
    EDITOR_ACTION_ADD_STRING,
} Editor_Action_Type;

typedef struct Editor_Coord {
    int y;
    int x;
} Editor_Coord;

typedef struct Editor_Action {
    Editor_Action_Type type;
    Editor_Coord coord;
    union {
        char character;
        char *string;
        Editor_Coord extra_coord;
    };
} Editor_Action;

typedef struct Editor {
    Font font;

    Editor_Theme theme;

    char current_file[EDITOR_FILENAME_MAX_LENGTH];

    int line_count;
    char lines[EDITOR_LINE_CAPACITY][EDITOR_LINE_MAX_LENGTH];

    float size_multiplier;

    Editor_Coord cursor;
    int selection_x;
    int selection_y;
    int visual_vertical_offset;
    int preferred_x;

    int autoclick_key;
    float autoclick_down_time;

    int clipboard_line_count;
    char *clipboard;

    char file_search_buffer[EDITOR_FILE_SEARCH_BUFFER_MAX];
    int file_search_buffer_length;
    char filename_buffer[EDITOR_FILENAMES_MAX_AMOUNT * EDITOR_FILENAME_MAX_LENGTH];
    int file_cursor;

    char finder_buffer[EDITOR_FINDER_BUFFER_MAX];
    int finder_buffer_length;
    int finder_match_idx;
    int finder_matches;

    char go_to_line_buffer[EDITOR_GO_TO_LINE_BUFFER_MAX];
    int go_to_line_buffer_length;

    float cursor_anim_time;

    int undo_buffer_start;
    int undo_buffer_end;
    Editor_Action undo_buffer[EDITOR_UNDO_BUFFER_MAX];
    int undo_buffer_size;

    char console_text[CONSOLE_LINE_CAPACITY][CONSOLE_LINE_MAX_LENGTH];
    int console_line_count;
    int console_highlight_idx;
} Editor;

typedef struct Editor_Selection_Data {
    Editor_Coord start;
    Editor_Coord end;
} Editor_Selection_Data;

typedef struct Editor_Cursor_Render_Data {
    int y;
    int x;
    int line_number_offset;
    int visual_vertical_offset;
    int char_width;
    int line_height;
    float alpha;
} Editor_Cursor_Render_Data;

typedef struct Editor_Selection_Render_Data {
    int line;
    int line_number_offset;
    int visual_vertical_offset;
    int start_x;
    int end_x;
    int char_width;
    int line_height;
} Editor_Selection_Render_Data;

typedef enum Token_Type {
    TOKEN_NONE = 0,
    TOKEN_START,
    TOKEN_SINE,
    TOKEN_TRIANGLE,
    TOKEN_SQUARE,
    TOKEN_SAWTOOTH,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_SEMI,
    TOKEN_RISE,
    TOKEN_FALL,
    TOKEN_PAREN_OPEN,
    TOKEN_PAREN_CLOSE,
    TOKEN_NOTE,
    TOKEN_BPM,
    TOKEN_PLAY,
    TOKEN_WAIT,
    TOKEN_CHORD,
    TOKEN_SCALE,
    TOKEN_REPEAT,
    TOKEN_ROUNDS,
    TOKEN_FOREVER,
    TOKEN_DEFINE,
} Token_Type;

typedef union Token_Value {
    int int_number;
    struct {
        float duration;
        int char_count;
    } play_or_wait;
    char *string;
} Token_Value;

typedef struct Token {
    enum Token_Type type;
    int address;
    int line_number;
    int char_index;
    Token_Value value;
} Token;

typedef struct Token_Variable {
    char *ident;
    int address;
} Token_Variable;

typedef struct Repetition {
    int target;
    int round;
} Repetition;

typedef enum Compiler_Flags {
    COMPILER_FLAG_NONE = 0,
    COMPILER_FLAG_CANCELLED = 1 << 0,
    COMPILER_FLAG_IN_PROCESS = 1 << 1,
    COMPILER_FLAG_OUTPUT_AVAILABLE = 1 << 2,
    COMPILER_FLAG_OUTPUT_HANDLED = 1 << 3,
} Compiler_Flags;

typedef enum Compiler_Error {
    NO_ERROR = 0,
    ERROR_NO_SOUND,
    ERROR_SYNTAX_ERROR,
    ERROR_INVALID_SEMI,
    ERROR_UNKNOWN_IDENTIFIER,
    ERROR_EXPECTED_IDENTIFIER,
    ERROR_EXPECTED_PAREN_OPEN,
    ERROR_NO_MATCHING_CLOSING_PAREN,
    ERROR_MULTIPLE_DEFINITIONS,
    ERROR_NO_SCALE_IDENTIFIER,
    ERROR_EXPECTED_NUMBER,
    ERROR_NUMBER_TOO_BIG,
    ERROR_NO_MATCHING_OPENING_PAREN,
    ERROR_NESTING_TOO_DEEP,
    ERROR_CHORD_CAN_ONLY_CONTAIN_NOTES,
    ERROR_CHORD_CAN_NOT_BE_EMPTY,
    ERROR_CHORD_TOO_MANY_NOTES,
    ERROR_SCALE_CAN_ONLY_CONTAIN_NOTES,
    ERROR_SCALE_CAN_NOT_BE_EMPTY,
} Compiler_Error;

typedef struct Compiler_Error_Address {
    Compiler_Error type;
    int token_idx;
} Compiler_Error_Address;

typedef struct Parser {
    int token_idx;
    int nest_idx;
    Repetition nest_repetitions[16];
    int current_bpm;
    Waveform current_waveform;
    int token_ptr_return_positions[32];
    int token_ptr_return_idx;
    Chord current_chord;
    int current_scale;
} Parser;

typedef struct Compiler {
    Compiler_Flags flags;
    Compiler_Error error_type;
    Parser parser;
    char error_message[256];
    char **data;
    int data_len;
    int line_number;
    int char_idx;
    int token_amount;
    Token *tokens;
    int tone_amount;
    Tone tones[SYNTHESIZER_TONE_CAPACITY];
    Token_Variable variables[VARIABLE_MAX_COUNT];
    int variable_count;
    Thread thread;
    Mutex mutex;
    int extra_compiler_entry_token_idx;
} Compiler;

typedef struct Synthesizer_Sound {
    int16_t *raw_data;
    Tone tone;
    Sound sound;
} Synthesizer_Sound;

typedef enum Synthesizer_Flags {
    SYNTHESIZER_FLAG_NONE = 0,
    SYNTHESIZER_FLAG_SHOULD_CANCEL = 1 << 0,
    SYNTHESIZER_FLAG_SOUND_BUFFER_SWAP_REQUIRED = 1 << 1,
    SYNTHESIZER_FLAG_COMPLETED = 1 << 2,
} Synthesizer_Flags;

typedef struct Sound_Buffer {
    Synthesizer_Sound sounds[SYNTHESIZER_TONE_CAPACITY];
    int sound_count;
    Mutex mutex;
} Sound_Buffer;

typedef struct Synthesizer {
    Synthesizer_Flags flags;
    int current_sound_idx;
    Sound_Buffer buffers[2];
    Sound_Buffer *front_buffer;
    Sound_Buffer *back_buffer;
} Synthesizer;

typedef enum Big_State {
    STATE_EDITOR,
    STATE_EDITOR_THEME_ERROR,
    STATE_EDITOR_SAVE_FILE,
    STATE_EDITOR_SAVE_FILE_ERROR,
    STATE_EDITOR_FILE_EXPLORER_THEMES,
    STATE_EDITOR_FILE_EXPLORER_PROGRAMS,
    STATE_EDITOR_FIND_TEXT,
    STATE_EDITOR_GO_TO_LINE,
    STATE_TRY_COMPILE,
    STATE_COMPILATION_ERROR,
    STATE_WAITING_TO_PLAY,
    STATE_PLAY,
    STATE_INTERRUPT,
    STATE_QUIT,
} Big_State;

typedef struct State {
    int window_width;
    int window_height;
    Keyboard_Layout keyboard_layout;
    Big_State state;
    float delta_time;
    Editor editor;
    Compiler compiler;
    Synthesizer synthesizer;
    Synthesizer_Sound *current_sound;
    float sound_time;
} State;

inline static void resize_window(State *state, float multiplier) {
    state->window_width = WINDOW_BASE_WIDTH * multiplier;
    state->window_height = WINDOW_BASE_HEIGHT * multiplier;
    SetWindowSize(
        state->window_width,
        state->window_height
    );
}

inline static bool is_alphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline static bool is_numeric(char c) {
    return (c >= '0' && c <= '9');
}

inline static bool is_uppercase(char c) {
    return (c >= 'A' && c <= 'Z');
}

inline static int is_valid_in_identifier(char c) {
    return is_alphabetic(c) || is_numeric(c) || c == '_';
}

inline static bool has_flag(int flags, int flag) {
    return (flags & flag) == flag;
}

#ifdef DEBUG
#include <stdio.h>
static int allocations = 0;
#endif

inline static void *dyn_mem_alloc(size_t size) {
    #ifdef DEBUG
        allocations++;
        void *m = malloc(size);
        printf("malloc %p (%i)\n", m, allocations);
        return m;
    #else
        return malloc(size);
    #endif
}

inline static void *dyn_mem_alloc_zero(size_t size) {
    #ifdef DEBUG
        allocations++;
        void *m = calloc(1, size);
        printf("calloc %p (%i)\n", m, allocations);
        return m;
    #else
        return calloc(1, size);
    #endif
}

inline static void dyn_mem_release(void *m) {
    #ifdef DEBUG
        allocations--;
        printf("free %p (%i)\n", m, allocations);
    #endif
    free(m);
}

#endif
