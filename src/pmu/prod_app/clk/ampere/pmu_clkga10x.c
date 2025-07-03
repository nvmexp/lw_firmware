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
 * @file   pmu_clkga10x.c
 * @brief  Hal functions for CLK code in GA10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"
#include "dev_fuse.h"
#include "pmu_objpg.h"
#include "config/g_clk_private.h"
#include "pmu_objdisp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Check whether the frequency domain program bypass path is disabled.
 *
 * @return LW_TRUE  Disabled
 * @return LW_FALSE Enabled
 */
LwBool
clkFreqDomainProgramDisabled_GA10X(void)
{
    LwU32 val = fuseRead(LW_FUSE_SPARE_BIT_1);

    //
    // We are using the fuse spare bit to indicate whether to
    // throttle the performance for mining applications. If the
    // mining throttling is enabled, we want to close all internal
    // debug paths to swich MCLK.
    //
    return FLD_TEST_DRF(_FUSE, _SPARE_BIT_1, _DATA, _ENABLE, val);
}

/*!
 * @brief   Handles removal of clock domains from Domain List when domains must be removed.
 */
FLCN_STATUS
clkScrubDomainList_GA10X(void)
{
    // Sanity check that clkFreqDomainArray has been created
    if (clkFreqDomainArray == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }
    
    if (PMU_IS_ZERO_FB_CONFIG())
    {
        clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)] = NULL;
    }
    
#ifndef CLK_SD_CHECK            // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    // Disable DISPCLK and HUBCLK if the display has been floorswept.
    if (!DISP_IS_PRESENT())
    {
        clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK)] =
        clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK)]  = NULL;
    }
#endif                          // CLK_SD_CHECK See pmu_sw/prod_app/clk/clk3/clksdcheck.c

    return FLCN_OK;
}
/* ------------------------- Private Functions ------------------------------ */
