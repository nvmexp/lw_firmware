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
  * @file   lpwr_seqad10x.c
  * @brief  Implements LPWR Sequencer for Ampere
  */

  /* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * RAM Staggering logic runs on utilsclk which is 108Mhz ( i.e 9.26 ns)
 * To honor the RAM spec, the RAM sequencer takes 27.78 ns (= 3*9.26 ns )
 * to toggle each RAM_RET_EN and RAM_SLEEP_EN bits
 *
 * Max Poll time can be 250nS according to callwlations i.e.
 * (4 Sleep Toggle (SLEEP_EN) + 1 RET_EN Toggle + 3 Wait Cycles) * Sequence time for each step (27.78nS))
 *  8 * 27.78 = 222.24nS
 *  Adding some margin and making it 250nS
 */
#define LPWR_SEQ_FG_RPPG_SEQ_POLL_TIME_NS  250

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Function to initialize the SRAM Sequencer for FG_RPPG
 */
void
lpwrSeqSramFgRppgInit_AD10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 val = 0;

    // Program the _PROD Values for Thresholds
    val = REG_RD32(CSB, LW_CPWR_PMU_THRESHOLD_FG_RPPG);

    val = FLD_SET_DRF(_CPWR, _PMU_THRESHOLD_FG_RPPG, _ENTRY, __PROD, val);
    val = FLD_SET_DRF(_CPWR, _PMU_THRESHOLD_FG_RPPG, _EXIT, __PROD, val);

    REG_WR32(CSB, LW_CPWR_PMU_THRESHOLD_FG_RPPG, val);

    // Enable the FG-RPPG by default.
    lpwrSeqSramFgRppgEnable_HAL(&Lpwr);

#endif //(!PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS)
}

/*!
 * @brief Disable the FG-RPPG
 *
 *  @return    FLCN_OK   FG_RPPG is disabled
 *             FLCN_ERROR  otherwise

 */
FLCN_STATUS
lpwrSeqSramFgRppgDisable_AD10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_FG_RPPG);
    val = FLD_SET_DRF(_CPWR, _PMU_FG_RPPG, _EN, _DIS, val);
    REG_WR32(CSB, LW_CPWR_PMU_FG_RPPG, val);

    // Wait for the GR FSM to report as PWR_ON.
    if (!PMU_REG32_POLL_NS(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR_FSM),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR_FSM, _POWER_ON),
                        LPWR_SEQ_FG_RPPG_SEQ_POLL_TIME_NS,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_BREAKPOINT();

        return FLCN_ERROR;
    }

#endif // (!PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS)
    return FLCN_OK;
}

/*!
 * @brief Disable the FG-RPPG
 *
 */
void
lpwrSeqSramFgRppgEnable_AD10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_FG_RPPG);
    val = FLD_SET_DRF(_CPWR, _PMU_FG_RPPG, _EN, _EN, val);
    REG_WR32(CSB, LW_CPWR_PMU_FG_RPPG, val);

#endif // (!PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS)
}

/* ------------------------- Private Functions ------------------------------ */
