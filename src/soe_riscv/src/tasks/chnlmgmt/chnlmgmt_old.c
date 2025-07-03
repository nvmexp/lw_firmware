/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_chnlmgmt.c
 * @brief  Channel management task that is responsible for managing the
 *         work coming for channels from host method.
 */
/* ------------------------- System Includes -------------------------------- */

/* ------------------------- Application Includes --------------------------- */
#include "dev_sec_csb.h"
#include "engine.h"
#include "class/cl95a1.h"
#include "lwmisc.h"

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmsoecmdif.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <syslib/syslib.h>
#include <shlib/syscall.h>
#include <shlib/partswitch.h>
#include "partrpc.h"
#include "partswitch.h"


/* ------------------------ Module Includes -------------------------------- */
#include "config/g_sections_riscv.h"
#include "tasks.h"
#include "config/g_soe_hal.h"
#include "config/g_ic_hal.h"

#include "chnlmgmt.h"
#include "cmdmgmt.h"


extern LwBool msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody);

#define SOE_BREAKPOINT()            lwrtosHALT()

/*!
 * Invalid watchdog timer value. Indicates to us that we shouldn't program it.
 */
#define WATCHDOG_TIMER_VALUE_ILWALID (0)

/*!
 * Invalid application ID. It is used for initialization of a member of our ctx
 * struct, but is not used in code, so it doesn't matter what this value is.
 */
#define APPLICATION_ID_ILWALID (0)

/*!
 * Initialize the dummy patern for the ctx save area with known values. For
 * more details, refer @SOE_CTX_DUMMY_PATTERN
 */
SOE_CTX_DUMMY_PATTERN soeCtxDummyPattern
    GCC_ATTRIB_SECTION("dmem_", "soeCtxDummyPattern")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT) =
{
    ENGINE_CTX_DUMMY_PATTERN_FILL1,
    ENGINE_CTX_DUMMY_PATTERN_FILL2,
    ENGINE_CTX_DUMMY_PATTERN_FILL3,
    ENGINE_CTX_DUMMY_PATTERN_FILL4
};

/*!
 * A copy of the dummy pattern structure used temporarily during ctx restore to
 * check if the dummy pattern exists in the ctx save area. Fore more details,
 * refer @SOE_CTX_DUMMY_PATTERN
 */
SOE_CTX_DUMMY_PATTERN soeCtxDummyCheck
    GCC_ATTRIB_SECTION("dmem_", "soeCtxDummyCheck")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * Array to store semaphore method data
 */
SOE_SEMAPHORE_MTHD_ARRAY SemaphoreMthdArray
GCC_ATTRIB_SECTION("dmem_","SemaphoreMthdArray")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * Array to store semaphore method data (shared with individual app tasks,
 * which are the consumers of the array)
 */
LwU32 appMthdArray[APP_METHOD_ARRAY_SIZE]
GCC_ATTRIB_SECTION("dmem_", "appMthdArray")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * All state belonging to the chnmgmt task that we need to save/restore
 */
SOE_CTX_CHNMGMT chnmgmtCtx
    GCC_ATTRIB_SECTION("dmem_", "chnmgmtCtx")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT) =
{
    APPLICATION_ID_ILWALID,       // Just for initialization; not used in code
    WATCHDOG_TIMER_VALUE_ILWALID,
};

/*!
 * Mask that indicates which app methods are lwrrently valid. This mask is also
 * shared with individual app tasks. Chnmgmt task will set the valid masks when
 * it sees methods coming in, and individual app tasks will clear the mask when
 * it handles methods.
 */
SOE_APP_METHOD_VALID_MASK appMthdValidMask
    GCC_ATTRIB_SECTION("dmem_", "appMthdValidMask")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * RC Recovery state of task_chnmgmt
 *
 */
LwBool g_isRcRecoveryInProgress = LW_FALSE;

/*!
 * Global state pointing to whether a channel app is lwrrently active or not
 */
static LwBool chnAppActive;

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
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Obtain the block of memory from the instance block that holds the
    // address of the memory where our state lives. It also contains the target
    // memory where it lives.
    //
    engStateAddr = soeGetEngStateAddr_HAL(pSoe, ctx);

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
                         soeGetEngCtxPhysDmaIdx_HAL(pSoe, target) :
                         RM_SOE_DMAIDX_VIRT),
                         pMemDesc->params);

    return FLCN_OK;
}

/*
 * @brief Send trivial ack for the pending CTXSW Intr request
 *
 */
void
soeSendCtxSwIntrTrivialAck
(
    void
)
{
    // Ack that request
    if (soeIsLwrrentChannelCtxValid_HAL(&Soe, NULL))
    {
        soeAckCtxSave_HAL();
    }
    else if (soeIsNextChannelCtxValid_HAL(&Soe, NULL))
    {
        soeAckCtxRestore_HAL();
    }
}

/*!
 * @brief Signal the chnmgmt task that a method or ctxsw has completed.
 *
 * This function should not be called directly, it should be ilwoked using
 * the macros defined in soe_chnmgmt.h
 *
 * @param [in] bDmaNackReceived Indicates an error caused by a DMA NACK.
 *                              This error should be handled by chnmgmt.
 */
void
notifyExelwteCompleted
(
    LwBool bDmaNackReceived
)
{
    DISP2CHNMGMT_COMPLETION completion;
    completion.eventType        = DISP2CHNMGMT_EVT_COMPLETION;
    completion.bDmaNackReceived = bDmaNackReceived;

    // Clear all the app method valid bits
    lwrtosQueueSendBlocking(SoeChnMgmtCmdDispQueue, &completion, sizeof(completion));
}

/*!
 * Send an event to RM to unblock RC recovery
 */
static void _RCRecoverySendUnblockEvent(void)
{
    RM_FLCN_QUEUE_HDR   hdr;
    RM_FLCN_MSG_SOE    msg;

    hdr.unitId    = RM_SOE_UNIT_CHNMGMT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SOE_MSG_SIZE(CHNMGMT, UNBLOCK_RC_RECOVERY);

    msg.hdr = hdr;

    msg.msg.chnmgmt.unblockRC.msgType = RM_SOE_MSG_TYPE(CHNMGMT, UNBLOCK_RC_RECOVERY);
    dbgPrintf(LEVEL_ALWAYS, "_RCRecoverySendUnblockEvent \n");

    if (!msgQueuePostBlocking(&hdr, &msg))
    {
        dbgPrintf(LEVEL_ALWAYS, "_RCRecoverySendUnblockEvent Failed to send UNBLOCK_RC_RECOVERY\n");
    }
    //
    // Internally notify ourselves that the task failed so we can do our own
    // recovery.
    //
    NOTIFY_EXELWTE_NACK();
}

/*!
 * Send an event to RM to trigger RC recovery and notify ourselves that the
 * task faulted so that we can do our own recover process.
 */
static void
_RCRecoverySendTriggerEvent(void)
{
    RM_FLCN_QUEUE_HDR   hdr;
    RM_FLCN_MSG_SOE    msg;

    hdr.unitId    = RM_SOE_UNIT_CHNMGMT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SOE_MSG_SIZE(CHNMGMT, TRIGGER_RC_RECOVERY);

    msg.hdr = hdr;

    msg.msg.chnmgmt.unblockRC.msgType = RM_SOE_MSG_TYPE(CHNMGMT, TRIGGER_RC_RECOVERY);
    dbgPrintf(LEVEL_ALWAYS, "_RCRecoverySendTriggerEvent \n");

    if (!msgQueuePostBlocking(&hdr, &msg))
    {
        dbgPrintf(LEVEL_ALWAYS, "_RCRecoverySendTriggerEvent Failed while sending TRIGGER_RC_RECOVERY\n");
    }
    //
    // Internally notify ourselves that the task failed so we can do our own
    // recovery.
    //
    NOTIFY_EXELWTE_NACK();
}

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
        SOE_BREAKPOINT();
    }

    //
    // 0. If save, save off the dummy pattern.
    //    If restore, check if the dummy pattern already exists. If not, we
    //    interpret this as the very first context restore and treat it as
    //    void, and simply ACK the request
    //
    lwrSize = sizeof(SOE_CTX_DUMMY_PATTERN);
    if (bSave)
    {
        status = dmaWrite(&soeCtxDummyPattern, &memDesc, offset, lwrSize);
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
        status = dmaRead(&soeCtxDummyCheck, &memDesc, offset, lwrSize);

        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
        if (memcmp((void *)&soeCtxDummyCheck, (void *)&soeCtxDummyPattern,
                   sizeof(SOE_CTX_DUMMY_PATTERN)) != 0)
        {
            // Bail out early
            goto _saveRestoreCtx_ack;
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
        status = dmaWrite(&chnmgmtCtx, &memDesc, 0, lwrSize);
        if (FLCN_OK != status)
        {
             goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&chnmgmtCtx, &memDesc, 0, lwrSize);
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
        status = dmaWrite(&appMthdValidMask, &memDesc, 0, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&appMthdValidMask, &memDesc, 0, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

_saveRestoreCtx_ack:
    if (status == FLCN_OK)
    {
        // Ack the context save/restore request
        if (bSave)
        {
            //
            // On a ctx save, clear the last bound GFID in the intr controller
            // for notification interrupts
            //
            icSetNonstallIntrCtrlGfid_HAL(&Ic, 0);
            soeAckCtxSave_HAL(pSoe);
        }
        else
        {
            //
            // On a ctx restore, set the next GFID in the intr controller for
            // notification interrupts. The NXTCTX/NXTCTX2 will be copied to
            // LWRCTX/LWRCTX2 after we ack the restore
            //
            icSetNonstallIntrCtrlGfid_HAL(&Ic, soeGetNextCtxGfid_HAL(pSoe));
            soeAckCtxRestore_HAL(pSoe);
        }
    }
    else if (status == FLCN_ERR_DMA_NACK)
    {
        _RCRecoverySendUnblockEvent();
    }
    else
    {
       _RCRecoverySendTriggerEvent();
    }
    return status;
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
    LwU32      sizeBytes;
    LwBool     bSuccess = LW_TRUE;
    FLCN_STATUS status;

    if ((SemaphoreMthdArray.inputBitMask & SOE_SEMAPHORE_INPUT_VALID) == SOE_SEMAPHORE_INPUT_VALID)
    {
        if (!bLongReport)
        {
            sizeBytes = sizeof(SemaphoreMthdArray.longPayload.payload);
            SemaphoreMthdArray.semDesc.params =
                FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                sizeBytes, SemaphoreMthdArray.semDesc.params);

            status = dmaWrite(&(SemaphoreMthdArray.longPayload.payload),
                              &(SemaphoreMthdArray.semDesc), 0, sizeBytes);
            if (status == FLCN_ERR_DMA_NACK)
            {
                bSuccess = LW_FALSE;
            }
        }
        else
        {
            sizeBytes = sizeof(SemaphoreMthdArray.longPayload);

            // Also include the ptimer timestamp for long report type
            osPTimerTimeNsLwrrentGet(&(SemaphoreMthdArray.longPayload.time));
            SemaphoreMthdArray.semDesc.params =
                FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                sizeBytes, SemaphoreMthdArray.semDesc.params);

            status = dmaWrite(&(SemaphoreMthdArray.longPayload),
                              &(SemaphoreMthdArray.semDesc), 0, sizeBytes);
            if (status == FLCN_ERR_DMA_NACK)
            {
                bSuccess = LW_FALSE;
            }
        }
        if (bSuccess && !bFlushDisable)
        {
            //
            // We suppress the error from soeFbifFlush today. There isn't much
            // action we can take if flush times out since that is a system
            // wide failure. For now, just march on. We don't want to halt
            // since halting is bad for security and safety.
            //
            (void)soeFbifFlush_HAL(pSoe);
        }
    }

    //
    // Clear the input valid mask after releasing the semaphore instead of
    // clearing the entire semaphore method array
    //
    SemaphoreMthdArray.inputBitMask = 0x0;

    return bSuccess;
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
        case SOE_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_A):
        {
            SemaphoreMthdArray.semDesc.address.hi = mthdData;
            SemaphoreMthdArray.inputBitMask |= BIT(SOE_SEMAPHORE_A_RECEIVED);
            break;
        }
        case SOE_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_B):
        {
            SemaphoreMthdArray.semDesc.address.lo = mthdData;
            SemaphoreMthdArray.inputBitMask |= BIT(SOE_SEMAPHORE_B_RECEIVED);
            break;
        }
        case SOE_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_C):
        {
            SemaphoreMthdArray.longPayload.payload = mthdData;
            SemaphoreMthdArray.inputBitMask |=
                BIT(SOE_SEMAPHORE_PAYLOAD_RECEIVED);
            break;
        }
        case SOE_METHOD_TO_METHOD_ID(LW95A1_SEMAPHORE_D):
        {
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
                    dbgPrintf(LEVEL_ALWAYS, "_releaseSemaphore failed \n");
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
                icSemaphoreDTrapIntrTrigger_HAL(&Ic);
            }
            break;
        }
        case SOE_METHOD_TO_METHOD_ID(LW95A1_EXELWTE):
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
                        dbgPrintf(LEVEL_ALWAYS, "_processOSMethod  mthdId %d releaseSempahore failed \n ", mthdId);
                        _RCRecoverySendUnblockEvent();
                    }
                }
            }
            // Trigger the appropriate application
            if (bTriggerApp)
            {
                        dbgPrintf(LEVEL_ALWAYS, "_processOSMethod  mthdId %d not triiggering to app \n ", mthdId);
            }
            break;
        }
        default:
        {
            dbgPrintf(LEVEL_ALWAYS, "_processOSMethod  Not handled mthdId %d\n", mthdId);
            // Do nothing
            break;
        }
    }
    return bUnmaskIntr;
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
    LwBool bReady = LW_TRUE; // ready to handle more work from host interface
    while (soePopMthd_HAL(pSoe, &mthdId, &mthdData))
    {
        dbgPrintf(LEVEL_ALWAYS, "Mthdid = 0x%x Mthddata = 0x%x\n", mthdId, mthdData);
        if ((mthdId < APP_METHOD_MIN) || (mthdId > APP_METHOD_MAX))
        {
            // Handle Host internal methods first
            if (soeProcessHostInternalMethods_HAL(pSoe, mthdId, mthdData) == FLCN_OK)
            {
                // Skip this method if handled above
                dbgPrintf(LEVEL_ALWAYS, "_processMethods its an HostInternalMethiod \n");
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
          dbgPrintf(LEVEL_ALWAYS, "_processMethods its an app Method \n");
        }
    }
    return bReady;
}

/*
 * @brief Triggers RC recovery process if method is recieved with no
 *        valid context. Refer Hopper-187 RFD
 *
 * returns LW_TRUE is RC recovery was initiated else LW_FALSE
 */
LwBool _soeTriggerRCRecoveryMthdRcvdForIlwalidCtx(void)
{
    LwU16 mthdId;
    LwU32 mthdData;
      
    if (!soeIsLwrrentChannelCtxValid_HAL(&Soe, NULL))
    {
        //
        // If context is not valid send msg to RM
        // 1. In the absence of a valid context, ucode must drain the fifo of all methods so that MTHD interrupt
        //    does not keep on firing
        //
        while (soePopMthd_HAL(&Soe, &mthdId, &mthdData))
        {
            // NOP
        }
        // 2. Ucode will then clear the MTHD bit 
        icHostIfIntrUnmask_HAL();
        //
        // 3. Ucode will send raise an error using SWGEN0 and send it a ENG_MTHD_NO_CTX error code
        //    Directly Trigger RC recovery instead of sending new error code
        _RCRecoverySendTriggerEvent();
        return LW_TRUE;
    } 
    else
    {
        return LW_FALSE;
    }
}


/*!
 * Process the method interrupt
 */
static void
_processMethodIntr(void)
{
    // Check whether method recieved for valid context
    if (_soeTriggerRCRecoveryMthdRcvdForIlwalidCtx())
    {
        return;
    }
    
    if (soeIsMthdFifoEmpty_HAL(pSoe))
    {
        //
        // No methods to process (they should have been processed as part of
        // processing an earlier method interrupt). Unmask mthd and ctxsw
        // interrupts (they were masked in ISR) and just return
        //
        dbgPrintf(LEVEL_ALWAYS, "processMethodIntr soeIsMthdFifoEmpty_HAL returned false\n");
        icHostIfIntrUnmask_HAL(pSoe);
        return;
    }

    //
    // Before we start popping methods out of our FIFO, assert that we're busy.
    // We will de-assert the busy signal once we're done, unless we got an
    // execute method. If we got an execute method, we will continue asserting
    // busy until the task handling the methods sends us the completion signal.
    //

    soeHostIdleProgramBusy_HAL(pSoe);
    if (_processMethods())
    {
        //
        // Indicate to host that we're idle and ready to receive more work from
        // it.
        //
        soeHostIdleProgramIdle_HAL(pSoe);

        // unmask mthd and ctxsw interrupts (they were masked in ISR)
        icHostIfIntrUnmask_HAL(pSoe);
        dbgPrintf(LEVEL_ALWAYS, "processMethodIntr interrupt cleared\n");
    }
}

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
        soeSendCtxSwIntrTrivialAck();
        //
        // In new RC recovery steps Ctxsw intr is unmask hence make sure that
        // only this intr is unmask and not the method interrupt.
        //
        icHostIfIntrCtxswUnmask_HAL(pSoe);
    }
    else
    {
        bLwrCtxValid  = soeIsLwrrentChannelCtxValid_HAL(pSoe, &lwrCtx);

        bNextCtxValid = soeIsNextChannelCtxValid_HAL(pSoe, &nxtCtx);
        dbgPrintf(LEVEL_ALWAYS, "Lwrr ctx 0x%x and nect ctx 0x%x\n",lwrCtx, nxtCtx);
        if (!bLwrCtxValid && !bNextCtxValid)
        {
            //
            // Neither current nor next ctx is valid
            // Ack the restore immediately
            //
            soeAckCtxRestore_HAL(pSoe);
        }
        else if (bLwrCtxValid)
        {
            // save current ctx
            dbgPrintf(LEVEL_ALWAYS, "Lwrr ctx valid\n");
            status = _saveRestoreCtx(lwrCtx, LW_TRUE);
        }
        else if (!bLwrCtxValid && bNextCtxValid)
        {
            // restore next ctx
            dbgPrintf(LEVEL_ALWAYS, "Next ctx valid\n");
            status = _saveRestoreCtx(nxtCtx, LW_FALSE);
        }
        else
        {
            dbgPrintf(LEVEL_ALWAYS, "ERROR case\n");

            //
            // We should not get here as long as host behaves properly.
            // TODO-ssapre: We can probably remove this once code development is
            // complete
            //
            SOE_BREAKPOINT();
        }
        //
        // Nothing to do for SW-controlled idle. We will report busy until we ack
        // the ctxsw request, so that is automatically the correct behavior.
        //
        if (status == FLCN_OK)
        {
            // unmask mthd and ctxsw interrupts (they were masked in ISR)
            dbgPrintf(LEVEL_ALWAYS, "save restore successfull unmask interrupt\n");
            icHostIfIntrUnmask_HAL(pSoe);
        }
    }
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
    RM_FLCN_MSG_SOE  msg;

    switch (pChnmgmtEvt->cmd.pCmd->cmd.chnmgmt.cmdType)
    {
        case RM_SOE_CMD_TYPE(CHNMGMT, ENGINE_RC_RECOVERY):
        {
            dbgPrintf(LEVEL_ALWAYS, "_chnmgmtProcessCmd recieved RM command ENGINE_RC_RECOVERY\n");
            // a) First pop all methods
            while (soePopMthd_HAL(&Soe, &mthdId, &mthdData))
            {
                // NOP
            }

            soeProcessEngineRcRecoveryCmd_HAL();

            // Respond that we're done
            hdr.unitId    = RM_SOE_UNIT_CHNMGMT;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pChnmgmtEvt->cmd.pCmd->hdr.seqNumId;
            hdr.size      = RM_SOE_MSG_SIZE(CHNMGMT, ENGINE_RC_RECOVERY);
            msg.hdr       = hdr;
            msg.msg.chnmgmt.engineRC.msgType = RM_SOE_MSG_TYPE(CHNMGMT, ENGINE_RC_RECOVERY);

            if (!msgQueuePostBlocking(&hdr, &msg))
            {
                dbgPrintf(LEVEL_ALWAYS, "_chnmgmtProcessCmd Failed sending response to ENGINE_RC_RECOVERY\n");
            }
            break;
        }
        case RM_SOE_CMD_TYPE(CHNMGMT, FINISH_RC_RECOVERY):
        {
            dbgPrintf(LEVEL_ALWAYS, "_chnmgmtProcessCmd recieved RM command FINISH_RC_RECOVERY\n");
            // Make HostIf Interrupt Ctxsw Unmask to false
            g_isRcRecoveryInProgress = LW_FALSE;

            //
            // Unset the NACK bit unconditionally. It could have been set when
            // we saw a ctxsw request pending during RC error recovery. We
            // should only unest it now that RM has finished resetting the host
            // side and host has deasserted the ctxsw request. If it wasn't set
            // before, unsetting it now causes no harm.
            //
            soeSetHostAckMode_HAL(&Soe, LW_FALSE);

            // Unmask host interface interrupts
            icHostIfIntrUnmask_HAL();
            break;
        }
        default:
            dbgPrintf(LEVEL_ALWAYS, "_chnmgmtProcessCmd recieved Invalid RM command %d\n",pChnmgmtEvt->cmd.pCmd->cmd.chnmgmt.cmdType);
            break;
    }
}


/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(chnlMgmtMain, pvParameters)
{
    DISPATCH_CHNMGMT dispatch;
    FLCN_STATUS      ret      = -1;

    // Initialize static data for semaphore method array
    SemaphoreMthdArray.semDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SOE_DMAIDX_VIRT,
                                        SemaphoreMthdArray.semDesc.params);


    // MK TODO: Remove sample code, once any other task does partition switch.
    dbgPrintf(LEVEL_ALWAYS, "ChnlMgmtMain testing partition switch ...\n");

    {
        int i;

        for (i=0; i<10; ++i)
        {
            int x, y;
            x = 1234;
            y = i;

            //
            // MK note: if there is more than one task calling worklaunch,
            // make sure to do this in critical section, as this is shared
            // resource.
            //
            rpcGetRequest()->type = WL_REQUEST_TYPE_TEST;
            rpcGetRequest()->test.num1 = x;
            rpcGetRequest()->test.num2 = y;
            partitionSwitch(PARTITION_ID_WORKLAUNCH, 0, 0, 0, 0, 0);
            dbgPrintf(LEVEL_ALWAYS,
                    "ChnlMgmtMain status %d, %d + %d = %d\n",
                    rpcGetReply()->status, x, y, rpcGetReply()->test.num);
        }
    }

    //
    // When we start off, we have nothing pending on the host interface, so
    // tell host that we're idle using the SW override. When there are any
    // methods in our FIFO, or a ctxsw pending, HW will automatically switch us
    // to busy. Programming the override to idle also atomically switches SOE
    // to using the new idle signal, instead of the old HW-generated idle
    // signal.
    //
    soeHostIdleProgramIdle_HAL(pSoe);
    for (;;)
    {
        if (FLCN_OK == lwrtosQueueReceive(SoeChnMgmtCmdDispQueue, &dispatch, sizeof(dispatch), lwrtosMAX_DELAY))
        {
            dbgPrintf(LEVEL_ALWAYS, "ChnlMgmtMain recieved event %d ...\n", dispatch.eventType);
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

                    if (dispatch.completionEvent.bDmaNackReceived)
                    {
                         dbgPrintf(LEVEL_ALWAYS, "ChnlMgmtMain RC recovery completed\n");
                        // Notify RM to start RC recovery
                        _RCRecoverySendUnblockEvent();
                    }
                    else
                    {
                        LwBool bUnmaskIntr= LW_TRUE;

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
                            soeHostIdleProgramIdle_HAL(pSoe);
                            icHostIfIntrUnmask_HAL(pSoe);
                        }
                    }
                    break;

                case DISP2CHNMGMT_EVT_CMD:
                    _chnmgmtProcessCmd(&dispatch);
                    break;

                default:
                    //
                    // Only internally generated events can bring us here, so
                    // putting a halt here will enable us to catch bugs, but cannot
                    // be used to cause a DoS attack from CPU.
                    //
                    lwuc_halt();
                    break;
            }
        }
    }
    sysTaskExit("chnlmgmt: Failed waiting on queue. No RM communication is possible.", ret);
}

