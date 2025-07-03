/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file main.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "rmsec2cmdif.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2sw.h"
#include "main.h"
#include "sec2_cmdmgmt.h"
#include "sec2_chnmgmt.h"
#include "lwsr/sec2_lwsr.h"
#include "sec2_objsec2.h"
#include "sec2_objic.h"
#include "sec2_objlsf.h"
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#include "sec2_objhdcpmc.h"
#endif
#include "unit_dispatch.h"
#include "sec2_objgfe.h"
#include "sec2_objhwv.h"
#include "sec2_objrttimer.h"
#include "sec2_objpr.h"
#include "sec2_objvpr.h"
#include "sec2_objacr.h"
#include "sec2_objapm.h"
#include "sec2_objspdm.h"
#include "config/g_hal_register.h"
#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Handles for all tasks spawned by the SEC2 application.  Each is named using
 * a common-prefix for colwenient symbol look-up.
 */
LwrtosTaskHandle OsTaskCmdMgmt       = NULL;
LwrtosTaskHandle OsTaskChnMgmt       = NULL;
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_WKRTHD))
LwrtosTaskHandle *pOsTaskWkrThd      = NULL; // Array of handles for worker threads
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
LwrtosTaskHandle OsTaskHdcpmc        = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_GFE))
LwrtosTaskHandle OsTaskGfe           = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_LWSR))
LwrtosTaskHandle OsTaskLwsr          = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
LwrtosTaskHandle OsTaskHwv           = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_PR))
LwrtosTaskHandle OsTaskPr            = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_VPR))
LwrtosTaskHandle OsTaskVpr           = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_ACR))
LwrtosTaskHandle OsTaskAcr           = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
LwrtosTaskHandle OsTaskApm           = NULL;
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_SPDM))
LwrtosTaskHandle OsTaskSpdm          = NULL;
#endif

/*!
 * A dispatch queue for sending commands to the command management task
 */
LwrtosQueueHandle  Sec2CmdMgmtCmdDispQueue;

/*!
 * A dispatch queue for sending commands to the channel management task
 */
LwrtosQueueHandle  Sec2ChnMgmtCmdDispQueue;

/*!
 * stack canary for SSP
 */
void * __stack_chk_guard;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 0)) | ((dmaIdx) << 0))
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 8)) | ((dmaIdx) << 8))
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~((LwU32)0x7U << 12)) | ((dmaIdx) << 12))

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Implementation --------------------------------- */

/*!
 * We have two flags to enable HS ucode encryption (one is a compile time flag
 * used everywhere, and one is a flag sent to the linker script generator).
 * If the flag to generate the linker script symbols is set, then so should the
 * compile-time flag that enables all the code that uses the symbols, else it
 * results in resident data waste. Catch that case here to remind engineers that
 * either both must be set or both must be not set.
 * Setting just HS_UCODE_ENCRYPTION without HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
 * is not going to work anyway. Compilation/link will fail if we don't generate
 * the symbols required for it.
 */
#if HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
#ifndef HS_UCODE_ENCRYPTION
ct_assert(0);
#endif
#endif

ct_assert(NUM_SIG_PER_UCODE == NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL);

/*!
 * Main entry-point for the RTOS application.
 *
 * @param[in]  argc    Number of command-line arguments (always 1)
 * @param[in]  ppArgv  Pointer to the command-line argument structure
 *
 * @return zero upon success; non-zero negative number upon failure
 */
GCC_ATTRIB_NO_STACK_PROTECT()
int
main
(
    int    argc,
    char **ppArgv
)
{
    LwU8    imemOverlaysLs[3];
    LwU8    numImemOverlaysLs = 0;
    LwU32   blk;
    LwU32   imemBlks = 0;

    // Read the total number of IMEM blocks
    imemBlks = REG_RD32(CSB, LW_CSEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CSEC_FALCON, _HWCFG, _IMEM_SIZE, imemBlks);

    // Ilwalidate bootloader. It may not be at the default block
    falc_imtag(&blk, SEC2_LOADER_OFFSET);
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

#ifdef BOOT_FROM_HS_BUILD
    // Program the IMB/IMB1/DMB/DMB1 registers.
    if (sec2ProgramCodeDataRegistersFromCommonScratch_HAL() != FLCN_OK)
    {
        falc_halt();
    }
#endif // BOOT_FROM_HS_BUILD

    // Set the CTX (needs to be set for IMREAD,DMREAD, and DMWRITE
    falc_wspr(CTX, DMA_SET_IMREAD_CTX(0U,  RM_SEC2_DMAIDX_UCODE ) |
                   DMA_SET_DMREAD_CTX(0U,  RM_SEC2_DMAIDX_UCODE ) |
                   DMA_SET_DMWRITE_CTX(0U, RM_SEC2_DMAIDX_UCODE ));

    imemOverlaysLs[numImemOverlaysLs++] = OVL_INDEX_IMEM(init);

    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        imemOverlaysLs[numImemOverlaysLs++] = OVL_INDEX_IMEM(rttimer);
    }

#ifdef IS_SSP_ENABLED
    imemOverlaysLs[numImemOverlaysLs++] = OVL_INDEX_IMEM(libSE);
#endif // IS_SSP_ENABLED

#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
    LwU8    imemOverlaysHs[1];
    LwU8    numImemOverlaysHs = 0;
    imemOverlaysHs[numImemOverlaysHs++] = OVL_INDEX_IMEM(initHs);
    OSTASK_LOAD_IMEM_OVERLAYS_HS(imemOverlaysHs, numImemOverlaysHs);
#endif

    OSTASK_LOAD_IMEM_OVERLAYS_LS(imemOverlaysLs, numImemOverlaysLs);

    // Load the resident HS overlays
#ifdef HS_OVERLAYS_ENABLED
    lwosOvlImemLoadAllResidentHS();
#endif


#ifdef IS_SSP_ENABLED

    //
    // Since HW does not enforce NONCE mode on emulation and neither does the SE lib enforce
    // this in SW, TRNG random seeding is used on pre-silicon resulting in hard hang.
    // As suggested by HW pre-silicon should enforce NONCE mode. Thus adding a WAR to enable
    // PTIMER0 based random canary on pre-silicon. However we use this as an additional basic canary even for
    // silicon before we move to SE based canary below since BAR0 TMOUT is not set here and cannot do a
    // BAR0 PTOP register to check for silicon here.
    // TODO: Once HW fixes this in emulator we can remove this WAR
    //
    __stack_chk_guard = (void *)REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
#endif // IS_SSP_ENABLED

    InitSec2App();
    return 0;
}

/*!
 * Initializes the SEC2 RTOS application.  This function is responsible for
 * creating all tasks, queues, mutexes, and semaphores.
 */
void
InitSec2App(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    //
    // Disable all Falcon interrupt sources.  This must be done in case
    // interrupts (on IV0 or IV1) are enabled between now and when the
    // scheduler is enabled.  This is avoid scenarios where the OS timer tick
    // fires on IV1 before the scheduler is started or a GPIO interrupt fires
    // on IV0 before the IV0 interrupt handler is installed.
    //
    REG_WR32_STALL(CSB, LW_CSEC_FALCON_IRQMCLR, 0xFFFFFFFF);

    //
    // Unmask the halt interrupt to host early to detect an init failure before
    // icPreInit_HAL is exelwted
    //
    icHaltIntrUnmask_HAL();

    // Initialize the LWOS specific data.
    status = lwosInit();
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }

    //
    // Init HW mutex as early as possible to make sure HW mutex is ready at the
    // earliest point.
    //
#if (SEC2CFG_FEATURE_ENABLED(SEC2_USE_MUTEX))
    sec2MutexEstablishMapping_HAL();
#endif

    sec2EnableEmemAperture_HAL();

    //
    // Then finish our preinit sequence before other engines are constructed or
    // initialized.
    //
    sec2PreInit_HAL();

#ifdef IS_SSP_ENABLED
    // Setup LS SSP canary
    if (FLCN_OK != (status = sec2InitializeStackCanaryLS()))
    {
        goto InitSec2App_exit;
    }
#endif //IS_SSP_ENABLED
 
    // Check if binary is running on correct chip - LS
    if (FLCN_OK != (status = sec2CheckIfChipIsSupportedLS_HAL()))
    {
        goto InitSec2App_exit;
    }


    // LS revocation by checking for LS ucode version against SEC2 rev in HW fuse
    if (FLCN_OK != (status = sec2CheckForLSRevocation()))
    {
        goto InitSec2App_exit;
    }

    //
    // In case RUNTIME_HS_OVL_SIG_PATCHING is enabled(only possible for PKC based signing), HS ovls can be used ONLY after this function
    // This function DMA's the HS and HS LIB OVL signatures from sec2's Data subwpr into the DMEM
    //
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
    status = sec2DmaHsOvlSigBlobFromWpr_HAL();
    if (status != FLCN_OK)
    {
        goto InitSec2App_exit;
    }
#endif // RUNTIME_HS_OVL_SIG_PATCHING

#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
    // Do any initialization that must be done in HS mode
    if (FLCN_OK != (status = InitSec2HS()))
    {
        goto InitSec2App_exit;
    }
#endif

    //
    // initialize falc-debug to write print data to corresponding register
    // falc_debug tool to decode print logs from the sec2 falcon spew is
    // at //sw/dev/gpu_drv/chips_a/uproc/sec2/tools/flcndebug/falc_debug
    //
    if (SEC2CFG_FEATURE_ENABLED(DBG_PRINTF_FALCDEBUG))
    {
        sec2FalcDebugInit_HAL();
    }

    DBG_PRINTF(("InitSec2App\n", 0, 0, 0, 0));

    //
    // Create command mgmt/dispatcher task.  This task is NOT optional.  It
    // must be alive to forward commands to all tasks that are running.
    //
    OSTASK_CREATE(status, SEC2, CMDMGMT);
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }

    status = lwrtosQueueCreateOvlRes(&Sec2CmdMgmtCmdDispQueue, 4,
                                     sizeof(DISPATCH_CMDMGMT));
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_CHNMGMT))
    //
    // Create channel mgmt task.  This task is NOT optional.  It must be alive
    // to handle all work coming in from host interface.
    // We need a queue size of at least 3 because in the worst case, we could
    // have either one of ctxsw/mthd events, wdtmr event and completion event
    // in the chnmgmt task queue. In addition, we will leave buffer for an
    // additional event since the above size of 3 assumes some HW and SW
    // behavior, which may change in the future.
    //
    OSTASK_CREATE(status, SEC2, CHNMGMT);
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }

    status = lwrtosQueueCreateOvlRes(&Sec2ChnMgmtCmdDispQueue, 4,
                                     sizeof(DISPATCH_CHNMGMT));
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }
#endif

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_WKRTHD)) && (OSTASK_ATTRIBUTE_WKRTHD_COUNT > 0)
    // Allocate memory for the worker thread handles
    pOsTaskWkrThd = lwosMallocResidentType(OSTASK_ATTRIBUTE_WKRTHD_COUNT,
                                           LwrtosTaskHandle);

    // Create worker threads
    OSWKRTHD_CREATE(status, SEC2, OSTASK_ATTRIBUTE_WKRTHD_COUNT);
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }

    status = lwrtosQueueCreateOvlRes(&Disp2QWkrThd, 4, sizeof(DISP2UNIT_CMD));
    if (FLCN_OK != status)
    {
        goto InitSec2App_exit;
    }
#endif

    //
    // Create Miracast HDCP task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
    {
        OSTASK_CREATE(status, SEC2, HDCPMC);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&HdcpmcQueue, 1,
                                         sizeof(DISPATCH_HDCPMC));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create GFE task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_GFE))
    {
        OSTASK_CREATE(status, SEC2, GFE);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&GfeQueue, 1, sizeof(DISPATCH_GFE));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create LWSR task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_LWSR))
    {
        OSTASK_CREATE(status, SEC2, LWSR);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&LwsrQueue, 1, sizeof(DISPATCH_LWSR));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create HWV task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
    {
        OSTASK_CREATE(status, SEC2, HWV);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&HwvQueue, 1, sizeof(DISPATCH_HWV));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create PLAYREADY task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_PR))
    {
        OSTASK_CREATE(status, SEC2, PR);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&PrQueue, 1, sizeof(DISPATCH_PR));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create VPR task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_VPR))
    {
        OSTASK_CREATE(status, SEC2, VPR);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&VprQueue, 1, sizeof(DISPATCH_VPR));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    // Setup ACR task only if SEC2 is in LSMODE
    if (lsfVerifyFalconSelwreRunMode_HAL())
    {
        //
        // Create ACR Task.
        //
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_ACR))
        {
            OSTASK_CREATE(status, SEC2, ACR);
            if (FLCN_OK != status)
            {
                goto InitSec2App_exit;
            }

            status = lwrtosQueueCreateOvlRes(&AcrQueue, 4, sizeof(DISPATCH_ACR));
            if (FLCN_OK != status)
            {
                goto InitSec2App_exit;
            }
        }
#endif
    }

    //
    // Create APM task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    {
        OSTASK_CREATE(status, SEC2, APM);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&ApmQueue, 1, sizeof(DISPATCH_APM));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }
    #endif

    //
    // Create SPDM task. This task is optional.
    //
    #if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_SPDM))
    {
        OSTASK_CREATE(status, SEC2, SPDM);
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }

        status = lwrtosQueueCreateOvlRes(&SpdmQueue, 1, sizeof(DISPATCH_SPDM));
        if (FLCN_OK != status)
        {
            goto InitSec2App_exit;
        }
    }

    //
    // Performs initialization for the data shared between SPDM and APM tasks.
    // This needs to run before APM and SPDM tasks run the first time.
    // DO NOT MOVE IT WITHOUT A GOOD REASON!
    //
    apm_spdm_shared_pre_init();

    #endif

#if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS) && \
     SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
    // Only enable GPTMR lib if GPTMR not used for OS ticks, and task needs it
    sec2GptmrLibPreInit();
#endif

    osCmdmgmtPreInit();

    // Set the mask used to identify the timer interrupt for OS ticks
    lwrtosFlcnSetTickIntrMask(icOsTickIntrMaskGet_HAL());

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

    if (SEC2CFG_ENGINE_ENABLED(IC))
    {
        icPreInit_HAL();
    }

    if (SEC2CFG_ENGINE_ENABLED(LSF))
    {
        lsfPreInit();
    }

    if (SEC2CFG_ENGINE_ENABLED(RTTIMER))
    {
        rttimerPreInit();
    }

    // start the scheduler's timer
    sec2StartOsTicksTimer();

InitSec2App_exit:

    if (FLCN_OK == status)
    {
        // Start the scheduler.
        status = lwrtosTaskSchedulerStart();
    }

    // Update Sec2's Mailbox0
    REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX0, status);

    if (FLCN_OK != status)
    {
        //
        // TODO: Find a way to communicate to the RM (even behind the message
        // queue's back) that the falcon has failed to initialize itself and
        // start the task scheduler.
        //
        SEC2_HALT();
    }
}
