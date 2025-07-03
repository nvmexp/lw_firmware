/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 - 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwrga10x.c
 * @brief  PMU Lowpower Object
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sysc.h"
#include "dev_pwr_pri.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_lw_xve.h"
#endif
#include "hwproject.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"
#include "pmu/pmumailboxid.h"
#include "pmu_objlpwr.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_lpwrIsGpcChipletAccessible_GA10X(void);
static LwBool s_lpwrPrivPathFlush_GA10X         (LwBool bXveFlush,
                                                 LwBool bSec2Flush,
                                                 LwBool bGspFlush,
                                                 LwU32  flushTimeOutNs);

/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Function to return the GPU Engine's Idle Mask
 *
 * @param[in] pIdleMask Pointer to the array for idleMask
 *
 * This function will return the various GPU Engine's Idle
 * mask. We can use these idle mask to check if corresponding
 * GPU engine's are idle or not.
 */
void
lpwrGpuIdleMaskGet_GA10X
(
    LwU32 *pIdleMask
)
{
    LwU32 im0 = 0;
    LwU32 im1 = 0;
    LwU32 im2 = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);
    }

    pIdleMask[0] = im0;
    pIdleMask[1] = im1;
    pIdleMask[2] = im2;
}

/*!
 * @brief Function to Flush the GPU Priv Path. This function flushes
 * the following path
 * 1. Source to SYS PRI HUB PATH (XVE to SYS PRI, SEC2 to SYS PRI and
 *    GSP to SYS PRI
 * 2. SYS PRI to destiation Chiplet.
 *
 * @param[in]  bXveFlush  LW_TRUE -> Flush XVE Path
 *                        LW_FALSE -> Do not flush XVE Path
 * @param[in]  bSec2Flush LW_TRUE -> Flush SEC2 Path
 *                            LW_FALSE -> Do not flush SEC2 Path
 * @param[in]  bGspFlush  LW_TRUE -> Flush GSP Path
 *                            LW_FALSE -> Do not flush GSP Path
 * @param[in]  flushTimeOutNs Timeout value in nS within which flush should
 *                            be completed

 * @return     LW_TRUE   -> Flush completed in time.
 *             LW_FALSE  -> Flush is not completed
 */
LwBool
lpwrPrivPathFlush_GA10X
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
    // XVE/GSp/SEC Priv blocker to SYS_PRI_HUB
    //

    if (!s_lpwrPrivPathFlush_GA10X(bXveFlush, bSec2Flush, bGspFlush,
                                   flushTimeOutNs))
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

    //
    // If SYSC and SYSB Cluster are present we need to
    // flush them as well. On GA10X, there is no SYSB
    // cluster which is confirmed by LW_SCAL_LITTER_NUM_SYSB == 0.
    // So we are not adding the flush for SYSB cluster.
    //
    if (LW_SCAL_LITTER_NUM_SYSC != 0)
    {
        REG_WR32(FECS, LW_PPRIV_SYSC_PRI_FENCE, 0);
        REG_RD32(FECS, LW_PPRIV_SYSC_PRI_FENCE);
    }

    // Flush the GPC Chiplet only if its accessbile
    if (s_lpwrIsGpcChipletAccessible_GA10X())
    {
        REG_WR32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE, 0);
        REG_RD32(FECS, LW_PPRIV_GPC_GPCS_PRI_FENCE);
    }

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
lpwrEngineToEschedIdleMask_GA10X
(
    LwU8   pmuEngId,
    LwU32 *pIdleMask
)
{
    // Get the RunlistId based on PMU Eng Id
    LwU8 runlistId = GET_ENG_RUNLIST(pmuEngId);

    //
    // For reference we can find the mapping of runlist and engine here:
    // https://sc-refmanuals.lwpu.com/~gpu_refmanuals/ga102/dev_top.ref#927
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
        case 4:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED4,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 5:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED5,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 6:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED6,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 7:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED7,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 8:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED8,
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
 * @param[in]  bXveFlush  Variable to tell if XVE path flush is required
 * @param[in]  bSec2Flush Variable to tell if SEC2 path flush is required
 * @param[in]  bGspFlush  Variable to tell if GSP path flush is required
 * @param[in]  flushTimeOutNs flushtimeout value in nS
 *
 * @return     LW_TRUE   -> Flush completed
 *             LW_FALSE  -> Flush Timedout
 */
static LwBool
s_lpwrPrivPathFlush_GA10X
(
    LwBool bXveFlush,
    LwBool bSec2Flush,
    LwBool bGspFlush,
    LwU32  flushTimeOutNs
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    FLCN_TIMESTAMP privFlushStartTimeNs;
    LwBool         bStopPoll = LW_FALSE;
    LwU32          val;

    // Start time for Blocker PRIV FLUSH Path
    osPTimerTimeNsLwrrentGet(&privFlushStartTimeNs);

    if (bXveFlush)
    {
        //
        // Once the flushing of PRIV path is completed,
        // Read the LW_XVE_DBG0 register to ensure no
        // outstanding non-posted read transactions are
        // present i.e
        // (LW_XVE_DBG0_OUTSTANDING_DOWNSTREAM_READ==0)
        //
        if (!FLD_TEST_DRF_NUM(_XVE, _DBG0, _OUTSTANDING_DOWNSTREAM_READ,
                              0, REG_RD32(FECS_CFG, LW_XVE_DBG0)))
        {
            return LW_FALSE;
        }

        // Flush XVE Priv Path
        REG_WR32_STALL(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX, 0);

        REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_WR_ADDR,
                 LW_PPWR_PMU_WSP_GP_MSGBOX);
        REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_WR_DATA,
                 PMU_LPWR_PRIV_PATH_FLUSH_VAL);
        REG_WR32(FECS_CFG, LW_XVE_BAR_BLOCKER_FLUSH_CTRL,
                 LW_XVE_BAR_BLOCKER_FLUSH_CTRL_WR_CMD_TRIGGER);
    }

    if (bSec2Flush)
    {
        // Flush SEC2 Priv Path
        REG_WR32_STALL(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX_1, 0);

        REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_FLUSH_ADDR,
                 LW_PPWR_PMU_WSP_GP_MSGBOX_1);
        REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_FLUSH_DATA,
                 PMU_LPWR_PRIV_PATH_FLUSH_VAL);
        REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_FLUSH_CTRL,
                 LW_PSEC_PRIV_BLOCKER_FLUSH_CTRL_WR_CMD_TRIG);
    }

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

        if (bXveFlush)
        {
            // Read XVE Blocker Flush Address
            val = REG_RD32(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX);
            bStopPoll = bStopPoll && ((val == PMU_LPWR_PRIV_PATH_FLUSH_VAL) ?
                                                         LW_TRUE : LW_FALSE);
        }

        if (bSec2Flush)
        {
            // Read SEC2 Blocker Flush Address
            val = REG_RD32(CSB, LW_CPWR_PMU_WSP_GP_MSGBOX_1);
            bStopPoll = bStopPoll && ((val == PMU_LPWR_PRIV_PATH_FLUSH_VAL) ? LW_TRUE : LW_FALSE);
        }

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

#endif  // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    return LW_TRUE;
}

/*!
 * @brief Function to check if Graphics chiplet is accessible or not
 *
 * @return     LW_TRUE   -> Accessible
 *             LW_FALSE  -> Not Accessible
 */
static LwBool
s_lpwrIsGpcChipletAccessible_GA10X(void)
{
    //
    // If any of the Graphics Lowpower feature which make the
    // graphics chiplet in accessible are engaged e.g.
    // GR-RG, we should not access the Graphics
    // region.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)         &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG) &&
        !PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        return LW_FALSE;
    }
    else
    {
        return LW_TRUE;
    }
}
