#ifndef COMPILER_H
#define COMPILER_H

#include "../main.h"

void compiler_init(Compiler *c);
void compiler_start(Compiler *c, int data_len, char data[EDITOR_LINE_CAPACITY][EDITOR_LINE_MAX_LENGTH]);
bool compiler_can_continue(Compiler *c);
void compiler_continue(Compiler *c);
void compiler_output_handled(Compiler *c);
void compiler_reset(Compiler *c);
void compiler_free(Compiler *c);

#endif