/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/dom/clkfreqdomain_stub.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE_CLK3__DOMAIN(StubFreqDomain);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @brief       Get signal as though reading the hardware or cache.
 */
FLCN_STATUS clkRead_StubFreqDomain(ClkStubFreqDomain *this)
{
    //
    // If we don't have the output signal cached, copy from 'target', which was
    // initialized in ClkSchematicDag.  If this assertion fails, it means that
    // ClkSchematicDag does not contain the initialzation.
    //
    if (!CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz)
    {
        CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super) = this->target;
        PMU_HALT_COND(this->target.freqKHz);
    }

    // Done
    return FLCN_OK;
}


/*!
 * @brief       Configure domain as though hardware exists.
 */
FLCN_STATUS clkConfig_StubFreqDomain(ClkStubFreqDomain *this, const ClkTargetSignal *pTarget)
{
    // We support anything exceot zero frequency.
    if (!this->target.freqKHz)
    {
        return FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    //
    // Programming would happen in one phase, so the array would have two
    // elements if it actually existed.
    //
    this->super.phaseCount = 2;

    // Save the target signal for the subsequent programming.
    this->target = pTarget->super;

    // Done
    return FLCN_OK;
}


/*!
 * @brief       Update the status as though programming the domain.
 */
FLCN_STATUS clkProgram_StubFreqDomain(ClkStubFreqDomain *this)
{
    //
    // If one of these assertions fail, it probably means that clkConfig was
    // not called.
    //
    PMU_HALT_COND(this->target.freqKHz);
    PMU_HALT_COND(this->super.phaseCount == 2);

    // Update the status as though hardware has been programmed.
    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super) = this->target;

    // Done
    return FLCN_OK;
}

