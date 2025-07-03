#include "libos.h"
#include "libos_log.h"
#include "task_test.h"

DEFINE_LOG_TEST()

void task_test_entry()
{
    LwU32 access_perms;
    void *base;
    LwU64 size;
    LwU32 aperture;
    LwU64 physical_offset;
    LwU32 pmc_boot_0;
    LibosStatus status;

    construct_log_test();

    LOG_TEST(LOG_LEVEL_INFO, "instance-small 0x%llx\n", TASK_INIT_PMC_SMALL);
    LOG_TEST(LOG_LEVEL_INFO, "instance-huge 0x%llx\n", TASK_INIT_PMC_HUGE);

    /* get ptr to our device region */
    status = libosMemoryQuery(
        LIBOS_TASK_SELF, TASK_INIT_PMC_SMALL, &access_perms, &base, &size, &aperture, &physical_offset);

    LOG_TEST(LOG_LEVEL_INFO, "status=0x%x\n", status);
    LOG_TEST(LOG_LEVEL_INFO, "base=0x%llx\n", base);
    LOG_TEST(LOG_LEVEL_INFO, "offset=0x%llx\n", physical_offset);
    LOG_TEST(LOG_LEVEL_INFO, "pmc_boot_42=0x%x\n", *((LwU32 *)(TASK_INIT_PMC_SMALL+0xa00)));

    /* get ptr to our device region */
    status = libosMemoryQuery(
        LIBOS_TASK_SELF, TASK_INIT_PMC_HUGE, &access_perms, &base, &size, &aperture, &physical_offset);

    LOG_TEST(LOG_LEVEL_INFO, "status=0x%x\n", status);
    LOG_TEST(LOG_LEVEL_INFO, "base=0x%llx\n", base);
    LOG_TEST(LOG_LEVEL_INFO, "offset=0x%llx\n", physical_offset);
    LOG_TEST(LOG_LEVEL_INFO, "pmc_boot_42=0x%x\n", *((LwU32 *)(TASK_INIT_PMC_HUGE+0xa00)));
}
