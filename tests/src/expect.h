#include <core/asserts.h>
#include <core/logger.h>
#include <math/kmath.h>

#define expect_should_be(expected, actual)                                     \
    if (actual != expected) {                                                  \
        KERROR("--> Expected %lld, but got: %lld. File: %s:%d.", expected,     \
               actual, __FILE__, __LINE__);                                    \
        failed = 1;                                                            \
    }

#define expect_should_not_be(expected, actual)                                 \
    if (actual == expected) {                                                  \
        KERROR("--> Expected %lld != %lld, but they are. File: %s:%d.",        \
               expected, actual, __FILE__, __LINE__);                          \
        failed = 1;                                                            \
    }

#define expect_float_to_be(expected, actual)                                                   \
    if (kabs(expected - actual) > 0.001f) {                                    \
        KERROR("--> Expected %f, but got: %f. File: %s:%d.", expected, actual, \
               __FILE__, __LINE__);                                            \
        failed = 1;                                                            \
    }

#define expect_to_be_true(actual)                                              \
    if (!actual) {                                                             \
        KERROR("--> Expected true but got: false. File: %s:%d.", __FILE__,     \
               __LINE__);                                                      \
        failed = 1;                                                            \
    }

#define expect_to_be_false(actual)                                             \
    if (actual) {                                                              \
        KERROR("--> Expected false but got: true. File: %s:%d.", __FILE__,     \
               __LINE__);                                                      \
        failed = 1;                                                            \
    }
