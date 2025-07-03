/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see     Bug 2557340: Turn off VPLL, HUBPLL and DISPPLL based on usecase
 * @see     Bug 2571031: Ampere DISPCLK/HUBCLK: Switching to bypass after initialization
 * @see     Perforce:  hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_Structure.vsdx
 * @author  Antone Vogt Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkropdiv.h"
#include "clk3/generic_dev_trim.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(RoPDiv);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implementations ------------------------ */

/*!
 * @brief       Get the frequency.
 * @memberof    ClkRoPDiv
 * @see         ClkFreqSrc_Virtual::clkRead
 *
 * @details     This implementation reads the divider after calling clkRead
 *              on the input.  From that it computes the output frequency.
 *
 * @param[in]   this        Instance of ClkRoPDiv from which to read
 * @param[out]  pOutput     Frequency will be written to this struct.
 */
FLCN_STATUS
clkRead_RoPDiv
(
    ClkRoPDiv   *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    // Read the input frequency.
    FLCN_STATUS status = clkRead_Wire(&(this->super), pOutput, bActive);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Read the divider value.
    LwU32 regData = CLK_REG_RD32(this->regAddr);

    // Extract the divider value.
    LwU32 divValue = (regData >> this->base) & (LWBIT32(this->size) - 1);

    //
    // On some dividers, zero means powered-off, but even if that's not the
    // case on this divider, we can't divide by zero and expect a good result.
    //
    if (divValue == 0)
    {
        *pOutput = clkReset_Signal;
        return FLCN_ERR_ILWALID_STATE;
    }

    // Compute the output value if this LDIV unit.
    pOutput->freqKHz /= divValue;

    // Done
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkRoPDiv
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_RoPDiv
(
    ClkRoPDiv       *this,
    ClkPhaseIndex    phaseCount
)
{
    // Read and print the hardware state.
    CLK_PRINT_REG("Read-Only PDIV: COEFF", this->regAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif


/* ------------------------- Private Functions ------------------------------ */

