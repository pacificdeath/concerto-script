#ifndef BIRD_WINDOWS_H
#define BIRD_WINDOWS_H
typedef unsigned long u32;
typedef void *Mutex;
typedef void *Thread;
void sleep(u32 milliseconds);
Thread thread_create(void (*thread_function)(void *), void *thread_argument);
void thread_join(Thread thread);
void thread_destroy(Thread thread);
Mutex mutex_create();
void mutex_lock(Mutex mutex);
void mutex_unlock(Mutex mutex);
void mutex_destroy(Mutex mutex);
#endif