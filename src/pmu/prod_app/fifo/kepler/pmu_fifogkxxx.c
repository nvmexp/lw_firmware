/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogkxxx.c
 * @brief  HAL-interface for the GKXXX Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "dev_pwr_csb.h"
#include "dev_top.h"
#include "hwproject.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "dbgprintf.h"

#include "config/g_fifo_private.h"


//
// Compile time check to ensure that number of PBDMAs does not exceed 16.
// We cache the pbdma IDs in a 16 bit mask as of now, so need to extend
// the mask to 32 bit if the number of PBDMAs exceeds 16 on any chip.
//
ct_assert(LW_HOST_NUM_PBDMA <= 16);

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
static FLCN_STATUS s_fifoPreemptChannel_GMXXX(LwU32 chId, LwU32 fifoEngId, LwBool bIdIsTsgType,
                                             LwBool bPollForPbdmaPreempt, LwU32 pbdmaId, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libFifo", "s_fifoPreemptChannel_GMXXX");
static FLCN_STATUS s_fifoPreemptSequencePbdma_GMXXX(LwU32 fifoEngId, LwU32 pbdmaId,
                                                   LwU32 *pPreemptedChId, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libFifo", "_fifoPreemptSequencePBDMA_GMXXX");
static FLCN_STATUS s_fifoPreemptSequenceEngine_GMXXX(LwU32 fifoEngId, LwU32 pbdmaId,
                                                     LwU32 preemptedChId, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libFifo", "s_fifoPreemptSequenceEngine_GMXXX");
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Initialize FIFO values
 *
 */
void
fifoPreInit_GMXXX(void)
{
    LwU8   i;
    LwU32  tableEntry;
    LwU32  fifoEngineData   = 0;
    LwU32  engLookupTblSize = 0;
    LwU8   fifoEngineType   = 0;
    LwU8   pmuEngineId      = PMU_ENGINE_ILWALID;
    LwU8   fifoEngineId     = 0;
    LwU16  pbdmaIdMask      = 0;
    LwU8   runlistId        = 0;
    LwU8   intrId           = 0;
    LwBool bFifoIdFound     = LW_FALSE;
    LwBool bFifoDataFound   = LW_FALSE;
    PMU_ENGINE_LOOKUP_TABLE *engineLookupTable = NULL;

    fifoDisablePlcRecompressWar_HAL(&Fifo);

    fifoGetEngineLookupTable_HAL(&Fifo, &engineLookupTable, &engLookupTblSize);

    for (i = 0; i < LW_PTOP_DEVICE_INFO__SIZE_1; ++i)
    {
        tableEntry = REG_RD32(BAR0, LW_PTOP_DEVICE_INFO(i));

        if (FLD_TEST_DRF(_PTOP, _DEVICE_INFO, _ENTRY, _NOT_VALID, tableEntry))
        {
            continue;
        }

        switch (DRF_VAL(_PTOP, _DEVICE_INFO, _ENTRY, tableEntry))
        {
            case LW_PTOP_DEVICE_INFO_ENTRY_DATA:
            {
                fifoEngineData = tableEntry;
                bFifoDataFound = LW_TRUE;
                break;
            }
            case LW_PTOP_DEVICE_INFO_ENTRY_ENUM:
            {
                if (FLD_TEST_DRF(_PTOP, _DEVICE_INFO, _INTR, _NOT_VALID,
                                 tableEntry))
                {
                    //
                    // This should never happen, but in case HW misbehaves, we
                    // wouldn't want to get garbage values for the enums and get
                    // burned later.
                    //
                    PMU_HALT();
                }
                fifoEngineId = DRF_VAL(_PTOP, _DEVICE_INFO, _ENGINE_ENUM,
                                       tableEntry);
                runlistId    = DRF_VAL(_PTOP, _DEVICE_INFO, _RUNLIST_ENUM,
                                       tableEntry);
                intrId       = DRF_VAL(_PTOP, _DEVICE_INFO, _INTR_ENUM,
                                       tableEntry);
                pbdmaIdMask  = fifoFindPbdmaMaskForRunlist_HAL(&Fifo, runlistId);

                bFifoIdFound = LW_TRUE;
                break;
            }
            case LW_PTOP_DEVICE_INFO_ENTRY_ENGINE_TYPE:
            {
                fifoEngineType = DRF_VAL(_PTOP, _DEVICE_INFO, _TYPE_ENUM, tableEntry);
                break;
            }
        }
        if (FLD_TEST_DRF(_PTOP, _DEVICE_INFO, _CHAIN, _DISABLE, tableEntry))
        {
            //
            // Use the engine type to match the HW entry to an entry
            // containing the SW info about the engine.
            //
            pmuEngineId = fifoGetPmuEngineFromLookupTable_HAL(&Fifo,
                                                              engineLookupTable,
                                                              engLookupTblSize,
                                                              fifoEngineData,
                                                              fifoEngineType,
                                                              bFifoDataFound);

            if (bFifoIdFound && pmuEngineId != PMU_ENGINE_ILWALID
                && !fifoIsEngineDisabled_HAL(&Fifo, pmuEngineId))
            {
                Fifo.pmuEngTbl[pmuEngineId].fifoEngId   = fifoEngineId;
                Fifo.pmuEngTbl[pmuEngineId].pbdmaIdMask = pbdmaIdMask;
                Fifo.pmuEngTbl[pmuEngineId].runlistId   = runlistId;
                Fifo.pmuEngTbl[pmuEngineId].intrId      = intrId;
            }
            pmuEngineId    = PMU_ENGINE_ILWALID;
            bFifoIdFound   = LW_FALSE;
            bFifoDataFound = LW_FALSE;
        }
    }
}

/*!
 * Disables engine scheduling.
 *
 * @param[in]  pmuEngId
 *    pmu engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 *
 * @return  LW_TRUE  - if scheduler was already disabled and we don't need
 *                     to re-enable it at the end of sequence
 *          LW_FALSE - if scheduler gets disabled through this API and needs
 *                     to be re-enabled at the end of sequence
 */
LwBool
fifoSchedulerDisable_GMXXX
(
    LwU32   pmuEngId
)
{
    LwU32  runlistId = GET_ENG_RUNLIST(pmuEngId);
    LwU32  data;

    PMU_HALT_COND(runlistId < LW_PFIFO_SCHED_DISABLE_RUNLIST__SIZE_1);

    // disable new work from being scheduled
    data = REG_RD32(BAR0, LW_PFIFO_SCHED_DISABLE);
    if (data & BIT(runlistId))
    {
        // Scheduler is already disabled, return early
        return LW_TRUE;
    }

    data |= BIT(runlistId);
    REG_WR32(BAR0, LW_PFIFO_SCHED_DISABLE, data);

    // Flush the write
    data = REG_RD32(BAR0, LW_PFIFO_SCHED_DISABLE);

    return LW_FALSE;
}

/*!
 * Enable engine scheduling
 *
 * @param[in] pmuEngId
 *    pmu engine id specfies the engine to enable scheduling for.
 *    Note that this is different from elpg engine id.
 */
void
fifoSchedulerEnable_GMXXX
(
    LwU32  pmuEngId
)
{
    LwU32 data;
    LwU32 runlistId = GET_ENG_RUNLIST(pmuEngId);

    PMU_HALT_COND(runlistId < LW_PFIFO_SCHED_DISABLE_RUNLIST__SIZE_1);

    // Enable the scheduler
    data = REG_RD32(BAR0, LW_PFIFO_SCHED_DISABLE);
    data &= ~BIT(runlistId);
    REG_WR32(BAR0, LW_PFIFO_SCHED_DISABLE, data);

    // Flush the write
    data = REG_RD32(BAR0, LW_PFIFO_SCHED_DISABLE);
}

/*!
 * @brief Gets the scheduling status of an engine.
 *
 * @param[in]  pmuEngId
 *    pmu engine id specfies the engine.
 * @param[out] pEngStatus
 *    pEngStatus is set to the scheduling status of the engine.
 */
void
fifoGetEngineStatus_GMXXX
(
    LwU32 pmuEngId,
    LwU32 *pEngStatus
)
{
    LwU32  fifoEngId = GET_FIFO_ENG(pmuEngId);

    *pEngStatus = REG_RD32(BAR0, LW_PFIFO_ENGINE_STATUS(fifoEngId));
}

/*!
 * @brief Execute the Host Preempt Sequence.
 *
 * Step 3 to 9 in the following wiki which outlines host sequence for ELPG
 * https://wiki.lwpu.com/gpuhwkepler/index.php/KeplerHost/ELPG_Sequence#Power_Down_Sequence

 * @param[in] pmuEngId
 *    fifo engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 *
 * @return
 *    FLCN_ERROR if preempt sequence is not successful
 *    FLCN_OK   success
 */
FLCN_STATUS
fifoPreemptSequence_GMXXX
(
    LwU32 pmuEngId,
    LwU8  ctrlId
)
{
    LwU32  preemptedChId   = LW_U32_MAX;
    LwU32  fifoEngId       = GET_FIFO_ENG(pmuEngId);
    LwU32  pbdmaIdMask     = GET_ENG_PBDMA_MASK(pmuEngId);
    LwU32  pbdmaId         = BIT_IDX_32(pbdmaIdMask);

    //
    // For all chips calling into this HAL, there should be only
    // one PBDMA associated with a runlist. So BIT(pbdmaId) should
    // be equal to pbdmaIdMask, assert otherwise.
    //
    PMU_HALT_COND(BIT(pbdmaId) == pbdmaIdMask);

    //
    // Abort if a fifo stalling interrupt is pending. Preemption cannot be
    // guaranteed if a stalling interrept is pending on host.
    //
    if (fifoIsStallingIntrPending_HAL(&Fifo, pbdmaIdMask))
    {
        return FLCN_ERROR;
    }
    //
    // Check if a preempt of the channel/TSG on the PBDMA is necessary and
    // start the preempt if required.
    //
    if (FLCN_OK != s_fifoPreemptSequencePbdma_GMXXX(fifoEngId, pbdmaId, &preemptedChId, ctrlId))
    {
        return FLCN_ERROR;
    }
    //
    // Now check if the PBDMA preempt also kicked off the engine context or
    // if the engine still has to save out. Issue the channel preempt if required.
    //
    if (FLCN_OK != s_fifoPreemptSequenceEngine_GMXXX(fifoEngId, pbdmaId, preemptedChId, ctrlId))
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * Check if preempt stalling interrupts are pending.
 *
 * @paramp[in] pbdmaIdMask - Mask of PBDMA Ids
 *
 * @return  LW_TRUE    - if stalling interrupt pending
 *          LW_FALSE   - no stalling interrupt pending
 */
LwBool
fifoIsStallingIntrPending_GMXXX
(
    LwU16 pbdmaIdMask
)
{
    LwU32 fifoIntr;
    LwU32 pbdmaId;

    fifoIntr = REG_RD32(BAR0, LW_PFIFO_INTR_0);

    FOR_EACH_INDEX_IN_MASK(16, pbdmaId, pbdmaIdMask)
    {
        if (FLD_TEST_DRF(_PFIFO, _INTR_0, _PBDMA_INTR, _PENDING, fifoIntr))
        {
            if (FLD_IDX_TEST_DRF(_PFIFO, _INTR, _PBDMA_ID_STATUS,
                                 pbdmaId, _PENDING,
                                 REG_RD32(BAR0, LW_PFIFO_INTR_PBDMA_ID)))
            {
                //
                // pbdma interrupt for the pbdma which serves the engine
                // is pending. This is a stalling interrupt.
                //
                return LW_TRUE;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // check for stalling interrupts
    fifoIntr &= REG_RD32(BAR0, LW_PFIFO_INTR_STALL);

    return fifoIntr != 0;
}

/* ------------------------ Static Functions ------------------------------- */
/*!
 * @brief Helper function to Execute the Host Preempt Sequence.
 *
 * Exelwtes the PBDMA preempt sequence steps specified in
 * https://wiki.lwpu.com/gpuhwkepler/index.php/KeplerHost/ELPG_Sequence#Power_Down_Sequence

 * @param[in] fifoEngId
 *    fifo engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 * @param[in] pbdmaId
 *    id of the pbdma to be preempted
 * @param[out] *pPreemptedChId
 *    Returns ID of the channel preempted by this sequence
 *
 * @return
 *    FLCN_ERROR if preempt sequence is not successful
 *    FLCN_OK   success
 */
static FLCN_STATUS
s_fifoPreemptSequencePbdma_GMXXX
(
    LwU32  fifoEngId,
    LwU32  pbdmaId,
    LwU32 *pPreemptedChId,
    LwU8   ctrlId
)
{
    LwU32  retryCount   = 0;
    LwU32  chId         = LW_U32_MAX;
    LwBool bCHIdIsValid = LW_FALSE;
    LwBool bIdIsTsgType = LW_FALSE;
    LwU32  lastChId;
    LwU32  data;

    do
    {
        retryCount++;
        if (retryCount > FIFO_MAX_PREEMPT_RETRY_COUNT)
        {
            return FLCN_ERROR;
        }

        lastChId = chId;

        data = REG_RD32(BAR0, LW_PFIFO_PBDMA_STATUS(pbdmaId));
        if (FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                         _VALID, data) ||
            FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                         _CHSW_SAVE, data))
        {
            chId = DRF_VAL(_PFIFO, _PBDMA_STATUS, _ID, data);
            bCHIdIsValid = LW_TRUE;
            bIdIsTsgType = FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _ID_TYPE,
                                        _TSGID, data);
        }
        else if (FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                              _CHSW_LOAD,   data) ||
                 FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                              _CHSW_SWITCH, data))
        {
            chId = DRF_VAL(_PFIFO, _PBDMA_STATUS, _NEXT_ID, data);
            bCHIdIsValid = LW_TRUE;
            bIdIsTsgType = FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _NEXT_ID_TYPE,
                                        _TSGID, data);
        }

    } while (chId != lastChId);

    if (bCHIdIsValid)
    {
        // Abort if a fifo stalling interrupt is pending
        if (fifoIsStallingIntrPending_HAL(&Fifo, BIT(pbdmaId)))
        {
            return FLCN_ERROR;
        }

        if (FLCN_OK != s_fifoPreemptChannel_GMXXX(chId, fifoEngId, bIdIsTsgType,
                                                 LW_TRUE, pbdmaId, ctrlId))
        {
            return FLCN_ERROR;
        }
        *pPreemptedChId = chId;
    }

    return FLCN_OK;
}

/*!
 * @brief Helper function to Execute the Engine Preempt Sequence.
 *
 * Exelwtes the Engine channel preempt sequence steps specified in
 * https://wiki.lwpu.com/gpuhwkepler/index.php/KeplerHost/ELPG_Sequence#Power_Down_Sequence

 * @param[in] fifoEngId
 *    fifo engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 * @param[in] pbdmaId
 *    id of the pbdma to be preempted
 * @param[in] pPreemptedChId
 *    id of the channel preempted by the PBDMA sequence
 *
 * @return
 *    FLCN_ERROR if preempt sequence is not successful
 *    FLCN_OK   success
 */
static FLCN_STATUS
s_fifoPreemptSequenceEngine_GMXXX
(
    LwU32  fifoEngId,
    LwU32  pbdmaId,
    LwU32  preemptedChId,
    LwU8   ctrlId
)
{
    LwU32  retryCount   = 0;
    LwU32  chId         = LW_U32_MAX;
    LwBool bCHIdIsValid = LW_FALSE;
    LwBool bIdIsTsgType = LW_FALSE;
    LwU32  lastChId;
    LwU32  data;

    do
    {
        retryCount++;
        if (retryCount > FIFO_MAX_PREEMPT_RETRY_COUNT)
        {
            return FLCN_ERROR;
        }

        lastChId = chId;

        data = REG_RD32(BAR0, LW_PFIFO_ENGINE_STATUS(fifoEngId));
        if (FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _CTX_STATUS, _VALID, data) ||
            FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _CTX_STATUS, _CTXSW_SAVE, data))
        {
            chId = DRF_VAL(_PFIFO, _ENGINE_STATUS, _ID, data);
            bCHIdIsValid = LW_TRUE;
            bIdIsTsgType = FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS,
                                        _ID_TYPE, _TSGID, data);
        }
        else if (FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _CTX_STATUS,
                              _CTXSW_LOAD, data) ||
                 FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS,
                              _CTX_STATUS, _CTXSW_SWITCH, data))
        {
            chId = DRF_VAL(_PFIFO, _ENGINE_STATUS, _NEXT_ID, data);
            bCHIdIsValid = LW_TRUE;
            bIdIsTsgType = FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS,
                                        _NEXT_ID_TYPE, _TSGID, data);
        }
        else
        {
            //
            // This case should only happen if
            // LW_PFIFO_ENGINE_STATUS_CTX_STATUS==INVALID,
            // and we should never loop if we hit this.
            //
            break;
        }

    } while (chId != lastChId);

    //
    // We're probably preempting an idle channel. If it was busy, we should
    // have bailed out already.
    //
    if (bCHIdIsValid && (chId != preemptedChId))
    {
        if (FLCN_OK != s_fifoPreemptChannel_GMXXX(chId, fifoEngId, bIdIsTsgType,
                                                 LW_FALSE, 0, ctrlId))
        {
            return FLCN_ERROR;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Preempts a channel out of host.
 *
 * Caller is responsible for first disabling channel via fifoChannelData to
 * ensure channel does not get rescheduled.
 * Caller is responsible to detect if the channel is in a time slice group(TSG)
 * All channels in a TSG share same context and treated as a single unit
 * by host. Preempt will fail if this is not properly specified.
 *
 * @param[in] chId                 channel id to preeempt
 * @param[in] fifoEngId            fifo Id to disable holdoffs
 * @param[in] bIsIsTsgType         (Tsg) Time slice group type
 * @param[in] bPollForPbdmaPreempt Wait for PBDMA to see the preempt
 * @param[in] pbdmaId              PBDMA id to preempt
 *
 * @return  FLCN_OK    - if preemption was successful
 *          FLCN_ERROR - otherwise
 */
static FLCN_STATUS
s_fifoPreemptChannel_GMXXX
(
    LwU32  chId,
    LwU32  fifoEngId,
    LwBool bIdIsTsgType,
    LwBool bPollForPbdmaPreempt,
    LwU32  pbdmaId,
    LwU8   ctrlId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwBool      bCHIdIsValid;
    LwBool      bCHIdNextIsValid;
    LwU32       data;

    FLCN_TIMESTAMP  blockStartTimeNs;
    //
    // bump up the priority of the current task greater than that of the
    // sequencer task to prevent fb from being disabled.
    //
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(SEQ) + 1);

    // Issue channel preempt
    REG_WR32(BAR0, LW_PFIFO_PREEMPT,
            DRF_NUM(_PFIFO, _PREEMPT, _ID, chId) |
            (bIdIsTsgType ? DRF_DEF(_PFIFO, _PREEMPT, _TYPE, _TSG)
                          : DRF_DEF(_PFIFO, _PREEMPT, _TYPE, _CHANNEL)));

    // Wait for PBDMA to see the preempt
    if (bPollForPbdmaPreempt)
    {
        bCHIdIsValid     = LW_FALSE;
        bCHIdNextIsValid = LW_FALSE;

        osPTimerTimeNsLwrrentGet(&blockStartTimeNs);
        do
        {
            if (osPTimerTimeNsElapsedGet(&blockStartTimeNs) >
                FIFO_PREEMPT_WAIT_TIMEOUT_NS)
            {
                // lower it back to the original priority.
                lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));
                return FLCN_ERROR;
            }

            data = REG_RD32(BAR0, LW_PFIFO_PBDMA_STATUS(pbdmaId));

            bCHIdIsValid = (FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                                         _VALID, data) &&
                           (DRF_VAL(_PFIFO, _PBDMA_STATUS, _ID, data) == chId));

            bCHIdNextIsValid = ((FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                                              _CHSW_LOAD, data)    ||
                                 FLD_TEST_DRF(_PFIFO, _PBDMA_STATUS, _CHAN_STATUS,
                                              _CHSW_SWITCH, data)) &&
                                (DRF_VAL(_PFIFO, _PBDMA_STATUS, _NEXT_ID, data) == chId));

        } while (bCHIdIsValid || bCHIdNextIsValid);
    }

    // Release engine holdoffs for preempt to complete
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                  LPWR_HOLDOFF_MASK_NONE);

    // Wait for preempt to finish
    if (!PMU_REG32_POLL_US(
            LW_PFIFO_PREEMPT,
            DRF_SHIFTMASK(LW_PFIFO_PREEMPT_PENDING),
            DRF_DEF(_PFIFO, _PREEMPT, _PENDING, _FALSE),
            FIFO_PREEMPT_PENDING_TIMEOUT_US, // 1 ms
            PMU_REG_POLL_MODE_BAR0_EQ))
    {
        //
        // Temporary fix to disable this breakpoint. Bug 883281 logged.
        // Root cause the timeout and enable this breakpoint later.
        //
        //PMU_BREAKPOINT();
        status = FLCN_ERROR;
    }

    // lower it back to the original priority.
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));

    return status;
}

/**
 * @brief Get corresponding pmu engine from lookup table
 *
 * @param[in]  engineLookupTable
 * @param[in]  engLookupTblSize
 * @param[in]  fifoEngineData
 * @param[in]  fifoEngineType
 * @param[in]  bFifoDataFound
 */
LwU8
fifoGetPmuEngineFromLookupTable_GMXXX
(
    PMU_ENGINE_LOOKUP_TABLE *engineLookupTable,
    LwU32                    engLookupTblSize,
    LwU32                    fifoEngineData,
    LwU8                     fifoEngineType,
    LwBool                   bFifoDataFound
)
{
    LwU32 tableIdx;
    for (tableIdx = 0; tableIdx < engLookupTblSize; ++tableIdx)
    {
        if (engineLookupTable[tableIdx].typeEnum == fifoEngineType)
        {
            return engineLookupTable[tableIdx].pmuEngineId;
        }
    }
    return PMU_ENGINE_ILWALID;
}

/*!
 * Check if engine is idle or busy
 *
 * @paramp[in] fifoEngId - Engine ID to check
 *
 * @return  LW_TRUE    - If Engine is Idle
 *          LW_FALSE   - Engine is Busy
 */
LwBool
fifoIsEngineIdle_GMXXX
(
    LwU32 fifoEngId
)
{
    LwU32  engStatus;

    engStatus = REG_RD32(BAR0, LW_PFIFO_ENGINE_STATUS(fifoEngId));
    if (FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _ENGINE, _BUSY, engStatus) ||
        FLD_TEST_DRF(_PFIFO, _ENGINE_STATUS, _CTXSW, _IN_PROGRESS, engStatus))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}
