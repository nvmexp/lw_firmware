/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    init.c
 * @brief   Init code for SEC2 application.
 *
 * This is start "C" code. It creates tasks and starts RTOS.
 */

#define KERNEL_SOURCE_FILE 1

/* ------------------------ LW Includes ------------------------------------- */
#include <lwtypes.h>
#include <riscvifriscv.h>
#include <rmsec2cmdif.h>

/* ------------------------ Register Includes ------------------------------- */
#include <dev_top.h>
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
#include "config/g_sec2_hal.h"
#include "config/sec2-config.h"
#include "sec2_objlsf.h"

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
sysTASK_DATA LwrtosQueueHandle Sec2ChnMgmtCmdDispQueue;
#endif // DO_TASK_RM_MSGMGMT


/* ------------------------ External Function Prototypes -------------------- */
/*
 * WARNING
 * This file runs in *S* mode in *KERNEL* context
 */
static RM_SEC2_BOOT_PARAMS bootArgs;
int
appMain
(
    LwU64               bootargsPa,
    PRM_SEC2_BOOT_PARAMS pBootArgsUnsafe,
    LwUPtr              elfAddrPhys,
    LwU64               elfSize,
    LwUPtr              loadBase,
    LwU64               wprId
)
{
    // MK TODO: error handling
    FLCN_STATUS status = FLCN_OK;
    LwU64 riscvPhysAddrBase = 0;

    // Setup canary for SSP
    LWOS_SETUP_STACK_CANARY();

    tlsInit();

    // Sanity check size, this is / may be unselwred memory.
    if (pBootArgsUnsafe->riscv.bl.size != sizeof(RM_SEC2_BOOT_PARAMS))
    {
        appHalt();
    }

    // Worst that can happen here - we read junk or get read exception.
    memcpy(&bootArgs, pBootArgsUnsafe, sizeof(RM_SEC2_BOOT_PARAMS));

    // Check version
   if (bootArgs.riscv.bl.version != RM_RISCV_BOOTLDR_VERSION)
    {
        appHalt();
    }

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

    dbgPuts(LEVEL_ALWAYS, "Initializing SEC2 application...\n");

    // Initialize memory subsystem first
    status = mmInit(loadBase, elfAddrPhys, wprId);
    if (status != FLCN_OK)
    {
        appHalt();
    }

    //
    // Initialize drivers
    //

    irqInit();

    //
    // Init Lsf
    //
    lsfPreInit();

    //
    // App entry checks
    //
    if (FLCN_OK != (status = sec2EntryChecks_HAL()))
    {
        appHalt();
    }

    //
    // Check if FALCON_CTXSW_ERROR is reported
    //
    if (FLCN_OK != (status = sec2CheckIfCtxswErrorReported_HAL()))
    {
        appHalt();
    }

    while (LW_TRUE)
    {
#ifdef LW_RISCV_AMAP_SYSGPA_START
        if ((loadBase >= LW_RISCV_AMAP_SYSGPA_START) &&
            (loadBase < LW_RISCV_AMAP_SYSGPA_END))
        {
            dmaInit(RM_PMU_DMAIDX_PHYS_SYS_COH);
            riscvPhysAddrBase = LW_RISCV_AMAP_SYSGPA_START;
            break;
        }
#endif // LW_RISCV_AMAP_SYSGPA_START

#ifdef LW_RISCV_AMAP_FBGPA_START
        if ((loadBase >= LW_RISCV_AMAP_FBGPA_START) &&
            (loadBase < LW_RISCV_AMAP_FBGPA_END))
        {
            dmaInit(RM_PMU_DMAIDX_UCODE);
            riscvPhysAddrBase = LW_RISCV_AMAP_FBGPA_START;
            break;
        }
#endif // LW_RISCV_AMAP_FBGPA_START

        // Bases do not match any known configuration
        appHalt();
    }
    coreDumpInit(bootArgs.riscv.rtos.coreDumpPhys, bootArgs.riscv.rtos.coreDumpSize);

#if ODP_ENABLED
    odpInit(UPROC_SECTION_dmemHeap_REF, riscvPhysAddrBase);
#else
    // Quash compile warning
    (void) riscvPhysAddrBase;
#endif

   // Must create and prepare the scheduler before any interactions with it.
    RUN_INIT_JOB("initialize scheduler",
        lwrtosTaskSchedulerInitialize());

    MAP_TASK(idle, Idle);
    RUN_INIT_JOB("create idle task",
        lwrtosTaskCreate(&idleTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = task_idle,
            .taskID = RM_SEC2_TASK_ID__IDLE,
            .pMpuInfo = &idleTaskMpuInfo,
            .pTaskStack = (void*)idleStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 0,
        }));

#if DO_TASK_RM_MSGMGMT

    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create rm-msg queue",
        lwrtosQueueCreate(&rmMsgRequestQueue, 4, sizeof(RM_FLCN_MSG_SEC2), 0));

    MAP_TASK(rmMsg, RmMsg);
    RUN_INIT_JOB("create rm-msg task",
        lwrtosTaskCreate(&rmMsgTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = rmMsgMain,
            .taskID = RM_SEC2_TASK_ID_RMMSG,
            .pMpuInfo = &rmMsgTaskMpuInfo,
            .pTaskStack = (void*)rmMsgStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 7,
        }));

    coreDumpRegisterTask(rmMsgTaskHandle, "rm-msg");
#endif


#if DO_TASK_RM_CMDMGMT
    MAP_TASK(rmCmd, RmCmd);
    RUN_INIT_JOB("create rm-cmd task",
        lwrtosTaskCreate(&rmCmdTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode  = rmCmdMain,
            .taskID     = RM_SEC2_TASK_ID_CMDMGMT,
            .pMpuInfo   = &rmCmdTaskMpuInfo,
            .pTaskStack = (void*)rmCmdStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .pParameters = rmMsgTaskHandle,
            .priority   = 7,
        }));

    coreDumpRegisterTask(rmCmdTaskHandle, "rm-cmd");
    dbgPrintf(LEVEL_ALWAYS, "init: Command Mgmt Task created.\n");
#endif // DO_TASK_RM_CMDMGMT


#if DO_TASK_RM_CHNLMGMT
    dbgPuts(LEVEL_ALWAYS, "Channel mgmt starting...\n");


    // MK TODO: queue size probably has to change.
    // Best thing would be if tasks can init their own queues but then we'd need init process for tasks.
    RUN_INIT_JOB("create Chnl mgmt queue",
        lwrtosQueueCreate(&Sec2ChnMgmtCmdDispQueue, 4, sizeof(RM_FLCN_CMD_SEC2), 0));

    MAP_TASK(chnlMgmt, Chnlmgmt);
    RUN_INIT_JOB("create channel management task",
        lwrtosTaskCreate(&chnlMgmtTaskHandle, &(const LwrtosTaskCreateParameters) {
            .pTaskCode = chnlMgmtMain,
            .taskID = RM_SEC2_TASK_ID_CHNMGMT,
            .pMpuInfo = &chnlMgmtTaskMpuInfo,
            .pTaskStack = (void*)chnlMgmtStack,
            .stackDepth = TASK_STACK_SIZE_WORDS,
            .priority = 7,
        }));

    coreDumpRegisterTask(chnlMgmtTaskHandle, "RM Channel Mgmt");
    dbgPrintf(LEVEL_ALWAYS, "init: Channel Mgmt Task created.\n");
#endif

    dbgPrintf(LEVEL_ALWAYS, "init: Starting scheduler.\n");

    // TODO : sahilc : Uncomment in next CL
    //mutexEstablishMapping_HAL();

    csr_set(LW_RISCV_CSR_SCOUNTEREN,
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _TM, 1) |
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _IR, 1));

    RUN_INIT_JOB("start scheduler", lwrtosTaskSchedulerStart());
err:
    dbgPrintf(LEVEL_ALWAYS, "Initialisation failed: 0x%x\n", status);
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
    switch (DRF_VAL(_PTOP, _PLATFORM, _TYPE, bar0Read(LW_PTOP_PLATFORM)))
    {
    /*
     * Clocks at dFPGA are different than on other platforms - ptimer *seems*
     * to run at 250MHz, mtime is 32x slower.
     */
    case LW_PTOP_PLATFORM_TYPE_FPGA:
        // TODO: patch this for the GH100 mtime tick change if needed!!!!!!
        tick = 250000000 / LWRTOS_TICK_RATE_HZ;
        dbgPrintf(LEVEL_INFO, "Configuring systick for FPGA to %llu... \n", tick);
        break;
    /*
     * For Si and RTL use real timings based on what is expected in PTIME.
     * One mtime tick is 32ns == 31250000 Hz.
     * Bring-up TODO: check if clocks run at declared speed.
     */
    case LW_PTOP_PLATFORM_TYPE_FMODEL: // fmodel is fast enough, no need to decrease timer period.
    case LW_PTOP_PLATFORM_TYPE_SILICON:
    case LW_PTOP_PLATFORM_TYPE_RTL:
    case LW_PTOP_PLATFORM_TYPE_EMU: // MK TODO: qtfpga is fast enough, no need to tamper with ticks
    default:
        tick = LWRISCV_MTIME_TICK_FREQ_HZ / LWRTOS_TICK_RATE_HZ;
        dbgPrintf(LEVEL_INFO, "Configuring systick for Silicon to %llu... \n", tick);
        break;
    }

    if (tick == 0)
    {
        dbgPrintf(LEVEL_CRIT, "Tick config (%lld) is wrong.\n", tick);
        appHalt();
    }

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
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _HALT, _SET));

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
    intioWrite(LW_PRGNLCL_RISCV_IRQDEST, DRF_DEF(_PRGNLCL, _RISCV_IRQDEST, _HALT, _HOST) | intioRead(LW_PRGNLCL_RISCV_IRQDEST));
    intioWrite(LW_PRGNLCL_RISCV_IRQMSET, DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _HALT, _SET));
    intioWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _HALT, _SET));
    intioWrite(LW_PRGNLCL_RISCV_ICD_CMD, DRF_DEF(_PRGNLCL, _RISCV_ICD_CMD, _OPC, _STOP));

    while (LW_TRUE)
    {
    }
}
