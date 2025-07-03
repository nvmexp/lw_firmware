#include "test_app.h"
#include "libos.h"
#include "libos_log.h"
#include "../common/test-common.h"
#include "test-registers.h"
#include <drivers/common/inc/hwref/ampere/ga102/dev_gsp.h>

DEFINE_LOG_TASK()

volatile LwU8 * pri = (LwU8 *)TASK_INIT_PRIV;

LwU32 priv_read(LwU32 addr) {
    return *(volatile LwU32 *)(pri + addr);
}

void priv_write(LwU32 addr, LwU32 value) {
    *(volatile LwU32 *)(pri + addr) = value;
}

void task_entry()
{
    construct_log_task();
    LibosLocalShuttleId shuttleFired;

    LOG_TASK(LOG_LEVEL_INFO, "Case 0. Interrupt raised while in wait for shuttle. (scheduler_wait)\n");

    // Kick off an interrupt receive
    LOG_AND_VALIDATE(LOG_TASK, 
        libosInterruptAsyncRecv(LIBOS_SHUTTLE_DEFAULT, 0xFFFFFFFF) == LIBOS_OK);

    LOG_TASK(LOG_LEVEL_INFO, "   Wait with timeout.\n");
    LOG_AND_VALIDATE(LOG_TASK, 
        libosPortWait(LIBOS_SHUTTLE_DEFAULT, &shuttleFired, 0, 0, 100) == LIBOS_ERROR_TIMEOUT);

    LOG_TASK(LOG_LEVEL_INFO, "   Setting LW_PTEST_RAISE_INTERRUPT_ON_NEXT_WFI\n");
    priv_write(LW_PTEST_RAISE_INTERRUPT_ON_NEXT_WFI, 1);

    // Wait for the interrupt to arrive.
    // Since no interrupt is pending, we'll enter the "idle" state triggering WFI.
    // the test-driver.cpp sees this and fires an interrupt hen.
    shuttleFired = 0;
    LOG_AND_VALIDATE(LOG_TASK, 
        libosPortWait(LIBOS_SHUTTLE_DEFAULT, &shuttleFired, 0, 0, 10000000) == LIBOS_OK);
    LOG_TASK(LOG_LEVEL_INFO, "   Shuttle fired = %d\n", shuttleFired);

    LOG_TASK(LOG_LEVEL_INFO, "   Wait with timeout.\n");
    libosShuttleReset(LIBOS_SHUTTLE_DEFAULT);
    LOG_AND_VALIDATE(LOG_TASK, 
        libosPortWait(LIBOS_SHUTTLE_DEFAULT, &shuttleFired, 0, 0, 100) == LIBOS_ERROR_TIMEOUT);

    LOG_TASK(LOG_LEVEL_INFO, "   **SUBTEST PASSED**\n");
    
    LOG_TASK(LOG_LEVEL_INFO, "Case 1. Interrupt already pending when wait starts\n");

    // Kick off an interrupt receive
    LOG_AND_VALIDATE(LOG_TASK, 
        libosInterruptAsyncRecv(LIBOS_SHUTTLE_DEFAULT, 0xFFFFFFFF) == LIBOS_OK);
    
    LOG_TASK(LOG_LEVEL_INFO, "   Setting LW_PTEST_RAISE_INTERRUPT\n");
    priv_write(LW_PTEST_RAISE_INTERRUPT, 1);

    // Wait for the interrupt to arrive.
    // Since no interrupt is pending, we'll enter the "idle" state triggering WFI.
    // the test-driver.cpp sees this and fires an interrupt hen.
    shuttleFired = 0;
    LOG_AND_VALIDATE(LOG_TASK, 
        libosPortWait(LIBOS_SHUTTLE_DEFAULT, &shuttleFired, 0, 0, 1000000) == LIBOS_OK);
    LOG_TASK(LOG_LEVEL_INFO, "   Shuttle fired = %d\n", shuttleFired);

    LOG_TASK(LOG_LEVEL_INFO, "   **SUBTEST PASSED**\n");
    
    LOG_TASK(LOG_LEVEL_INFO, "Case 2. Interrupt already pending when recv starts\n");

    LOG_TASK(LOG_LEVEL_INFO, "   Setting LW_PTEST_RAISE_INTERRUPT\n");
    priv_write(LW_PTEST_RAISE_INTERRUPT, 1);

    // Kick off an interrupt receive
    LOG_AND_VALIDATE(LOG_TASK, 
        libosInterruptAsyncRecv(LIBOS_SHUTTLE_DEFAULT, 0xFFFFFFFF) == LIBOS_OK);   

    // Wait for the interrupt to arrive.
    // Since no interrupt is pending, we'll enter the "idle" state triggering WFI.
    // the test-driver.cpp sees this and fires an interrupt hen.
    shuttleFired = 0;
    LOG_AND_VALIDATE(LOG_TASK, 
        libosPortWait(LIBOS_SHUTTLE_DEFAULT, &shuttleFired, 0, 0, 1000000) == LIBOS_OK);
    LOG_TASK(LOG_LEVEL_INFO, "   Shuttle fired = %d\n", shuttleFired);

    LOG_TASK(LOG_LEVEL_INFO, "   **SUBTEST PASSED**\n");
        
}
