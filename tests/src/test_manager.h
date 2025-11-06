#pragma once

#include <defines.h>

#define BYPASS 2

typedef u8 (*PFN_test)(void);

void test_manager_init();

void test_manager_register_test(PFN_test func, char *desc);

void test_manager_run_tests();
