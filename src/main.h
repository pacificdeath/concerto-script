#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#include "raylib.h"
#include "windows_wrapper.h"

#define OCTAVE_OFFSET 12.0f
#define MAX_OCTAVE 8
#define A4_OFFSET 48
#define A4_FREQ 440.0f
#define MAX_NOTE 50
#define MIN_NOTE -44
#define SILENCE (MAX_NOTE + 1)

#define EDITOR_MAX_VISUAL_LINES 30
#define EDITOR_FILENAME_MAX_LENGTH 256
#define EDITOR_LINE_CAPACITY 256
#define EDITOR_LINE_MAX_LENGTH 64
#define EDITOR_LINE_NUMBER_PADDING 5
#define AUTO_CLICKABLE_KEYS_AMOUNT 6
#define AUTOCLICK_LOWER_THRESHOLD 0.3F
#define AUTOCLICK_UPPER_THRESHOLD 0.33F
#define EDITOR_CURSOR_WIDTH 3
#define EDITOR_CURSOR_ANIMATION_SPEED 10.0f
#define EDITOR_SCROLL_MULTIPLIER 3
#define EDITOR_NORMAL_COLOR (Color) { 255, 255, 255, 255 }
#define EDITOR_PLAY_COLOR (Color) { 0, 255, 0, 255 }
#define EDITOR_WAIT_COLOR (Color) { 255, 0, 0, 255 }
#define EDITOR_KEYWORD_COLOR (Color) { 0, 128, 255, 255 }
#define EDITOR_PAREN_COLOR (Color) { 255, 192, 0, 255 }
#define EDITOR_SPACE_COLOR (Color) { 64, 64, 64, 255 }
#define EDITOR_CURSOR_COLOR (Color) { 255, 0, 255, 255 }
#define EDITOR_SELECTION_COLOR (Color) { 0, 128, 255, 128 }
#define EDITOR_LINENUMBER_COLOR (Color) { 128, 128, 128, 255 }
#define EDITOR_COMMENT_COLOR (Color) { 128, 0, 255, 255 }

#define CONSOLE_LINE_COUNT 8
#define CONSOLE_LINE_MAX_LENGTH 64
#define CONSOLE_BG_COLOR (Color) {0, 0, 0, 255}
#define CONSOLE_TEXT_COLOR (Color) {0, 255, 255, 255}

#define COMPILER_MAX_PAREN_NESTING 32
#define COMPILER_VARIABLE_MAX_COUNT 255

typedef struct Tone {
    uint32_t idx;
    uint16_t line_idx;
    uint16_t char_idx;
    uint16_t char_count;
    int8_t start_note;
    int8_t end_note;
    float start_frequency;
    float end_frequency;
    float duration;
    uint8_t octave;
} Tone;

typedef enum Editor_State {
    EDITOR_STATE_WRITE,
    EDITOR_STATE_READY_TO_PLAY,
    EDITOR_STATE_WAITING_TO_PLAY,
    EDITOR_STATE_PLAY,
    EDITOR_STATE_INTERRUPT,
} Editor_State;

typedef struct Editor {
    Editor_State state;
    char lines[EDITOR_LINE_CAPACITY][EDITOR_LINE_MAX_LENGTH];
    char word[EDITOR_LINE_MAX_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_line;
    int selection_x;
    int selection_line;
    char current_file[EDITOR_FILENAME_MAX_LENGTH];
    int autoclick_key;
    float autoclick_down_time;
    int auto_clickable_keys[AUTO_CLICKABLE_KEYS_AMOUNT];
    int visual_vertical_offset;
    float cursor_anim_time;
    int clipboard_line_count;
    char clipboard_lines[EDITOR_LINE_CAPACITY][EDITOR_LINE_MAX_LENGTH];
} Editor;

typedef struct Editor_Cursor_Render_Data {
    int line;
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

typedef struct Editor_Selection_Data {
    int start_line;
    int end_line;
    int start_x;
    int end_x;
} Editor_Selection_Data;

typedef enum Token_Type {
    TOKEN_NONE = 0,
    TOKEN_START,
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
    TOKEN_DURATION,
    TOKEN_SCALE,
    TOKEN_REPEAT,
    TOKEN_ROUNDS,
    TOKEN_DEFINE,
} Token_Type;

typedef union Token_Value {
    int number;
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

typedef enum Compiler_Error_Type {
    NO_ERROR = 0,
    ERROR_NO_SOUND,
    ERROR_SYNTAX_ERROR,
    ERROR_INVALID_SEMI,
    ERROR_UNKNOWN_IDENTIFIER,
    ERROR_EXPECTED_IDENTIFIER,
    ERROR_EXPECTED_PAREN_OPEN,
    ERROR_NO_MATCHING_CLOSING_PAREN,
    ERROR_NO_SCALE_IDENTIFIER,
    ERROR_EXPECTED_NUMBER,
    ERROR_NUMBER_TOO_BIG,
    ERROR_NO_MATCHING_OPENING_PAREN,
    ERROR_NESTING_TOO_DEEP,
    ERROR_SCALE_CAN_ONLY_HAVE_NOTES,
    ERROR_SCALE_CAN_NOT_BE_EMPTY,
    ERROR_INTERNAL,
} Compiler_Error_Type;

typedef struct Compiler_Result {
    Compiler_Error_Type error_type;
    char error_message[256];
    char **data;
    int data_len;
    int line_number;
    int char_idx;
    int token_amount;
    Token *tokens;
    int tone_amount;
    Tone *tones;
} Compiler_Result;

typedef enum Note_Direction {
    DIRECTION_RISE = 1,
    DIRECTION_FALL = -1,
} Note_Direction;

typedef struct Optional_Scale_Offset_Data {
    Compiler_Result *result;
    int *token_idx;
    int current_note;
    uint16_t current_scale;
    Note_Direction direction;
} Optional_Scale_Offset_Data;

typedef struct Tone_Add_Data {
    uint32_t idx;
    Compiler_Result *result;
    Token *token;
    int start_note;
    int end_note;
    float duration;
} Tone_Add_Data;

typedef struct Synthesizer_Sound {
    int16_t *raw_data;
    Tone tone;
    Sound sound;
} Synthesizer_Sound;

typedef struct Synthesizer {
    int sound_capacity;
    Synthesizer_Sound *sounds;
    int current_sound_idx;
    int sound_count;
    Mutex mutex;
    bool should_cancel;
} Synthesizer;

typedef struct State {
    int window_width;
    int window_height;

    Font font;

    float delta_time;

    Editor editor;

    char console_text[CONSOLE_LINE_COUNT][CONSOLE_LINE_MAX_LENGTH];

    Compiler_Result *compiler_result;
    Synthesizer_Sound *current_sound;
} State;

int is_valid_in_identifier (char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

#endif
