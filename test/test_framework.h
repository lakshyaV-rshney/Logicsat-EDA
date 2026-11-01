// test_framework.h - Minimalist unit testing framework
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef void (*test_func_t)(void);

typedef struct {
    const char *suite_name;
    const char *case_name;
    test_func_t func;
} test_entry_t;

#define MAX_TESTS 128

static test_entry_t g_test_registry[MAX_TESTS];
static size_t g_num_tests = 0;
static size_t g_tests_passed = 0;
static size_t g_tests_failed = 0;
static bool g_current_test_failed = false;

static inline void register_test(const char *suite, const char *case_name, test_func_t func) {
    if (g_num_tests < MAX_TESTS) {
        g_test_registry[g_num_tests].suite_name = suite;
        g_test_registry[g_num_tests].case_name = case_name;
        g_test_registry[g_num_tests].func = func;
        g_num_tests++;
    }
}

#define TEST(Suite, Case) \
    void test_##Suite##_##Case(void); \
    __attribute__((constructor)) static void register_##Suite##_##Case(void) { \
        register_test(#Suite, #Case, test_##Suite##_##Case); \
    } \
    void test_##Suite##_##Case(void)

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("    [FAIL] %s:%d: Expected TRUE: %s\n", __FILE__, __LINE__, #cond); \
        g_current_test_failed = true; \
    } \
} while (0)

#define EXPECT_FALSE(cond) do { \
    if ((cond)) { \
        printf("    [FAIL] %s:%d: Expected FALSE: %s\n", __FILE__, __LINE__, #cond); \
        g_current_test_failed = true; \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    long long val_a = (long long)(a); \
    long long val_b = (long long)(b); \
    if (val_a != val_b) { \
        printf("    [FAIL] %s:%d: Expected %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, val_a, val_b); \
        g_current_test_failed = true; \
    } \
} while (0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("    [FATAL] %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #cond); \
        g_current_test_failed = true; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    long long val_a = (long long)(a); \
    long long val_b = (long long)(b); \
    if (val_a != val_b) { \
        printf("    [FATAL] %s:%d: Assertion failed: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, val_a, val_b); \
        g_current_test_failed = true; \
        return; \
    } \
} while (0)

static inline int run_all_unit_tests(void) {
    printf("======================================================================\n");
    printf("  Running LogiSAT Unit Test Suite                                     \n");
    printf("======================================================================\n");

    g_tests_passed = 0;
    g_tests_failed = 0;

    for (size_t i = 0; i < g_num_tests; ++i) {
        g_current_test_failed = false;
        printf("[ RUN      ] %s.%s\n", g_test_registry[i].suite_name, g_test_registry[i].case_name);
        g_test_registry[i].func();
        if (g_current_test_failed) {
            printf("[  FAILED  ] %s.%s\n", g_test_registry[i].suite_name, g_test_registry[i].case_name);
            g_tests_failed++;
        } else {
            printf("[       OK ] %s.%s\n", g_test_registry[i].suite_name, g_test_registry[i].case_name);
            g_tests_passed++;
        }
    }

    printf("----------------------------------------------------------------------\n");
    printf("[==========] %zu test(s) executed.\n", g_num_tests);
    printf("[  PASSED  ] %zu test(s).\n", g_tests_passed);
    if (g_tests_failed > 0) {
        printf("[  FAILED  ] %zu test(s).\n", g_tests_failed);
    }
    printf("======================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}

#endif
