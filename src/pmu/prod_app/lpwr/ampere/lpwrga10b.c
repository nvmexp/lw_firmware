/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwrga10b.c
 * @brief  PMU Lowpower Object
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pwr_pri.h"
#include "dev_gsp.h"
#include "hwproject.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"
#include "pmu_objlpwr.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_lpwrPrivPathFlush_GA10B(LwBool bGspFlush,
                                               LwU32  flushTimeOutNs);

/* ------------------------  Public Functions  ------------------------------ */
/*!
 *  * @brief Function to Flush the GPU Priv Path. This function flushes
* the following path
* 1. Source to SYS PRI HUB PATH (GSP to SYS PRI)
* 2. SYS PRI to destiation Chiplet.
*
* @param[in]  bGspFlush  LW_TRUE -> Flush GSP Path
*                            LW_FALSE -> Do not flush GSP Path
* @param[in]  flushTimeOutNs Timeout value in nS within which flush should
*                            be completed

* @return     LW_TRUE   -> Flush completed in time.
*             LW_FALSE  -> Flush is not completed
*/
LwBool
lpwrPrivPathFlush_GA10B
(
    LwBool bXveFlush,
    LwBool bSec2Flush,
    LwBool bGspFlush,
    LwU32  flushTimeOutNs
)
{
    FLCN_TIMESTAMP privFlushStartTimeNs;

    // Start the flush timer from PRI FENCE
    osPTimerTimeNsLwrrentGet(&privFlushStartTimeNs);

    //
    // Flush Sequence Part 1 - Flush the Priv path from
    // GSp Priv blocker to SYS_PRI_HUB
    //
    if (!s_lpwrPrivPathFlush_GA10B(bGspFlush, flushTimeOutNs))
    {
        // Priv Blocker Flush Timed out
                return LW_FALSE;
    }

    //
    // Flush Sequene Part 2 - Flush the path from SYS_PRI_HUB to
    // destination target units(Chiplets)
    //
    // Note for PRI FENCE Flush:
    // We are using the write to PRI FENCE register to flush the chiplet.
    // Writing is Blocking call here in HW. Also as per dislwssion with HW
    // folks, write is more appropriate call here reason being, writing
    // will always be broadcast write. However, reading might get colwerted
    // to unicast read and we might just flush one unicast region. So its
    // better to use the write call here.
    //

    // Flush FBP Chiplet
    REG_WR32(FECS, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0);
    REG_RD32(FECS, LW_PPRIV_FBP_FBPS_PRI_FENCE);

    // Flush SYS Chiplet
    REG_WR32(FECS, LW_PPRIV_SYS_PRI_FENCE, 0);
    REG_RD32(FECS, LW_PPRIV_SYS_PRI_FENCE);

    // Flush the GPC Chiplet
    REG_WR32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE, 0);
    REG_RD32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE);

    //
    // Check if Flushing took more time than allowed timeout value
    // If so, mark the flushing as failed.
    //
    if (osPTimerTimeNsElapsedGet(&privFlushStartTimeNs) >=
        flushTimeOutNs)
    {
        return LW_FALSE;
    }
    else
    {
        return LW_TRUE;
    }
}

/*!
 * @brief Function to map the engine to Esched Idle signal
 *
 * @param[in]     PMU engine Id
 * @param[in/out] pIdleMask pointer to idleMask array
 */
void
lpwrEngineToEschedIdleMask_GA10B
(
    LwU8   pmuEngId,
    LwU32 *pIdleMask
)
{
    // Get the RunlistId based on PMU Eng Id
    LwU8 runlistId = GET_ENG_RUNLIST(pmuEngId);

    //
    // For reference we can find the mapping of runlist and engine here:
    // https://sc-refmanuals.lwpu.com/~gpu_refmanuals/ga10b/dev_top.ref#927
    //
    switch (runlistId)
    {
        case 0:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED0,
                                       _ENABLED, pIdleMask[0]);
            break;
        }
        case 1:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED1,
                                       _ENABLED, pIdleMask[0]);
             break;
        }
        case 2:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED2,
                                       _ENABLED, pIdleMask[0]);
            break;
        }
        case 3:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED3,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
}

/* ------------------------ Private Functions  ------------------------------ */
/*!
 * @brief Function to flush the PRIV blocker paths
 *
 * @param[in]  bGspFlush  Variable to tell if GSP path flush is required
 * @param[in]  flushTimeOutNs flushtimeout value in nS
 *
 * @return     LW_TRUE   -> Flush completed
 *             LW_FALSE  -> Flush Timedout
 */
static LwBool
s_lpwrPrivPathFlush_GA10B
(
    LwBool bGspFlush,
    LwU32  flushTimeOutNs
)
{
    FLCN_TIMESTAMP privFlushStartTimeNs;
    LwBool         bStopPoll = LW_FALSE;
    LwU32          val;

    // Start time for Blocker PRIV FLUSH Path
    osPTimerTimeNsLwrrentGet(&privFlushStartTimeNs);

    if (bGspFlush)
    {
        // Flush GSP Priv Path.
        REG_WR32_STALL(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX_2, 0);

        REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_FLUSH_ADDR,
                 LW_PPWR_PMU_WSP_GP_MSGBOX_2);
        REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_FLUSH_DATA,
                 PMU_LPWR_PRIV_PATH_FLUSH_VAL);
        REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_FLUSH_CTRL,
                 LW_PGSP_PRIV_BLOCKER_FLUSH_CTRL_WR_CMD_TRIG);
    }

    do
    {
        bStopPoll = LW_TRUE;


        if (bGspFlush)
        {
            // Read GSP Blocker Flush Address
            val = REG_RD32(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX_2);
            bStopPoll = bStopPoll && ((val == PMU_LPWR_PRIV_PATH_FLUSH_VAL) ? LW_TRUE : LW_FALSE);
        }

        //
        // Check if Flushing took more time than allowed timeout value
        // If so, abort the sequence.
        //
        if (osPTimerTimeNsElapsedGet(&privFlushStartTimeNs) >=
            flushTimeOutNs)
        {
            return LW_FALSE;
        }
    }
    while (!bStopPoll);

    return LW_TRUE;
}
