#ifndef EDITOR_H
#define EDITOR_H

#include "../main.h"

void editor_init(State *state, char *filename);
Big_State editor_input(State *state);
void editor_error_display(State *state, char *error_message);
void editor_render(State *state);
void editor_free(State *state);

#endif