/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifoga10b.c
 * @brief  HAL-interface for the GA10B Fifo Engine.
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

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GA10B[] =
{
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR0,    0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR1,    1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,    0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,    1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,    2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,    3},
};
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Returns the list of engines for this chip
 * @param[out]  pEngineLookupTable
 *    list of engines for this chip
 * @param[out]  engLookupTblSize
 */
void
fifoGetEngineLookupTable_GA10B
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GA10B;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GA10B);
}

/*!
 * @brief Execute the Host Preempt Sequence.
 *
 * @param[in] pmuEngId
 *    fifo engine id specfies the engine to stop scheduling for. Note that this
 *    is different from elpg engine id.
 *
 * @return
 *    FLCN_ERROR   if preempt sequence is not successfull
 *    FLCN_OK      if successfull
 */
FLCN_STATUS
fifoPreemptSequence_GA10B
(
    LwU32 pmuEngId,
    LwU8  ctrlId
)
{
    LwU32 runlistPriBase = GET_ENG_RUNLIST_PRI_BASE(pmuEngId);

    //  
    // Check the idleness of runlist and associated engines
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

    // Preempt the runlist.
    REG_WR32(FECS, runlistPriBase + LW_RUNLIST_PREEMPT,
    DRF_DEF(_RUNLIST, _PREEMPT, _TYPE, _RUNLIST));

    return FLCN_OK;
} 
