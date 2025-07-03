#include "debug/elf.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h"
#include "include/libos_init_daemon.h"
#include "include/libos_log.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "task_test.h"

DEFINE_LOG_TEST()

int stack_test(int size)
{
    volatile char foo[128 * 1024];
    volatile char *bar;
    int i;

    bar = &foo[0];

    for (i = 0; i < size; i++)
    {
        bar[i * 4096] = '0' + i;
    }

    for (i = 0; i < size; i++)
    {
        if (bar[i * 4096] != ('0' + i))
        {
            bar  = 0;
            *bar = 'F';
            // assert(0);
        }
    }

    return 0xdeadbeef;
}

void task_test_entry()
{
    int j;
    construct_log_test();

    LOG_TEST(LOG_LEVEL_INFO, "Starting simple DMEM paging test.\n", j);

    j = stack_test(32);
    LOG_TEST(LOG_LEVEL_INFO, "stack_test() returned %08x\n", j);
}
