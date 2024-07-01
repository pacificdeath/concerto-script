#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <stdint.h>

#include "bird_windows.h"

void sleep(u32 milliseconds) {
    Sleep(milliseconds);
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
