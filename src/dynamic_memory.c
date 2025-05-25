#ifndef DYN_MEMORY_H
#define DYN_MEMORY_H

#ifdef DEBUG
#include <stdio.h>
static int global_allocations = 0;
#ifdef VERBOSE
    static void print_allocations(const char *name) {
        reset_console_color();
        printf("(");
        set_console_color(CONSOLE_FG_BLUE);
        printf("%s", name);
        reset_console_color();
        printf(") : ");
        set_console_color(CONSOLE_FG_YELLOW);
        printf("%i global allocations\n", global_allocations);
        reset_console_color();
    }
    #define PRINT_ALLOCATIONS(NAME) print_allocations(NAME)
#else
    #define PRINT_ALLOCATIONS(NAME)
#endif
#endif

inline static void *dyn_mem_alloc(int size) {
    #ifdef DEBUG
        global_allocations++;
        void *m = malloc(size);
        PRINT_ALLOCATIONS("MALLOC");
        return m;
    #else
        return malloc(size);
    #endif
}

inline static void *dyn_mem_alloc_zero(int size) {
    #ifdef DEBUG
        global_allocations++;
        void *m = calloc(1, size);
        PRINT_ALLOCATIONS("CALLOC");
        return m;
    #else
        return calloc(1, size);
    #endif
}

inline static void *dyn_mem_realloc(void *m, int size) {
    #ifdef DEBUG
        m = realloc(m, size);
        PRINT_ALLOCATIONS("REALLOC");
        return m;
    #else
        (void)m;
        return calloc(1, size);
    #endif
}

inline static void dyn_mem_release(void *m) {
    #ifdef DEBUG
        global_allocations--;
        PRINT_ALLOCATIONS("FREE");
    #endif
    free(m);
}

#endif
