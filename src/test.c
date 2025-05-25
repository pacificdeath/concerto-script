#include "./main.h"

#define TEST_TRUE(CONDITION) do { printf("%i: ", __LINE__); test_true(#CONDITION, CONDITION); } while (0)
#define TEST_EQUAL_INT(LEFT, RIGHT) do { printf("%i: ", __LINE__); test_equal_int(#LEFT, LEFT, #RIGHT, RIGHT); } while (0)
#define TEST_EQUAL_CHAR(LEFT, RIGHT) do { printf("%i: ", __LINE__); test_equal_char(#LEFT, LEFT, #RIGHT, RIGHT); } while (0)

static void print_string(char *name) {
    set_console_color(CONSOLE_FG_CYAN);
    printf("\"%s\"", name);
    reset_console_color();
}

static void print_named_int(char *name, int value) {
    print_string(name);
    printf(":");
    set_console_color(CONSOLE_FG_BLUE);
    printf("%i", value);
    reset_console_color();
}

static void print_named_char(char *name, char value) {
    print_string(name);
    printf(":");
    set_console_color(CONSOLE_FG_BLUE);
    printf("\'%c\'", value);
    reset_console_color();
}

static void validate_test(bool condition) {
    if (condition) {
        set_console_color(CONSOLE_FG_GREEN);
        printf("luck\n");
        reset_console_color();
    } else {
        set_console_color(CONSOLE_FG_RED);
        printf("you are a horrible person\n");
        reset_console_color();
        exit(1);
    }
}

static void test_true(char *literal_condition, bool condition) {
    set_console_color(CONSOLE_FG_CYAN);
    printf("\"%s\"", literal_condition);
    printf(" -> ");
    validate_test(condition);
}

static void test_equal_int(char *left_name, int left_value, char *right_name, int right_value) {
    print_named_int(left_name, left_value);
    printf(" == ");
    print_named_int(right_name, right_value);
    printf(" -> ");
    validate_test(left_value == right_value);
}

static void test_equal_char(char *left_name, char left_value, char *right_name, char right_value) {
    print_named_char(left_name, left_value);
    printf(" == ");
    print_named_char(right_name, right_value);
    printf(" -> ");
    validate_test(left_value == right_value);
}

void run_tests() {
    printf("TEST DYNAMIC ARRAY OF CHARS:\n");
    TEST_EQUAL_INT(global_allocations, 0);

    DynArray array = {0};
    dyn_array_alloc(&array, sizeof(char));
    TEST_EQUAL_INT(global_allocations, 1);
    TEST_EQUAL_INT(array.capacity, DYN_ARRAY_DEFAULT_CAPACITY);
    TEST_EQUAL_INT(array.element_size, sizeof(char));
    TEST_TRUE(array.data != NULL);

    char c = 'a';
    dyn_array_push(&array, &c);
    TEST_EQUAL_INT(array.length, 1);
    c = dyn_char_get(&array, 0);
    TEST_EQUAL_CHAR(c, 'a');

    c = 'b';
    dyn_array_push(&array, &c);
    TEST_EQUAL_INT(array.length, 2);
    c = dyn_char_get(&array, 0);
    TEST_EQUAL_CHAR(c, 'a');
    c = dyn_char_get(&array, 1);
    TEST_EQUAL_CHAR(c, 'b');

    c = 'c';
    dyn_array_insert(&array, 1, &c, 1);
    TEST_EQUAL_INT(array.length, 3);
    c = dyn_char_get(&array, 0);
    TEST_EQUAL_CHAR(c, 'a');
    c = dyn_char_get(&array, 1);
    TEST_EQUAL_CHAR(c, 'c');
    c = dyn_char_get(&array, 2);
    TEST_EQUAL_CHAR(c, 'b');

    dyn_array_clear(&array);
    TEST_EQUAL_INT(global_allocations, 1);
    TEST_TRUE(array.data != NULL);
    TEST_EQUAL_INT(array.length, 0);
    TEST_EQUAL_INT(array.capacity, DYN_ARRAY_DEFAULT_CAPACITY);
    int expected_length = 0;
    while (array.length <= DYN_ARRAY_DEFAULT_CAPACITY) {
        c = 'a';
        dyn_array_push(&array, &c);
        expected_length++;
        TEST_EQUAL_INT(array.length, expected_length);
    }
    int x2_default_capacity = DYN_ARRAY_DEFAULT_CAPACITY * 2;
    TEST_EQUAL_INT(array.capacity, x2_default_capacity);
    TEST_EQUAL_INT(array.length, DYN_ARRAY_DEFAULT_CAPACITY + 1);
    TEST_EQUAL_INT(global_allocations, 1);
    while (array.length <= x2_default_capacity) {
        c = 'b';
        dyn_array_push(&array, &c);
        expected_length++;
        TEST_EQUAL_INT(array.length, expected_length);
    }
    int x4_default_capacity = DYN_ARRAY_DEFAULT_CAPACITY * 4;
    TEST_EQUAL_INT(array.capacity, x4_default_capacity);
    TEST_EQUAL_INT(array.length, (x2_default_capacity + 1));
    char string[x4_default_capacity];
    for (int i = 0; i < x4_default_capacity; i++) {
        string[i] = 'c';
    }
    dyn_array_insert(&array, DYN_ARRAY_DEFAULT_CAPACITY + 1, string, x4_default_capacity);
    int x8_default_capacity = DYN_ARRAY_DEFAULT_CAPACITY * 8;
    TEST_EQUAL_INT(array.capacity, x8_default_capacity);
    TEST_EQUAL_INT(array.length, (x2_default_capacity + 1) + x4_default_capacity);
    TEST_EQUAL_CHAR(dyn_char_get(&array, 0), 'a');
    TEST_EQUAL_CHAR(dyn_char_get(&array, DYN_ARRAY_DEFAULT_CAPACITY), 'a');
    TEST_EQUAL_CHAR(dyn_char_get(&array, DYN_ARRAY_DEFAULT_CAPACITY + 1), 'c');
    TEST_EQUAL_CHAR(dyn_char_get(&array, DYN_ARRAY_DEFAULT_CAPACITY + x4_default_capacity), 'c');
    TEST_EQUAL_CHAR(dyn_char_get(&array, DYN_ARRAY_DEFAULT_CAPACITY + x4_default_capacity + 1), 'b');
    TEST_EQUAL_CHAR(dyn_char_get(&array, array.length - 1), 'b');

    DynArray array_of_arrays = {0};
    dyn_array_alloc(&array_of_arrays, sizeof(DynArray));
    TEST_EQUAL_INT(global_allocations, 2);
    TEST_EQUAL_INT(array_of_arrays.capacity, DYN_ARRAY_DEFAULT_CAPACITY);
    TEST_EQUAL_INT(array_of_arrays.element_size, sizeof(DynArray));
    TEST_TRUE(array_of_arrays.data != NULL);

    dyn_array_push(&array_of_arrays, &array);
    TEST_EQUAL_INT(array_of_arrays.length, 1);
    DynArray *inner_array = dyn_array_get(&array_of_arrays, 0);
    TEST_TRUE(inner_array->data == array.data);
}

