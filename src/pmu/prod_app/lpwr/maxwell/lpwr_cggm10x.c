/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cggm10x.c
 * @brief  Lpwr Clock Gating related functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */
/*!
 * @Brief : Initialize LPWR Clock Gating Structure.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NO_FREE_MEM    No free space in DMEM or in DMEM overlay.
 */
FLCN_STATUS
lpwrCgFrameworkPostInit_GM10X(void)
{
    FLCN_STATUS status  = FLCN_OK;
    LPWR_CG    *pLpwrCg = Lpwr.pLpwrCg;
    LwU32       regVal;

    //
    // Allocate space for LPWR CG. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pLpwrCg == NULL)
    {
        pLpwrCg = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, LPWR_CG);
        if (pLpwrCg == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrCgFrameworkPostInit_GM10X_exit;
        }

        Lpwr.pLpwrCg = pLpwrCg;
    }

    // Set the respective bit from the mask based on capability bit.
    regVal = REG_RD32(FECS, LW_PTRIM_SYS_PHYSICAL_CLKS_0);

    if (FLD_TEST_DRF(_PTRIM_SYS, _PHYSICAL_CLKS_0, _GPC2CLK_CAPABILITY,
                     _PRESENT, regVal))
    {
        pLpwrCg->clkDomainSupportMask |= CLK_DOMAIN_MASK_GPC;
    }

    if (FLD_TEST_DRF(_PTRIM_SYS, _PHYSICAL_CLKS_0, _XBAR2CLK_CAPABILITY,
                     _PRESENT, regVal))
    {
        pLpwrCg->clkDomainSupportMask |= CLK_DOMAIN_MASK_XBAR;
    }

    //
    // CLK_DOMAIN_MASK_LTC is defined as 0x0 on GPU which does not support
    // LTC clock.
    //
#if CLK_DOMAIN_MASK_LTC
    if (FLD_TEST_DRF(_PTRIM_SYS, _PHYSICAL_CLKS_0, _LTC2CLK_CAPABILITY,
                     _PRESENT, regVal))
    {
        pLpwrCg->clkDomainSupportMask |= CLK_DOMAIN_MASK_LTC;
    }
#endif // CLK_DOMAIN_MASK_LTC

lpwrCgFrameworkPostInit_GM10X_exit:

    return status;
}
