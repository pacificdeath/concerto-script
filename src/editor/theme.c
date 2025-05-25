#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../main.h"

inline static void trunc_str(char *original, char trunc[16]) {
    int len = TextLength(original);
    if (len <= 15) {
        for (int i = 0; i < len; i++) {
            trunc[i] = original[i];
        }
    } else {
        for (int i = 0; i < 12; i++) {
            trunc[i] = original[i];
        }
        for (int i = 12; i < 15; i++) {
            trunc[i] = '.';
        }
        trunc[15] = '\0';
    }
}

void editor_theme_set_minimal(State *state) {
    const Color black = {0,0,0,255};
    const Color white = {255,255,255,255};
    const Color transparent = {255,255,255,64};
    Editor_Theme *theme = &(state->editor.theme);
    theme->bg = black;
    theme->fg = white;
    theme->play = white;
    theme->wait = white;
    theme->keyword = white;
    theme->note = white;
    theme->paren = white;
    theme->space = black;
    theme->comment = white;
    theme->linenumber = white;
    theme->cursor = white;
    theme->cursor_line = black;
    theme->selection = transparent;

    theme->console_bg = black;
    theme->console_foreground = white;
    theme->console_foreground_error = white;
    theme->console_highlight = white;

    theme->play_cursor = white;
}

Editor_Theme_Status editor_theme_update(State *state, void (*on_error)(State*,const char*)) {
    Editor_Theme *theme = &(state->editor.theme);

    theme->file_mod_time = GetFileModTime(theme->filepath);
    editor_theme_set_minimal(state);

    int data_size;
    unsigned char *data = LoadFileData(theme->filepath, &data_size);
    if (data == NULL) {
        char trunc_filename[16];
        trunc_str(theme->filepath, trunc_filename);
        const char *error = TextFormat("%i Theme error\nFile %s not found", FileExists(theme->filepath), trunc_filename);
        on_error(state, error);
        return EDITOR_THEME_CHANGED_ERROR;
    }

    int line = 0;
    int i = 0;
    while (true) {
        while (isspace(data[i])) {
            i++;
        }

        if (i >= data_size) {
            break;
        }

        line++;
        bool key_found = false;
        char key[16];
        int j;
        for (j = 0;; j++) {
            if (key_found) {
                break;
            }
            char c = data[i + j];
            if (isspace(c)) {
                key_found = true;
                key[j] = '\0';
                while (isspace(data[i + j])) {
                    j++;
                }
                if (data[i + j] != ':') {
                    const char *error = TextFormat("Theme error\nExpected: \':\'\nLine %i", line);
                    on_error(state, error);
                    UnloadFileData(data);
                    return EDITOR_THEME_CHANGED_ERROR;
                }
            } else {
                key[j] = c;
            }
        }
        i += j;

        Color *color;
        if (strcmp(key, "background") == 0) {
            color = &(theme->bg);
        } else if (strcmp(key, "foreground") == 0) {
            color = &(theme->fg);
        } else if (strcmp(key, "play") == 0) {
            color = &(theme->play);
        } else if (strcmp(key, "wait") == 0) {
            color = &(theme->wait);
        } else if (strcmp(key, "keyword") == 0) {
            color = &(theme->keyword);
        } else if (strcmp(key, "note") == 0) {
            color = &(theme->note);
        } else if (strcmp(key, "paren") == 0) {
            color = &(theme->paren);
        } else if (strcmp(key, "space") == 0) {
            color = &(theme->space);
        } else if (strcmp(key, "comment") == 0) {
            color = &(theme->comment);
        } else if (strcmp(key, "linenumber") == 0) {
            color = &(theme->linenumber);
        } else if (strcmp(key, "cursor") == 0) {
            color = &(theme->cursor);
        } else if (strcmp(key, "cursor_line") == 0) {
            color = &(theme->cursor_line);
        } else if (strcmp(key, "selection") == 0) {
            color = &(theme->selection);
        } else if (strcmp(key, "console_background") == 0) {
            color = &(theme->console_bg);
        } else if (strcmp(key, "console_foreground") == 0) {
            color = &(theme->console_foreground);
        } else if (strcmp(key, "console_foreground_error") == 0) {
            color = &(theme->console_foreground_error);
        } else if (strcmp(key, "console_highlight") == 0) {
            color = &(theme->console_highlight);
        } else if (strcmp(key, "play_cursor") == 0) {
            color = &(theme->play_cursor);
        } else {
            int len = TextLength(key);
            if (len >= 16) {
                for (int k = 12; k < 15; k++) {
                    key[k] = '.';
                }
                key[15] = '\0';
            }
            const char *error = TextFormat("Theme error\nUnknown key: \"%s\"", key);
            on_error(state, error);
            UnloadFileData(data);
            return EDITOR_THEME_CHANGED_ERROR;
        }

        while (isspace(data[i])) {
            i++;
        }
        for (j = 0; j < 4; j++) {
            char c = data[i+j];
            uint8 *byte_ptr;
            switch (j) {
            case 0: byte_ptr = &(color->r); break;
            case 1: byte_ptr = &(color->g); break;
            case 2: byte_ptr = &(color->b); break;
            case 3: byte_ptr = &(color->a); break;
            default: {
                char *error = "Theme error\nThe code is terrible";
                on_error(state, error);
                UnloadFileData(data);
                return EDITOR_THEME_CHANGED_ERROR;
            }
            }

            uint8 nibble;
            bool done = false;
            switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                nibble = c - '0';
                break;
            }
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f': {
                nibble = c - 'a' + 10;
                break;
            }
            default: {
                if (isspace(c) || c == '\0') {
                    done = true;
                    const char *error;
                    switch (j) {
                    case 0:
                        error = TextFormat("Theme error\nExpected color hex values\nLine %i", line);
                        on_error(state, error);
                        UnloadFileData(data);
                        return EDITOR_THEME_CHANGED_ERROR;
                    case 1:
                        color->g = color->r;
                        color->b = color->r;
                        color->a = 255;
                        break;
                    case 2:
                        color->b = 0;
                        color->a = 255;
                        break;
                    case 3:
                        color->a = 255;
                        break;
                    case 4:
                        color->a = 255;
                        break;
                    default:
                        break;
                    }
                } else {
                    const char *error = TextFormat("Theme error\nInvalid hex digit: \'%i\'\nLine %i", c, line);
                    on_error(state, error);
                    UnloadFileData(data);
                    return EDITOR_THEME_CHANGED_ERROR;
                }
            }
            }
            if (done) {
                break;
            }
            uint8 byte = (nibble * 16) + nibble;
            *byte_ptr = byte;
        }
        i += j + 1;
    }

    UnloadFileData(data);
    return EDITOR_THEME_CHANGED_OK;
}

Editor_Theme_Status editor_theme_update_if_modified(State *state, void (*on_error)(State*,const char*)) {
    Editor_Theme *theme = &(state->editor.theme);
    if (theme->file_mod_time != GetFileModTime(theme->filepath)) {
        Editor_Theme_Status theme_status = editor_theme_update(state, on_error);
        return theme_status;
    }
    return EDITOR_THEME_UNCHANGED;
}
