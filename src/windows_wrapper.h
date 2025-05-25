#ifndef WINDOWS_WRAPPER_H
#define WINDOWS_WRAPPER_H

typedef enum Keyboard_Layout {
    KEYBOARD_LAYOUT_DEFAULT     = 0x0409,
    KEYBOARD_LAYOUT_SWEDISH     = 0X041D,
} Keyboard_Layout;

typedef enum ConsoleColor {
    CONSOLE_FG_RED = 1,
    CONSOLE_FG_GREEN = 2,
    CONSOLE_FG_BLUE = 4,

    CONSOLE_FG_YELLOW = (CONSOLE_FG_RED|CONSOLE_FG_GREEN),
    CONSOLE_FG_MAGENTA = (CONSOLE_FG_RED|CONSOLE_FG_BLUE),
    CONSOLE_FG_CYAN = (CONSOLE_FG_GREEN|CONSOLE_FG_BLUE),

    CONSOLE_FG_WHITE = (CONSOLE_FG_RED|CONSOLE_FG_GREEN|CONSOLE_FG_BLUE),
} ConsoleColor;

typedef void *Mutex;
typedef void *Thread;

Keyboard_Layout get_keyboard_layout();
void sleep(unsigned long milliseconds);
int list_files(const char *dir, char *buffer, int max);
Thread thread_create(void (*thread_function)(void *), void *thread_argument);
void thread_join(Thread thread);
void thread_error();
Mutex mutex_create();
void mutex_lock(Mutex mutex);
void mutex_unlock(Mutex mutex);
void mutex_destroy(Mutex mutex);
void reset_console_color();
void set_console_color(ConsoleColor color);

#endif
