/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkgf10x.c
 * @brief  PMU Hal Functions for generic GF10X clock counter functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define CLK_CNTR_ARRAY_SIZE_MAX_GMXXX   1

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initializes all SW state associated with the Clock Counter features.
 */
void
clkCntrInitSw_GMXXX()
{
    static CLK_CNTR ClkCntrArray_GMXXX[CLK_CNTR_ARRAY_SIZE_MAX_GMXXX] = {{ 0 }};

    // Initialize CLK_CNTRS SW state.
    Clk.cntrs.pCntr         = ClkCntrArray_GMXXX;
    Clk.cntrs.numCntrs      = CLK_CNTR_ARRAY_SIZE_MAX_GMXXX;
    Clk.cntrs.hwCntrSize    = clkCntrWidthGet_HAL(&Clk);

    // Initialize per-clock counter SW state.
    Clk.cntrs.pCntr[0].clkDomain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;
    Clk.cntrs.pCntr[0].partIdx   = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;
    Clk.cntrs.pCntr[0].scalingFactor = 2;
}

/*!
 * Returns the bit-width of the requested HW clock counter.
 *
 * @return Counter bit-width.
 */
LwU8
clkCntrWidthGet_GMXXX()
{
    return (LwU8)DRF_SIZE(LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT_VALUE);
}

/*!
 * Initialize the HW state of supported clock counters.
 */
void
clkCntrInitHw_GMXXX()
{
    // Reset the counter to 0x0 as base from which to start counting.
    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG,
        DRF_NUM(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _NOOFIPCLKS, 0x3FFF)    |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _WRITE_EN, _DEASSERTED) |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _ENABLE, _DEASSERTED)   |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _RESET, _ASSERTED)      |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _SOURCE, _GPCCLK));
}

/*!
 * Reads current HW counter value for the requested counter
 *
 * Has side-effect of restarting specified counter if counter only runs in
 * triggered mode.
 *
 * @param[in] pCntr     Pointer to the requested clock counter object.
 *
 * @return Current HW counter value.
 */
LwU64
clkCntrRead_GMXXX
(
    CLK_CNTR   *pCntr
)
{
    LwU64 cntr;

    // 1. Restart the counter.
    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG,
        DRF_NUM(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _NOOFIPCLKS, 0x3FFF)  |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _WRITE_EN, _ASSERTED) |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _ENABLE, _ASSERTED)   |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _RESET, _DEASSERTED)  |
        DRF_DEF(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG, _SOURCE, _GPCCLK));

    // 2. Read the latest value
    cntr = DRF_VAL(_PTRIM, _GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT, _VALUE,
            REG_RD32(FECS, LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT));

    return cntr;
}
/* ------------------------- Private Functions ------------------------------ */
