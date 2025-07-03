/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author Chandrabhanu Mahapatra
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "clk3/dom/clkfreqdomain_osm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! Virtual table
CLK_DEFINE_VTABLE_CLK3__DOMAIN(OsmFreqDomain);

/* ------------------------- Macros and Defines ----------------------------- */

// If a path is not specified, configure the OSM to SPPLL0.
#define PATH_DEFAULT (CLK_PUSH__SIGNAL_PATH((CLK_SIGNAL_PATH_EMPTY), 0xC))


/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @brief   Configure the clkosmfreqdomain to produce the target frequency
 */
FLCN_STATUS
clkConfig_OsmFreqDomain
(
    ClkOsmFreqDomain *this,
    const ClkTargetSignal *pTarget
)
{
    ClkTargetSignal     target = *pTarget;
    FLCN_STATUS         status;

    // There is only the OSM.
    switch (target.super.source)
    {
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:     // Good
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID:        // Don't care
            break;

        default:                                            // Not available
            return FLCN_ERR_ILWALID_SOURCE;
    }

    // No need to switch if we're already producing the required signal.
    if (CLK_CONFORMS_WITHIN_DELTA__SIGNAL(
                CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                pTarget->super,
                CLK_STANDARD_MARGIN__FREQDOMAIN(pTarget->super.freqKHz)))
    {
        this->super.phaseCount = 1;
        return FLCN_OK;
    }

    // Do not exceed a frequency supported by the current voltage.
    target.rangeKHz.max = CLK_PLUS_STANDARD_MARGIN__FREQDOMAIN(
                 LW_MAX(CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                        target.super.freqKHz));

    //
    // OSM domains have only one transition.
    // Phase 0: the current hardware state
    // Phase 1: the target state
    //
    this->super.phaseCount = 2;

    // If the path was not specified, use the default
    if (CLK_IS_EMPTY__SIGNAL_PATH(target.super.path))
    {
        target.super.path = PATH_DEFAULT;
    }

    //
    // Now that the paths have been determined and the range set, the root is
    // ready to be configured
    //
    status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                    &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                    1, &target, LW_TRUE);

    return status;
}

