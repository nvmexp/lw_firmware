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
 * @file   pmu_fifogp10x.c
 * @brief  HAL-interface for the GP10X Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GP10X[] =
{
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,   0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,  2},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,  3},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE4,  4},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE5,  5},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0, 0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC1, 1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC2, 2},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,  1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWDEC,    PMU_ENGINE_BSP,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_SEC,      PMU_ENGINE_SEC2, 0},
};

/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
static FLCN_STATUS s_fifoPreemptRunlist_GP10X(LwU32 fifoEngId,LwU32 runlistId, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libFifo", "s_fifoPreemptRunlist_GP10X");

/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Returns the list of engines for this chip
 * @param[out]  pmuEngineTable
 *    list of engines for this chip
 * @param[out]  engLookupTblSize
 */
void
fifoGetEngineLookupTable_GP10X
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GP10X;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GP10X);
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
fifoGetLwdecPmuEngineId_GP10X(LwU8 *pPmuEngIdLwdec)
{
    *pPmuEngIdLwdec = PMU_ENGINE_BSP;
}

/*!
 * Check if engine is disabled (e.g. floorswept).
 *
 * @return  Nonzero    - if disabled or floorswept
 *          Zero       - otherwise
 */
// TODO-Yogesh: Remove this #if from code.
#if (PMU_PROFILE_GP10X || PMU_PROFILE_GV10X)
LwBool
fifoIsEngineDisabled_GP10X
(
    LwU8 pmuEngineId
)
{
    LwU32 fuse;

    switch (pmuEngineId)
    {
        case PMU_ENGINE_GR:
        {
            // GR as a whole can never be floorswept.
            return LW_FALSE;
        }
        case PMU_ENGINE_VP:
        case PMU_ENGINE_PPP:
        {
            // VP and PPP have been removed and replaced by LWDEC for Maxwell.
            return LW_TRUE;
        }
        case PMU_ENGINE_BSP:
        {
            //
            // VP, PPP, and BSP are replaced by LWDEC in Maxwell
            // We are reusing the BSP object for LWDEC in Maxwell though
            //
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWDEC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_LWDEC, _DATA, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC0:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 0, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC1:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 1, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 2, _DISABLE, fuse);
        }
        case PMU_ENGINE_SEC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_SEC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_SEC, _DATA, _DISABLE, fuse);
        }
        //
        // This fuse is removed from GP10X+, CE will no longer be disabled in HW
        // See bug 200025697
        //
        case PMU_ENGINE_CE0:
        case PMU_ENGINE_CE1:
        case PMU_ENGINE_CE2:
        case PMU_ENGINE_CE3:
        case PMU_ENGINE_CE4:
        case PMU_ENGINE_CE5:
        {
            return LW_FALSE;
        }
    }
    return LW_FALSE;
}
#endif // (PMU_PROFILE_GP10X || PMU_PROFILE_GV10X)

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
fifoPreemptSequence_GP10X
(
    LwU32 pmuEngId,
    LwU8  ctrlId
)
{
    LwU32  fifoEngId       = GET_FIFO_ENG(pmuEngId);
    LwU16  pbdmaIdMask     = GET_ENG_PBDMA_MASK(pmuEngId);
    LwU32  runlistId       = GET_ENG_RUNLIST(pmuEngId);

    //
    // Check the idleness of runlist and associated engines
    //
    // Do not preempt runlist if its not idle. Runlist will push the remaining
    // workload after completion of preemption. That will anyway cause the
    // wake-up. So its desirable to abort here to avoid perf hit. This is
    // optimization to abort in early stage. Functionally w.r.t HW preemption
    // should work even if runlist has pending work.
    //
    if (!fifoIsRunlistAndEngIdle_HAL(&Fifo, runlistId))
    {
        return FLCN_ERROR;
    }

    //
    // Abort if a fifo stalling interrupt is pending. Preemption cannot be
    // guaranteed if a stalling interrupt is pending on host.
    // 
    if (fifoIsStallingIntrPending_HAL(&Fifo, pbdmaIdMask))
    {
        return FLCN_ERROR;
    }

    //
    // Preempt the runlist.
    //
    if (FLCN_OK != s_fifoPreemptRunlist_GP10X(fifoEngId, runlistId, ctrlId))
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/* ------------------------ Static Functions ------------------------------- */
/*!
 * @brief Preempts a runlist out of host.
 *
 * Caller is responsible for first disabling channel via fifoChannelData to
 * ensure channel does not get rescheduled.
 *
 * @param[in] fifoEngId  engine Id for runlist to be preempted
 *
 * @return  FLCN_OK    - if preemption was successful
 *          FLCN_ERROR - otherwise
 */
static FLCN_STATUS
s_fifoPreemptRunlist_GP10X
(
    LwU32  fifoEngId,
    LwU32  runlistId,
    LwU8   ctrlId
)
{
    OBJPGSTATE *pGrState    = GET_OBJPGSTATE(ctrlId);
    FLCN_STATUS status      = FLCN_OK;
    LwU32       holdoffMask = 0;
    LwU32       holdoffClientId;

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
    REG_WR32(BAR0, LW_PFIFO_RUNLIST_PREEMPT, BIT(runlistId));

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Release engine holdoffs for preempt to complete
        lpwrHoldoffMaskSet_HAL(&Lpwr, holdoffClientId,
                                      LPWR_HOLDOFF_MASK_NONE);
    }

    if (!PMU_REG32_POLL_US(
            LW_PFIFO_RUNLIST_PREEMPT,
            BIT(runlistId),
            LW_PFIFO_RUNLIST_PREEMPT_DONE,
            FIFO_PREEMPT_PENDING_TIMEOUT_US,
            PMU_REG_POLL_MODE_BAR0_EQ))
    {
        status = FLCN_ERROR;
    }

    if (!LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, NEW_HOST_SEQ))
    {
        // Enable holdoff interrupts after preempt is serviced
        pgEnableHoldoffIntr_HAL(&Pg, holdoffMask, LW_TRUE);
    }

    return status;
}
