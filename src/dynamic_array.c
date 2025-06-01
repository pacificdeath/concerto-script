#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H
#define DYN_ARRAY_DEFAULT_CAPACITY 8

typedef struct DynArray {
    void *data;
    int element_size;
    int length;
    int capacity;
} DynArray;

inline static void dyn_array_expand(DynArray *arr, int size) {
    while (arr->capacity < size) {
        arr->capacity *= 2;
    }
    arr->data = dyn_mem_realloc(arr->data, arr->capacity * arr->element_size);
}

static void dyn_array_alloc(DynArray *arr, int element_size) {
    arr->element_size = element_size;
    arr->length = 0;
    arr->capacity = DYN_ARRAY_DEFAULT_CAPACITY;
    arr->data = dyn_mem_alloc(arr->capacity * element_size);
}

static void dyn_array_push(DynArray *arr, const void *element) {
    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = dyn_mem_realloc(arr->data, arr->capacity * arr->element_size);
    }
    void *target = (char *)arr->data + arr->length * arr->element_size;
    memcpy(target, element, arr->element_size);
    arr->length++;
}

static void dyn_array_insert(DynArray *arr, int idx, const void *data, int count) {
    ASSERT(idx <= arr->length);

    int new_len = arr->length + count;
    if (new_len > arr->capacity) {
        dyn_array_expand(arr, new_len);
    }

    void *dst = (char *)arr->data + (idx + count) * arr->element_size;
    void *src = (char *)arr->data + idx * arr->element_size;
    int bytes_to_move = (arr->length - idx) * arr->element_size;
    memmove(dst, src, bytes_to_move);

    void *insert_pos = (char *)arr->data + idx * arr->element_size;
    memcpy(insert_pos, data, count * arr->element_size);

    arr->length += count;
}

static void dyn_array_remove(DynArray *arr, int idx, int count) {
    ASSERT(idx + count <= arr->length);

    int bytes_to_move = (arr->length - (idx + count)) * arr->element_size;
    void *dst = (char *)arr->data + idx * arr->element_size;
    void *src = (char *)arr->data + (idx + count) * arr->element_size;

    memmove(dst, src, bytes_to_move);
    arr->length -= count;
}

static void *dyn_array_get(DynArray *arr, int idx) {
    ASSERT(idx < arr->length);
    return (char *)arr->data + idx * arr->element_size;
}

static char dyn_char_get(DynArray *arr, int idx) {
    if (idx >= arr->length) {
        return '\0';
    }
    return *((char *)dyn_array_get(arr, idx));
}

static void dyn_array_set(DynArray *arr, int idx, const void *data) {
    ASSERT(idx < arr->length);
    void *set_pos = (char *)arr->data + idx * arr->element_size;
    memcpy(set_pos, data, arr->element_size);
}

static void dyn_array_resize(DynArray *arr, int length) {
    arr->length = length;
    if (length > arr->length) {
        dyn_array_expand(arr, length);
    }
}

static void dyn_array_clear(DynArray *arr) {
    arr->length = 0;
}

static void dyn_array_release(DynArray *arr) {
    dyn_mem_release(arr->data);
    arr->data = NULL;
    arr->length = 0;
    arr->capacity = 0;
}

#ifdef DEBUG
    __attribute__((unused))
    static void dyn_array_print(DynArray *arr) {
        printf("data: \"");
        for (int i = 0; i < arr->length; i++) {
            printf("%c", dyn_char_get(arr, i));
        }
        printf("\"\n");
        printf("element_size: %i\n", arr->element_size);
        printf("length: %i\n", arr->length);
        printf("capacity: %i\n", arr->capacity);
    }
#endif

#endif

