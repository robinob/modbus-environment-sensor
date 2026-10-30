/**
 * @file    test.h
 * @brief   A minimal unit-test helper for the PC tests.
 *
 * Bigger projects use a framework such as Unity or GoogleTest. We only
 * need one thing: check a condition, print where it failed, and count
 * failures. A few macros are enough for that, and nothing to install.
 *
 * Usage:
 *   CHECK_EQ_HEX(modbus_crc16(buf, 2), 0x1234);
 *   ...
 *   return test_summary();   // last line of main(); 0 = all passed
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>

/* Counters shared by all checks. "static" gives each test program its own
 * copy; each test is one .c file with its own main(), so that is fine. */
static int test_count;
static int test_failures;

/* Compare two integer values and print both in hex if they differ.
 * __FILE__ and __LINE__ are filled in by the compiler with the location
 * of the CHECK, so a failure message points straight at the right line.
 * The do { ... } while (0) wrapper makes the macro behave like a single
 * statement, so it is safe to use after an "if" without braces. */
#define CHECK_EQ_HEX(actual, expected)                                        \
    do {                                                                      \
        unsigned long a_ = (unsigned long)(actual);                           \
        unsigned long e_ = (unsigned long)(expected);                         \
        test_count++;                                                         \
        if (a_ != e_) {                                                       \
            test_failures++;                                                  \
            printf("%s:%d: FAIL  %s\n    got 0x%04lX, expected 0x%04lX\n",    \
                   __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                     \
    } while (0)

/* Print the result and return the exit code for main(): 0 if everything
 * passed, 1 otherwise. Build tools and scripts treat 0 as success. */
static inline int test_summary(void)
{
    printf("%d checks, %d failed\n", test_count, test_failures);
    return test_failures == 0 ? 0 : 1;
}

#endif /* TEST_H */
