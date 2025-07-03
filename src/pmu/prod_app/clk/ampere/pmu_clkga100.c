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
 * @file   pmu_clkga100.c
 * @brief  Hal functions for CLK code in GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "clk3/clk3.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"
#include "dev_fuse.h"
#include "pmu_objpg.h"
#include "dev_fuse.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Handles removal of clock domains from Domain List when domains must be removed.
 * 
 */
FLCN_STATUS
clkScrubDomainList_GA100(void)
{
    // Sanity check that clkFreqDomainArray has been created
    if (clkFreqDomainArray == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    //
    // ZERO_FB Configs cannot read from the FBIO PRIs, so we simply set the pointer to MCLK in the clkFreqDomain Array
    // To NULL to prevent any programing/reading of MCLK
    //
    if (PMU_IS_ZERO_FB_CONFIG())
    {
        clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)] = NULL;
    }
    
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
