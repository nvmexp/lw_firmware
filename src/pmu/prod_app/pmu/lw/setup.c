/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file setup.c
 */

/* ------------------------- System Includes -------------------------------- */
#ifdef UPROC_RISCV
#include <drivers/drivers.h>
#endif /* UPROC_RISCV */

#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_perfmon.h"
#include "cmdmgmt/cmdmgmt_instrumentation.h"
#include "cmdmgmt/cmdmgmt_ustreamer.h"
#if (PMUCFG_FEATURE_ENABLED(PMUTASK_ACR))
#include "acr/pmu_objacr.h"
#endif
#include "task_watchdog.h"
#include "pmu_fbflcn_if.h"
#include "pmu_objic.h"
#include "pmu_objdisp.h"
#include "pmu_objlpwr.h"
#include "pmu_objdi.h"
#include "pmu_objfifo.h"
#include "pmu_objms.h"
#include "objnne.h"
#include "pmu_objgcx.h"
#include "pmu_objspi.h"
#include "lwostask.h"
#include "lwostmrcallback.h"
#include "lib/lib_sandbag.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Handles for all tasks spawned by the PMU application.  Each is named using
 * a common-prefix for colwenient symbol look-up.
 */
LwrtosTaskHandle OsTaskCmdMgmt       = NULL;
LwrtosTaskHandle OsTaskPcm           = NULL;
LwrtosTaskHandle OsTaskPcmEvent      = NULL;
LwrtosTaskHandle OsTaskLpwr          = NULL;
LwrtosTaskHandle OsTaskLpwrLp        = NULL;
LwrtosTaskHandle OsTaskSeq           = NULL;
LwrtosTaskHandle OsTaskI2c           = NULL;
LwrtosTaskHandle OsTaskGCX           = NULL;
#if OSTASK_ENABLED(PERFMON)
LwrtosTaskHandle OsTaskPerfMon       = NULL;
#endif
LwrtosTaskHandle OsTaskTherm         = NULL;
LwrtosTaskHandle OsTaskDisp          = NULL;
LwrtosTaskHandle OsTaskHdcp          = NULL;
LwrtosTaskHandle OsTaskPmgr          = NULL;
LwrtosTaskHandle OsTaskAcr           = NULL;
LwrtosTaskHandle OsTaskSpi           = NULL;
LwrtosTaskHandle OsTaskPerf          = NULL;
LwrtosTaskHandle OsTaskPerfDaemon    = NULL;
LwrtosTaskHandle OsTaskNne           = NULL;

/*!
 * Cached copy of the initialization arguments used to boot the PMU.
 */
RM_PMU_CMD_LINE_ARGS PmuInitArgs;

LwrtosQueueHandle UcodeQueueIdToQueueHandleMap[PMU_QUEUE_ID__COUNT];

LwU8 UcodeQueueIdToTaskIdMap[PMU_QUEUE_ID__LAST_TASK_QUEUE + 1U] =
{
    [PMU_QUEUE_ID_CMDMGMT]      = RM_PMU_TASK_ID_CMDMGMT,
    [PMU_QUEUE_ID_GCX]          = RM_PMU_TASK_ID_GCX,
    [PMU_QUEUE_ID_LPWR]         = RM_PMU_TASK_ID_LPWR,
    [PMU_QUEUE_ID_I2C]          = RM_PMU_TASK_ID_I2C,
    [PMU_QUEUE_ID_SEQ]          = RM_PMU_TASK_ID_SEQ,
    [PMU_QUEUE_ID_PCMEVT]       = RM_PMU_TASK_ID_PCMEVT,
    [PMU_QUEUE_ID_PMGR]         = RM_PMU_TASK_ID_PMGR,
    [PMU_QUEUE_ID_PERFMON]      = RM_PMU_TASK_ID_PERFMON,
    [PMU_QUEUE_ID_DISP]         = RM_PMU_TASK_ID_DISP,
    [PMU_QUEUE_ID_THERM]        = RM_PMU_TASK_ID_THERM,
    [PMU_QUEUE_ID_HDCP]         = RM_PMU_TASK_ID_HDCP,
    [PMU_QUEUE_ID_ACR]          = RM_PMU_TASK_ID_ACR,
    [PMU_QUEUE_ID_SPI]          = RM_PMU_TASK_ID_SPI,
    [PMU_QUEUE_ID_PERF]         = RM_PMU_TASK_ID_PERF,
    [PMU_QUEUE_ID_LOWLATENCY]   = RM_PMU_TASK_ID_LOWLATENCY,
    [PMU_QUEUE_ID_PERF_DAEMON]  = RM_PMU_TASK_ID_PERF_DAEMON,
    [PMU_QUEUE_ID_BIF]          = RM_PMU_TASK_ID_BIF,
    [PMU_QUEUE_ID_PERF_CF]      = RM_PMU_TASK_ID_PERF_CF,
    [PMU_QUEUE_ID_LPWR_LP]      = RM_PMU_TASK_ID_LPWR_LP,
    [PMU_QUEUE_ID_NNE]          = RM_PMU_TASK_ID_NNE,
};

const LwU8 UcodeQueueIdLastTaskQueue = PMU_QUEUE_ID__LAST_TASK_QUEUE;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Assure that the SW feature is in sync with the compiler profile flag.
 */
#if ON_DEMAND_PAGING_BLK && !defined(UTF_UCODE_BUILD)
    ct_assert(PMUCFG_FEATURE_ENABLED(PMU_ODP_SUPPORTED));
#else // ON_DEMAND_PAGING_BLK
    ct_assert(!PMUCFG_FEATURE_ENABLED(PMU_ODP_SUPPORTED));
#endif // ON_DEMAND_PAGING_BLK

/* ------------------------- Static Function Prototypes --------------------- */
static void        s_pmuInitOverlaysPreload(void);
static FLCN_STATUS s_pmuEnginesConstruct(void);
static void        s_pmuEnginesPreInit(void);
static FLCN_STATUS s_pmuTasksCreate(void);

/* ------------------------- Implementation --------------------------------- */
/*!
 * Initializes the PMU RTOS application.  This function is responsible for
 * creating all tasks, queues, mutexes, and semaphores.
 *
 * @param[in]   pArgs   Pointer to the command-line argument structure.
 *
 * @return  Status of the exelwtion (TODO: to be properly defined).
 */
int
initPmuApp
(
    RM_PMU_CMD_LINE_ARGS *pArgs
)
{
    FLCN_STATUS status;

    if (pArgs == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto initPmuApp_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VERIFY_ENGINE))
    {
        status = pmuPreInitVerifyEngine_HAL(&Pmu);
        if (status != FLCN_OK)
        {
            goto initPmuApp_exit;
        }
    }

    s_pmuInitOverlaysPreload();

    // Initialize stack canary (for SSP, if enabled).
#ifdef IS_SSP_ENABLED_WITH_SCP
    lwosSspRandInit();
#endif // IS_SSP_ENABLED_WITH_SCP
    LWOS_SETUP_STACK_CANARY();

    //
    // Save off a copy of the initialization arguments for ease-of-access and
    // debugging.
    //
    PmuInitArgs = *pArgs;
    //
    // Disable all Falcon interrupt sources.  This must be done in case
    // interrupts (on IV0 or IV1) are enabled between now and when the
    // scheduler is enabled.  This is avoid scenarios where the OS timer tick
    // fires on IV1 before the scheduler is started or a GPIO interrupt fires
    // on IV0 before the IV0 interrupt handler is installed.
    //
    icDisableAllInterrupts_HAL(&Ic);
    icHaltIntrEnable_HAL(&(Ic.hal));
    icRiscvSwgen1IntrEnable_HAL(&(Ic.hal));
    icRiscvIcdIntrEnable_HAL(&(Ic.hal));

    // Initialize the LWOS specific data.
    status = lwosInit();
    if (status != FLCN_OK)
    {
        goto initPmuApp_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackMapInit();
#endif

    // Construct PMU engines.
    status = s_pmuEnginesConstruct();
    if (status != FLCN_OK)
    {
        goto initPmuApp_exit;
    }

    // On Falcon, the first printf should appear AFTER PRINT construction.
    DBG_PRINTK(("initPmuApp\n"));

    status = s_pmuTasksCreate();
    if (status != FLCN_OK)
    {
        goto initPmuApp_exit;
    }

    // Pre-initialize PMU engines.
    s_pmuEnginesPreInit();

    // Pre-initialize PERFMON task.
    if (OSTASK_ENABLED(PERFMON))
    {
        perfmonPreInit();
    }

    // Pre-initialize SPI task.
    if (OSTASK_ENABLED(SPI))
    {
        spiPreInit();
    }

    if (OSTASK_ENABLED(WATCHDOG))
    {
        status = watchdogPreInit();
        if (status != FLCN_OK)
        {
            goto initPmuApp_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_INSTRUMENTATION))
    {
        status = pmuInstrumentationBegin();
        if (status != FLCN_OK)
        {
            goto initPmuApp_exit;
        }
    }

// TODO: yaotianf: Remove this ifdef guard when uStreamer is enabled in TOT.
#ifdef USTREAMER
    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_USTREAMER))
    {
        status = pmuUstreamerInitialize();
        if (status != FLCN_OK)
        {
            goto initPmuApp_exit;
        }
    }
#endif // USTREAMER

initPmuApp_exit:

    if (status == FLCN_OK)
    {
        // Start the scheduler.
        status = lwrtosTaskSchedulerStart();
    }

    if (status != FLCN_OK)
    {
        DBG_PRINTK(("PMU failed to initialize itself and start the task scheduler\n"));
        //
        // TODO: Find a way to communicate to the RM (even behind the message
        // queue's back) that the falcon has failed to initialize itself and
        // start the task scheduler.
        //
        PMU_HALT();
    }

    return 3;
}

/* ------------------------- Private Functions ------------------------------ */
static void
s_pmuInitOverlaysPreload(void)
{
#ifndef ON_DEMAND_PAGING_BLK
    LwU8  ovlList[PMU_INIT_IMEM_OVERLAYS__COUNT];
    LwU32 index;

    // Initialize overlay List
    for (index = 0; index < PMU_INIT_IMEM_OVERLAYS__COUNT; index++)
    {
        ovlList[index] = OVL_INDEX_ILWALID;
    }

    // Update the overlay list with INIT overlays
    ovlList[PMU_INIT_IMEM_OVERLAYS_INDEX_INIT]    = OVL_INDEX_IMEM(init);
    ovlList[PMU_INIT_IMEM_OVERLAYS_INDEX_LIBINIT] = OVL_INDEX_IMEM(libInit);
#if PMUCFG_FEATURE_ENABLED(PMU_LIGHT_SELWRE_FALCON_GC6_PRELOAD)
    ovlList[PMU_INIT_IMEM_OVERLAYS_INDEX_LIBGC6]  = OVL_INDEX_IMEM(libGC6);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIGHT_SELWRE_FALCON_GC6_PRELOAD)
#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_VC))
    ovlList[PMU_INIT_IMEM_OVERLAYS_INDEX_LIBVC]   = OVL_INDEX_IMEM(libVc);
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_VC))
#ifdef IS_SSP_ENABLED_WITH_SCP
    ovlList[PMU_INIT_IMEM_OVERLAYS_INDEX_SCP_RAND] = OVL_INDEX_IMEM(libScpRandHs);
#endif // IS_SSP_ENABLED_WITH_SCP

    OSTASK_LOAD_IMEM_OVERLAYS_LS(ovlList, PMU_INIT_IMEM_OVERLAYS__COUNT);
#endif // ON_DEMAND_PAGING_BLK
}

static FLCN_STATUS
s_pmuEnginesConstruct(void)
{
    FLCN_STATUS status;

    //
    // Init HW mutex as early as possible to make sure HW mutex is ready at the
    // earliest point.
    //
#if PMUCFG_ENGINE_ENABLED(MUTEX)
    {
        status = constructMutex();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(MUTEX)
    //
    // Now construct all objects and HAL modules. This will setup all of the
    // HAL function pointers. Note that not all engines are enabled by default
    // so checks must be made for some engines to determine if construction is
    // appropriate.
    //

    // The PMU engine is always enabled
    status = constructPmu();
    if (status != FLCN_OK)
    {
        goto s_pmuEnginesConstruct_exit;
    }

    //
    // Initialize the device ID sandbag filtering.
    //
    // The Pmu.gpuDevId must be initialized by @ref pmuPreInitChipInfo_HAL()
    // and that is happening as a part of @ref constructPmu()
    //
    status = sandbagInit();
    if (status != FLCN_OK)
    {
        goto s_pmuEnginesConstruct_exit;
    }

#if PMUCFG_ENGINE_ENABLED(LSF)
    {
        status = constructLsf();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(LSF)

#if PMUCFG_FEATURE_ENABLED(PMU_GC6_BSI_BOOTSTRAP)
    // Pre-initialize GC6 as early as possible after OJBPMU construction.
    status = pmuPreInitGc6Exit();
    if (status != FLCN_OK)
    {
        goto s_pmuEnginesConstruct_exit;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_GC6_BSI_BOOTSTRAP)
#if PMUCFG_ENGINE_ENABLED(FB)
    {
        status = constructFb();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(FB)
#if PMUCFG_ENGINE_ENABLED(IC)
    {
        status = constructIc();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(IC)
#if PMUCFG_ENGINE_ENABLED(DISP)
    {
        status = constructDisp();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(DISP)
#if PMUCFG_ENGINE_ENABLED(TIMER)
    {
        status = constructTimer();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(TIMER)
#if PMUCFG_ENGINE_ENABLED(FIFO)
    {
        status = constructFifo();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(FIFO)
#if PMUCFG_ENGINE_ENABLED(GR)
    {
        status = constructGr();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(GR)
#if PMUCFG_ENGINE_ENABLED(MS)
    {
        status = constructMs();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(MS)
#if PMUCFG_ENGINE_ENABLED(LPWR)
    {
        status = constructLpwr();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(LPWR)
#if PMUCFG_ENGINE_ENABLED(PG)
    {
        status = constructPg();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(PG)
#if PMUCFG_ENGINE_ENABLED(AP)
    {
        status = constructAp();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(AP)
#if PMUCFG_ENGINE_ENABLED(PSI)
    {
        status = constructPsi();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(PSI)
#if PMUCFG_ENGINE_ENABLED(EI)
    {
        status = constructEi();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(EI)
#if PMUCFG_ENGINE_ENABLED(DFPR)
    {
        status = constructDfpr();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(DFPR)
#if PMUCFG_ENGINE_ENABLED(CLK)
    {
        status = constructClk();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(CLK)
#if PMUCFG_ENGINE_ENABLED(THERM)
    {
        status = constructTherm();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(THERM)
#if PMUCFG_ENGINE_ENABLED(PMGR)
    {
        status = constructPmgr();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(PMGR)
#if PMUCFG_ENGINE_ENABLED(I2C)
    {
        status = constructI2c();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(I2C)
#if PMUCFG_ENGINE_ENABLED(PERF)
    {
        status = constructPerf();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(PERF)
#if OSTASK_ENABLED(PERF_DAEMON)
    {
        status = constructPerfDaemon();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // OSTASK_ENABLED(PERF_DAEMON)
#if PMUCFG_ENGINE_ENABLED(GCX)
    {
        status = constructGcx();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(GCX)
#if PMUCFG_ENGINE_ENABLED(SMBPBI)
    {
        status = constructSmbpbi();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(SMBPBI)
#if PMUCFG_ENGINE_ENABLED(FUSE)
    {
        status = constructFuse();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(FUSE)
#if OSTASK_ENABLED(SPI)
    {
        status = constructSpi();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // OSTASK_ENABLED(SPI)
#if PMUCFG_ENGINE_ENABLED(VOLT)
    {
        status = constructVolt();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(VOLT)
#if PMUCFG_ENGINE_ENABLED(BIF)
    {
        status = constructBif();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(BIF)
#if PMUCFG_ENGINE_ENABLED(PERF_CF)
    {
        status = constructPerfCf();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(PERF_CF)
#if PMUCFG_ENGINE_ENABLED(NNE)
    {
        status = constructNne();
        if (status != FLCN_OK)
        {
            goto s_pmuEnginesConstruct_exit;
        }
    }
#endif // PMUCFG_ENGINE_ENABLED(NNE)
#if PMUCFG_ENGINE_ENABLED(VBIOS)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            constructVbios(),
            s_pmuEnginesConstruct_exit);
    }
#endif // PMUCFG_ENGINE_ENABLED(VBIOS)

s_pmuEnginesConstruct_exit:
    return status;
}

static void
s_pmuEnginesPreInit(void)
{
#if PMUCFG_ENGINE_ENABLED(IC)
    {
        icPreInit_HAL(&Ic);
    }
#endif // PMUCFG_ENGINE_ENABLED(IC)
#if PMUCFG_ENGINE_ENABLED(LPWR)
    {
        lpwrPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(LPWR)
#if PMUCFG_ENGINE_ENABLED(PG)
    {
        pgPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(PG)
#if PMUCFG_ENGINE_ENABLED(DI)
    {
        diPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(DI)
#if PMUCFG_ENGINE_ENABLED(GCX)
    {
        gcxPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(GCX)
#if PMUCFG_ENGINE_ENABLED(FIFO)
    {
        fifoPreInit_HAL(&Fifo);
    }
#endif // PMUCFG_ENGINE_ENABLED(FIFO)
#if OSTASK_ENABLED(ACR)
    acrPreInit_HAL();
#endif // OSTASK_ENABLED(ACR)

    // Initialize PRIV_MASKS for raising security
#if PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC_RAISE_PRIV_LEVEL)
    {
        pmuRaisePrivSecLevelForRegisters_HAL(&Pmu,
            FLD_TEST_DRF(_RM_PMU, _CMD_LINE_ARGS_FLAGS, _RAISE_PRIV_SEC, _YES,
                         PmuInitArgs.flags));
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC_RAISE_PRIV_LEVEL)

    // Initialize PRIV_MASKS of queue registers.
#if PMUCFG_FEATURE_ENABLED(PMU_QUEUE_REGISTERS_PROTECT)
    {
        // Raise PRIV_MASKS for TAIL register of command queues.
        pmuQueueCmdProtect_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_HPQ);
        pmuQueueCmdProtect_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_LPQ);

        // If FBFLCN supported, priv protect write of HEAD and TAIL registers.
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED))
        {
            pmuQueueCmdProtect_HAL(&Pmu, PMU_CMD_QUEUE_IDX_FBFLCN);
        }

        // Raise PRIV_MASKS for HEAD register of message queue.
        pmuQueueMsgProtect_HAL();
    }
#endif

    // Initialize GC6 exit sequence.
    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_BSI_BOOTSTRAP) &&
        (!DMA_ADDR_IS_ZERO(&PmuInitArgs.gc6Ctx)))
    {
        pmuInitGc6Exit_HAL(&Pmu);
    }

    // Pre-initialize CLK engine.
#if PMUCFG_ENGINE_ENABLED(CLK)
    {
        clkPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(CLK)
// Pre-initialize THERM engine.
#if PMUCFG_ENGINE_ENABLED(THERM) && OSTASK_ENABLED(THERM)
    {
        thermPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(THERM) && OSTASK_ENABLED(THERM)
// Pre-initialize PMGR engine.
#if PMUCFG_ENGINE_ENABLED(PMGR)
    {
        pmgrPreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(PMGR)

#if PMUCFG_ENGINE_ENABLED(NNE)
    {
        nnePreInit();
    }
#endif // PMUCFG_ENGINE_ENABLED(NNE)
}

static FLCN_STATUS
s_pmuTasksCreate(void)
{
    FLCN_STATUS status;

    status = lwosTaskSyncInitialize();
    if (status != FLCN_OK)
    {
        goto s_pmuTasksCreate_exit;
    }

    status = idlePreInitTask();
    if (status != FLCN_OK)
    {
        goto s_pmuTasksCreate_exit;
    }

    //
    // Create command management/dispatcher task. This task is NOT optional.
    // It must be alive to forward commands to all tasks that are running.
    //
#if OSTASK_ENABLED(CMDMGMT)
    {
        status = cmdmgmtPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(CMDMGMT)
#if OSTASK_ENABLED(PERF)
    {
        status = perfPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PERF)
#if OSTASK_ENABLED(PERF_DAEMON)
    {
        status = perfDaemonPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PERF_DAEMON)
#if OSTASK_ENABLED(PCMEVT)
    {
        status = pcmPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PCMEVT)
#if OSTASK_ENABLED(LPWR)
    {
        status = lpwrPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(LPWR)
#if OSTASK_ENABLED(LPWR_LP)
    {
        status = lpwrLpPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(LPWR_LP)
#if OSTASK_ENABLED(SEQ)
    {
        status = seqPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(SEQ)
#if OSTASK_ENABLED(I2C)
    {
        status = i2cPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(I2C)
#if OSTASK_ENABLED(GCX)
    {
        status = gcxPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(GCX)
#if OSTASK_ENABLED(PMGR)
    {
        status = pmgrPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PMGR)
#if OSTASK_ENABLED(PERFMON)
    {
        status = perfmonPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PERFMON)

#if OSTASK_ENABLED(THERM)
    {
        status = thermPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(THERM)
#if OSTASK_ENABLED(HDCP)
    {
        status = hdcpPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(HDCP)
#if OSTASK_ENABLED(ACR)
    {
        status = acrPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(ACR)

#if OSTASK_ENABLED(SPI)
    {
        status = spiPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(SPI)
#if OSTASK_ENABLED(BIF)
    {
        status = bifPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(BIF)
#if OSTASK_ENABLED(PERF_CF)
    {
        status = perfCfPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(PERF_CF)
#if OSTASK_ENABLED(WATCHDOG)
    {
        status = watchdogPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(WATCHDOG)
#if OSTASK_ENABLED(LOWLATENCY)
    {
        status = lowlatencyPreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(LOWLATENCY)
#if OSTASK_ENABLED(NNE)
    {
        status = nnePreInitTask();
        if (status != FLCN_OK)
        {
            goto s_pmuTasksCreate_exit;
        }
    }
#endif // OSTASK_ENABLED(NNE)
    status = osCmdmgmtPreInit();
    if (status != FLCN_OK)
    {
        goto s_pmuTasksCreate_exit;
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_RISCV_COMPILATION_HACKS))
    // Set the mask used to identify the timer interrupt for OS ticks
    lwrtosFlcnSetTickIntrMask(pmuGetOsTickIntrMask_HAL());
#endif
    //
    // Interrupts may have been enabled above (safe since interrupts have
    // been masked). Be sure to disable interrupts before unmasking the
    // interrupt sources. This will NOT have a matching lwrtosEXIT_CRITICAL
    // call. Interrupts will be enabled when the scheduler starts and
    // restores the context of the first task it picks to run. Until the
    // scheduler starts, only lwrtosENTER_CRITICAL or lwrtosEXIT_CRITICAL
    // (or any critical section wrapper except for appTaskCriticalEnter/Exit)
    // may be used; otherwise, interrupts will be enabled again.
    //
    lwrtosENTER_CRITICAL();

    // Initialize all the various PMU engines.

#if PMUCFG_ENGINE_ENABLED(DISP)
    {
        dispPreInit();

        //
        // We don't need to create DISP task when display engine is not present
        // (displayless GPUs) so that delay creating task and queue here
        //
#if OSTASK_ENABLED(DISP)
        if (DISP_IS_PRESENT())
        {
            status = dispPreInitTask();
            if (status != FLCN_OK)
            {
                goto s_pmuTasksCreate_exit;
            }
        }
#endif // OSTASK_ENABLED(DISP)
    }
#endif // PMUCFG_ENGINE_ENABLED(DISP)
s_pmuTasksCreate_exit:
    return status;
}
