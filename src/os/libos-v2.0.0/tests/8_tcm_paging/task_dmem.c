#include "task_dmem.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_DMEM();

// number of 32bit entries in our paging buffer
#define PAGED_BUFFER_ENTRIES (1024 * 128)

__attribute__((section(".paged_data"))) LwU32 pagedBuffer[PAGED_BUFFER_ENTRIES];

int dmemPagingTest(LwU32 numEntries)
{
    LwU32 i;

    for (i = 0; i < numEntries; i++)
    {
        pagedBuffer[i] = i + (i % 1024);
    }

    for (i = 0; i < numEntries; i++)
    {
        if (pagedBuffer[i] != (i + (i % 1024)))
        {
            return 0xdeadbeef;
        }
    }

    return 0;
}

void task_dmem_entry(void)
{
    construct_log_dmem();
    LOG_DMEM(LOG_LEVEL_INFO, "DMEM task entry.\n");
    LwU32 numEntries = PAGED_BUFFER_ENTRIES;
    LwU32 rc         = 0;

    rc = dmemPagingTest(numEntries);
    LOG_DMEM(LOG_LEVEL_INFO, "DMEM task returned %08llx.\n", rc);
}
