/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntrgv10x_only.c
 * @brief  PMU Hal Functions for generic GV10X clock counter functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*
 * Helper macro to get clock counter register.
 */
#define CLK_CNTR_REG_GET_GV10X(pCntr, type)                                    \
    ((pCntr)->cfgReg + (LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_##type -       \
      LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG))

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static inline FLCN_STATUS s_clkCntrEnable_GV10X(CLK_CNTR *pCntr)
    GCC_ATTRIB_ALWAYSINLINE();
/* ------------------------- Public Functions ------------------------------- */

/*!
 * INIT time version of @ref s_clkCntrEnable_GV10X
 */
FLCN_STATUS
clkCntrEnable_GV10X
(
    CLK_CNTR   *pCntr
)
{
    return s_clkCntrEnable_GV10X(pCntr);
}

/*!
 * Runtime version of @ref s_clkCntrEnable_GV10X
 */
FLCN_STATUS
clkCntrEnableRuntime_GV10X
(
    CLK_CNTR   *pCntr
)
{
    return s_clkCntrEnable_GV10X(pCntr);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Enable the given clock counter such that it starts counting its
 * configured source (pCntr->cntrSource).
 * 
 * @param[in]   pCntr  Pointer to the counter that needs to be enabled
 *
 * @return FLCN_OK                   If counter is enabled successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT If pCntr is NULL
 */
static inline FLCN_STATUS
s_clkCntrEnable_GV10X
(
    CLK_CNTR   *pCntr
)
{
    LwU32   data;

    if (pCntr == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read config register
    data = REG_RD32(FECS, CLK_CNTR_REG_GET_GV10X(pCntr, CFG));

    // Reset clock counter.
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _COUNT_UPDATE_CYCLES, _INIT, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _ASYNC_MODE, _ENABLED, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _CONTINOUS_UPDATE, _ENABLED, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _START_COUNT, _DISABLED, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _RESET, _ASSERTED, data);
    data = FLD_SET_DRF_NUM(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                            _SOURCE, pCntr->cntrSource, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GV10X(pCntr, CFG), data);

    //
    // Based on the clock counter design, it takes 16 clock cycles of the
    // "counted clock" for the counter to completely reset. Considering
    // 27MHz as the slowest clock during boot time, delay of 16/27us (~1us)
    // should be sufficient. See Bug 1953217.
    //
    OS_PTIMER_SPIN_WAIT_US(1);

    // Un-reset clock counter
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _RESET, _DEASSERTED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GV10X(pCntr, CFG), data);

    //
    // Enable clock counter.
    // Note : Need to write un-reset and enable signal in different
    // register writes as the source (register block) and destination
    // (FR counter) are on the same clock and far away from each other,
    // so the signals can not reach in the same clock cycle hence some
    // delay is required between signals.
    //
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _START_COUNT, _ENABLED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GV10X(pCntr, CFG), data);

    return FLCN_OK;
}
