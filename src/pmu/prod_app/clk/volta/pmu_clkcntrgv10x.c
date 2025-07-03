/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntrgv10x.c
 * @brief  PMU Hal Functions for generic GV100+ clock counter functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_fbpa.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objfuse.h"
#include "pmu_objdisp.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*
 * Special macro to define maximum number of supported clock counters.
 * {SYS, LWD, XBAR, HOST, PWR, HUB, UTILS, DISP, 4 VCLKs, MCLK,
 *  Bcast GPC, 6 unicast GPCs}
 */
#define CLK_CNTR_ARRAY_SIZE_MAX_GV10X              20

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Returns pointer to the static clock counter array.
 */
CLK_CNTR *
clkCntrArrayGet_GV10X()
{
    static CLK_CNTR ClkCntrArray_GV10X[CLK_CNTR_ARRAY_SIZE_MAX_GV10X] = {{ 0 }};
    return ClkCntrArray_GV10X;
}

/*!
 * Get the mask of supported clock domains
 *
 * @return  Mask of supported clock doamins.
 *
 */
LwU32
clkCntrDomMaskGet_GV10X()
{
    LwU32  clkDomMask;

    //
    // Make sure to update CLK_CNTR_ARRAY_SIZE_MAX_GV10X if
    // number of clock counters are changing.
    //
    clkDomMask =
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_SYS2CLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_SYS     |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_LWDCLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_LWD     |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_XBAR2CLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_XBAR    |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_2_HOSTCLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_HOST    |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_PWRCLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_PWR     |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_UTILSCLK_CAPABILITY_INIT != 0)
                    CLK_DOMAIN_MASK_UTILS   |
#endif
                    CLK_DOMAIN_MASK_GPC;

    if (DISP_IS_PRESENT())
    {
        // Handle display capabilities only if Display is enabled
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_DISPCLK_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_DISP;
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK0_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_VCLK0;
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK1_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_VCLK1;
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK2_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_VCLK2;
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK3_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_VCLK3;
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_HUB2CLK_CAPABILITY_INIT != 0)
        clkDomMask |= CLK_DOMAIN_MASK_HUB;
#endif
    }

    // There is always MCLK
    clkDomMask |= LW2080_CTRL_CLK_DOMAIN_MCLK;

    return clkDomMask;
}

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
clkCntrDomInfoGet_GV10X
(
    CLK_CNTR *pCntr
)
{
    switch (pCntr->clkDomain)
    {
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_SYS2CLK_CAPABILITY_INIT != 0)
        //
        // Arun's proposal from an email dislwssion:
        // It seems in the previous chips, we used to count the ELCG'ed version
        // for GPCCLK, while the SYSCLK was reported at its full rate/ungated
        // value.
        // Hence to fix the reporting issue, while maintaining consistency with
        // previous chips, we can do the following:
        // *    For GPCCLK(i), continue to use the ELCG'ed version of the clocks
        //    as the source of the FR counters
        // *    For SYSCLK, change the source for FR counters to the NOEG version.
        //
        // Since the percentage of the Dynamic power drawn by units running on
        // SYSCLK is negligible/very small as compared to that of GPCCLK,
        // this shouldn't make any impact to any of the power controllers or
        // power models.
        //
        case LW2080_CTRL_CLK_DOMAIN_SYS2CLK:
        case LW2080_CTRL_CLK_DOMAIN_SYSCLK:
        {
            pCntr->cfgReg        = LW_PTRIM_SYS_FR_CLK_CNTR_SYSCLK_CFG;
            pCntr->cntrSource    =
                LW_PTRIM_SYS_FR_CLK_CNTR_SYSCLK_CFG_SOURCE_SYSCLK_NOEG_CTS_PROBE;
            pCntr->scalingFactor = (PMUCFG_FEATURE_ENABLED(
                        PMU_CLK_DOMAINS_1X_SUPPORTED) ? 1 : 2);
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_LWDCLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_LWDCLK:
        {
            pCntr->cfgReg        = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_LWDCLK_CFG;
            pCntr->cntrSource    =
#ifdef LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_LWDCLK_CFG_SOURCE_LWDCLK_NOBG_CTS_PROBE
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_LWDCLK_CFG_SOURCE_LWDCLK_NOBG_CTS_PROBE;
#else
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_LWDCLK_CFG_SOURCE_LWDCLK_NOBG;
#endif
            pCntr->scalingFactor = (PMUCFG_FEATURE_ENABLED(
                        PMU_CLK_DOMAINS_1X_SUPPORTED) ? 1 : 2);
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_XBAR2CLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_XBAR2CLK:
        case LW2080_CTRL_CLK_DOMAIN_XBARCLK:
        {
            pCntr->cfgReg   = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_XBARCLK_CFG;
            pCntr->cntrSource    =
#ifdef LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_XBARCLK_CFG_SOURCE_XBARCLK_NOBG
                      LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_XBARCLK_CFG_SOURCE_XBARCLK_NOBG;
#else
                      LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_XBARCLK_CFG_SOURCE_XBARCLK_NOBG_DUPLICATE;
#endif
            pCntr->scalingFactor = (PMUCFG_FEATURE_ENABLED(
                        PMU_CLK_DOMAINS_1X_SUPPORTED) ? 1 : 2);
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_2_HOSTCLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_HOSTCLK:
        {
#ifdef LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCHOSTNAFLL_FREQ_PID_CFG_SOURCE_HOSTCLK_NOBG
            pCntr->cfgReg   = LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCHOSTNAFLL_FREQ_PID_CFG;
            pCntr->cntrSource    =
                LW_PTRIM_SYS_NAFLL_FR_CLK_CNTR_NCHOSTNAFLL_FREQ_PID_CFG_SOURCE_HOSTCLK_NOBG;
#else
            pCntr->cfgReg   = LW_PTRIM_SYS_FR_CLK_CNTR_NCOSMCORE_CFG;
            pCntr->cntrSource    =
                LW_PTRIM_SYS_FR_CLK_CNTR_NCOSMCORE_CFG_SOURCE_HOSTCLK;
#endif
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_PWRCLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_PWRCLK:
        {
            pCntr->cfgReg   = LW_PTRIM_SYS_FR_CLK_CNTR_MISCCLKS_CFG;
            pCntr->cntrSource    =
                LW_PTRIM_SYS_FR_CLK_CNTR_MISCCLKS_CFG_SOURCE_PWRCLK_DIV32_FTM;
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_HUB2CLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_HUB2CLK:
        case LW2080_CTRL_CLK_DOMAIN_HUBCLK:
        {
#ifdef LW_PVTRIM_SYS_FR_CLK_CNTR_HUBCLK_CFG
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_HUBCLK_CFG;
            pCntr->cntrSource    =
                    LW_PVTRIM_SYS_FR_CLK_CNTR_HUBCLK_CFG_SOURCE_HUB2CLK_DIV32_FTM;
#else
            pCntr->cfgReg   = LW_PTRIM_SYS_FR_CLK_CNTR_CORECLKS_CFG;
            pCntr->cntrSource    =
                LW_PTRIM_SYS_FR_CLK_CNTR_CORECLKS_CFG_SOURCE_HUB2CLK_DIV32_FTM;
#endif
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_UTILSCLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_UTILSCLK:
        {
            pCntr->cfgReg   = LW_PTRIM_SYS_FR_CLK_CNTR_UTILS_XTAL_CFG;
            pCntr->cntrSource    =
#ifdef LW_PTRIM_SYS_FR_CLK_CNTR_UTILS_XTAL_CFG_SOURCE_UTILSCLK_DIV32_FTM
                LW_PTRIM_SYS_FR_CLK_CNTR_UTILS_XTAL_CFG_SOURCE_UTILSCLK_DIV32_FTM;
#else
                LW_PTRIM_SYS_FR_CLK_CNTR_UTILS_XTAL_CFG_SOURCE_UTILSCLK;
#endif
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_DISPCLK_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_DISPCLK:
        {
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_DISPCLK_CFG;
            pCntr->cntrSource    =
#ifdef LW_PVTRIM_SYS_FR_CLK_CNTR_DISPCLK_CFG_SOURCE_DISPPLL_CTS_PROBE
                LW_PVTRIM_SYS_FR_CLK_CNTR_DISPCLK_CFG_SOURCE_DISPPLL_CTS_PROBE;
#else
                LW_PVTRIM_SYS_FR_CLK_CNTR_DISPCLK_CFG_SOURCE_DISPPLL_O;
#endif
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK0_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_VCLK0:
        {
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS0_CFG;
            pCntr->cntrSource    =
                LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS0_CFG_SOURCE_VCLK0_CLK;
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK1_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_VCLK1:
        {
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS1_CFG;
            pCntr->cntrSource    =
                LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS1_CFG_SOURCE_VCLK1_CLK;
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK2_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_VCLK2:
        {
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS2_CFG;
            pCntr->cntrSource    =
                LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS2_CFG_SOURCE_VCLK2_CLK;
            pCntr->scalingFactor = 1;
            break;
        }
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_VCLK3_CAPABILITY_INIT != 0)
        case LW2080_CTRL_CLK_DOMAIN_VCLK3:
        {
            pCntr->cfgReg   = LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS3_CFG;
            pCntr->cntrSource    =
                LW_PVTRIM_SYS_FR_CLK_CNTR_VCLKS3_CFG_SOURCE_VCLK3_CLK;
            pCntr->scalingFactor = 1;
            break;
        }
#endif
        case LW2080_CTRL_CLK_DOMAIN_MCLK:
        {
            pCntr->cfgReg   = LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG;

#ifdef LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG_SOURCE_DRAMPA_CLK1          // e.g. GA10x
            pCntr->cntrSource    =
                LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG_SOURCE_DRAMPA_CLK1;
#else                                                                           // e.g. GA100
            pCntr->cntrSource    =
                LW_PTRIM_FBPA_BCAST_FR_CLK_CNTR_NCLTCCLK_CFG_SOURCE_DRAMDIV4_REC_CLK_CMD0_CNTR_PA0;
#endif

#if     PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)                            // e.g. TU10x GA10x
            pCntr->scalingFactor = 8;
#elif ! PMUCFG_FEATURE_ENABLED(PMU_FB_HBM_SUPPORTED)
#error Required: At least one PMUCFG feature to specify the supported memory type(s)
#elif   PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_HBM_MCLK_2X_SCALING)                // e.g. GA100
            pCntr->scalingFactor = 2;
#else                                                                           // e.g. GV100
            pCntr->scalingFactor = 1;
#endif
            break;
        }
        case LW2080_CTRL_CLK_DOMAIN_GPC2CLK:
        case LW2080_CTRL_CLK_DOMAIN_GPCCLK:
        {
            if (pCntr->partIdx == LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED)
            {
                pCntr->cfgReg = LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG;
                pCntr->cntrSource =
#ifdef LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOEG_CTS_PROBE
                    LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOEG_CTS_PROBE;
#elif defined(LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_CTS_PROBE)
                    LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_CTS_PROBE;
#else
                    LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_PROBE_OUT;
#endif
            }
            else
            {
                PMU_HALT_COND(pCntr->partIdx < LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX);
                pCntr->cfgReg = LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG(pCntr->partIdx);
                pCntr->cntrSource =
#ifdef LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOEG_CTS_PROBE
                    LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOEG_CTS_PROBE;
#elif defined(LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_CTS_PROBE)
                    LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_CTS_PROBE;
#else
                    LW_PTRIM_GPC_FR_CLK_CNTR_NCGPCCLK_CFG_SOURCE_GPCCLK_NOBG_PROBE_OUT;
#endif
            }
            pCntr->scalingFactor = (PMUCFG_FEATURE_ENABLED(
                            PMU_CLK_DOMAINS_1X_SUPPORTED) ? 1 : 2);
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
