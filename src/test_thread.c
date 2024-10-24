#include "raylib.h"
#include "windows_wrapper.h"

typedef struct Debug_Thread_Data {
    u32 number;
    Mutex mutex;
} Debug_Thread_Data;

static void random_sleep() {
    sleep(GetRandomValue(1, 100));
}

static void debug_mutex_function(char *id, Debug_Thread_Data *data, u32 add) {
    printf("thread %s attempting to lock mutex\n", id);
    random_sleep();
    mutex_lock(data->mutex);
        random_sleep();
        printf("thread %s locked mutex\n", id);
        random_sleep();
        printf("thread %s will modify value %d by adding %d\n", id, data->number, add);
        random_sleep();
        (data->number) += add;
        random_sleep();
        printf("thread %s modified value, it is now %d\n", id, data->number);
        random_sleep();
        printf("thread %s will unlock mutex\n", id);
        random_sleep();
    mutex_unlock(data->mutex);
    random_sleep();
    printf("thread %s unlocked mutex\n", id);
    random_sleep();
}

static void debug_thread_function(void *raw_data) {
    Debug_Thread_Data *data = (Debug_Thread_Data *)raw_data;
    for (int i = 0; i < 10; i += 1) {
        debug_mutex_function("  2", data, 100);
    }
}

void test_thread() {
    Debug_Thread_Data *data = (Debug_Thread_Data *)malloc(sizeof(Debug_Thread_Data));
    data->number = 0;
    data->mutex = mutex_create();
    Thread *debug_thread;
    u32 debug_thread_id;
    debug_thread = thread_create(debug_thread_function, data);
    for (int i = 0; i < 10; i += 1) {
        debug_mutex_function("1  ", data, 1);
    }
    thread_join(debug_thread);
    mutex_destroy(data->mutex);
    thread_destroy(debug_thread);
    free(data);
}