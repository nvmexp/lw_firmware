/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_chnmgmt.c
 * @brief  Channel management task that is responsible for managing the
 *         work coming for channels from host method and context switch
 *         interfaces
 */
/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objic.h"
#include "sec2_chnmgmt.h"
#include "main.h"
#include "sec2_hostif.h"
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#include "hdcpmc/hdcpmc_mthds.h"
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
#include "sec2_objapm.h"
#endif
#include "class/cl95a1.h"

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * The following structure, which is initialized with known values, is saved
 * off at the beginning of the channel context  save area. It is used to
 * distinguish the very first context restore request which should be treated
 * as void, from any other context restore request. If this pattern was stored
 * in the context save area, we assume that there was a context save that
 * happened previously, else, we assume that this is the very first context
 * restore, and do nothing.
 *
 * We have tried to make the dummy structure large enough to avoid a "chance"
 * event that would accidently show up the same sequence of bytes in the
 * context restore area.
 */
typedef struct
{
    LwU32 fill1;
    LwU32 fill2;
    LwU32 fill3;
    LwU32 fill4;
} SEC2_CTX_DUMMY_PATTERN;

/*!
 * All the app-specific state belonging to the chnmgmt task that we need to
 * save/restore
 */
typedef struct
{
    /*!
    * Lwrrently (or last) active application ID if the EXECUTE method was received.
    */
    LwU32 appId;

    /*!
    * Lwrrently (or last) active application's watchdog timer value (in cycles).
    */
    LwU32 wdTmrVal;
} SEC2_CTX_CHNMGMT;

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * The following defines are used to initialize a dummy pattern in the channel
 * context to know that a context save has happened there (to distinguish the
 * very first context restore request which should be treated as void, from any
 * other context restore request)
 */
#define ENGINE_CTX_DUMMY_PATTERN_FILL1 (0xF1F1F1F1)
#define ENGINE_CTX_DUMMY_PATTERN_FILL2 (0x2E2E2E2E)
#define ENGINE_CTX_DUMMY_PATTERN_FILL3 (0xD3D3D3D3)
#define ENGINE_CTX_DUMMY_PATTERN_FILL4 (0x4C4C4C4C)

/*!
 * From dev_ram.h, _MODE oclwpies bit 2 of PTR_LO.
 */
#define ENGINE_CTX_STATE_MODE_MASK  (0x4)

/*!
 * From dev_ram.h, _TARGET oclwpies bits 0 and 1 of PTR_LO.
 */
#define ENGINE_CTX_STATE_TARGET_MASK (0x3)

/*!
 * From dev_ram.h, ignore bits 0, 1 and 2 of PTR_LO for address
 */
#define ENGINE_CTX_STATE_PTR_LO_MASK (0xFFFFFFF8)

/*!
 * A colwenience macro to mask off all but the lowest 6 bits of app method IDs
 * received from the method FIFO. The assumption is that each app will have
 * upto 64 methods, and all app methods will share the same array space. The
 * masked method ID is used to index into the app method  array directly. One
 * exception is for HDCP methods which uses more than 64 non-contiguous methods
 * and hence, we use a workaround to make sure that the 65th non-contiguous
 * method maps to the 65th contiguous location in the app method array.
 */
#define APP_MTHD_MASK (0x3F)

/*!
 * Invalid application ID. It is used for initialization of a member of our ctx
 * struct, but is not used in code, so it doesn't matter what this value is.
 */
#define APPLICATION_ID_ILWALID (0)

/*!
 * Invalid watchdog timer value. Indicates to us that we shouldn't program it.
 */
#define WATCHDOG_TIMER_VALUE_ILWALID (0)

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Initialize the dummy patern for the ctx save area with known values. For
 * more details, refer @SEC2_CTX_DUMMY_PATTERN
 */
static SEC2_CTX_DUMMY_PATTERN sec2CtxDummyPattern
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT) =
{
    ENGINE_CTX_DUMMY_PATTERN_FILL1,
    ENGINE_CTX_DUMMY_PATTERN_FILL2,
    ENGINE_CTX_DUMMY_PATTERN_FILL3,
    ENGINE_CTX_DUMMY_PATTERN_FILL4,
};

/*!
 * A copy of the dummy pattern structure used temporarily during ctx restore to
 * check if the dummy pattern exists in the ctx save area. Fore more details,
 * refer @SEC2_CTX_DUMMY_PATTERN
 */
static SEC2_CTX_DUMMY_PATTERN sec2CtxDummyCheck
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * Array to store semaphore method data
 */
static SEC2_SEMAPHORE_MTHD_ARRAY SemaphoreMthdArray
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * All state belonging to the chnmgmt task that we need to save/restore
 */
static SEC2_CTX_CHNMGMT chnmgmtCtx
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT) =
{
    APPLICATION_ID_ILWALID,       // Just for initialization; not used in code
    WATCHDOG_TIMER_VALUE_ILWALID,
};

/*!
 * Global state pointing to whether a channel app is lwrrently active or not
 */
static LwBool chnAppActive;

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array to store semaphore method data (shared with individual app tasks,
 * which are the consumers of the array)
 */
LwU32 appMthdArray[APP_METHOD_ARRAY_SIZE]
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * RC Recovery state of task_chnmgmt
 *
 */
LwBool g_isRcRecoveryInProgress = LW_FALSE;

/*!
 * Mask that indicates which app methods are lwrrently valid. This mask is also
 * shared with individual app tasks. Chnmgmt task will set the valid masks when
 * it sees methods coming in, and individual app tasks will clear the mask when
 * it handles methods.
 */
SEC2_APP_METHOD_VALID_MASK appMthdValidMask
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/* ------------------------- Static Function Prototypes  -------------------- */
static void _processCtxSwIntr(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_processCtxSwIntr");

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
static FLCN_STATUS _saveRestoreSelwreChannelIvInfo(LwU32 ctx, LwBool bSave)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_saveRestoreSelwreChannelIvInfo");
#endif

static FLCN_STATUS _saveRestoreCtx(LwU32 ctx, LwBool bSave)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_saveRestoreCtx");

static void _processMethodIntr(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_processMethodIntr");

static LwBool _processMethods(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_processMethods");

static LwBool _releaseSemaphore(LwBool bLongReport, LwBool bFlushDisable)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_releaseSemaphore");

static LwBool _processOSMethod(LwU16 mthdId, LwU32 mthdData)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_processOSMethod");

static FLCN_STATUS _getEngStateDescFromInstBlock(LwU32 ctx, RM_FLCN_MEM_DESC *pMemDesc)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_getEngStateDescFromInstBlock");

static void _setAppMthdValid(LwU32 appMthdIndex)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_setAppMthdValid");

static void _triggerApp(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_triggerApp");

static void _RCRecoverySendUnblockEvent(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_RCRecoverySendUnblockEvent");

static void _chnmgmtProcessCmd(DISPATCH_CHNMGMT *pChnmgmtEvt)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_chnmgmtProcessCmd");

static void _processWdTmr(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_processWdTmr");

/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(task_chnmgmt, pvParameters)
{
    DISPATCH_CHNMGMT    dispatch;

    // Initialize static data for semaphore method array
    SemaphoreMthdArray.semDesc.params =
        FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                        RM_SEC2_DMAIDX_VIRT,
                        SemaphoreMthdArray.semDesc.params);

    //
    // When we start off, we have nothing pending on the host interface, so
    // tell host that we're idle using the SW override. When there are any
    // methods in our FIFO, or a ctxsw pending, HW will automatically switch us
    // to busy. Programming the override to idle also atomically switches SEC2
    // to using the new idle signal, instead of the old HW-generated idle
    // signal.
    //
    sec2HostIdleProgramIdle_HAL();

    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(Sec2ChnMgmtCmdDispQueue, &dispatch))
        {
            switch (dispatch.eventType)
            {
                case DISP2CHNMGMT_EVT_CTXSW:
                    _processCtxSwIntr();
                    break;

                case DISP2CHNMGMT_EVT_METHOD:
                    _processMethodIntr();
                    break;

                case DISP2CHNMGMT_EVT_COMPLETION:
                    chnAppActive = LW_FALSE;

                    //
                    // First disable the watchdog timer, and ilwalidate the
                    // timer value
                    //
                    sec2WdTmrDisable_HAL();
                    chnmgmtCtx.wdTmrVal = WATCHDOG_TIMER_VALUE_ILWALID;

                    if (dispatch.completionEvent.classErrorCode == LW95A1_ERROR_NONE)
                    {
                        LwBool bUnmaskIntr = LW_TRUE;

                        //
                        // Successful completion command, so release semaphore
                        // if required.
                        //
                        if (SemaphoreMthdArray.bNotifyOnEnd)
                        {
                            if (!_releaseSemaphore(LW_FALSE, LW_FALSE))
                            {
                                //
                                // Releasing the semaphore can fail and get a
                                // NACK, so Notify RM to start RC recovery and
                                // keep host interface interrupts masked
                                //
                                _RCRecoverySendUnblockEvent();
                                bUnmaskIntr = LW_FALSE;
                            }
                        }

                        //
                        // Also, we may have more methods pending in our FIFO
                        // since we stopped popping methods as soon as we saw
                        // the execute, so go back and double-check our method
                        // FIFO.
                        //
                        if (!_processMethods())
                        {
                            bUnmaskIntr = LW_FALSE;
                        }

                        // Unmask the host interface interrupts now
                        if (bUnmaskIntr)
                        {
                            sec2HostIdleProgramIdle_HAL();
                            icHostIfIntrUnmask_HAL();
                        }
                    }
                    else if (dispatch.completionEvent.classErrorCode == LW95A1_ERROR_DMA_NACK)
                    {
                        // Notify RM to start RC recovery
                        _RCRecoverySendUnblockEvent();
                    }
                    else
                    {
                        //
                        // Notify RM to trigger a RC recovery event
                        //
                        RCRecoverySendTriggerEvent(dispatch.completionEvent.classErrorCode);
                    }
                    break;

                case DISP2CHNMGMT_EVT_CMD:
                    _chnmgmtProcessCmd(&dispatch);
                    break;

                case DISP2CHNMGMT_EVT_WDTMR:
                    _processWdTmr();
                    break;

                default:
                    //
                    // Only internally generated events can bring us here, so
                    // putting a halt here will enable us to catch bugs, but cannot
                    // be used to cause a DoS attack from CPU.
                    //
                    SEC2_HALT();
                    break;
            }
        }
    }
}

/*!
 * @brief Signal the chnmgmt task that a method or ctxsw has completed.
 *
 * This function should not be called directly, it should be ilwoked using
 * the macros defined in sec2_chnmgmt.h
 *
 * @param [in] completionCode   Indicates status of chnmgmt task.
 *                              This error should be handled by chnmgmt.
 */
void
notifyExelwteCompleted
(
    LwU32 classErrorCode
)
{
    DISP2CHNMGMT_COMPLETION completion;
    completion.eventType        = DISP2CHNMGMT_EVT_COMPLETION;
    completion.classErrorCode   = classErrorCode;

    // Clear all the app method valid bits
    memset(appMthdValidMask.mthdGrp, 0x0, sizeof(appMthdValidMask.mthdGrp));
    lwrtosQueueSendBlocking(Sec2ChnMgmtCmdDispQueue, &completion,
                            sizeof(completion));
}

/*
 * @brief Send trivial ack for the pending CTXSW Intr request
 *
 */
void
sec2SendCtxSwIntrTrivialAck
(
    void
)
{
    // Ack that request
    if (sec2IsLwrrentChannelCtxValid_HAL(&Sec2, NULL))
    {
        sec2AckCtxSave_HAL();
    }
    else if (sec2IsNextChannelCtxValid_HAL(&Sec2, NULL))
    {
        sec2AckCtxRestore_HAL();
    }
}

/* ------------------------ Static Functions ------------------------------- */
/*!
 * Process the context switch interrupt
 */
static void
_processCtxSwIntr(void)
{
    LwBool           bLwrCtxValid, bNextCtxValid;
    LwU32            lwrCtx, nxtCtx;
    FLCN_STATUS      status = FLCN_OK;
 
    //
    // Special handling for hopper and later since CTXSW interrupt 
    // is unmask while RC Recovery in in progress.
    if (g_isRcRecoveryInProgress == LW_TRUE)
    {
        sec2SendCtxSwIntrTrivialAck();
        //
        // In new RC recovery steps Ctxsw intr is unmask hence make sure that 
        // only this intr is unmask and not the method interrupt.
        //
        icHostIfIntrCtxswUnmask_HAL();
    }
    else
    {
        bLwrCtxValid  = sec2IsLwrrentChannelCtxValid_HAL(&Sec2, &lwrCtx);
        bNextCtxValid = sec2IsNextChannelCtxValid_HAL(&Sec2, &nxtCtx);

        if (!bLwrCtxValid && !bNextCtxValid)
        {
            //
            // Neither current nor next ctx is valid
            // Ack the restore immediately
            //
            sec2AckCtxRestore_HAL();
        }
        else if (bLwrCtxValid)
        {
            // save current ctx
            DBG_PRINTF_CHNMGMT(("Lwrr ctx valid\n", 0, 0, 0, 0));
            status = _saveRestoreCtx(lwrCtx, LW_TRUE);
        }
        else if (!bLwrCtxValid && bNextCtxValid)
        {
            // restore next ctx
            DBG_PRINTF_CHNMGMT(("Next ctx valid\n", 0, 0, 0, 0));
            status = _saveRestoreCtx(nxtCtx, LW_FALSE);
        }
        else
        {
            //
            // We should not get here as long as host behaves properly.
            // TODO-ssapre: We can probably remove this once code development is
            // complete
            //
            SEC2_BREAKPOINT();
        }

        //
        // Nothing to do for SW-controlled idle. We will report busy until we ack
        // the ctxsw request, so that is automatically the correct behavior.
        //
        if (status == FLCN_OK)
        {
            // unmask mthd and ctxsw interrupts (they were masked in ISR)
            icHostIfIntrUnmask_HAL();
        }
    }
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
/*!
 * Save/restore additional APM secure channel context.
 * 
 * @param[in] ctx    Entire contents of current/previous ctx register
 * @param[in] bSave  LW_TRUE  => Save current context
 *                   LW_FALSE => Restore next context
 */
static FLCN_STATUS
_saveRestoreSelwreChannelIvInfo
(
    LwU32  ctx,
    LwBool bSave
)
{
    if (!bSave)
    {
        //
        // Set all current context to invalid before loading new one.
        // This has side effect of resetting the decryption IV block counter,
        // as we only (re)store the IV base in the secure channel context.
        //
        memset((LwU8 *)&g_selwreIvCtx, 0, sizeof(g_selwreIvCtx));
    }

    return sec2SaveRestoreSelwreChannelIvInfo_HAL(&Sec2, bSave,
                                                  &g_selwreIvCtx.encIvSlotId,
                                                  g_selwreIvCtx.decIv,
                                                  APM_IV_BASE_SIZE_IN_BYTES,
                                                  ctx);
}
#endif

/*!
 * Save/restore channel context
 *
 * @param[in] ctx    Entire contents of current/previous ctx register
 * @param[in] bSave  LW_TRUE  => Save current context
 *                   LW_FALSE => Restore next context
 */
static FLCN_STATUS
_saveRestoreCtx
(
    LwU32  ctx,
    LwBool bSave
)
{
    RM_FLCN_MEM_DESC memDesc;
    LwU32            offset  = 0; // offset into dmaAddr in memDesc
    LwU32            lwrSize = 0; // size of current DMA
    FLCN_STATUS      status = FLCN_OK;

    // Initialize before filling up its bit fields
    memDesc.params = 0;

    if (_getEngStateDescFromInstBlock(ctx, &memDesc) != FLCN_OK)
    {
        // Shouldn't get here as long as pointer to memDesc is valid
        SEC2_BREAKPOINT();
    }

    //
    // 0. If save, save off the dummy pattern.
    //    If restore, check if the dummy pattern already exists. If not, we
    //    interpret this as the very first context restore and treat it as
    //    void, and simply ACK the request
    //
    lwrSize = sizeof(SEC2_CTX_DUMMY_PATTERN);
    if (bSave)
    {
        status = dmaWrite(&sec2CtxDummyPattern, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        //
        // Restore the dummy pattern from the context save area and check if it
        // matches our expectation. If it does, continue, else bail out early.
        //
        status = dmaRead(&sec2CtxDummyCheck, &memDesc, offset, lwrSize);

        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
        if (memcmp((void *)&sec2CtxDummyCheck, (void *)&sec2CtxDummyPattern,
                   sizeof(SEC2_CTX_DUMMY_PATTERN)) != 0)
        {
            // Bail out early, however, we still may need to read IV.
            goto _saveRestoreCtx_Iv;
        }
    }

    // 1. Save/restore semaphore method array
    offset += lwrSize;
    lwrSize = sizeof(SemaphoreMthdArray);
    if (bSave)
    {
        status = dmaWrite(&SemaphoreMthdArray, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&SemaphoreMthdArray, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

    // 2. Save/restore app method array
    offset += lwrSize;
    lwrSize = sizeof(appMthdArray);
    if (bSave)
    {
        status = dmaWrite(appMthdArray, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(appMthdArray, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

    // Now increment offset by the size of our last DMA
    offset += lwrSize;
    lwrSize = sizeof(chnmgmtCtx);
    // 3. Save/restore chnmgmt task context
    if (bSave)
    {
        status = dmaWrite(&chnmgmtCtx, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&chnmgmtCtx, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

    // Now increment offset by the size of our last DMA
    offset += lwrSize;
    lwrSize = sizeof(appMthdValidMask);
    // 4. Save/restore app method valid mask
    if (bSave)
    {
        status = dmaWrite(&appMthdValidMask, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&appMthdValidMask, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

_saveRestoreCtx_Iv:
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    //
    // 5. Save/restore APM Secure Channel IV info.
    // Since we only expect to support scenarios using secure channels on APM,
    // we will fail hard here if this fails (i.e. if channel was not secure).
    //
    status = _saveRestoreSelwreChannelIvInfo(ctx, bSave);

    //
    // APM-TODO Bug 3411739: Ignore return value for PID2, as causing
    // intermittent failures. Needs to be root-caused before PID3.
    //
    status = FLCN_OK;
    if (status != FLCN_OK)
    {
        goto _saveRestoreCtx_ack;
    }
#endif

_saveRestoreCtx_ack:
    if (status == FLCN_OK)
    {
        // Ack the context save/restore request
        if (bSave)
        {
#if (SEC2CFG_FEATURE_ENABLED(SEC2_NOTIFICATION_INTR_GFID_PROGRAMMING))
            //
            // On a ctx save, clear the last bound GFID in the intr controller
            // for notification interrupts
            //
            icSetNonstallIntrCtrlGfid_HAL(&Ic, 0);
#endif
            sec2AckCtxSave_HAL();
        }
        else
        {
#if (SEC2CFG_FEATURE_ENABLED(SEC2_NOTIFICATION_INTR_GFID_PROGRAMMING))
            //
            // On a ctx restore, set the next GFID in the intr controller for
            // notification interrupts. The NXTCTX/NXTCTX2 will be copied to
            // LWRCTX/LWRCTX2 after we ack the restore
            //
            icSetNonstallIntrCtrlGfid_HAL(&Ic, sec2GetNextCtxGfid_HAL());
#endif
            sec2AckCtxRestore_HAL();
        }
    }
    else if (status == FLCN_ERR_DMA_NACK)
    {
        _RCRecoverySendUnblockEvent();
    }
    else
    {
       RCRecoverySendTriggerEvent(LW95A1_OS_ERROR_APPLICATION);
    }
    return status;
}

/*!
 * Get the memory descriptor of the engine's context state from the instance
 * block
 *
 * @param[in]  ctx      Address of the instance block obtained from falcon's
 *                      ctx ptr.
 * @param[out] pMemDesc Pointer to the memory descriptor that will be used to
 *                      save/restore context. We will fill up with address and
 *                      dmaIdx
 *
 * @return     FLCN_OK    => successfully filled up the memory descriptor
 *             FLCN_ERROR => invalid pointer to memory descriptor passed in
 */
static FLCN_STATUS
_getEngStateDescFromInstBlock
(
    LwU32             ctx,
    RM_FLCN_MEM_DESC *pMemDesc
)
{
    LwU64   engStateAddr;
    LwU8    target;
    LwBool  bPhysical;

    if (pMemDesc == NULL)
    {
        return FLCN_ERROR;
    }

    //
    // Obtain the block of memory from the instance block that holds the
    // address of the memory where our state lives. It also contains the target
    // memory where it lives.
    //
    engStateAddr = sec2GetEngStateAddr_HAL(&Sec2, ctx);

    //
    // First find the target memory from the address since host stores it
    // in the lowest 3 bits of the address
    //
    target    = (LwU8)(engStateAddr & ENGINE_CTX_STATE_TARGET_MASK);
    bPhysical = (engStateAddr & ENGINE_CTX_STATE_MODE_MASK) ?
                LW_FALSE : LW_TRUE;

    // Pack the address into the memory descriptor
    RM_FLCN_U64_PACK(&(pMemDesc->address), &engStateAddr);

    // Now mask off the bits used to find the target
    pMemDesc->address.lo &= ENGINE_CTX_STATE_PTR_LO_MASK;

    pMemDesc->params =
        FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                        (bPhysical ?
                         sec2GetPhysDmaIdx_HAL(&Sec2, target) :
                         RM_SEC2_DMAIDX_VIRT),
                        pMemDesc->params);

    return FLCN_OK;
}

/*!
 * Set the method valid flag
 * @param[in]  appMthdIndex    index of the method to set valid
 */
static void
_setAppMthdValid
(
    LwU32 appMthdIndex
)
{
    if (appMthdIndex < APP_METHOD_ARRAY_SIZE)
    {
        appMthdValidMask.mthdGrp[appMthdIndex / 32] |= BIT(appMthdIndex % 32);
    }
    else
    {
        //
        // We don't expect to get methods whose ID is greater than the size we
        // support.
        //
        SEC2_HALT();
    }
}

/*!
 * Process methods
 *
 * @return LW_FALSE  => Not ready to handle more work from the host interface
 *                      since an execute method was received or we page faulted
 *         LW_TRUE   => Ready to handle more work from the host interface
 */
static LwBool
_processMethods(void)
{
    LwU32  mthdData;
    LwU16  mthdId;
    LwU32  appMthdIndex; // used to index into the app method array
    LwBool bReady = LW_TRUE; // ready to handle more work from host interface

    while (sec2PopMthd_HAL(&Sec2, &mthdId, &mthdData))
    {
        DBG_PRINTF_CHNMGMT(("id = 0x%x data = 0x%x\n",
                            mthdId, mthdData,0,0));

        if ((mthdId < APP_METHOD_MIN) || (mthdId > APP_METHOD_MAX))
        {
            // Handle Host internal methods first
            if (sec2ProcessHostInternalMethods_HAL(&Sec2, mthdId, mthdData) == FLCN_OK)
            {
                // Skip this method if handled above
                continue;
            }
            // OS methods
            if (!_processOSMethod(mthdId, mthdData))
            {
                //
                // If an EXECUTE method is received while processing this batch
                // of methods, we would have triggered an application task, and
                // hence, we should not unmask the interrupts here, but instead,
                // unmask them when we get the completion cmd from it. We
                // should also continue to report busy to host using the SW
                // override since we're going to handle the app methods it sent
                // to us. Similarly, if processing the OS method resulted in us
                // getting a DMA NACK, follow the same procedure. We will
                // resume once RC recovery is complete.
                //
                bReady = LW_FALSE;

                //
                // Break out and don't process any more methods. The only two
                // ways to get here is if we received an EXECUTE method or if
                // we faulted while processing an OS method. In the former
                // case, we're intentionally delaying the processing of more
                // methods after execute has been handled. This is because KMD
                // often inserts semaphore methods at the end of the app method
                // stream (after EXECUTE), and expects that releasing the
                // semaphore indicates that the methods have been handled. In
                // the latter case, if we've faulted, we will empty the method
                // fifo as part of RC recovery anyway.
                //
                break;
            }
        }
        else
        {
            // app methods

            //
            // Mask off the lowest 6 bits in all cases to get the app method
            // array index, but make an exception for HDCP, which uses more
            // than 64 methods, where the first 64 are contiguous but the 65th
            // method onwards leaves a hole in between for a different app.
            // Make sure the 65th non-contiguous HDCP method maps to the 65th
            // contiguous location in the array. This is a dirty workaround and
            // in order to get rid of it, we have to either fork the class file
            // or make changes in the existing file, and change all clients and
            // ucode using it. Neither is possible right now, so we will live
            // with the workaround.
            //
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
            if (mthdId >= SEC2_METHOD_TO_METHOD_ID(LW95A1_HDCP_VALIDATE_SRM) &&
                mthdId <= SEC2_METHOD_TO_METHOD_ID(LW95A1_HDCP_ENCRYPT))
            {
                appMthdIndex = mthdId - HDCP_MTHD_OFFSET_WAR;
            }
            else
#endif
            {
                appMthdIndex = mthdId & APP_MTHD_MASK;
            }

            appMthdArray[appMthdIndex] = mthdData;
            _setAppMthdValid(appMthdIndex);
        }
    }
    return bReady;
}

/*!
 * Process the method interrupt
 */
static void
_processMethodIntr(void)
{
    if (sec2TriggerRCRecoveryMthdRcvdForIlwalidCtx_HAL())
    {
        return;
    }

    if (sec2IsMthdFifoEmpty_HAL())
    {
        //
        // No methods to process (they should have been processed as part of
        // processing an earlier method interrupt). Unmask mthd and ctxsw
        // interrupts (they were masked in ISR) and just return
        //
        icHostIfIntrUnmask_HAL();
        return;
    }

    //
    // Before we start popping methods out of our FIFO, assert that we're busy.
    // We will de-assert the busy signal once we're done, unless we got an
    // execute method. If we got an execute method, we will continue asserting
    // busy until the task handling the methods sends us the completion signal.
    //
    sec2HostIdleProgramBusy_HAL();

    if (_processMethods())
    {
        //
        // Indicate to host that we're idle and ready to receive more work from
        // it.
        //
        sec2HostIdleProgramIdle_HAL();

        // unmask mthd and ctxsw interrupts (they were masked in ISR)
        icHostIfIntrUnmask_HAL();
    }
}

/*!
 * Process an OS method and return a boolean that the caller can use to decide
 * whether to unmask host interface interrupts
 *
 * @param[in]  mthdId    ID of the method being processed from method FIFO
 * @param[in]  mthdData  Data received for the method
 *
 * @return LW_FALSE  => An EXECUTE method was received or a DMA NACK was
 *                      received, so the caller should not unmask the host
 *                      interface interrupts now. We will either unmask them
 *                      once the application task sends us the completion
 *                      command, or once RC recovery is complete
 *
 *         LW_TRUE   => The caller is free to unmask host interface interrupts
 *                      when all the work in the method FIFO has been handled
 */
static LwBool
_processOSMethod
(
    LwU16 mthdId,
    LwU32 mthdData
)
{
    LwBool bUnmaskIntr = LW_TRUE;

    switch (mthdId)
    {
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_PM_TRIGGER):
        {
            sec2SetPMTrigger_HAL(&Sec2,
                                 LW_TRUE); // bStart
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_A):
        {
            DBG_PRINTF_CHNMGMT(("SEM A dmaAddrHi = 0x%x\n", mthdData,0,0,0));
            SemaphoreMthdArray.semDesc.address.hi = mthdData;
            SemaphoreMthdArray.inputBitMask |= BIT(SEC2_SEMAPHORE_A_RECEIVED);
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_B):
        {
            DBG_PRINTF_CHNMGMT(("SEM B dmaAddrLo = 0x%x\n", mthdData,0,0,0));
            SemaphoreMthdArray.semDesc.address.lo = mthdData;
            SemaphoreMthdArray.inputBitMask |= BIT(SEC2_SEMAPHORE_B_RECEIVED);
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_C):
        {
            DBG_PRINTF_CHNMGMT(("SEM C payload = 0x%x\n", mthdData,0,0,0));
            SemaphoreMthdArray.longPayload.payload = mthdData;
            SemaphoreMthdArray.inputBitMask |=
                BIT(SEC2_SEMAPHORE_PAYLOAD_RECEIVED);
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_D):
        {
            DBG_PRINTF_CHNMGMT(("SEM D mthdData = 0x%x\n", mthdData,0,0,0));
            if (FLD_TEST_DRF(95A1, _SEMAPHORE_D,
                             _OPERATION, _RELEASE, mthdData))
            {
                LwBool bLongReport = FLD_TEST_DRF(95A1,
                                                  _SEMAPHORE_D,
                                                  _STRUCTURE_SIZE,
                                                  _FOUR,
                                                  mthdData);
                LwBool bFlushDisable = FLD_TEST_DRF(95A1,
                                                    _SEMAPHORE_D,
                                                    _FLUSH_DISABLE,
                                                    _TRUE,
                                                    mthdData);
                if (!_releaseSemaphore(bLongReport, bFlushDisable))
                {
                    // We received a NACK while releasing the semaphore
                    bUnmaskIntr = LW_FALSE;
                    _RCRecoverySendUnblockEvent();
                }
            }
            else if (FLD_TEST_DRF(95A1, _SEMAPHORE_D, _OPERATION, _TRAP,
                                  mthdData))
            {
                //
                // Assumes that semaphore release has been done previously.
                // Only send a non-stall interrupt.
                //
                icSemaphoreDTrapIntrTrigger_HAL();
            }
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_PM_TRIGGER_END):
        {
            sec2SetPMTrigger_HAL(&Sec2,
                                 LW_FALSE); // bStart
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SET_APPLICATION_ID):
        {
            chnmgmtCtx.appId =
                DRF_VAL(95A1, _SET_APPLICATION_ID, _ID, mthdData);
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_SET_WATCHDOG_TIMER):
        {
            chnmgmtCtx.wdTmrVal =
                DRF_VAL(95A1, _SET_WATCHDOG_TIMER, _TIMER, mthdData);
            break;
        }
        case SEC2_METHOD_TO_METHOD_ID(LW95A1_EXELWTE):
        {
            LwBool bTriggerApp = LW_TRUE;

            //
            // We received an EXECUTE method, so unmask host interface
            // interrupts only on receiving the completion command
            //
            bUnmaskIntr = LW_FALSE;

            // Clear potentially stale state
            SemaphoreMthdArray.bNotifyOnEnd = LW_FALSE;

            if (FLD_TEST_DRF(95A1, _EXELWTE, _NOTIFY, _ENABLE, mthdData))
            {
                // Check if EXECUTE asks us to notify

                // If so, does it ask us to notify on end?
                SemaphoreMthdArray.bNotifyOnEnd = FLD_TEST_DRF(95A1,
                                                               _EXELWTE,
                                                               _NOTIFY_ON,
                                                               _END,
                                                               mthdData);

                if (!SemaphoreMthdArray.bNotifyOnEnd)
                {
                    // We're asked to notify now.
                    if (!_releaseSemaphore(LW_FALSE, LW_FALSE))
                    {
                        // Don't trigger the app and start RC recovery
                        bTriggerApp = LW_FALSE;
                        _RCRecoverySendUnblockEvent();
                    }
                }
            }

            // Trigger the appropriate application
            if (bTriggerApp)
            {
                _triggerApp();
            }
            break;
        }
        default:
        {
            // Do nothing
            break;
        }
    }
    return bUnmaskIntr;
}

/*!
 * Release a semaphore by writing the payload.
 *
 * @param[in]  bLongReport  LW_TRUE =>  Release semaphore with long report type
 *                                      4 words of information -
 *                                      1 for payload,
 *                                      1 is reserved, and
 *                                      2 words for ptimer timestamp
 *
 *                          LW_FALSE => Release semaphore with short report
 *                                      type - 1 word of information for
 *                                      payload
 *
 * @return LW_TRUE  => Successful release of semaphore
 *         LW_FALSE => Unsuccessful release of semaphore because we received
 *                     a NACK
 */
static LwBool
_releaseSemaphore
(
    LwBool bLongReport,
    LwBool bFlushDisable
)
{
    LwU32      sizeBytesPayload;
    LwU32      sizeBytesTime;
    LwBool     bSuccess = LW_TRUE;
    FLCN_STATUS status;

    DBG_PRINTF_CHNMGMT(("Release Sem\n", 0, 0, 0, 0));

    if ((SemaphoreMthdArray.inputBitMask & SEC2_SEMAPHORE_INPUT_VALID) ==
        SEC2_SEMAPHORE_INPUT_VALID)
    {
        if (!bLongReport)
        {
            sizeBytesPayload = sizeof(SemaphoreMthdArray.longPayload.payload);
            SemaphoreMthdArray.semDesc.params =
                FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                sizeBytesPayload, SemaphoreMthdArray.semDesc.params);

            status = dmaWrite(&(SemaphoreMthdArray.longPayload.payload),
                              &(SemaphoreMthdArray.semDesc), 0, sizeBytesPayload);
            if (status == FLCN_ERR_DMA_NACK)
            {
                bSuccess = LW_FALSE;
            }
        }
        else
        {
            //
            // Break 128b semaphores into timestamp and release data
            // Bug 3348784
            //
            sizeBytesTime = sizeof(SemaphoreMthdArray.longPayload.time);

            // We need the size of the payload for the dma offset for timestamp
            sizeBytesPayload = sizeof(SemaphoreMthdArray.longPayload.payload) +
                               sizeof(SemaphoreMthdArray.longPayload.reserved);

            // Get the ptimer timestamp
            osPTimerTimeNsLwrrentGet(&(SemaphoreMthdArray.longPayload.time));
            SemaphoreMthdArray.semDesc.params =
                FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                sizeBytesTime, SemaphoreMthdArray.semDesc.params);

            status = dmaWrite(&(SemaphoreMthdArray.longPayload.time),
                              &(SemaphoreMthdArray.semDesc), sizeBytesPayload, sizeBytesTime);
            if (status == FLCN_ERR_DMA_NACK)
            {
                bSuccess = LW_FALSE;
            }

            SemaphoreMthdArray.semDesc.params =
                FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                 sizeBytesPayload, SemaphoreMthdArray.semDesc.params);

            status = dmaWrite(&(SemaphoreMthdArray.longPayload.payload),
                              &(SemaphoreMthdArray.semDesc), 0, sizeBytesPayload);
            if (status == FLCN_ERR_DMA_NACK)
            {
                bSuccess = LW_FALSE;
            }
        }
        if (bSuccess && !bFlushDisable)
        {
            // 
            // We suppress the error from sec2FbifFlush today. There isn't much
            // action we can take if flush times out since that is a system
            // wide failure. For now, just march on. We don't want to halt
            // since halting is bad for security and safety.
            //
            (void)sec2FbifFlush_HAL();
        }
    }

    //
    // Clear the input valid mask after releasing the semaphore instead of
    // clearing the entire semaphore method array
    //
    SemaphoreMthdArray.inputBitMask = 0x0;

    return bSuccess;
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
extern LwrtosTaskHandle OsTaskHdcpmc;
#endif

/*!
 * Trigger a host interface application
 */
static void
_triggerApp(void)
{
    LwrtosQueueHandle pQueue = NULL;

    switch (chnmgmtCtx.appId)
    {
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
        case LW95A1_SET_APPLICATION_ID_ID_HDCP:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
            {
                pQueue = HdcpmcQueue;
            }
            break;
        }
#endif

        case LW95A1_SET_APPLICATION_ID_ID_HWV:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
            {
                pQueue = HwvQueue;
            }
            break;
        }

        case LW95A1_SET_APPLICATION_ID_ID_GFE:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_GFE))
            {
                pQueue = GfeQueue;
            }
            break;
        }
        case LW95A1_SET_APPLICATION_ID_ID_LWSR:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_LWSR))
            {
                pQueue = LwsrQueue;
            }
            break;
        }
        case LW95A1_SET_APPLICATION_ID_ID_PR:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_PR))
            {
                pQueue = PrQueue;
            }
            break;
        }
        case LW95A1_SET_APPLICATION_ID_ID_APM:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
            {
                pQueue = ApmQueue;
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (pQueue != NULL)
    {
        DISP2APP_EVT_CMD dispatch;
        dispatch.eventType = DISP2APP_EVT_METHOD;
        lwrtosQueueSendBlocking(pQueue, &dispatch, sizeof(dispatch));
    }
    else
    {
        //
        // We don't have a task to handle the given application ID. Pretend as
        // if all the work was done.
        //
        NOTIFY_EXELWTE_COMPLETE();
    }

    //
    // Set the state that an application is active. This state will be unset
    // when we process the completion event.
    //
    chnAppActive = LW_TRUE;

    //
    // Now that we've triggered the task, program the watchdog timer if a timer
    // value was specified by the client
    //
    if (chnmgmtCtx.wdTmrVal != WATCHDOG_TIMER_VALUE_ILWALID)
    {
        sec2WdTmrEnable_HAL(&Sec2, chnmgmtCtx.wdTmrVal);
    }
}

/*!
 * Send an event to RM to trigger RC recovery and notify ourselves that the
 * task faulted so that we can do our own recover process.
 */
void
RCRecoverySendTriggerEvent(LwU32 classErrorCode)
{
    RM_FLCN_QUEUE_HDR   hdr;
    RM_SEC2_CHNMGMT_MSG msg;

    hdr.unitId    = RM_SEC2_UNIT_CHNMGMT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SEC2_MSG_SIZE(CHNMGMT, TRIGGER_RC_RECOVERY);

    msg.triggerRC.msgType        = RM_SEC2_MSG_TYPE(CHNMGMT, TRIGGER_RC_RECOVERY);
    msg.triggerRC.classErrorCode = classErrorCode;

    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);

    //
    // Disable the watchdog timer, and ilwalidate the timer value.
    // If RCRecoverySendTriggerEvent is called from the task entry 
    // point,  this has already been done but it won't hurt to do it 
    // again.
    //
    chnAppActive = LW_FALSE;
    sec2WdTmrDisable_HAL();
    chnmgmtCtx.wdTmrVal = WATCHDOG_TIMER_VALUE_ILWALID;

    //
    // Internally notify ourselves that the task failed so we can do our own
    // recovery.
    //
    _RCRecoverySendUnblockEvent();
}

/*!
 * Send an event to RM to unblock RC recovery
 */
static void
_RCRecoverySendUnblockEvent(void)
{
    RM_FLCN_QUEUE_HDR   hdr;
    RM_SEC2_CHNMGMT_MSG msg;

    hdr.unitId    = RM_SEC2_UNIT_CHNMGMT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SEC2_MSG_SIZE(CHNMGMT, UNBLOCK_RC_RECOVERY);

    msg.unblockRC.msgType = RM_SEC2_MSG_TYPE(CHNMGMT, UNBLOCK_RC_RECOVERY);

    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
}

/*!
 * Process commands from RM
 */
void _chnmgmtProcessCmd
(
    DISPATCH_CHNMGMT *pChnmgmtEvt
)
{
    LwU16 mthdId;
    LwU32 mthdData;
    RM_FLCN_QUEUE_HDR hdr;
    RM_SEC2_CHNMGMT_MSG msg;

    switch (pChnmgmtEvt->cmd.pCmd->cmd.chnmgmt.cmdType)
    {
        case RM_SEC2_CMD_TYPE(CHNMGMT, ENGINE_RC_RECOVERY):
        {
            // a) First pop all methods
            while (sec2PopMthd_HAL(&Sec2, &mthdId, &mthdData))
            {
                // NOP
            }

            sec2ProcessEngineRcRecoveryCmd_HAL();

            // Respond that we're done
            hdr.unitId    = RM_SEC2_UNIT_CHNMGMT;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pChnmgmtEvt->cmd.pCmd->hdr.seqNumId;
            hdr.size      = RM_SEC2_MSG_SIZE(CHNMGMT, ENGINE_RC_RECOVERY);
            msg.engineRC.msgType = RM_SEC2_MSG_TYPE(CHNMGMT, ENGINE_RC_RECOVERY);
            osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
            break;
        }
        case RM_SEC2_CMD_TYPE(CHNMGMT, FINISH_RC_RECOVERY):
        {
            // Make HostIf Interrupt Ctxsw Unmask to false
            g_isRcRecoveryInProgress = LW_FALSE;

            //
            // Unset the NACK bit unconditionally. It could have been set when
            // we saw a ctxsw request pending during RC error recovery. We
            // should only unest it now that RM has finished resetting the host
            // side and host has deasserted the ctxsw request. If it wasn't set
            // before, unsetting it now causes no harm.
            //
            sec2SetHostAckMode_HAL(&Sec2, LW_FALSE);

            // Unmask host interface interrupts
            icHostIfIntrUnmask_HAL();

            // Respond that we're done
            hdr.unitId    = RM_SEC2_UNIT_CHNMGMT;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pChnmgmtEvt->cmd.pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
            osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
            break;
        }
    }
    //
    // always sweep the commands in the queue, regardless of the validity of
    // the command.
    //
    osCmdmgmtCmdQSweep(&(pChnmgmtEvt->cmd.pCmd->hdr), pChnmgmtEvt->cmd.cmdQueueId);
}

/*!
 * Process the watchdog timer interrupt
 */
static void
_processWdTmr(void)
{
    LwrtosTaskHandle taskHandle = NULL;

    // Ilwalidate the watchdog timer duration
    chnmgmtCtx.wdTmrVal = WATCHDOG_TIMER_VALUE_ILWALID;

    if (!chnAppActive)
    {
        //
        // We already received the completion notifier from the app, so we can
        // just ignore the wdtmr interrupt
        //
        return;
    }

    switch (chnmgmtCtx.appId)
    {
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
        case LW95A1_SET_APPLICATION_ID_ID_HDCP:
        {
            extern LwrtosTaskHandle OsTaskHdcpmc;
            taskHandle = OsTaskHdcpmc;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
        case LW95A1_SET_APPLICATION_ID_ID_HWV:
        {
            extern LwrtosTaskHandle OsTaskHwv;
            taskHandle = OsTaskHwv;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_GFE))
        case LW95A1_SET_APPLICATION_ID_ID_GFE:
        {
            extern LwrtosTaskHandle OsTaskGfe;
            taskHandle = OsTaskGfe;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_VPR))
        case LW95A1_SET_APPLICATION_ID_ID_VPR:
        {
            extern LwrtosTaskHandle OsTaskVpr;
            taskHandle = OsTaskVpr;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_LWSR))
        case LW95A1_SET_APPLICATION_ID_ID_LWSR:
        {
            extern LwrtosTaskHandle OsTaskLwsr;
            taskHandle = OsTaskLwsr;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_PR))
        case LW95A1_SET_APPLICATION_ID_ID_PR:
        {
            extern LwrtosTaskHandle OsTaskPr;
            taskHandle = OsTaskPr;
            break;
        }
#endif
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
        case LW95A1_SET_APPLICATION_ID_ID_APM:
        {
            extern LwrtosTaskHandle OsTaskApm;
            taskHandle = OsTaskApm;
            break;
        }
#endif
        default:
        {
            // Do nothing
            return;
        }
    }

    //
    // Restart the task and then internally notify ourselves that the task has
    // finished. To trigger a RC error recovery on that channel, inform
    // ourselves that the task failed. That way, we have only one codepath that
    // resumes work from the host interface and thus, is much cleaner and
    // easier to maintain.
    //
    lwrtosTaskRestart(taskHandle);
    RCRecoverySendTriggerEvent(LW95A1_OS_ERROR_WDTIMER);
}
