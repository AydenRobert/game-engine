#include "test_manager.h"

#include <containers/darray.h>
#include <core/clock.h>
#include <core/kstring.h>
#include <core/logger.h>

typedef struct test_entry {
    PFN_test func;
    char *desc;
} test_entry;

static test_entry *tests;

void test_manager_init() { tests = darray_create(test_entry); }

void test_manager_register_test(PFN_test func, char *desc) {
    test_entry entry = {func, desc};
    darray_push(tests, entry);
}

void test_manager_run_tests() {
    u32 passed = 0;
    u32 failed = 0;
    u32 bypassed = 0;

    u32 count = darray_length(tests);

    clock total_time;
    clock_start(&total_time);

    for (u32 i = 0; i < count; i++) {
        clock test_time;
        clock_start(&test_time);
        u8 result = tests[i].func();
        u8 cur_failed = 0;

        clock_update(&test_time);
        clock_update(&total_time);

        switch (result) {
        case true:
            passed++;
            break;
        case BYPASS:
            KWARN("[BYPASSED]: %s", tests[i].desc);
            bypassed++;
            break;
        default:
            KERROR("[FAILED]: %s", tests[i].desc);
            failed++;
            cur_failed = 1;
            break;
        }

        char status[20];
        string_format(status, cur_failed ? "*** %d FAILED ***" : "SUCCESS",
                      failed);
        KINFO("Executed %d of %d (skipped %d) %s (%.6f / %.6f seconds)", i + 1,
              count, bypassed, status, test_time.elapsed, total_time.elapsed);

        clock_stop(&test_time);
    }

    clock_stop(&total_time);
    KINFO("Results: %d passed, %d failed, %d skipped.", passed, failed,
          bypassed);
}
