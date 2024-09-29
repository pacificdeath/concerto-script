#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <stdint.h>

#include "bird_windows.h"

void sleep(u32 milliseconds) {
    Sleep(milliseconds);
}

int list_files(char *dir, char *buffer) {
    buffer[0] = '\0';

    WIN32_FIND_DATA find_data;
    HANDLE handle = NULL;

    char path[2048];

    sprintf(path, "%s\\*.*", dir);
    if ((handle = FindFirstFile(path, &find_data)) == INVALID_HANDLE_VALUE) {
        return 1;
    }
    do
    {
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0 ||
            find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        strcat(buffer, find_data.cFileName);
        strcat(buffer, "\n");
    }
    while(FindNextFile(handle, &find_data));
    FindClose(handle); //Always, Always, clean things up!
    return 0;
}

Thread thread_create(void (*thread_function)(void *), void *thread_argument) {
    return (HANDLE)_beginthread(thread_function, 0, thread_argument);
}

void thread_join(Thread thread) {
    u32 result = WaitForSingleObject(thread, INFINITE);
}

void thread_destroy(Thread thread) {
    CloseHandle(thread);
}

Mutex mutex_create() {
    Mutex mutex = CreateMutex(NULL, 0, NULL);
    ReleaseMutex((HANDLE)mutex);
    return mutex;
}

void mutex_lock(Mutex mutex) {
    u32 result = WaitForSingleObject((HANDLE)mutex, INFINITE);
}

void mutex_unlock(Mutex mutex) {
    ReleaseMutex((HANDLE)mutex);
}

void mutex_destroy(Mutex mutex) {
    CloseHandle((HANDLE)mutex);
}
