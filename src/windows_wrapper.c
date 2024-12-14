#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <stdint.h>

#include "windows_wrapper.h"

Keyboard_Layout get_keyboard_layout() {
    HKL raw_layout = GetKeyboardLayout(0);
    Keyboard_Layout layout_id = (DWORD_PTR)raw_layout & 0xFFFF; // Get the low word
    switch (layout_id) {
    case KEYBOARD_LAYOUT_DEFAULT:
    case KEYBOARD_LAYOUT_SWEDISH:
        return layout_id;
    default:
        return KEYBOARD_LAYOUT_DEFAULT;
    }
}

void sleep(unsigned long milliseconds) {
    Sleep(milliseconds);
}

int list_files(char *dir, char *buffer, int max) {
    buffer[0] = '\0';

    WIN32_FIND_DATA find_data;
    HANDLE handle = NULL;

    char path[2048];

    sprintf(path, "%s", dir);
    if ((handle = FindFirstFile(path, &find_data)) == INVALID_HANDLE_VALUE) {
        return 1;
    }
    int amount = 0;
    do
    {
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0 ||
            find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        if (amount > 0) {
            strcat(buffer, "\n");
        }
        strcat(buffer, find_data.cFileName);
        amount++;

        if (amount == max) {
            break;
        }
    }
    while(FindNextFile(handle, &find_data));
    FindClose(handle); //Always, Always, clean things up!
    return 0;
}

Thread thread_create(void (*thread_function)(void *), void *thread_argument) {
    int stack_size = 0;
    return (HANDLE)_beginthread(thread_function, stack_size, thread_argument);
}

void thread_join(Thread thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void thread_error() {
    ExitThread(1);
}

Mutex mutex_create() {
    Mutex mutex = CreateMutex(NULL, 0, NULL);
    ReleaseMutex((HANDLE)mutex);
    return mutex;
}

void mutex_lock(Mutex mutex) {
    WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void mutex_unlock(Mutex mutex) {
    ReleaseMutex((HANDLE)mutex);
}

void mutex_destroy(Mutex mutex) {
    CloseHandle((HANDLE)mutex);
}
