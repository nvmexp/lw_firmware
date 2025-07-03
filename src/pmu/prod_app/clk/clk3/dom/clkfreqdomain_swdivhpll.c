/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt Varvak
 */

/* ------------------------- System Includes -------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif

/* ------------------------- Application Includes --------------------------- */

#include "clk3/generic_dev_trim.h"
#include "clk3/dom/clkfreqdomain_swdivhpll.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

///! Virtual table
CLK_DEFINE_VTABLE_CLK3__DOMAIN(SwDivHPllFreqDomain);

//! Register value map used for the PLL v. crystal multiplexer
const ClkFieldValue clkMuxValueMap_pllxtal_SwDivHPllFreqDomain[CLK_INPUT_PLLXTAL_COUNT__SWDIVHPLLFREQDOMAIN]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_pllxtal_SwDivHPllFreqDomain") =
{
    [CLK_INPUT_PLLXTAL_PLL__SWDIVHPLLFREQDOMAIN] =                   //  0   pll
            CLK_DRF__FIELDVALUE(_PTRIM, _SYS_PLL_CFG, _BYPASSPLL, _NO),
    [CLK_INPUT_PLLXTAL_XTAL__SWDIVHPLLFREQDOMAIN] =                  //  1   xtal
            CLK_DRF__FIELDVALUE(_PTRIM, _SYS_PLL_CFG, _BYPASSPLL, _YES),
};


/* ------------------------- Macros and Defines ----------------------------- */

// We accept a defaults tolerance of 1/2%
#define MARGIN(f)           ((f) / 200)
#define PLUS_MARGIN(f)      ((f) + MARGIN(f))
#define LESS_MARGIN(f)      ((f) - MARGIN(f))

#define ALMOST_EQUAL(f1, f2) (LW_MAX(f1, f2) - LW_MIN(f2, f2) < MARGIN(LW_MAX(f2, f2)))

// Path to the XTAL (alt)
#define PATH_XTAL                                           \
    CLK_PUSH__SIGNAL_PATH(CLK_SIGNAL_PATH_EMPTY,            \
        CLK_INPUT_SWDIV_XTAL__SWDIVHPLLFREQDOMAIN)          /* SWDIV = 0 = BIT(CLOCK_SOURCE_SEL_XTAL_PLL_REFCLK) */

// POR path for the PLL
#define PATH_PLL                                                                    \
    CLK_PUSH__SIGNAL_PATH(CLK_PUSH__SIGNAL_PATH(CLK_SIGNAL_PATH_EMPTY,              \
        CLK_INPUT_PLLXTAL_PLL__SWDIVHPLLFREQDOMAIN),        /* Bypass mux = 0 */    \
        CLK_INPUT_SWDIV_PLL__SWDIVHPLLFREQDOMAIN)           /* SWDIV = 1 = BIT(CLOCK_SOURCE_SEL_DISPPLL)*/

// POR path to SPPLL0 (alt)
#define PATH_SPPLL0_PDIV                                    \
    CLK_PUSH__SIGNAL_PATH(CLK_SIGNAL_PATH_EMPTY,            \
        CLK_INPUT_SWDIV_SPPLL0_PDIV__SWDIVHPLLFREQDOMAIN)   /* SWDIV = 2 = BIT(CLOCK_SOURCE_SEL_SPPLL0) */

// Path to SPPLL1 (alt)
#define PATH_SPPLL1_PDIV                                    \
    CLK_PUSH__SIGNAL_PATH(CLK_SIGNAL_PATH_EMPTY,            \
        CLK_INPUT_SWDIV_SPPLL1_PDIV__SWDIVHPLLFREQDOMAIN)   /* SWDIV = 3 = BIT(CLOCK_SOURCE_SEL_SPPLL1)*/

// We're using the domain-specific PLL only if both multiplexers are set properly.
#define IS_PLL_PATH(p)                                                            \
    (CLK_PEEK__SIGNAL_PATH((p), 0) == CLK_INPUT_SWDIV_PLL__SWDIVHPLLFREQDOMAIN && \
     CLK_PEEK__SIGNAL_PATH((p), 1) == CLK_INPUT_PLLXTAL_PLL__SWDIVHPLLFREQDOMAIN)


//
// True iff the signal path agrees with the source.  In other words, true if
// both or neither are using the domain-specific PLL path.
//
#define PATH_SOURCE_AGREES(_SIGNAL)                                            \
    (((_SIGNAL).source == LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL) ==               \
    IS_PLL_PATH((_SIGNAL).path))


/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*
 * @brief       Configure the ClkSwDivHPllFreqDomain to produce the target frequency
 *
 * @details     This configures the ClkSwDivHPllFreqDomain to produce the target
 *              frequency.  "PLL" refers to the domain-specific PLL (e.g DISPPLL
 *              or HUBDISPPLL).
 *
 *              There are three alt paths: XTAL, SPPLL0, and SPPLL1.  Per
 *              Bug 2557340/48, SPPLL0 is the only alt path suported by POR,
 *              but the others are supported in this class.
 *
 *              There is always one transition and therefore two elements in
 *              the phase array.
 *
 *              The SPPLL0 path is used below a particular frequency specified
 *              in ClkSwDivHPllFreqDomain::altPathMaxKHz.
 *
 * @note        The target range minimum is taken from the input.  The target
 *              range maximum from the input is ignored, and instead is set to
 *              1/2% above the maximum of the current and target frequency.
 */
FLCN_STATUS
clkConfig_SwDivHPllFreqDomain
(
    ClkSwDivHPllFreqDomain      *this,
    const ClkTargetSignal       *pTarget
)
{
    ClkTargetSignal     finalTarget;
    FLCN_STATUS         status;

    // Base the final target on the parameter.
    finalTarget = *pTarget;

    // If no path is specified, derive it from the source
    if (CLK_HEAD__SIGNAL_PATH(finalTarget.super.path) == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        switch (finalTarget.super.source)
        {
            case LW2080_CTRL_CLK_PROG_1X_SOURCE_DEFAULT:
            {
                finalTarget.super.path =
                    (finalTarget.super.freqKHz > this->altPathMaxKHz?
                        PATH_PLL : PATH_SPPLL0_PDIV);
                break;
            }

            case LW2080_CTRL_CLK_PROG_1X_SOURCE_XTAL:
            {
                finalTarget.super.path = PATH_XTAL;
                break;
            }

            //
            // POR ALT path corresponds to SPPLL0/PDIV for HUB/DISP clock
            // on GA10x and later.
            //
            case LW2080_CTRL_CLK_PROG_1X_SOURCE_ALT_PATH:
            case LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL0:
            {
                //
                // SPPLL0_PDIV path is only supported for frequencies lesser
                // than altPathMaxKHz. Assert if the target frequency is greater
                // than or equal to the threshold.
                //
                if (finalTarget.super.freqKHz < this->altPathMaxKHz)
                {
                    finalTarget.super.path = PATH_SPPLL0_PDIV;
                }
                else
                {
                    PMU_BREAKPOINT();
                    return FLCN_ERR_ILWALID_SOURCE;
                }
                break;
            }

            case LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL1:
            {
                finalTarget.super.path = PATH_SPPLL1_PDIV;
                break;
            }

            case LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL:
            {
                //
                // PLL path is only supported for frequencies greater than or
                // equal to altPathMaxKHz. Assert if the target frequency is
                // lesser than the threshold.
                //
                if (finalTarget.super.freqKHz >= this->altPathMaxKHz)
                {
                    finalTarget.super.path = PATH_PLL;
                }
                else
                {
                    PMU_BREAKPOINT();
                    return FLCN_ERR_ILWALID_SOURCE;
                }
                break;
            }

            default:
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_SOURCE;
            }
        }
    }

    // No need to switch if we're already producing the required signal
    if (CLK_CONFORMS_WITHIN_DELTA__SIGNAL(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                    finalTarget.super, MARGIN(finalTarget.super.freqKHz)))
    {
        this->super.phaseCount = 1;
        return FLCN_OK;
    }

    //
    // The SWDIV mux is switched to the domain-specific PLL, but the bypass mux
    // inside the PLL has not been specified.  Use the PLL as a default.
    //
    if (CLK_PEEK__SIGNAL_PATH(finalTarget.super.path, 0) == CLK_INPUT_SWDIV_PLL__SWDIVHPLLFREQDOMAIN &&
        CLK_PEEK__SIGNAL_PATH(finalTarget.super.path, 1) == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        finalTarget.super.path = PATH_PLL;
    }

    //
    // The maximum ensures we don't have a violation with respect to voltage.
    // Use the larger of the current hardware state and the target.  Add margin.
    //
    finalTarget.rangeKHz.max = PLUS_MARGIN(LW_MAX(
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                    pTarget->super.freqKHz));

    //
    // There is always exactly one transition per bug 2557340/60.
    // The phase array therefore contains two elements.
    // Phase 0:  Current hardware state
    // Phase 1:  Target hardware state
    //
    this->super.phaseCount = 2;

    //
    // Now that the paths have been determined and the range set, the root is
    // ready to be configured, which will daisy-chain to down to the leaf.
    //
    status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                    &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                    this->super.phaseCount - 1,  // final phase
                    &finalTarget, LW_TRUE);

    //
    // ClkFreqSrc logic should set both 'path' and 'source' to agree.
    // In short, if it doesn't agree, there had better be a status code to explain.
    //
    PMU_HALT_COND((status != FLCN_OK) ||
            PATH_SOURCE_AGREES(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super)));

    // Done
    return status;
}

