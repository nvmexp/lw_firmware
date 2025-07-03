/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkldivv2.h"
#include "clk3/generic_dev_trim.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(LdivV2);


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @brief       Read the coefficients from hardware
 * @memberof    ClkLdivV2
 * @see         ClkFreqSrc_Virtual::clkRead
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 */
FLCN_STATUS
clkRead_LdivV2
(
    ClkLdivV2           *this,
    ClkSignal           *pOutput,
    LwBool               bActive
)
{
    LwU32               ldivRegData;
    LwU8                ldivValue;
    FLCN_STATUS         status;

    // Read the divider value and the input frequency.
    ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
    status = clkRead_Wire(&(this->super), pOutput, bActive);
    ldivValue = DRF_VAL(_PTRIM, _SYS_CLK_LDIV, _V2, ldivRegData) + 1;

    // Compute the output value if this LDIV unit.
    pOutput->freqKHz = pOutput->freqKHz / ldivValue;

    // Done
    return status;
}


/*!
 * @brief       Configure the hardware to be programmed to a certain frequency
 * @memberof    ClkLdivV2
 * @see         ClkFreqSrc_Virtual::clkConfig
 *
 * @details     Per POR, there is no support for programming LDIVs in chips
 *              that use version 2.  However, POR does support programming
 *              the domain, which essentially means that we must configure and
 *              program the input, while leaving this object unchanged.
 */
FLCN_STATUS clkConfig_LdivV2
(
    ClkLdivV2               *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    ClkTargetSignal inputTarget;
    LwU32           ldivRegData;
    LwU8            ldivValue;
    FLCN_STATUS     status;

    // Read the divider value, which is not change per POR.
    ldivRegData = CLK_REG_RD32(this->ldivRegAddr);
    ldivValue = DRF_VAL(_PTRIM, _SYS_CLK_LDIV, _V2, ldivRegData) + 1;

    // Formulate the input target for this divider value.
    inputTarget = *pTarget;
    CLK_MULTIPLY__TARGET_SIGNAL(inputTarget, ldivValue);

    // Configure the input
    status = clkConfig_Wire(&this->super, pOutput, phase, &inputTarget, bHotSwitch);

    // Apply the divider (error or not).
    pOutput->freqKHz /= ldivValue;

    // Done
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkLdivV2
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program; zero if reading
 */
#if CLK_PRINT_ENABLED
void
clkPrint_LdivV2
(
    ClkLdivV2       *this,
    ClkPhaseIndex    phaseCount
)
{
    // Read and print hardware state.
    CLK_PRINT_REG("LDIV", this->ldivRegAddr);

    // Print state of the input.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif


/* ------------------------- Private Functions ------------------------------ */

