#include "../common/test-common.h"
#include "libos.h"
#include "task_test.h"

DEFINE_LOG_TEST()

#define VALIDATE(x)                                                                                          \
    if (!(x))                                                                                                \
        __asm("li a0, 0xDEADDEAD ;  csrw 0x780, a0; " ::);

void task_test_entry()
{
    LwU64 payload = 0;

    construct_log_test();

    LOG_TEST(LOG_LEVEL_INFO, "Test 1\n");
    __asm__ __volatile__("wfi" ::: "memory");
    LOG_TEST(LOG_LEVEL_INFO, "Test 2\n");
    __asm__ __volatile__("wfi" ::: "memory");
    LOG_TEST(LOG_LEVEL_INFO, "Test 3\n");
}
