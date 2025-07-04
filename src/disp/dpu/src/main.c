/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file main.c
 */

/* ------------------------- System Includes ------------------------------- */
#include "dpusw.h"
#include "lwoslayer.h"
#include "rmdpucmdif.h"
#include "dispflcn_regmacros.h"

#include "lwostimer.h"
#if (DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
#include "lwostmrcallback.h"
#endif

/* ------------------------- Application Includes -------------------------- */
#include "dpu_gpuarch.h"
#include "unit_dispatch.h"
#include "lwdpu.h"
#include "dpu_mgmt.h"
#include "dmemovl.h"

#include "dpu_objdpu.h"
#include "lib_intfchdcp_cmn.h"
#include "lib_intfchdcp22wired.h"
#include "lib_intfchdcp.h"
#include "dpu_objic.h"
#include "dpu_objlsf.h"
#include "dpu_objisohub.h"
#include "dpu_objvrr.h"
#include "dpu_scanoutlogging.h"
#include "dpu_mscgwithfrl.h"
#include "dpu_vrr.h"
#include "config/g_hal_register.h"
#ifdef BOOT_FROM_HS
#include "lwosselwreovly.h"
#endif

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- External Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
#ifdef IS_SSP_ENABLED
void *gPCanaryFromMainEntry=0;
#endif
/*!
 * Handles for all tasks spawned by the DPU application.  Each is named using
 * a common-prefix for colwenient symbol look-up.
 */
LwrtosTaskHandle  OsTaskCmdMgmt           = NULL;
LwrtosTaskHandle  OsTaskVrr               = NULL;
LwrtosTaskHandle  OsTaskHdcp              = NULL;
LwrtosTaskHandle  OsTaskHdcp22wired       = NULL;
LwrtosTaskHandle  OsTaskScanoutLoggging   = NULL;
LwrtosTaskHandle  OsTaskMscgWithFrl       = NULL;
LwrtosTaskHandle *pOsTaskWkrThd           = NULL; // Array of handles for worker threads

#if (!DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
OS_TIMER    Hdcp22WiredOsTimer;
#endif

/*!
 * Cached copy of the initialization arguments used to boot the DPU.
 */
RM_DPU_CMD_LINE_ARGS DpuInitArgs;

/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros and Defines ---------------------------- */
// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 0)) | ((dmaIdx) << 0))
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 8)) | ((dmaIdx) << 8))
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~((LwU32)0x7U << 12)) | ((dmaIdx) << 12))

/* ------------------------- Prototypes ------------------------------------- */
#ifdef IS_SSP_ENABLED
static FLCN_STATUS _dpuUpdateStackCanary(void)
    GCC_ATTRIB_NO_STACK_PROTECT()
    GCC_ATTRIB_SECTION("imem_resident", "_dpuUpdateStackCanary")
    GCC_ATTRIB_USED();

static void _dpuRestoreStackCanary(void)
    GCC_ATTRIB_NO_STACK_PROTECT()
    GCC_ATTRIB_SECTION("imem_resident", "_dpuRestoreStackCanary")
    GCC_ATTRIB_USED();
#endif
/* ------------------------- Static Functions ------------------------------ */
/* ------------------------- Implementation -------------------------------- */
#ifdef IS_SSP_ENABLED
/*
 *   @brief Update Stack canary to true random number generated by SE after entering LS.
 *
 *   return FLCN_STATUS return FLCN_OK in case of succesful updation of canary value
 */
static FLCN_STATUS
_dpuUpdateStackCanary(void)
{
    LwU32 num = 0;
    FLCN_STATUS status = FLCN_OK;

    // Save canary previously set in before main to restore before exiting LS code.
    gPCanaryFromMainEntry = __stack_chk_guard;

    // Generate random number from SE trng and assign it to canary
    status = dpuGetRandNumber_HAL(&Dpu.hal, &num, 1);
    if (status != FLCN_OK)
    {
        goto label_return;
    }

    // Update Stack canary value
    _dpuSetStackCanary((void *)num);

label_return:
    return status;
}

/*
 *   @brief Restore canary before returning to Previous code.
 */
static void
_dpuRestoreStackCanary(void)
{
    // Restore canary value
    _dpuSetStackCanary(gPCanaryFromMainEntry);
}
#endif

/*!
 * We have two flags to enable HS ucode encryption (one is a compile time flag
 * used everywhere, and one is a flag sent to the linker script generator).
 * If the flag to generate the linker script symbols is set, then so should the
 * compile-time flag that enables all the code that uses the symbols, else it
 * results in resident data waste. Catch that case here to remind engineers that
 * either both must be set or both must be not set.
 * Setting just HS_UCODE_ENCRYPTION without HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
 * is not going to work anyway. Compilation will fail if we don't generate the
 * symbols required for it.
 */
#if HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
#ifndef HS_UCODE_ENCRYPTION
ct_assert(0);
#endif
#endif

ct_assert(NUM_SIG_PER_UCODE == NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL);

//
// We see in code for even for GSP/GSPLite code we are using 
// RM_DPU_DMAIDX_UCODE instead of RM_GSP_DMAIDX_UCODE or
// RM_GSPLITE_DMAIDX_UCODE. Presently values are same so no problem
// But if values changes we should make to check all usuage and 
// modify accordingly
// 
ct_assert(RM_GSP_DMAIDX_UCODE == RM_DPU_DMAIDX_UCODE);
ct_assert(RM_GSPLITE_DMAIDX_UCODE == RM_DPU_DMAIDX_UCODE);

/*!
 * Main entry-point for the RTOS application.
 *
 * @param[in]  argc    Number of command-line arguments (always 1)
 * @param[in]  ppArgv  Pointer to the command-line argument structure (@ref
 *                     RM_DPU_CMD_LINE_ARGS).
 *
 * @return zero upon success; non-zero negative number upon failure
 */
int
main
(
    int    argc,
    char **ppArgv
)
{
    LwU8    imemOverlays[3];
    LwU8    numImemOverlays = 0;
    LwU32   blk;
    LwU32   imemBlks = 0;

    // Read the total number of IMEM blocks
    imemBlks = DFLCN_REG_RD_DRF(HWCFG, _IMEM_SIZE);

    // Ilwalidate bootloader. It may not be at the default block
    falc_imtag(&blk, DPU_LOADER_OFFSET);
    if (!IMTAG_IS_MISS(blk))
    {
        LwU32 tmp;
        blk = IMTAG_IDX_GET(blk);

        // Bootloader might take more than a block, so ilwalidate those as well.
        for (tmp = blk; tmp < imemBlks; tmp++)
        {
            falc_imilw(tmp);
        }
    }

#if BOOT_FROM_HS
    // 
    // During Boot From HS case, with HS BL entry, interrupt get disabled. 
    // Clear SEC here for command submission/response to work correctly.
    //
    falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));

    // Program the IMB/IMB1/DMB/DMB1 registers.
    if (dpuProgramImbDmbFromCommonScratch_HAL() != FLCN_OK)
    {
        DPU_HALT();
    }
#endif

    //
    // Set the CTX (needs to be set for IMREAD,DMREAD, and DMWRITE)
    // CTX DMA need to programmed by ucode with Boot From HS as 
    // BL does not does it anymore. 
    // For Non Boot From HS also, CTX DMA could be potentially changed by 
    // attacker, hence it is good to program CTX DMA again here before any 
    // overlay moment. 
    // 
    falc_wspr(CTX, DMA_SET_IMREAD_CTX(0U,  RM_DPU_DMAIDX_UCODE ) |
                   DMA_SET_DMREAD_CTX(0U,  RM_DPU_DMAIDX_UCODE ) |
                   DMA_SET_DMWRITE_CTX(0U, RM_DPU_DMAIDX_UCODE ));

    imemOverlays[numImemOverlays++] = OVL_INDEX_IMEM(init);
    if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    {
        imemOverlays[numImemOverlays++] = OVL_INDEX_IMEM(rttimer);
    }
#ifdef IS_SSP_ENABLED
    imemOverlays[numImemOverlays++] = OVL_INDEX_IMEM(libSE);
#endif

    OSTASK_LOAD_IMEM_OVERLAYS_LS(imemOverlays, numImemOverlays);

#ifdef IS_SSP_ENABLED
    // Set canary to random number generated by SE engine
    (void)_dpuUpdateStackCanary();
#endif

#if (DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackMapInit();
#endif

    if (FLCN_OK != InitDpuApp((RM_DPU_CMD_LINE_ARGS*)ppArgv))
    {
        // TBD: Clean up code to be added.
    }

#ifdef IS_SSP_ENABLED
    // Restore canary to Previous value before exiting.
    _dpuRestoreStackCanary();
#endif
    return 0;
}

/*!
 * Initializes the DPU RTOS application.  This function is responsible for
 * creating all tasks, queues, mutexes, and semaphores.
 *
 * @param[in]  pArgs  Pointer to the command-line argument structure.
 */
FLCN_STATUS
InitDpuApp
(
    RM_DPU_CMD_LINE_ARGS *pArgs
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 intrMask = 0;

    //
    // Disable all Falcon interrupt sources except those which are
    // supposed to be enabled by RM. Avoid disabling them in DPU ucode
    // This must be done in case interrupts (on IV0 or IV1) are enabled between
    // now and when the scheduler is enabled.
    // This is avoid scenarios where the OS timer tick fires on IV1 before
    // the scheduler is started or a GPIO interrupt fires
    // on IV0 before the IV0 interrupt handler is installed.
    //
    intrMask = ~(DFLCN_DRF(IRQMASK_ENABLED_BY_RM));
    DFLCN_REG_WR32_STALL(IRQMCLR, intrMask);

    // Initialize the LWOS specific data.
    status = lwosInit();
    if (FLCN_OK != status)
    {
        goto InitDpuApp_exit;
    }

    //
    // Enable all HAL modules.  Calling this function prepares each HAL module's
    // interface-setup functions. This must be done prior to calling the
    // constructor for each module.
    //
    REGISTER_ALL_HALS();

    //
    // Init HW mutex as early as possible to make sure HW mutex is ready at the
    // earliest point.
    //
    dpuMutexEstablishMapping_HAL();

#if EMEM_SUPPORTED
    // Enable EMEM perture
    dpuEnableEmemAperture_HAL();
#endif

    //
    // Once our EMEM aperture has been set up (on builds where EMEM is present)
    // save off a copy of the initialization arguments for ease of access.
    //
    DpuInitArgs = *pArgs;

    //
    // Now construct all objects and HAL modules. This will setup all of the
    // HAL function pointers. Note that not all engines are enabled by default
    // so checks must be made for some engines to determine if construction is
    // appropriate.
    //

    if (DPUCFG_ENGINE_ENABLED(DPU))
    {
        constructDpu();
        //
        // Initializing DPU engine earlier so that the IP versions get
        // initialized.
        //
        status = dpuInit_HAL(&Dpu.hal);
    }

    if (DPUCFG_ENGINE_ENABLED(IC))
    {
        constructIc();
    }

    if (DPUCFG_FEATURE_ENABLED(LIGHT_SELWRE_FALCON))
    {
        if (DPUCFG_ENGINE_ENABLED(LSF))
        {
            constructLsf();
        }
    }

    if (DPUCFG_ENGINE_ENABLED(ISOHUB))
    {
        constructIsohub();
    }

    if (DPUCFG_ENGINE_ENABLED(VRR))
    {
        constructVrr();
    }

    // initialize falc-trace if used for debug-spew
    if (DPUCFG_FEATURE_ENABLED(DBG_PRINTF_OS_TRACE_FB))
    {
        osTraceFbInit(&pArgs->osTraceFbBuff);
    }

    // initialize falc-debug to write print data to corresponding register
    if (DPUCFG_FEATURE_ENABLED(DBG_PRINTF_OS_TRACE_REG))
    {
        osTraceRegInit(DFLCN_DRF(SCRATCH1));
    }

    // the first printf should appear AFTER UART construction
    DBG_PRINTF(("InitDpuApp\n", 0, 0, 0, 0));

    //
    // Create command dispatcher task.  This task is NOT optional.  It must
    // be alive to forward commands to all tasks that are running.
    //
    {
        OSTASK_CREATE(status, DPU, DISPATCH);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&DpuMgmtCmdDispQueue, 4,
                                         sizeof(DISPATCH_CMDMGMT));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_WKRTHD))
    {
#if OSTASK_ATTRIBUTE_WKRTHD_COUNT > 0
        // Allocate memory for the worker thread handles
        pOsTaskWkrThd = lwosMallocResidentType(OSTASK_ATTRIBUTE_WKRTHD_COUNT,
                                               LwrtosTaskHandle);

        // Create worker threads
        OSWKRTHD_CREATE(status, DPU, OSTASK_ATTRIBUTE_WKRTHD_COUNT);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&Disp2QWkrThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
#endif
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_VRR))
    {
        OSTASK_CREATE(status, DPU, VRR);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&VrrQueue, 2, sizeof(DISPATCH_VRR));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_SCANOUTLOGGING))
    {
        OSTASK_CREATE(status, DPU, SCANOUTLOGGING);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&ScanoutLoggingQueue, 2,
                                         sizeof(DISPATCH_SCANOUTLOGGING));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_MSCGWITHFRL))
    {
        OSTASK_CREATE(status, DPU, MSCGWITHFRL);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&MscgWithFrlQueue, 2,
                                         sizeof(DISPATCH_MSCGWITHFRL));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    //
    // In case RUNTIME_HS_OVL_SIG_PATCHING is enabled(only possible for PKC based signing),
    // HS ovls can be used ONLY after this function
    // This function DMA's the HS and HS LIB OVL signatures from gsp's Data subwpr into the DMEM
    //
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
    dpuDmaHsOvlSigBlobFromWpr_HAL();
#endif // RUNTIME_HS_OVL_SIG_PATCHING

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP) || 
        DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED))
    {
        dpuHdcpCmnInit_HAL();
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP))
    {
        OSTASK_CREATE(status, DPU, HDCP);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }

        status = lwrtosQueueCreateOvlRes(&HdcpQueue, 2, sizeof(DISPATCH_HDCP));
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED))
    {
#if (DPU_PROFILE_v0205)
        if (DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_06))
#endif
        {
            OSTASK_CREATE(status, DPU, HDCP22WIRED);
            if (FLCN_OK != status)
            {
                goto InitDpuApp_exit;
            }

            status = lwrtosQueueCreateOvlRes(&Hdcp22WiredQueue, 2,
                                             sizeof(DISPATCH_HDCP22WIRED));
            if (FLCN_OK != status)
            {
                goto InitDpuApp_exit;
            }
#if (!DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
            osTimerInitTracked(OSTASK_TCB(HDCP22WIRED), &Hdcp22WiredOsTimer,
                                   HDCP22WIRED_OS_TIMER_ENTRY_NUM_ENTRIES);
#endif
        }
    }

    osCmdmgmtPreInit();

    // Set the mask used to identify the timer interrupt for OS ticks
    lwrtosFlcnSetTickIntrMask(dpuGetOsTickIntrMask_HAL());

#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    //
    // Set Timer0 register and written value to clear pending status at
    // irq1_interrupt.
    //
    lwrtosFlcnSetOsTickIntrClearRegAndValue(
        dpuGetOsTickTimer0IntrReg_HAL(),
        dpuGetOsTickTimer0IntrClearValue_HAL());
#endif

    //
    // Interrupts may have been enabled above (safe since interrupts have
    // been masked). Be sure to disable interrupts before unmasking the
    // interrupt sources. This will NOT have a matching lwrtosEXIT_CRITICAL
    // call. Interrupts will be enabled when the scheduler starts and
    // restores the context of the first task it picks to run. Until the
    // scheduler starts, only lwrtosENTER_CRITICAL or lwrtosEXIT_CRITICAL may be
    // used; otherwise, interrupts will be enabled again.
    //
    lwrtosENTER_CRITICAL();

    // Initialize Engines
    if (DPUCFG_ENGINE_ENABLED(DPU))
    {
        status = dpuInit_HAL(&Dpu.hal);
        if (FLCN_OK != status)
        {
            goto InitDpuApp_exit;
        }
    }

    if (DPUCFG_ENGINE_ENABLED(IC))
    {
        icInit_HAL(&IcHal);
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_MSCGWITHFRL))
    {
        mscgWithFrlInit();
    }

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_VRR))
    {
        vrrInit();
    }

    if (DPUCFG_FEATURE_ENABLED(LIGHT_SELWRE_FALCON))
    {
        if (DPUCFG_ENGINE_ENABLED(LSF))
        {
            status = lsfInit_HAL(&LsfHal);
            if (FLCN_OK != status)
            {
                // We halt in case of failure in LSF init
                goto InitDpuApp_exit;
            }
        }
    }

    // start the scheduler's timer
    status = dpuStartOsTicksTimer();

InitDpuApp_exit:

    if (FLCN_OK == status)
    {
        // Start the scheduler.
        status = lwrtosTaskSchedulerStart();
    }

    if (FLCN_OK != status)
    {
        //
        // TODO: Find a way to communicate to the RM (even behind the message
        // queue's back) that the falcon has failed to initialize itself and
        // start the task scheduler.
        //
        DPU_HALT();
    }

    return status;
}
