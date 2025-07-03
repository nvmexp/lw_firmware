/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogaxxx.c
 * @brief  HAL-interface for the GAXXX Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objfifo.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_bar0.h"
#include "dev_top.h"
#include "dev_runlist.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GA10X[] =
{
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,     0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC1,   1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,    0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,    1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,    2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,    3},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE4,    4},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE5,    5},    
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC0,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC1,   1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC2,   2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_SEC,      PMU_ENGINE_SEC2,   0},
};

/* ------------------------ Macros and Defines ----------------------------- */
//
// TODO-ssapre: These are lwrrently in rmbase.h, which is only available to RM.
// We should move these to a common place where both RM and ucode can access
// it - some place like lwmisc.h
//
#define SF_INDEX(sf)            ((0?sf)/32)
#define SF_SHIFT(sf)            ((0?sf)&31)
#define SF_MASK(sf)             (0xFFFFFFFF>>(31-(1?sf)+(0?sf)))

#define SF_ARR32_VAL(s,f,arr) \
    (((arr)[SF_INDEX(LW ## s ## f)] >> SF_SHIFT(LW ## s ## f)) & SF_MASK(LW ## s ## f))

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
static FLCN_STATUS s_fifoPreemptRunlist_GA10X(LwU32 pmuEngId, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libFifo", "s_fifoPreemptRunlist_GA10X");
static LwU8 s_fifoGetPmuEngId_GA10X(LwU32 fifoEngId)
    GCC_ATTRIB_SECTION("imem_lpwr", "s_fifoGetPmuEngId_GA10X");

/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Returns the list of engines for this chip
 * @param[out]  pEngineLookupTable
 *    list of engines for this chip
 * @param[out]  engLookupTblSize
 */
void
fifoGetEngineLookupTable_GA10X
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GA10X;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GA10X);
}

/**
 * @brief Get corresponding pmu engine from lookup table.
 *        For Pascal, both engine type enum and inst id
 *        will be used to map the correct pmu engine
 *
 * @param[in]  engineLookupTable
 * @param[in]  engLookupTblSize
 * @param[in]  instanceId
 * @param[in]  fifoEngineType
 * @param[in]  bFifoDataFound
 */
LwU8
fifoGetPmuEngineFromLookupTable_GA10X
(
    PMU_ENGINE_LOOKUP_TABLE *engineLookupTable,
    LwU32                    engLookupTblSize,
    LwU32                    instanceId,
    LwU8                     devType,
    LwBool                   bFifoDataFound
)
{
    LwU32 tableIdx;

    for (tableIdx = 0; tableIdx < engLookupTblSize; ++tableIdx)
    {
        if ((engineLookupTable[tableIdx].typeEnum == devType) &&
            (engineLookupTable[tableIdx].instId == instanceId))
        {
            return engineLookupTable[tableIdx].pmuEngineId;
        }
    }
    return PMU_ENGINE_ILWALID;
}

/*!
 * Initialize the data structures of all host-driven engines
 */
void
fifoPreInit_GA10X(void)
{
    LwU32  i;
    LwU32  j = 0;
    LwBool bInChain = LW_FALSE;
    LwU32  deviceArray[LW_PTOP_DEVICE_INFO_CFG_MAX_ROWS_PER_DEVICE_STATIC] = {0};
    LwU32  engLookupTblSize = 0;
    PMU_ENGINE_LOOKUP_TABLE *pEngineLookupTable = NULL;
    LwU32  numRows = DRF_VAL(_PTOP, _DEVICE_INFO_CFG, _NUM_ROWS, REG_RD32(BAR0, LW_PTOP_DEVICE_INFO_CFG));

    fifoGetEngineLookupTable_HAL(&Fifo, &pEngineLookupTable, &engLookupTblSize);

    for (i = 0; i < numRows; i++)
    {
        //
        // PTOP is lwrrently not accessible from PRI HUB on fmodel, but is
        // accessible on RTL. Once HW fixes that, we can move this to the FECS
        // fast path. Bug 2117452 tracks the fmodel fix.
        //
        LwU32 tableEntry = REG_RD32(BAR0, LW_PTOP_DEVICE_INFO2(i));

        if (!FLD_TEST_DRF(_PTOP, _DEVICE_INFO2, _ROW_VALUE, _ILWALID, tableEntry) ||
            bInChain)
        {
            bInChain = FLD_TEST_DRF(_PTOP, _DEVICE_INFO2, _ROW_CHAIN, _MORE, tableEntry);

            PMU_HALT_COND(j < LW_PTOP_DEVICE_INFO_CFG_MAX_ROWS_PER_DEVICE_STATIC);

            deviceArray[j++] = tableEntry;

            if (!bInChain)
            {
                //
                // Parse this device (only if it is a host-driven engine). We
                // only care about host-driven engines on the PMU today.
                //
                if (SF_ARR32_VAL(_PTOP, _DEVICE_INFO2_DEV_IS_ENGINE, deviceArray))
                {
                    LwU8   devType, pmuEngineId, fifoEngineId, runlistId;
                    LwU32  runlistPriBase, runlistEngId, instanceId;

                    // Parse this device from the created array
                    runlistPriBase = SF_ARR32_VAL(_PTOP, _DEVICE_INFO2_DEV_RUNLIST_PRI_BASE, deviceArray)
                        << SF_SHIFT(LW_PTOP_DEVICE_INFO2_DEV_RUNLIST_PRI_BASE);
                    runlistEngId   = SF_ARR32_VAL(_PTOP, _DEVICE_INFO2_DEV_RLENG_ID, deviceArray);
                    devType        = SF_ARR32_VAL(_PTOP, _DEVICE_INFO2_DEV_TYPE_ENUM, deviceArray);
                    instanceId     = SF_ARR32_VAL(_PTOP, _DEVICE_INFO2_DEV_INSTANCE_ID, deviceArray);

                    //
                    // TODO-ssapre: Eventually, LW_RUNLIST will be accessible as a
                    // PRI target of the PRI HUB (aka FECS in the past), but the
                    // support is lwrrently not there. We will add it once HW adds
                    // support for it on all netlists that SW supports. For now, we
                    // will just access from the BAR0 path.
                    //
                    fifoEngineId = DRF_VAL(_RUNLIST, _ENGINE_STATUS_DEBUG, _ENGINE_ID,
                        REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_ENGINE_STATUS_DEBUG(runlistEngId)));
                    runlistId = DRF_VAL(_RUNLIST, _DOORBELL_CONFIG, _ID,
                        REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_DOORBELL_CONFIG));

                    //
                    // Use the engine type to match the HW entry to an entry
                    // containing the SW info about the engine.
                    //
                    pmuEngineId = fifoGetPmuEngineFromLookupTable_HAL(&Fifo,
                                                                      pEngineLookupTable,
                                                                      engLookupTblSize,
                                                                      instanceId,
                                                                      devType,
                                                                      LW_TRUE);

                    if (pmuEngineId != PMU_ENGINE_ILWALID &&
                        !fifoIsEngineDisabled_HAL(&Fifo, pmuEngineId))
                    {
                        Fifo.pmuEngTbl[pmuEngineId].fifoEngId      = fifoEngineId;
                        Fifo.pmuEngTbl[pmuEngineId].runlistId      = runlistId;
                        Fifo.pmuEngTbl[pmuEngineId].runlistPriBase = runlistPriBase;
                        Fifo.pmuEngTbl[pmuEngineId].rlEngId        = runlistEngId;
                    }
                }
                j = 0;
                memset(deviceArray, 0,
                    LW_PTOP_DEVICE_INFO_CFG_MAX_ROWS_PER_DEVICE_STATIC * sizeof(LwU32));
            }           
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
fifoSchedulerDisable_GA10X
(
    LwU32 pmuEngId
)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
    LwU32 data;

    // disable new work from being scheduled
    data = REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_SCHED_DISABLE);
    if (data == LW_RUNLIST_SCHED_DISABLE_RUNLIST_DISABLED)
    {
        // Scheduler is already disabled, return early
        return LW_TRUE;
    }

    REG_WR32(BAR0, runlistPriBase + LW_RUNLIST_SCHED_DISABLE,
        LW_RUNLIST_SCHED_DISABLE_RUNLIST_DISABLED);

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
fifoSchedulerEnable_GA10X
(
    LwU32 pmuEngId
)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);

    REG_WR32(BAR0, runlistPriBase + LW_RUNLIST_SCHED_DISABLE,
        LW_RUNLIST_SCHED_DISABLE_RUNLIST_ENABLED);
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
fifoGetEngineStatus_GA10X
(
    LwU32  pmuEngId,
    LwU32 *pEngStatus
)
{
    LwU32  runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
    LwU16  runlistEngId   = GET_ENG_RUNLIST_ENG_ID(pmuEngId);

    *pEngStatus = REG_RD32(BAR0,
        runlistPriBase + LW_RUNLIST_ENGINE_STATUS0(runlistEngId));
}

/*!
 * Returns the PMU engine ID for LWDEC.
 *
 * Pmu engine id for LWDEC varies with chip generations.
 *
 * @param[out]  pmuEngIdLwdec
 *    PMU engine ID for LWDEC
 */
void
fifoGetLwdecPmuEngineId_GA10X
(
    LwU8 *pPmuEngIdLwdec
)
{
    *pPmuEngIdLwdec = PMU_ENGINE_BSP;
}

/*!
 * @brief Execute the Host Preempt Sequence.
 *
 * Step 5 to 7 in the following wiki which outlines host sequence for ELPG
 * https://wiki.lwpu.com/gpuhwpascal/index.php/Host/ELPG_Sequence#Power_Down_Sequence

 * @param[in] pmuEngId
 *    fifo engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 *
 * @return
 *    FLCN_ERROR   if preempt sequence is not successfull
 *    FLCN_OK      if successfull
 */
FLCN_STATUS
fifoPreemptSequence_GA10X
(
    LwU32 pmuEngId,
    LwU8  ctrlId
)
{
    // Check the idleness of runlist and associated engines
    //
    // Do not preempt runlist if its not idle. Runlist will push the remaining
    // workload after completion of preemption. That will anyway cause the
    // wake-up. So its desirable to abort here to avoid perf hit. This is
    // optimization to abort in early stage. Functionally w.r.t HW preemption
    // should work even if runlist has pending work.
    //
    if (!fifoIsRunlistAndEngIdle_HAL(&Fifo, pmuEngId))
    {
        return FLCN_ERROR;
    }

    //
    // Abort if a fifo stalling interrupt is pending. Preemption cannot be
    // guaranteed if a stalling interrupt is pending on host.
    // 
    if (fifoIsPbdmaIntrPending_HAL(&Fifo, pmuEngId))
    {
        return FLCN_ERROR;
    }
    //
    // Preempt the runlist.
    //
    if (FLCN_OK != s_fifoPreemptRunlist_GA10X(pmuEngId, ctrlId))
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * Check if PBDMA interrupts are pending on the engine.
 *
 * @paramp[in] pmuEngId - engine ID to check
 *
 * @return  LW_TRUE    - if any interrupts are pending
 *          LW_FALSE   - no interrupts are pending
 */
LwBool
fifoIsPbdmaIntrPending_GA10X
(
    LwU32 pmuEngId
)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
    LwU32 intr           = REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_INTR_0);

    //
    // As a note, this relies that the PBDMA interrupt enables in the LW_PPDBMA
    // space are set to enable, and if they're not, they won't report here. But
    // this was the behavior on previous HAL implementations as well, so this
    // intends to keep that behavior.
    //
    if (FLD_TEST_DRF(_RUNLIST, _INTR_0, _PBDMA0_INTR_TREE_0, _PENDING, intr) ||
        FLD_TEST_DRF(_RUNLIST, _INTR_0, _PBDMA1_INTR_TREE_0, _PENDING, intr) || 
        FLD_TEST_DRF(_RUNLIST, _INTR_0, _PBDMA0_INTR_TREE_1, _PENDING, intr) ||
        FLD_TEST_DRF(_RUNLIST, _INTR_0, _PBDMA1_INTR_TREE_1, _PENDING, intr))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * Check if engine is idle or busy
 *
 * @paramp[in] fifoEngId - engine ID to check
 *
 * @return  LW_TRUE    - If Engine is Idle
 *          LW_FALSE   - Engine is Busy
 */
LwBool
fifoIsEngineIdle_GA10X
(
    LwU32 fifoEngId
)
{
    LwU8   pmuEngId;
    LwU32  engStatus;
    LwU32  runlistPriBase;
    LwU16  runlistEngId;
    LwBool bIdle = LW_FALSE;

    // Find the pmuEngId based on fifoEng Id.
    pmuEngId = s_fifoGetPmuEngId_GA10X(fifoEngId);

    if (pmuEngId != PMU_ENGINE_ILWALID)
    {
        runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
        runlistEngId   = GET_ENG_RUNLIST_ENG_ID(pmuEngId);
        engStatus      = REG_RD32(BAR0, runlistPriBase +
                                  LW_RUNLIST_ENGINE_STATUS0(runlistEngId));

        if (FLD_TEST_DRF(_RUNLIST, _ENGINE_STATUS0, _CTXSW, _NOT_IN_PROGRESS, engStatus) &&
            FLD_TEST_DRF(_RUNLIST, _ENGINE_STATUS0, _ENGINE, _IDLE, engStatus))
        {
            bIdle = LW_TRUE;
        }
    }

    return bIdle;
}

/*!
 * @brief Check if runlist and all engines associated with runlist are idle
 *
 * return   LW_TRUE     Runlist and all engines associated with runlist
 *                      are idle
 *          LW_FALSE    Otherwise
 */
LwBool
fifoIsRunlistAndEngIdle_GA10X
(
    LwU32 pmuEngId
)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
    LwU32 val;

    val = REG_RD32(FECS, runlistPriBase + LW_RUNLIST_INFO);

    if (FLD_TEST_DRF(_RUNLIST, _INFO, _RUNLIST_IDLE, _TRUE, val) &&
        FLD_TEST_DRF(_RUNLIST, _INFO, _ENG_IDLE, _TRUE, val))
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/* ------------------------ Static Functions ------------------------------- */
/*!
 * @brief Preempts a runlist out of host.
 *
 * Caller is responsible for first disabling channel via fifoChannelData to
 * ensure channel does not get rescheduled.
 *
 * @param[in] pmuEngId  engine Id for runlist to be preempted
 *
 * @return  FLCN_OK    - if preemption was successful
 *          FLCN_ERROR - otherwise
 */
static FLCN_STATUS
s_fifoPreemptRunlist_GA10X
(
    LwU32 pmuEngId,
    LwU8  ctrlId
)
{
    LwU32          fifoEngId      = GET_FIFO_ENG(pmuEngId);
    LwU32          runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);
    OBJPGSTATE    *pGrState       = GET_OBJPGSTATE(ctrlId);
    FLCN_STATUS    status         = FLCN_OK;
    LwU32          holdoffMask    = 0;
    FLCN_TIMESTAMP preemptStartTimeNs;
    LwU32          regVal;
    LwU32          holdoffClientId;

    holdoffMask = FIFO_CHECK_ENGINE(GR, fifoEngId) ? pGrState->holdoffMask :
                                                     BIT(fifoEngId);

    holdoffClientId = (ctrlId == RM_PMU_LPWR_CTRL_ID_GR_RG) ?
                       LPWR_HOLDOFF_CLIENT_ID_GR_RG:
                       LPWR_HOLDOFF_CLIENT_ID_GR_PG;

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        //
        // Disable holdoff interrupts before issuing preempt.
        // This is done to ignore the holdoff interrupt
        // generated by the preempt itself.
        //
        pgEnableHoldoffIntr_HAL(&Pg, holdoffMask, LW_FALSE);
    }

    // Issue runlist preempt
    REG_WR32(BAR0, runlistPriBase + LW_RUNLIST_PREEMPT,
        DRF_DEF(_RUNLIST, _PREEMPT, _TYPE, _RUNLIST));

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Release engine holdoffs for preempt to complete
        lpwrHoldoffMaskSet_HAL(&Lpwr, holdoffClientId,
                                      LPWR_HOLDOFF_MASK_NONE);
    }

    // Start the preemption time
    osPTimerTimeNsLwrrentGet(&preemptStartTimeNs);

    //
    // Wait till the preemption is complete, or it fails with either of below -
    //  1. Lpwr task got a pending wakeup event  OR
    //  2. Preemption request timed out
    //
    do
    {
        // Return early if any of the failure conditions is met
        if ((osPTimerTimeNsElapsedGet(&preemptStartTimeNs) >= (FIFO_PREEMPT_PENDING_TIMEOUT_US * 1000)) ||
            (PMUCFG_FEATURE_ENABLED(PMU_GR_WAKEUP_PENDING) && grWakeupPending()))
        {
            status = FLCN_ERROR;
            break;
        }

        regVal = REG_RD32(BAR0, runlistPriBase + LW_RUNLIST_PREEMPT);
    }
    while (!FLD_TEST_DRF(_RUNLIST, _PREEMPT, _RUNLIST_PREEMPT_PENDING, _FALSE, regVal));

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Enable holdoff interrupts after preempt is serviced
        pgEnableHoldoffIntr_HAL(&Pg, holdoffMask, LW_TRUE);
    }

    return status;
}

/*!
 * @brief Find the pmu eng id using the fifo engId.
 *
 * @param[in] fifEngId  Fifo Engine Id
 *
 * @return  pmuEngId PMU Engine ID for given fifoEngId
 */
LwU8
s_fifoGetPmuEngId_GA10X
(
    LwU32 fifoEngId
)
{
    LwU8 pmuEngId;

    for (pmuEngId = PMU_ENGINE_ID__START; pmuEngId < PMU_ENGINE_ID__COUNT;
         pmuEngId++)
    {
        if (Fifo.pmuEngTbl[pmuEngId].fifoEngId == (LwU8)fifoEngId)
        {
            return pmuEngId;
        }
    }

    return PMU_ENGINE_ILWALID;
}
