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

#include "clk3/dom/clkfreqdomain_bif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! Virtual table
CLK_DEFINE_VTABLE_CLK3__DOMAIN(BifFreqDomain);


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @brief   Configure the clkbiffreqdomain to produce the target GEN SPEED
 */
FLCN_STATUS
clkConfig_BifFreqDomain
(
    ClkBifFreqDomain        *this,
    const ClkTargetSignal   *pTarget
)
{
    FLCN_STATUS         status = FLCN_OK;

    // Current domain has no multiplexers, as such target path is empty.
    if (!CLK_IS_EMPTY__SIGNAL_PATH(pTarget->super.path))
    {
        return FLCN_ERR_ILWALID_PATH;
    }

    // None of the source indicators applies, so no need to check for source.

    //
    // Now that the paths have been determined and the range set, the root is
    // ready to be configured
    //
    status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                        &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                        1, pTarget, LW_TRUE);

    //
    // Switch only if we're already not producing the required signal.
    // Phase 0: the current hardware state
    // Phase 1: the target hardware state
    //
    this->super.phaseCount = 1 +
        (this->bif.targetGenSpeed != this->bif.lwrrentGenSpeed);

    // Done
    return status;
}


/****************************************************************************
 * Helper function definitions
 ****************************************************************************/
