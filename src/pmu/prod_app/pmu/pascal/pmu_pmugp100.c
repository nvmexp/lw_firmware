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
 * @file    pmu_pmugp100.c
 *          PMU HAL Functions for GP100 and later GPUs
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_oslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "dev_pwr_csb.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Define maxuimum time to wait for FB to 'flush'.  Set to huge value of 1 sec as
 * delays can be huge on system like the IBM P9, so want to give this every
 * possible chance of completing.
 */
#define FB_FLUSH_TIMEMOUT_NS    1000000000

/*!
 * @brief   Used to compile out FB_FLUSH code profiling
 *
 * @note    TODO: Code to be fully deleted after early 2020.
 */
#define FB_FLUSH_DEBUGGING      0

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
#if FB_FLUSH_DEBUGGING
/*
 * Data area for fbFlush statistics.
 */
LwU32   fbFlushStatsMaxTime     = 0;
LwU32   fbFlushStatsNumSamples  = 0;
LwU64   fbFlushStatsTotal       = 0;
#endif // FB_FLUSH_DEBUGGING

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS)
/*!
 * @brief   Cache the Vbios Security Bypass Register (VSBR).
 */
void
pmuPreInitVbiosSelwrityBypassRegisterCache_GP100(void)
{
    PmuVSBCache = REG_RD32(CSB, LW_CPWR_THERM_SELWRE_WR_SCRATCH(0));
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS)

/*!
 * @brief   Using FBIF_CTL_FLUSH, flush all DMA writes to FB.
 *
 * @return      FLCN_OK             Success.
 * @return      FLCN_ERR_TIMEOUT    Timeout on FB flush.
 */
FLCN_STATUS
pmuFbFlush_GP100(void)
{
#if FB_FLUSH_DEBUGGING
    FLCN_TIMESTAMP  time;
    LwU32           elapsedTime = 0;
#endif // FB_FLUSH_DEBUGGING
    FLCN_STATUS     status      = FLCN_OK;

    appTaskCriticalEnter();
    {
        //
        // Flush the write to FB:
        //  - set LW_CPWR_FBIF_CTL_FLUSH_SET
        //  - poll unit LW_CPWR_FBIF_CTL_FLUSH_SET goes off or timeout
        //
        REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FBIF_CTL, _FLUSH, _SET);
#if FB_FLUSH_DEBUGGING
        osPTimerTimeNsLwrrentGet(&time);
#endif // FB_FLUSH_DEBUGGING
        if (!PMU_REG32_POLL_NS(
                 LW_CPWR_FBIF_CTL,
                 DRF_SHIFTMASK(LW_CPWR_FBIF_CTL_FLUSH),
                 DRF_DEF(_CPWR, _FBIF_CTL, _FLUSH, _CLEAR),
                 FB_FLUSH_TIMEMOUT_NS,
                 PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_TIMEOUT;
        }

#if FB_FLUSH_DEBUGGING
        // Collect Statistics
        elapsedTime         = osPTimerTimeNsElapsedGet(&time);
        fbFlushStatsMaxTime = LW_MAX(fbFlushStatsMaxTime, elapsedTime);
        fbFlushStatsTotal  += elapsedTime;
        fbFlushStatsNumSamples++;
#endif // FB_FLUSH_DEBUGGING
    }
    appTaskCriticalExit();

    return status;
}
