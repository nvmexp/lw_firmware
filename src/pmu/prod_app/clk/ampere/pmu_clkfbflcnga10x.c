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
 * @file   pmu_clkfbflcnga10x.c
 * @brief  Hal functions for FBFLCN code in GA10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fbpa.h"
#if (PMU_PROFILE_GA10X_RISCV)
#include <lwriscv/print.h>
#endif

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"
#include "perf/cf/perf_cf.h"

/* ------------------------- Type Definitions ------------------------------- */
#define CLK_FBFLCN_MEM_TUNE_WAR_DBG_PRINT                                    0U
#define CLK_FBFLCN_MEM_TUNE_WAR_MCLK_FREQ_KHZ_THRESHOLD                 810000U

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   HAL interface to set tRRD.
 *
 * @param[in]   tFAW            Current tFAW value to be programmed in HW
 * @param[in]   mclkFreqKHz     MCLK frequency.
 *
 * @return FLCN_OK       Successfully set tRRD.
 */
FLCN_STATUS
clkFbflcnTrrdSet_GA10X
(
    LwU8    tFAW,
    LwU32   mclkFreqKHz
)
{
    if ((mclkFreqKHz > CLK_FBFLCN_MEM_TUNE_WAR_MCLK_FREQ_KHZ_THRESHOLD))
    {
        if (tFAW != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)
        {
            LwU16 trrdValLast;
            LwU32 regVal;

            trrdValLast = clkFbflcnTrrdGet_GA10X();
            if (trrdValLast != tFAW)
            {
                regVal = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
                regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _FAW, tFAW, regVal);
                REG_WR32(BAR0, LW_PFB_FBPA_CONFIG3, regVal);

    #if (PMU_PROFILE_GA10X_RISCV && CLK_FBFLCN_MEM_TUNE_WAR_DBG_PRINT)
                dbgPrintf(LEVEL_ALWAYS, "Engage : MCLK frequency = %d, tRRD Old = %d, tRRD New = %d\n.",
                    mclkFreqKHz,
                    trrdValLast,
                    DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal));
    #endif
            }
        }
        else
        {
            LwU16 trrdValLast;
            LwU16 trrdValNext;
            LwU32 regVal;

            // If static throttle enabled, use the always on tFAW value.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE_STATIC_ALWAYS_ON_OVERRIDE) &&
                perfCfMemTuneControllerStaticAlwaysOnOverrideEnabled_HAL(&PerfCf))
            {
                trrdValNext = LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ALWAYS_ON_TFAW_VAL;
            }
            else
            {
                // Use POR value
                trrdValNext = clkFbflcnTrrdValResetGet();
            }

            trrdValLast = clkFbflcnTrrdGet_GA10X();
            if (trrdValLast != trrdValNext)
            {
                regVal = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
                regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _FAW, trrdValNext, regVal);
                REG_WR32(BAR0, LW_PFB_FBPA_CONFIG3, regVal);

#if (PMU_PROFILE_GA10X_RISCV && CLK_FBFLCN_MEM_TUNE_WAR_DBG_PRINT)
            dbgPrintf(LEVEL_ALWAYS, "Disengage sandbag: MCLK frequency = %d, tRRD Old = %d, tRRD New = %d\n.",
                mclkFreqKHz,
                trrdValLast,
                DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal));
#endif
            }
        }
    }
    else
    {
        LwU16 trrdValLast;
        LwU32 regVal;

        trrdValLast = clkFbflcnTrrdGet_GA10X();
        if (trrdValLast != clkFbflcnTrrdValResetGet())
        {
            regVal = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
            regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _FAW, clkFbflcnTrrdValResetGet(), regVal);
            REG_WR32(BAR0, LW_PFB_FBPA_CONFIG3, regVal);

#if (PMU_PROFILE_GA10X_RISCV && CLK_FBFLCN_MEM_TUNE_WAR_DBG_PRINT)
            dbgPrintf(LEVEL_ALWAYS, "Disengage : MCLK frequency = %d, tRRD Old = %d, tRRD New = %d\n.",
                mclkFreqKHz,
                trrdValLast,
                DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal));
#endif
        }
    }
    return FLCN_OK;
}

/*!
 * @brief   HAL interface to get tRRD.
 *
 * @param[in]   mclkFreqKHz   MCLK frequency.
 *
 * @return FLCN_OK       Successfully programmed tRRD.
 */
LwU16
clkFbflcnTrrdGet_GA10X(void)
{
    LwU16 trrdVal;
    LwU32 regVal;

    // Trigger tRRD read.
    regVal  = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);

    trrdVal = (LwU16)DRF_VAL(_PFB, _FBPA_CONFIG3, _FAW, regVal);

#if (PMU_PROFILE_GA10X_RISCV && CLK_FBFLCN_MEM_TUNE_WAR_DBG_PRINT)
    dbgPrintf(LEVEL_ALWAYS, "tRRD Reset Value = %d\n.", trrdVal);
#endif

    return trrdVal;
}
/* ------------------------- Private Functions ------------------------------ */
