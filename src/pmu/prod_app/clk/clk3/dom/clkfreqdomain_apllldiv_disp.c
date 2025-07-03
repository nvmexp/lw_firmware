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
 * @author  Eric Colter
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/dom/clkfreqdomain_apllldiv_disp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! Virtual table
CLK_DEFINE_VTABLE_CLK3__DOMAIN(DispFreqDomain);

/* ------------------------- Macros and Defines ----------------------------- */

// We accept a defaults tolerance of 1/2%
#define MARGIN(f)           ((f) / 200)
#define PLUS_MARGIN(f)      ((f) + MARGIN(f))
#define LESS_MARGIN(f)      ((f) - MARGIN(f))


//
// Where do we get the 3240MHz constant below?  The OSM signal is generated
// by SPPLL0, which runs at 1620MHz and is divided by an LDIV.  The LDIV
// has a fractional divide feature, which means it can divide by 'n/2' (i.e.
// 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, etc.).  We get 1620MHz, 1080MHz, 810MHz,
// 648MHz, 540MHz, 462.857MHz, etc.  There are additional limits, but those
// are handled by other objects such as ClkLdivUnit.
//

// Largest OSM frequency below f
#define BYPASS_FREQ_FLOOR_KHZ(f)    (3240000 / (3240000 / (f) + 1))

// Smallest OSM frequency above f
#define BYPASS_FREQ_CEILING_KHZ(f)  (3240000 / (3240000 / (f)))

//
// If fractional division is not allowed, then we can only divide by n,
// (1.0, 2.0, 3.0 ... )
//

// Largest OSM frequency below f without fractional divide
#define BYPASS_FREQ_FLOOR_NO_FRACDIV_KHZ(f)    (1620000 / (1620000 / (f) + 1))

// Smallest OSM frequency above f without fractional divide
#define BYPASS_FREQ_CEILING_NO_FRACDIV_KHZ(f)  (1620000 / (1620000 / (f)))

//
// Is this frequency within 1/2% of an OSM frequency?
//
// Let 'f' be the frequency to test (either target or previous phase).
// Let 'n' be a positive integer.
// Let 'B' be the set of OSM frequencies (i.e. every '3240000KHz / n').
// Let 'fb' be the OSM frequency closest to 'f'.
// Let 'nb' be the 'n' such that '3240000KHz / n = fb'.
// Therefore 'fb * n = 3240000KHz'.
// When 'f <= fb',
//      '3240000KHz % f == 3240000KHz - f * n == fb * n - f * n == (fb - f) * n'
// Therefore,
//      'fb - f < MARGIN(fb)' when '(fb - f) * n < MARGIN(3240000KHz)'
//      (i.e. within margin).
// Similarly, 'f > fb',
//      'f - 3240000KHz % f == f * n - 3240000KHz == f * n - fb * n == (f - fb) * n'
// Therefore,
//      'f - fb < MARGIN(fb)' when '(f - fb) * n < MARGIN(3240000KHz)' (i.e. within margin).
//
#define IS_OSM_FREQ_KHZ(f) (3240000 % (f)  < MARGIN(3240000) ||  \
                      ((f) - 3240000 % (f)) < MARGIN(3240000))

//
// See clkTypicalPath_dispclk defined on clk/clk2/sd/clksdgp100.c.
// NOTE: If we were to use a flag in this->super.typicalPath and search for
// it here, then we could use this class for other domains.
//
#define IS_OSM_PATH(p)   (CLK_HEAD__SIGNAL_PATH(p) == 0x0)

// If a path is not specified, this is the default OSM (bypass) path to choose
#define DEFAULT_PATH_OSM (CLK_PUSH__SIGNAL_PATH(CLK_PUSH__SIGNAL_PATH((CLK_SIGNAL_PATH_EMPTY), 0xC), 0x0))

// If a path is not specified, this is the default PLL path to choose
#define DEFAULT_PATH_PLL (CLK_PUSH__SIGNAL_PATH(CLK_PUSH__SIGNAL_PATH((CLK_SIGNAL_PATH_EMPTY), 0x8), 0x1))

//
// This macro determines whether or not the second node of the path is
// undefined. This is important because the pTarget could use the first node
// to indicate that an ALT or REF path is desired, but not care which route is
// taken within the ALT or REF path
//
#define SECOND_NODE_UNDEFINED(path)  (CLK_HEAD__SIGNAL_PATH(CLK_POP__SIGNAL_PATH(path)) == CLK_SIGNAL_NODE_INDETERMINATE)

//
// True iff the signal path agrees with the source
//
#define PATH_SOURCE_AGREES(_SIGNAL)                                            \
    ((_SIGNAL).source == (IS_OSM_PATH((_SIGNAL).path) ?                        \
                LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE :                    \
                LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL))


/* ------------------------- Prototypes ------------------------------------- */

// Takes in a target and returns one to configure to the alt path
static void clkConfig_FindOsmTarget_DispFreqDomain(ClkDispFreqDomain* this, ClkTargetSignal* target)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_FindOsmTarget_DispFreqDomain");


/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*
 * @brief       Configure the clkDispFreqDomain to produce the target frequency
 *
 * @details     This configures the clkDispFreqDomain to produce the target
 *              frequency.  The OSM path is one that goes through the DISPCLK_ALT
 *              osm and the PLL path is one that goes through the DISPPLL and
 *              DISPCLK_REF osm.
 *
 *              The following paths are used when switching:
 *              PLL -> PLL
 *                  Only one phase is needed. Ndiv sliding is used to adjust the
 *                  disppll hot.
 *              PLL -> OSM
 *                  Only one phase is needed. The mux that serves as the root of
 *                  the disp clkdomain is switched to the OSM path.
 *              OSM -> PLL
 *                  Only one phase is needed. The mux that serves as the root of
 *                  the dispclk domain is switched to the PLL path and
 *                  the disppll is adjusted to produce the desired frequency.
 *              OSM -> OSM
 *                  Two phases are required.  The first phase switches to the
 *                  PLL path, and the second switches back to the OSM path.
 *                  This is because, although the ldivs that control the OSM
 *                  frequency can be changed hot, it can result in problematic
 *                  frequency jumps.
 *
 *              Intermediate frequencies will be selected to be between the
 *              current and target frequency.  If a suitable frequency cannot
 *              be found, a frequency below the current and target frequencies
 *              will be selected.
 *
 *              The default and most precise path is the PLL path.  In
 *              general, a frequency is selected by changing the pll's ndiv and
 *              the ldiv of the top level linear divider.  Although this isn't
 *              lwrrently implemented, selecting the largest possible ldiv value
 *              results in a finer resolution and the ability to more closly
 *              match the target output frequency. When used in ndiv sliding
 *              mode, the pll and ldiv could be combined into a single class so
 *              they can better share information and choose more appropriate
 *              frequencies.
 *
 * @note        The target range minimum is taken from the input.  The target
 *              range maximum from the input is ignored, and instead is set to
 *              1/2% above the maximum of the current and targer frequency.
 */
FLCN_STATUS
clkConfig_DispFreqDomain
(
    ClkDispFreqDomain           *this,
    const ClkTargetSignal       *pTarget
)
{
    ClkTargetSignal     finalTarget;
    FLCN_STATUS         status;

    // Base the final target on the parameter.
    finalTarget = *pTarget;

    // If no path is specified, derive it from the source
    if (CLK_IS_EMPTY__SIGNAL_PATH(finalTarget.super.path))
    {
        switch (finalTarget.super.source)
        {
            case LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL:
                finalTarget.super.path = DEFAULT_PATH_PLL;
                break;

            case LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:
                finalTarget.super.path = DEFAULT_PATH_OSM;
                break;

            case LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID:        // Don't care
                break;

            default:
                return FLCN_ERR_ILWALID_SOURCE;
        }
    }

    // No need to switch if we're already producing the required signal
    if (CLK_CONFORMS_WITHIN_DELTA__SIGNAL(
                CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                finalTarget.super, MARGIN(finalTarget.super.freqKHz)))
    {
        this->super.phaseCount = 1;
        return FLCN_OK;
    }

    // If the first mux is specified, but the second is not, apply a default.
    if (SECOND_NODE_UNDEFINED(finalTarget.super.path))
    {
        switch (CLK_HEAD__SIGNAL_PATH(finalTarget.super.source))
        {
            case CLK_HEAD__SIGNAL_PATH(DEFAULT_PATH_PLL):
                finalTarget.super.path = DEFAULT_PATH_PLL;
                break;

            case CLK_HEAD__SIGNAL_PATH(DEFAULT_PATH_OSM):
                finalTarget.super.path = DEFAULT_PATH_OSM;
                break;
        }
    }

    //
    // The maximum ensures we don't have a violation with respect to voltage.
    // Use the larger of the current hardware state and the target.  Add margin.
    //
    finalTarget.rangeKHz.max = PLUS_MARGIN(LW_MAX(
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                    pTarget->super.freqKHz));

    //
    // Assume one transition until shown otherwise.
    // The phase array therefore contains two elements.
    // Phase 0:  Current hardware state
    // Phase 1:  Target hardware state
    //
    this->super.phaseCount = 2;

    // ANYTHING -> OSM
    if (IS_OSM_PATH(finalTarget.super.path))
    {
        //
        // OSM -> OSM requires two transitions.
        // Phase 0:  OSM (the current hardware state)
        // Phase 1:  PLL
        // Phase 2:  OSM (the target hardware state)
        //
        if (IS_OSM_PATH(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).path))
        {
            // Base the intermediate (phase 1) target on the final (phase 2) target.
            ClkTargetSignal intermediateTarget = finalTarget;

            // Phase 2 is required; array has three elements.
            this->super.phaseCount = 3;

            // The intermediate path is the PLL path.
            intermediateTarget.super.path = DEFAULT_PATH_PLL;

            // Configure phase 2 (the final target).
            clkConfig_FindOsmTarget_DispFreqDomain(this, &intermediateTarget);

            //
            // Configure target ranges.  The intermediate frequency (phase one)
            // must be between the current hardware frequency (phase zero) and
            // the final target frequency (phase 2).  Add margin.
            //
            intermediateTarget.rangeKHz.max = PLUS_MARGIN(
                    LW_MAX(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                           finalTarget.super.freqKHz));
            intermediateTarget.rangeKHz.min = LESS_MARGIN(
                    LW_MIN(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                           finalTarget.super.freqKHz));

            // Configure the intermediate phase (phase 1).
            status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                        &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                        1, &intermediateTarget, LW_TRUE);

            // Return early on error.
            if (status != FLCN_OK)
            {
                return status;
            }

            // ClkFreqSrc logic should set both 'path' and 'source' to agree.
            PMU_HALT_COND(PATH_SOURCE_AGREES(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super)));
        }

        // PLL -> OSM requires only one transition.
        else
        {
            // Change target to the best OSM frequency
            clkConfig_FindOsmTarget_DispFreqDomain(this, &finalTarget);
            finalTarget.rangeKHz.max = PLUS_MARGIN(finalTarget.super.freqKHz);
            finalTarget.rangeKHz.min = LESS_MARGIN(finalTarget.super.freqKHz);
        }
    }

    //
    // Now that the paths have been determined and the range set, the root is
    // ready to be configured
    //
    status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                    &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                    this->super.phaseCount - 1,  // final phase
                    &finalTarget, LW_TRUE);

    // ClkFreqSrc logic should set both 'path' and 'source' to agree.
    PMU_HALT_COND(status != FLCN_OK ||
            PATH_SOURCE_AGREES(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super)));

    // Done
    return status;
}


/* ------------------------- Private Functions ------------------------------ */

/*!
 * @memberof    ClkDispFreqDomain
 * @brief       Set the target for the OSM path
 *
 * @details     Given a target frequency, try to find a OSM frequency that is
 *              between the current frequency and the target frequency.  If it
 *              isn't possible, choose a frequency below the minimum of those
 *              two.
 *
 * @param[in]       this    The DispFreqDomain object which is being configured
 * @param[in\out]   target  The ClkSignalTarget containing the target frequency.
 *                          This will be altered to contain the target frequency
 *                          for the OSM route.
 *
 */
static void
clkConfig_FindOsmTarget_DispFreqDomain
(
    ClkDispFreqDomain           *this,
    ClkTargetSignal             *target
)
{
    LwU32 aboveFreqKHz;
    LwU32 belowFreqKHz;
    LwRangeU32 range;

    //
    // Depending on whether fractional division is allowed or not, choose the
    // two closest values to the target.
    //
    if (target->super.fracdiv)
    {
        belowFreqKHz = BYPASS_FREQ_FLOOR_KHZ(target->super.freqKHz);
        aboveFreqKHz = BYPASS_FREQ_CEILING_KHZ(target->super.freqKHz);
    }
    else
    {
        belowFreqKHz =
            BYPASS_FREQ_FLOOR_NO_FRACDIV_KHZ(target->super.freqKHz);
        aboveFreqKHz =
            BYPASS_FREQ_CEILING_NO_FRACDIV_KHZ(target->super.freqKHz);
    }

    range.max = PLUS_MARGIN( LW_MAX(
                CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                target->super.freqKHz));
    range.min = LESS_MARGIN( LW_MIN(
                CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                target->super.freqKHz));

    //
    // We absolutely cannot exceed the upper bound as the voltage might not
    // support it. If the alt freqeuncy above the target exceeds this range,
    // then choose the lower one
    //
    if (aboveFreqKHz > range.max)
    {
        target->super.freqKHz = belowFreqKHz;
    }

    //
    // If the frequency below is too low, but the frequency above is ok,
    // choose the frequency above as this will avoid losing performance.
    //
    else if (belowFreqKHz < range.min)
    {
        target->super.freqKHz = aboveFreqKHz;
    }

    else if (LW_DELTA(belowFreqKHz, target->super.freqKHz) <
        LW_DELTA(aboveFreqKHz, target->super.freqKHz))
    {
        target->super.freqKHz = belowFreqKHz;
    }

    else
    {
        target->super.freqKHz = aboveFreqKHz;
    }
}

