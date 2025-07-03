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
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 * @author  Ming Zhong
 * @author  Prafull Gupta
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkrovco.h"
#include "clk3/generic_dev_trim.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(RoVco);

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkRoVco
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *
 * @param[in]   this            Instance of ClkReadOnly_Object to read.
 * @param[out]  pOutput         Actual cached clock signal
 * @param[in]   bActive         Unused
 *
 * @return      FLCN_OK or the status returned from calling clkRead on the input.
 */
FLCN_STATUS
clkRead_RoVco
(
    ClkRoVco       *this,
    ClkSignal      *pOutput,
    LwBool          bActive
)
{
    FLCN_STATUS status;

    // Read the coefficients from hardware.
    LwU32 coeffRegData = CLK_REG_RD32(this->coeffRegAddr);
    LwU32 mdiv = DRF_VAL(_PTRIM, _SYS_ROPLL_COEFF, _MDIV, coeffRegData);
    LwU32 ndiv = DRF_VAL(_PTRIM, _SYS_ROPLL_COEFF, _NDIV, coeffRegData);

    //
    // DRAMPLL does not have a PLDIV.  Instead it has a SEL_DIVBY2 which acts
    // as a glitchy version of PLDIV that always divides by 2 when enabled.
    //
    LwU32 div2 = ((this->bDiv2Enable &&
        FLD_TEST_DRF(_PFB, _FBPA_PLL_COEFF, _SEL_DIVBY2, _ENABLE, coeffRegData))?
        2 : 1);

    //
    // For GA10x and later, on GDDR6x only, DRAMPLL output frequency is twice the
    // frequency callwlated from its coefficients because of PAM4.
    //
    LwU32 mul2 = (this->bDoubleOutputFreq? 2 : 1);

    // Read the input.
    status = clkRead_Wire(&this->super, pOutput, bActive);

    // Do the arithmetic, error or not.
    pOutput->freqKHz *= ndiv;
    pOutput->freqKHz /= mdiv;
    pOutput->freqKHz /= div2;
    pOutput->freqKHz *= mul2;

    // Indicate the type of object which is the soruce of the clock signal.
    pOutput->source = this->source;

    // Done
    return status;
}


/*******************************************************************************
    Print
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkRoVco
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_RoVco
(
    ClkRoVco        *this,
    ClkPhaseIndex    phaseCount
)
{
    // Read and print hardware state.
    // (All software state is static const or local to clkRead.)
    CLK_PRINT_REG("Read-Only VCO: COEFF", this->coeffRegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif
