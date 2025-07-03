#include "libos.h"
#include "libos_log.h"
#include "task_test.h"

DEFINE_LOG_TEST();

__attribute__((section(".paged_text"))) static void testFunc2(volatile LwU32 *ptr) { *ptr += 66666; }

__attribute__((section(".paged_text"))) LwU32 testFunc(volatile LwU32 *ptr)
{
    int i;
    LwU32 ret = 0xbabebabe;

    *ptr = 0xdeadbeef;

    for (i = 0; i < 4096; i++)
        ret++;

    testFunc2(&ret);

    return ret;
}

void task_test_entry()
{
    construct_log_test();
    LOG_TEST(LOG_LEVEL_INFO, "Starting simple IMEM paging test.\n");

    volatile LwU32 foo = 10;
    volatile LwU32 bar = 0;

    bar = testFunc(&foo);
    LOG_TEST(LOG_LEVEL_INFO, "bar = %08X\n", bar);

    foo = foo + bar;
    LOG_TEST(LOG_LEVEL_INFO, "foo = %08X\n", foo);
}
