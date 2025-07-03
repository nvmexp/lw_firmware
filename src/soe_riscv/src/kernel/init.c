/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    init.c
 * @brief   Init code for SOE application.
 *
 * This is start "C" code. It creates tasks and starts RTOS.
 */

#define KERNEL_SOURCE_FILE 1

/* ------------------------ LW Includes ------------------------------------- */
#include <lwtypes.h>
#include <riscvifriscv.h>
#include <rmsoecmdif.h>

/* ------------------------ Register Includes ------------------------------- */
#include <engine.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>

/* ------------------------ SafeRTOS Includes ------------------------------- */
#include <lwrtos.h>
#include <sections.h>
#include <lwoslayer.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <drivers/drivers.h>
#include <drivers/mm.h>
#include <drivers/mpu.h>
#include <drivers/odp.h>
#include <syslib/idle.h>
#include <sbilib/sbilib.h>
#include <lw_riscv_address_map.h>

/* ------------------------ Module Includes --------------------------------- */
#include "tasks.h"
#include "unit_dispatch.h"
#include "config/g_profile.h"
#include "config/g_sections_riscv.h"
#include "config/g_soe_hal.h"
#include "config/soe-config.h"
#include "soe_objlsf.h"
#include "soe_objsoe.h"
#include "soe_objintr.h"
#include "soe_objlwlink.h"
#include "soe_objspi.h"
#include "soe_objtherm.h"
#include "soe_objcore.h"
#include "soe_objsmbpbi.h"

/* ------------------------ Macros -------------------- */

#define LWRISCV_MTIME_TICK_FREQ_HZ  (1000000000ULL >> LWRISCV_MTIME_TICK_SHIFT)

#define RUN_INIT_JOB(what, call) do {\
    status = call; \
    if (status != FLCN_OK) \
    { \
        dbgPrintf(LEVEL_ALWAYS, "init: failed to %s: 0x%x\n", what, status); \
        goto err; \
    } \
} while (LW_FALSE)

sysTASK_DATA LwrtosQueueHandle testRequestQueue;

DEFINE_TASK(idle, Idle, TASK_STACK_SIZE, 8);

#define MAP_TASK(name, camelname) \
    RUN_INIT_JOB("map "#name " task.", \
        mmInitTaskMemory(#name, \
                         &name##TaskMpuInfo, \
                         name##MpuHandles, \
                         sizeof(name##MpuHandles) / sizeof(MpuHandle), \
                         UPROC_SECTION_REF(taskCode##camelname), \
                         UPROC_SECTION_REF(taskData##camelname)))

#if DO_TASK_RM_CMDMGMT
DEFINE_TASK(rmCmd, RmCmd, TASK_STACK_SIZE, 8);
#endif // DO_TASK_RM_CMDMGMT

#if DO_TASK_RM_MSGMGMT
DEFINE_TASK(rmMsg, RmMsg, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle rmMsgRequestQueue;
#endif // DO_TASK_RM_MSGMGMT

#if DO_TASK_RM_CHNLMGMT
DEFINE_TASK(chnlMgmt, Chnlmgmt, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle SoeChnMgmtCmdDispQueue;
#endif // DO_TASK_RM_MSGMGMT

#if DO_TASK_RM_THERM
DEFINE_TASK(therm, Therm, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle Disp2QThermThd;
#endif // DO_TASK_RM_THERM

#if DO_TASK_RM_CORE
DEFINE_TASK(core, Core, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle Disp2QCoreThd;
#endif // DO_TASK_RM_CORE

#if DO_TASK_RM_SPI
DEFINE_TASK(spi, Spi, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle Disp2QSpiThd;
#endif // DO_TASK_RM_SPI

#if DO_TASK_RM_BIF
DEFINE_TASK(bif, Bif, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle Disp2QBifThd;
#endif // DO_TASK_RM_BIF

#if DO_TASK_RM_SMBPBI
DEFINE_TASK(smbpbi, Smbpbi, TASK_STACK_SIZE, 8);
sysTASK_DATA LwrtosQueueHandle Disp2QSmbpbiThd;
#endif // DO_TASK_RM_SMBPBI

/* ------------------------ External Function Prototypes -------------------- */
/*
 * WARNING
 * This file runs in *S* mode in *KERNEL* context
 */

int
appMain
(
    LwU64               bootargsPa,
    PRM_SOE_BOOT_PARAMS pBootArgsUnsafe,
    LwUPtr              elfAddrPhys,
    LwU64               elfSize,
    LwUPtr              loadBase,
    LwU64               wprId
)
{
    // MK TODO: error handling
    FLCN_STATUS status = FLCN_OK;

    //
    // These arguments are not used by SOE and are always zero/NULL in
    // offline-extracted builds anyway.
    //
    (void)bootargsPa;
    (void)pBootArgsUnsafe;
    (void)elfAddrPhys;
    (void)elfSize;
    (void)loadBase;
    (void)wprId;

    // Setup canary for SSP
    LWOS_SETUP_STACK_CANARY();

    tlsInit();

    status = debugInit();
    if (status != FLCN_OK)
    {
        appHalt();
    }

    // Install real exception handler now that system is ready.
    exceptionInit();

    //
    // Configure trace buffer (if not enabled already)
    // We want it on as soon as possible because it helps us to debug crashes.
    // Trace buffer will not generate interrupts, so we can enable it before
    // disabling interrupts.
    //
    traceInit();

    lwrtosENTER_CRITICAL(); // Kernel runs with interrupts disabled. Always.

    dbgPuts(LEVEL_ALWAYS, "Initializing SOE application...\n");

    // Initialize MPU
    if (!mpuInit())
    {
        dbgPrintf(LEVEL_CRIT, "Failed to initalize MPU!\n");
        OS_HALT();
    }

    // Initialize IRQ
    irqInit();

    //
    // Init Lsf
    //
    lsfPreInit();

    //
    // Init Intr
    //
    intrPreInit_HAL();

    //
    // Init Lwlink
    //
    lwlinkPreInit_HAL();

    //
    // Init Soe
    //
    soePreInit_HAL();

    //
    // Init Spi
    //
    spiPreInit_HAL();

    //
    // Init Core
    //
    corePreInit_HAL();

    // Init DMA
    dmaInit(SOE_DMAIDX_PHYS_SYS_COH_FN0);

    //
    // Init Smbpbi
    // TODO: Debug the smbpbi task build failure
    //
#if DO_TASK_RM_SMBPBI
    smbpbiPreInit();
#endif
    //
    // LS revocation by checking for LS ucode version against SOE rev in HW fuse
    //
    if (FLCN_OK != (status = soeCheckForLSRevocation_HAL()))
    {
        dbgPrintf(LEVEL_CRIT, "Failed revocation check!\n");
        OS_HALT();
    }

    // Must create and prepare the scheduler before any interactions with it.
    RUN_INIT_JOB("initialize scheduler",
        lwrtosTaskSchedulerInitialize());

    MAP_TASK(idle, Idle);
    RUN_INIT_JOB("create idle task",
        lwrtosTaskCreate(&idleTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = task_idle,
            .taskID = RM_SOE_TASK_ID__IDLE,
            .pMpuInfo = &idleTaskMpuInfo,
            .pTaskStack = (void*)idleStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 0,
        }));

#if DO_TASK_RM_MSGMGMT
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create rm-msg queue",
        lwrtosQueueCreate(&rmMsgRequestQueue, 4, sizeof(RM_FLCN_MSG_SOE), 0));

    MAP_TASK(rmMsg, RmMsg);
    RUN_INIT_JOB("create rm-msg task",
        lwrtosTaskCreate(&rmMsgTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = rmMsgMain,
            .taskID = RM_SOE_TASK_ID_RMMSG,
            .pMpuInfo = &rmMsgTaskMpuInfo,
            .pTaskStack = (void*)rmMsgStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 7,
        }));
#endif


#if DO_TASK_RM_CMDMGMT
    MAP_TASK(rmCmd, RmCmd);
    RUN_INIT_JOB("create rm-cmd task",
        lwrtosTaskCreate(&rmCmdTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = rmCmdMain,
            .taskID     = RM_SOE_TASK_ID_CMDMGMT,
            .pMpuInfo   = &rmCmdTaskMpuInfo,
            .pTaskStack = (void*)rmCmdStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = rmMsgTaskHandle,
            .priority   = 7,
        }));
    dbgPrintf(LEVEL_ALWAYS, "init: Command Mgmt Task created.\n");
#endif // DO_TASK_RM_CMDMGMT


#if DO_TASK_RM_CHNLMGMT
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create Chnl mgmt queue",
        lwrtosQueueCreate(&SoeChnMgmtCmdDispQueue, 4, sizeof(RM_FLCN_CMD_SOE), 0));

    MAP_TASK(chnlMgmt, Chnlmgmt);
    RUN_INIT_JOB("create channel management task",
        lwrtosTaskCreate(&chnlMgmtTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = chnlMgmtMain,
            .taskID = RM_SOE_TASK_ID_CHNMGMT,
            .pMpuInfo = &chnlMgmtTaskMpuInfo,
            .pTaskStack = (void*)chnlMgmtStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 7,
        }));
    dbgPrintf(LEVEL_ALWAYS, "init: Channel Mgmt Task created.\n");
#endif // DO_TASK_RM_CHNLMGMT

#if DO_TASK_RM_THERM
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create Thermal mgmt queue",
        lwrtosQueueCreate(&Disp2QThermThd, 4, sizeof(DISPATCH_THERM), 0));

    MAP_TASK(therm, Therm);
    RUN_INIT_JOB("create thermal task",
        lwrtosTaskCreate(&thermTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = thermMain,
            .taskID     = RM_SOE_TASK_ID_THERM,
            .pMpuInfo   = &thermTaskMpuInfo,
            .pTaskStack = (void*)thermStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = thermTaskHandle,
            .priority   = 7,
        }));

    dbgPrintf(LEVEL_ALWAYS, "init: Thermal mgmt Task created.\n");
#endif // DO_TASK_RM_THERM

#if DO_TASK_RM_CORE
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create Core mgmt queue",
        lwrtosQueueCreate(&Disp2QCoreThd, 4, sizeof(DISPATCH_CORE), 0));

    MAP_TASK(core, Core);
    RUN_INIT_JOB("create Core task",
        lwrtosTaskCreate(&coreTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = coreMain,
            .taskID     = RM_SOE_TASK_ID_CORE,
            .pMpuInfo   = &coreTaskMpuInfo,
            .pTaskStack = (void*)coreStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = coreTaskHandle,
            .priority   = 7,
        }));

    dbgPrintf(LEVEL_ALWAYS, "init: Core mgmt Task created.\n");
#endif // DO_TASK_RM_CORE

#if DO_TASK_RM_SPI
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create spi mgmt queue",
        lwrtosQueueCreate(&Disp2QSpiThd, 4, sizeof(RM_FLCN_CMD_SOE), 0));

    MAP_TASK(spi, Spi);
    RUN_INIT_JOB("create spi task",
        lwrtosTaskCreate(&spiTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = spiMain,
            .taskID     = RM_SOE_TASK_ID_SPI,
            .pMpuInfo   = &spiTaskMpuInfo,
            .pTaskStack = (void*)spiStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = spiTaskHandle,
            .priority   = 7,
        }));

    dbgPrintf(LEVEL_ALWAYS, "init: Spi mgmt Task created.\n");
#endif // DO_TASK_RM_SPI

#if DO_TASK_RM_BIF
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create bif mgmt queue",
        lwrtosQueueCreate(&Disp2QBifThd, 4, sizeof(RM_FLCN_CMD_SOE), 0));

    MAP_TASK(bif, Bif);
    RUN_INIT_JOB("create bif task",
        lwrtosTaskCreate(&bifTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = bifMain,
            .taskID     = RM_SOE_TASK_ID_BIF,
            .pMpuInfo   = &bifTaskMpuInfo,
            .pTaskStack = (void*)bifStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = bifTaskHandle,
            .priority   = 7,
        }));

    dbgPrintf(LEVEL_ALWAYS, "init: Bif mgmt Task created.\n");
#endif // DO_TASK_RM_BIF

#if DO_TASK_RM_SMBPBI
    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create smbpbi mgmt queue",
        lwrtosQueueCreate(&Disp2QSmbpbiThd, 4, sizeof(DISPATCH_SMBPBI), 0));

    MAP_TASK(smbpbi, Smbpbi);
    RUN_INIT_JOB("create smbpbi task",
        lwrtosTaskCreate(&smbpbiTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = smbpbiMain,
            .taskID     = RM_SOE_TASK_ID_SMBPBI,
            .pMpuInfo   = &smbpbiTaskMpuInfo,
            .pTaskStack = (void*)smbpbiStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = smbpbiTaskHandle,
            .priority   = 7,
        }));

    dbgPrintf(LEVEL_ALWAYS, "init: Smbpbi mgmt Task created.\n");
#endif // DO_TASK_RM_SMBPBI

    dbgPrintf(LEVEL_ALWAYS, "init: Starting scheduler.\n");

    csr_set(LW_RISCV_CSR_SCOUNTEREN,
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _TM, 1) |
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _IR, 1));

    RUN_INIT_JOB("start scheduler", lwrtosTaskSchedulerStart());

err:
    dbgPrintf(LEVEL_ALWAYS, "Init failed: 0x%x\n", status);
    return status;
}

void
lwrtosHookError(void *pTask, LwS32 errorCode)
{
    dbgPrintf(LEVEL_ALWAYS, "%s: Task %p, code: %d\n", __FUNCTION__, pTask, errorCode);
    appHalt();
}

sysSYSLIB_CODE void
lwrtosHookIdle(void)
{
}

/* Sets and enable the timer interrupt */
#if USE_CBB
//TODO Implement properly on cheetah, don't just assume fmodel
LwU64
lwrtosHookGetTimerPeriod(void)
{
    LwU64 tick = 0;
    tick = 31250000 / LWRTOS_TICK_RATE_HZ / 5;
    dbgPrintf(LEVEL_ALWAYS, "Configuring systick for FMODEL/EMU to %llu... \n", tick);

    if (tick == 0)
    {
        dbgPrintf(LEVEL_ALWAYS, "Tick config (%lld) is wrong.\n", tick);
        appHalt();
    }

    return tick;
}

#else

LwU64
lwrtosHookGetTimerPeriod(void)
{
    LwU64 tick = 0;

    /*
     * Verify platform type. That is mostly to fix dFPGA
     */
    tick = LWRISCV_MTIME_TICK_FREQ_HZ / LWRTOS_TICK_RATE_HZ;
    dbgPrintf(LEVEL_INFO, "Configuring systick to %llu... \n", tick);
    return tick;
}
#endif

void
appShutdown(void)
{
    //
    // This is a "clean" shutdown, so disable HALT interrupt
    // to prevent the shutdown() call from sending a HALT to RM
    //
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, DRF_DEF(_PRISCV, _RISCV_IRQMCLR, _HALT, _SET));

    //
    // MK TODO: this should also send "i shut down" message to RM
    // (TODO once messaging is handled in kernel)
    //

    dbgPuts(LEVEL_ALWAYS, "Shutting down...\n");

    notifyExit();

    sbiShutdown();
}

/*
 * Temporary intrinsic to avoid linking with not fully implemented
 * falcon_intrinsics_sim.c
 *
 * This has to be removed once intrinsics implementation is fixed.
 */
void
appHalt(void)
{
    dbgPuts(LEVEL_ALWAYS, "RTOS Halt requested.\n");

    // Manually trigger the _HALT interrupt to RM so RM can handle it for better debugability
    intioWrite(LW_PRGNLCL_RISCV_IRQDEST,  DRF_DEF(_PRGNLCL, _RISCV_IRQDEST, _HALT, _HOST) | intioRead(LW_PRGNLCL_RISCV_IRQDEST));
    intioWrite(LW_PRGNLCL_RISCV_IRQMSET,  DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _HALT, _SET));
    intioWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _HALT, _SET));
    intioWrite(LW_PRGNLCL_RISCV_ICD_CMD,  DRF_DEF(_PRGNLCL, _RISCV_ICD_CMD, _OPC, _STOP));

    while (LW_TRUE)
    {
    }
}
