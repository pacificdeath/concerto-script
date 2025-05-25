#include <stdio.h>

#include "../main.h"
#include "editor_utils.c"

void editor_load_program(State *state, const char *filename) {
    Editor *e = &state->editor;

    editor_clear(state);

    if (filename == NULL) {
        return;
    }

    FILE *file;
    file = fopen(filename, "r");
    if (file == NULL) {
        return;
    }

    int c_int;
    int current_line_idx = 0;
    DynArray *current_line = dyn_array_get(&e->lines, current_line_idx);
    while ((c_int = fgetc(file)) != EOF) {
        if (c_int == '\n') {
            DynArray new_line;
            dyn_array_alloc(&new_line, sizeof(char));
            dyn_array_push(&e->lines, &new_line);
            current_line_idx++;
            current_line = dyn_array_get(&e->lines, current_line_idx);
        } else {
            char c = (char)c_int;
            dyn_array_push(current_line, &c);
        }
    }

    strcpy(e->current_file, filename);
    fclose(file);
    set_cursor_y(state, 0);
    set_cursor_x(state, 0);
    snap_visual_vertical_offset_to_cursor(state);
}

bool editor_save_file(State *state) {
    Editor *e = &state->editor;
    FILE *file;
    file = fopen(e->current_file, "w");
    if (file == NULL) {
        return false;
    }
    for (int i = 0; i < e->lines.length; i++) {
        DynArray *line = dyn_array_get(&e->lines, i);
        char c = '\n';
        dyn_array_insert(line, line->length, &c, 1);
        fwrite(line->data, sizeof(char), line->length, file);
        c = '\0';
        dyn_array_set(line, line->length - 1, &c);
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
    return true;
}

void update_filename_buffer(State *state, char *directory) {
    Editor *e = &state->editor;
    const char *filter = TextFormat("%s\\%s*.*", directory, e->file_search_buffer);
    list_files(filter, e->filename_buffer, EDITOR_FILENAMES_MAX_AMOUNT);
    const char *console_text = TextFormat("%s\n%s", e->file_search_buffer, e->filename_buffer);
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
        const char *filepath = TextFormat("%s\\%s", directory, filename);
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
