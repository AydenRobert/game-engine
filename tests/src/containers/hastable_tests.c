#include "hastable_tests.h"
#include "containers/hashtable.h"

#include "../expect.h"
#include "../test_manager.h"

#include <containers/hashtable.h>
#include <defines.h>

u8 hashtable_should_create_and_destroy() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(u64);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, false, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(u64), table.element_size);
    expect_should_be(3, table.element_count);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 hashtable_should_set_and_get_successfully() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(u64);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, false, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(u64), table.element_size);
    expect_should_be(3, table.element_count);

    u64 testval1 = 23;
    hashtable_set(&table, "test1", &testval1);
    u64 get_testval1 = 0;
    hashtable_get(&table, "test1", &get_testval1);
    expect_should_be(testval1, get_testval1);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

typedef struct ht_test_struct {
    b8 b_value;
    f32 f_value;
    u64 u_value;
} ht_test_struct;

u8 hashtable_should_set_and_get_ptr_successfully() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(ht_test_struct *);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, true, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(ht_test_struct *), table.element_size);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct *testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    hashtable_set_ptr(&table, "test1", (void **)&testval1);

    ht_test_struct *get_testval1 = 0;
    hashtable_get_ptr(&table, "test1", (void **)&get_testval1);

    expect_should_be(testval1->b_value, get_testval1->b_value);
    expect_should_be(testval1->u_value, get_testval1->u_value);
    expect_should_be(testval1->f_value, get_testval1->f_value);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 hashtable_should_set_and_get_nonexistant() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(u64);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, false, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(u64), table.element_size);
    expect_should_be(3, table.element_count);

    u64 testval1 = 23;
    hashtable_set(&table, "test1", &testval1);
    u64 get_testval1 = 0;
    hashtable_get(&table, "test2", &get_testval1);
    expect_should_be(0, get_testval1);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 hashtable_should_set_and_get_ptr_nonexistant() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(ht_test_struct *);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, true, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(ht_test_struct *), table.element_size);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct *testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    hashtable_set_ptr(&table, "test1", (void **)&testval1);

    ht_test_struct *get_testval1 = 0;
    b8 result = hashtable_get_ptr(&table, "test2", (void **)&get_testval1);

    expect_to_be_false(result);
    expect_should_be(0, get_testval1);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 hashtable_should_set_and_unset_ptr() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(ht_test_struct *);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, true, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(ht_test_struct *), table.element_size);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct *testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    b8 result = hashtable_set_ptr(&table, "test1", (void **)&testval1);
    expect_to_be_true(result);

    ht_test_struct *get_testval1 = 0;
    result = hashtable_get_ptr(&table, "test1", (void **)&get_testval1);

    expect_should_be(testval1->b_value, get_testval1->b_value);
    expect_should_be(testval1->u_value, get_testval1->u_value);
    expect_should_be(testval1->f_value, get_testval1->f_value);

    result = hashtable_set_ptr(&table, "test1", 0);
    expect_to_be_true(result);

    ht_test_struct *get_testval2 = 0;
    result = hashtable_get_ptr(&table, "test1", (void **)&get_testval2);

    expect_to_be_false(result);
    expect_should_be(0, get_testval2);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 try_call_non_ptr_on_ptr_table() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(ht_test_struct *);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, true, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(ht_test_struct *), table.element_size);
    expect_should_be(3, table.element_count);

    KDEBUG("The following 2 error messages are intentional.");

    ht_test_struct t;
    t.b_value = true;
    t.u_value = 63;
    t.f_value = 3.1415f;
    b8 result = hashtable_set(&table, "test1", &t);
    expect_to_be_false(result);

    ht_test_struct *get_testval1 = 0;
    result = hashtable_get(&table, "test1", (void **)&get_testval1);
    expect_to_be_false(result);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 try_call_ptr_on_non_ptr_table() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(u64);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, false, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(u64), table.element_size);
    expect_should_be(3, table.element_count);

    KDEBUG("The following 2 error messages are intentional.");

    u64 t = 23;
    u64 *testval1 = &t;
    b8 result = hashtable_set_ptr(&table, "test1", (void **)&testval1);
    expect_to_be_false(result);

    u64 *get_testval1 = 0;
    result = hashtable_get_ptr(&table, "test1", (void **)&get_testval1);
    expect_to_be_false(result);
    expect_should_be(0, get_testval1);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

u8 hashtable_should_set_get_and_update_ptr_successfully() {
    u8 failed = false;

    hashtable table;
    u64 element_size = sizeof(ht_test_struct *);
    u64 element_count = 3;
    u64 memory[3];

    hashtable_create(element_size, element_count, memory, true, &table);

    expect_should_not_be(0, table.memory);
    expect_should_be(sizeof(ht_test_struct *), table.element_size);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct *testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    hashtable_set_ptr(&table, "test1", (void **)&testval1);

    ht_test_struct *get_testval1 = 0;
    hashtable_get_ptr(&table, "test1", (void **)&get_testval1);

    expect_should_be(testval1->b_value, get_testval1->b_value);
    expect_should_be(testval1->u_value, get_testval1->u_value);
    expect_should_be(testval1->f_value, get_testval1->f_value);

    testval1->b_value = false;
    testval1->u_value = 64;
    testval1->f_value = 2.7182f;

    ht_test_struct *get_testval2 = 0;
    hashtable_get_ptr(&table, "test1", (void **)&get_testval2);

    expect_to_be_false(get_testval2->b_value);
    expect_should_be(64, get_testval2->u_value);
    expect_float_to_be(2.7182f, get_testval2->f_value);

    hashtable_destroy(&table);

    expect_should_be(0, table.memory);
    expect_should_be(0, table.element_size);
    expect_should_be(0, table.element_count);

    return failed ? false : true;
}

void hashtable_register_tests() {
    test_manager_register_test(
        hashtable_should_create_and_destroy,
        "Hashtable should create and destroy successfully.");
    test_manager_register_test(hashtable_should_set_and_get_successfully,
                               "Hashtable should set and get successfully.");
    test_manager_register_test(
        hashtable_should_set_and_get_ptr_successfully,
        "Hashtable should set and get pointer successfully.");
    test_manager_register_test(
        hashtable_should_set_and_get_nonexistant,
        "Hashtable should set and get non-existant entry as nothing.");
    test_manager_register_test(
        hashtable_should_set_and_get_ptr_nonexistant,
        "Hashtable should set and get non-existant pointer as nothing.");
    test_manager_register_test(
        hashtable_should_set_and_unset_ptr,
        "Hashtable should set and unset pointer entry as nothing.");
    test_manager_register_test(
        try_call_non_ptr_on_ptr_table,
        "Hashtable try calling pointer functions on non-pointer table.");
    test_manager_register_test(
        try_call_ptr_on_non_ptr_table,
        "Hashtable try calling non-pointer functions on pointer table.");
    test_manager_register_test(
        hashtable_should_set_get_and_update_ptr_successfully,
        "Hashtable should set, get, update, and get pointer again "
        "successfully.");
}
