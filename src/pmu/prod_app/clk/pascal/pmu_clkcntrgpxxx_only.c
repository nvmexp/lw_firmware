/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntrgpxxx_only.c
 * @brief  PMU Hal Functions for generic PASCAL clock counter functionality.
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
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Get the clock counter information for the specified
 * clock domain @pCntr->clkDomain and partition index @pCntr->partIdx.
 *
 * @param[in/out] pCntr
 *              Pointer to the requested clock counter object.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *              If the passed clock domain is not supported.
 * @return FLCN_OK
 *              Succesfully updated the counter info.
 */
FLCN_STATUS
clkCntrDomInfoGet_GPXXX
(
    CLK_CNTR *pCntr
)
{
    //
    // Scaling factor because of distribution mode is required only if
    // we are counting 2x clock value which is the case for all clock
    // domains except MCLK in this function.
    //
    pCntr->scalingFactor = ((Clk.clkDist1xDomMask & pCntr->clkDomain) ? 2 : 1);

    switch (pCntr->clkDomain)
    {
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_SYS2CLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_SYS2CLK:
        {
            pCntr->cfgReg     = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCSYSNAFLL_CFG;
            //
            // There is a HW bug in GP100 that _SYS2CLK is connected to XTAL and hence
            // need to point the source to _SYSNAFLL_CLKOUT though it'll not work for
            // PLL/BYPASS. Bug 1677020.
            //
            pCntr->cntrSource =
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_SYS2CLK_WAR_BUG_1677020))
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCSYSNAFLL_CFG_SOURCE_SYSNAFLL_CLKOUT;
#else
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCSYSNAFLL_CFG_SOURCE_SYS2CLK;
#endif
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_LTC2CLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_LTC2CLK:
        {
            pCntr->cfgReg     = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCLTCNAFLL_CFG;
            pCntr->cntrSource =
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCLTCNAFLL_CFG_SOURCE_LTC2CLK;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_XBAR2CLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_XBAR2CLK:
        {
            pCntr->cfgReg     = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCXBARNAFLL_CFG;
            pCntr->cntrSource =
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCXBARNAFLL_CFG_SOURCE_XBAR2CLK;
            break;
        }
#endif
        case LW2080_CTRL_CLK_DOMAIN_GPC2CLK:
        {
            if (pCntr->partIdx == LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED)
            {
                pCntr->cfgReg = LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG;
                pCntr->cntrSource =
                    LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPC2CLK;
            }
            else
            {
                PMU_HALT_COND(pCntr->partIdx < LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX);
                pCntr->cfgReg = LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG(pCntr->partIdx);
                pCntr->cntrSource =
                    LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPC2CLK;
            }
            break;
        }
        case LW2080_CTRL_CLK_DOMAIN_MCLK:
        {
            pCntr->cfgReg     = LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG;
            pCntr->cntrSource =
                LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG_SOURCE_DRAMDIV2_REC_CLK1_PA0_DUPLICATE;
            pCntr->scalingFactor = 2;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
