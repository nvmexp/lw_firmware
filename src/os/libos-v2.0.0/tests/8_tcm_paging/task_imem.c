#include "task_imem.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_IMEM();

// from generated _out/$(BUILD_CFG)/pageable_imem.c
extern LwU32 numPages;
extern LwU32 testFuncAsm[];
extern LwU32 instrSeqOffsets[];

void task_imem_entry()
{
    construct_log_imem();
    LOG_IMEM(LOG_LEVEL_INFO, "IMEM entry point.\n");
    LwU32 pageCount;
    LwU32 (*testFunc)(LwU64);

    // set our function pointer to first instruction sequence
    testFunc = (LwU32(*)(LwU64))(testFuncAsm + instrSeqOffsets[0]);

    pageCount = testFunc(0);

    LOG_IMEM(LOG_LEVEL_INFO, "Processed %d pages\n", pageCount);
}
