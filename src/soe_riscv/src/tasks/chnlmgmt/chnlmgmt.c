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
#include "engine.h"
#include "class/clcba2.h"
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
 * A colwenience macro to mask off all but the lowest 6 bits of app method IDs
 * received from the method FIFO. The assumption is that each app will have
 * upto 64 methods, and all app methods will share the same array space. The
 * masked method ID is used to set the valid mask for the worklaunch partition
 * to quickly identify which method IDs were received.
 */
#define APP_MTHD_MASK (0x3F)

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

SOE_CTX_WORKLAUNCH worklaunch
    GCC_ATTRIB_SECTION("dmem_", "worklaunch")
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

/*!
 * RC Recovery state of task_chnmgmt
 */
LwBool g_isRcRecoveryInProgress = LW_FALSE;

/*!
 * Global state pointing to whether a channel app is lwrrently active or not
 */
//static LwBool chnAppActive;

/*!
 * @brief DMA the address to the engine's context state from the instance
 * memory and return it
 *
 * @param[in] ctx   entire ctx (current/next) register value
 *
 * @return Memory address of the engine's state context
 */
static sysSYSLIB_CODE LwU64
_soeGetEngStateAddr
(
    LwU32  ctx
)
{    
    LwU64 address;
    RM_FLCN_MEM_DESC memDesc;
    static LwU64 engGlobalCtx GCC_ATTRIB_SECTION("dmem_", "engGlobalCtx");
    LwU32 maxSize        = sizeof(engGlobalCtx);
    LwU8 dmaIdx;

    // Initialize before filling up its bit fields
    memDesc.params = 0;

    soeGetInstBlkOffsetAndDmaIdx_HAL(pSoe, ctx, LW_TRUE, &address, &dmaIdx);

    RM_FLCN_U64_PACK(&(memDesc.address), &address);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     dmaIdx, memDesc.params);

    if (FLCN_OK != dmaRead(&engGlobalCtx, &memDesc, 0, maxSize))
    {
        SOE_BREAKPOINT();
    }

    return engGlobalCtx;
}


/*!
 * @brief Save/restore IV from the instance block
 *
 * @param[in] ctx   entire ctx (current/next) register value
 *
 * @return Memory address of the engine's state context
 */
static sysSYSLIB_CODE FLCN_STATUS
_soeSaveRestoreIv
(
    LwU32  ctx,
    LwU32 *pIv,
    LwU32  sizeBytes,
    LwBool bSave
)
{
    LwU64 address;
    RM_FLCN_MEM_DESC memDesc;
    LwU8 dmaIdx;

    // Initialize before filling up its bit fields
    memDesc.params = 0;

    soeGetInstBlkOffsetAndDmaIdx_HAL(pSoe, ctx, LW_FALSE, &address, &dmaIdx);

    RM_FLCN_U64_PACK(&(memDesc.address), &address);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     dmaIdx, memDesc.params);

    if (bSave)
        return dmaWrite(pIv, &memDesc, 0, sizeBytes);
    else
        return dmaRead(pIv, &memDesc, 0, sizeBytes);
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
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Obtain the block of memory from the instance block that holds the
    // address of the memory where our state lives. It also contains the target
    // memory where it lives.
    //
    engStateAddr = _soeGetEngStateAddr(ctx);

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
            // Bail out early since engine ctx is 0, but need to read IV.
            goto _saveRestoreCtx_Iv;
        }
    }

    offset += lwrSize;
    lwrSize = sizeof(worklaunch);
    if (bSave)
    {
        status = dmaWrite(&worklaunch, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }
    else
    {
        status = dmaRead(&worklaunch, &memDesc, offset, lwrSize);
        if (FLCN_OK != status)
        {
            goto _saveRestoreCtx_ack;
        }
    }

_saveRestoreCtx_Iv:
    //
    // RM updates IV in RAMFC, so use that as our source of truth, overwriting
    // what we save/restored from engine ctx above. It is only saved/restored
    // in instance block for simplicity to keep the entire worklaunch context
    // in one structure.
    //
    if (_soeSaveRestoreIv(ctx, worklaunch.iv, GCM_IV_SIZE_BYTES, bSave) != FLCN_OK)
    {
        // shouldn't get here as long as instance block is accessible.
        SOE_BREAKPOINT();
    }

_saveRestoreCtx_ack:
    if (status == FLCN_OK)
    {
        // Ack the context save/restore request
        if (bSave)
        {
            // memset local data to 0 after every save.
            memset(&worklaunch, 0x0, sizeof(worklaunch));

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
    LwU32 appMthdIndex;

    while (soePopMthd_HAL(pSoe, &mthdId, &mthdData))
    {
        // Allow handling of host internal methods first (not sent to worklaunch partition)
        if (soeProcessHostInternalMethods_HAL(pSoe, mthdId, mthdData) == FLCN_OK)
        {
            continue;
        }

        // Everything else is a work launch method
        if (worklaunch.methodCount >= WORKLAUNCH_METHOD_ARRAY_SIZE)
            continue; // TODO-ssapre: trigger RC

        appMthdIndex = mthdId & APP_MTHD_MASK;
        worklaunch.methods[worklaunch.methodCount][WORKLAUNCH_METHOD_ARRAY_IDX_ADDR] = SOE_METHOD_ID_TO_METHOD(mthdId);
        worklaunch.methods[worklaunch.methodCount][WORKLAUNCH_METHOD_ARRAY_IDX_DATA] = mthdData;
        worklaunch.methodCount++;
        worklaunch.validMask |= BIT64(appMthdIndex);

#ifdef STAGING_TEMPORARY_CODE
        if (mthdId == SOE_METHOD_TO_METHOD_ID(LWCBA2_ENABLE_DBG_PRINT))
            worklaunch.bDbgPrint = LW_TRUE;
#endif

        // Check if we just got a "trigger" method
        if (mthdId == SOE_METHOD_TO_METHOD_ID(LWCBA2_EXELWTE) || mthdId == SOE_METHOD_TO_METHOD_ID(LWCBA2_SEMAPHORE_D))
        {
            if (worklaunch.bDbgPrint)
            {
                dbgPrintf(LEVEL_ALWAYS, "chnlmgmt worklaunch.methodCount = 0x%x\n", worklaunch.methodCount);
                dbgPrintf(LEVEL_ALWAYS, "chnlmgmt worklaunch.validMask = 0x%llx\n", worklaunch.validMask);
                for (LwU32 i = 0; i < worklaunch.methodCount; i++)
                {
                    dbgPrintf(LEVEL_ALWAYS, "chnlmgmt worklaunch.methods[%d][0]= 0x%x worklaunch.methods[%d][1] = 0x%x\n",
                        i, worklaunch.methods[i][WORKLAUNCH_METHOD_ARRAY_IDX_ADDR], i, worklaunch.methods[i][WORKLAUNCH_METHOD_ARRAY_IDX_DATA]);
                }
            }
            
            // trigger partition switch
            memcpy(rpcGetRequest(), &worklaunch, sizeof(SOE_CTX_WORKLAUNCH));
            partitionSwitch(PARTITION_ID_WORKLAUNCH, 0, 0, 0, 0, 0);
            if (worklaunch.bDbgPrint)
                dbgPrintf(LEVEL_ALWAYS, "chnlmgmt: back in partition\n");

            WL_REPLY *rep = rpcGetReply();
            LwU32 errorCode = rep->errorCode;
            if (errorCode != LWCBA2_ERROR_NONE)
            {
                dbgPrintf(LEVEL_ALWAYS, "error code from worklaunch: 0x%x\n", errorCode);
                csbWrite(LW_CSOE_FALCON_MAILBOX1, errorCode);
                SOE_BREAKPOINT(); // TODO-ssapre: trigger RC
            }
            if (worklaunch.bDbgPrint)
                dbgPrintf(LEVEL_ALWAYS, "IV after return from work launch: iv[0] = 0x%x, iv[1] = 0x%x, iv[2] = 0x%x\n", rep->iv[0], rep->iv[1], rep->iv[2]);

            memcpy(worklaunch.iv, rep->iv, GCM_IV_SIZE_BYTES); // copy back potentially incremented returned IV

            worklaunch.validMask = 0;
            worklaunch.methodCount = 0;
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
/*
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
*/
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

            // Respond that we're done
            hdr.unitId    = RM_SOE_UNIT_CHNMGMT;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pChnmgmtEvt->cmd.pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
            if (!msgQueuePostBlocking(&hdr, &msg))
            {
                dbgPrintf(LEVEL_ALWAYS, "_chnmgmtProcessCmd Failed sending response to FINISH_RC_RECOVERY\n");
            }
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
    FLCN_STATUS      ret      = FLCN_ERROR;


    // MK TODO: Remove sample code, once any other task does partition switch.
    dbgPrintf(LEVEL_ALWAYS, "ChnlMgmtMain testing partition switch ...\n");

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

                case DISP2CHNMGMT_EVT_CMD:
                    _chnmgmtProcessCmd(&dispatch);
                    break;

                default:
                    //
                    // Only internally generated events can bring us here, so
                    // putting a halt here will enable us to catch bugs, but cannot
                    // be used to cause a DoS attack from CPU.
                    //
                    SOE_BREAKPOINT();
                    break;
            }
        }
    }
    sysTaskExit("chnlmgmt: Failed waiting on queue. No RM communication is possible.", ret);
}

