#include <stdio.h>

#include "../main.h"
#include "editor_utils.c"

void editor_load_program(State *state, char *filename) {
    Editor *e = &state->editor;
    FILE *file;
    char line[EDITOR_LINE_MAX_LENGTH];
    file = fopen(filename, "r");
    if (file == NULL) {
        return;
    }
    e->line_count = 1;
    int i;
    for (i = 0; fgets(line, sizeof(line), file) != NULL; i += 1) {
        strcpy(e->lines[i], line);
        int len = strlen(e->lines[i]);
        e->lines[i][len - 1] = '\0';
    }
    if (i == 0) {
        e->lines[i][0] = '\0';
        e->line_count = 1;
    } else {
        e->line_count = i;
    }
    strcpy(e->current_file, filename);
    fclose(file);
    set_cursor_y(state, 0);
    set_cursor_x(state, 0);
    snap_visual_vertical_offset_to_cursor(state);
}

bool editor_save_file(State *state) {
    FILE *file;
    file = fopen(state->editor.current_file, "w");
    if (file == NULL) {
        return false;
    }
    for (int i = 0; i < state->editor.line_count; i++) {
        int len = strlen(state->editor.lines[i]);
        state->editor.lines[i][len] = '\n';
        fwrite(state->editor.lines[i], sizeof(char), len, file);
        state->editor.lines[i][len] = '\0';
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
    return true;
}

void update_filename_buffer(State *state, char *directory) {
    Editor *e = &state->editor;
    char filter[64];
    sprintf(filter, "%s\\%s*.*", directory, e->file_search_buffer);
    int file_amount = list_files(filter, e->filename_buffer, EDITOR_FILENAMES_MAX_AMOUNT);
    char console_text[EDITOR_FILENAMES_MAX_AMOUNT * EDITOR_FILENAME_MAX_LENGTH];
    sprintf(console_text, "%s\n%s", e->file_search_buffer, e->filename_buffer);
    console_set_text(state, console_text);
    e->console_highlight_idx = 1;
}

Big_State file_explorer(State *state, bool shift) {
    Editor *e = &state->editor;
    char *directory;
    switch (state->state) {
    case STATE_EDITOR_FILE_EXPLORER_THEMES: directory = THEMES_DIRECTORY; break;
    case STATE_EDITOR_FILE_EXPLORER_PROGRAMS: directory = PROGRAMS_DIRECTORY; break;
    default: {
        TraceLog(LOG_ERROR, "Horrible code in file explorer");
        return STATE_EDITOR;
    }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        e->file_search_buffer[0] = '\0';
        e->file_search_buffer_length = 0;
        e->file_cursor = 0;
        e->console_highlight_idx = -1;
        return STATE_EDITOR;
    } else if (IsKeyPressed(KEY_DOWN)) {
        if (e->console_highlight_idx < (e->console_line_count - 1)) {
            e->console_highlight_idx++;
        }
    } else if (IsKeyPressed(KEY_UP)) {
        if (e->console_highlight_idx > 1) {
            e->console_highlight_idx--;
        }
    } else if (auto_click(state, KEY_BACKSPACE) && e->file_search_buffer_length > 0) {
        e->file_search_buffer_length--;
        e->file_search_buffer[e->file_search_buffer_length] = '\0';
        update_filename_buffer(state, directory);
    } else if (IsKeyPressed(KEY_ENTER)) {
        char filename[128];
        console_get_highlighted_text(state, filename);
        char filepath[128];
        sprintf(filepath, "%s\\%s", directory, filename);
        switch (state->state) {
        default: return STATE_EDITOR;
        case STATE_EDITOR_FILE_EXPLORER_THEMES:
            TextCopy(e->theme.filepath, filepath);
            break;
        case STATE_EDITOR_FILE_EXPLORER_PROGRAMS:
            editor_load_program(state, filepath);
            break;
        }
        e->file_search_buffer[0] = '\0';
        e->file_search_buffer_length = 0;
        e->file_cursor = 0;
        e->console_highlight_idx = -1;
        return STATE_EDITOR;
    } else {
        for (KeyboardKey key = 32; key < 127; key++) {
            if (IsKeyPressed(key) && e->file_search_buffer_length < EDITOR_FILE_SEARCH_BUFFER_MAX - 1) {
                char c = keyboard_key_to_char(state, key, shift);
                e->file_search_buffer[e->file_search_buffer_length] = c;
                e->file_search_buffer_length++;
                e->file_search_buffer[e->file_search_buffer_length] = '\0';
                update_filename_buffer(state, directory);
                break;
            }
        }
    }

    return state->state;
}