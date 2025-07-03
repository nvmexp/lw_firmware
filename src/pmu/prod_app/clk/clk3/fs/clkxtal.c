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
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/fs/clkxtal.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! Virtual table
CLK_DEFINE_VTABLE__FREQSRC(Xtal);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @see         ClkRead_FreqSrc_VIP
 * @brief       Get the frequency in KHz of the crystal.
 * @memberof    ClkXtal
 *
 * @param[in]   this            instance of ClkXtal to read the frequency of
 * @param[out]  pOutput         struct to which the frequency will be written
 * @retval      FLCN_OK         under all cirlwmstances
 */
FLCN_STATUS
clkRead_Xtal
(
    ClkXtal     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    *pOutput = clkEmpty_Signal;
    pOutput->freqKHz = this->freqKHz;
    pOutput->source = LW2080_CTRL_CLK_PROG_1X_SOURCE_XTAL;
    return FLCN_OK;
}


/*!
 * @see         ClkConfig_FreqSrc_VIP
 * @brief       Check configuration
 * @memberof    ClkXtal
 *
 * @param[in]   this            Instance of ClkXtal to configure
 * @param[out]  pOutput         Struct to which the frequency will be written
 * @param[in]   phase           Unused
 * @param[in]   pTarget         Target frequency and realted characteristics
 * @param[in]   bHotSwitch      Unused
 *
 * @retval      FLCN_OK                         Success
 * @retval      FLCN_ERR_ILWALID_PATH           Path is not empty.
 * @retval      FLCN_ERR_MISMATCHED_TARGET      Target parameter mismatch
 * @retval      FLCN_ERR_FREQ_NOT_SUPPORTED     Frequency is not within target range.
 */
FLCN_STATUS
clkConfig_Xtal
(
    ClkXtal                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    // Set the output to a simple frequency.
    *pOutput = clkEmpty_Signal;
    pOutput->freqKHz = this->freqKHz;
    pOutput->source = LW2080_CTRL_CLK_PROG_1X_SOURCE_XTAL;

    // This is the end of the clocks DAG, so there can not be more nodes in the path.
    if (pTarget->super.path != CLK_SIGNAL_PATH_EMPTY)
    {
        return FLCN_ERR_ILWALID_PATH;
    }

    //
    // Fail is the target parameter requires a specific range that does not
    // contain the crystal frequency.  There is no point in checking the other
    // fields (e.g. fracdiv/overclock/regimeId) because they do not apply to XTALs.
    //
    if (!LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, pOutput->freqKHz))
    {
        return FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    // All good.
    return FLCN_OK;
}


/*!
 * @brief       Do nothing, a crystal is not programmable
 *
 * @memberof    ClkXtal
 *
 * @param[in]   this the instance of ClkFreqSrc to program.
 *
 * @param[in]   phase the phase of the phase array to set the crystal to.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkProgram_Xtal
(
    ClkXtal         *this,
    ClkPhaseIndex    phase
)
{
    return FLCN_OK;
}


/*!
 * @brief       Do nothing, there is no cleanup work to be done
 *
 * @memberof    ClkXtal
 *
 * @param[in]   this the instance of ClkFreqSrc to clean up.
 *
 * @param[in]   bActive whether this object is part of the path that produces
 *              the final clock signal.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkCleanup_Xtal
(
    ClkXtal    *this,
    LwBool      bActive
)
{
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkXtal
 *
 * @details     Nothing to print since crystals don't have interesting state
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 *
 */
#if CLK_PRINT_ENABLED
void
clkPrint_Xtal
(
    ClkXtal         *this,
    ClkPhaseIndex    phaseCount
)
{
    // Nothing to print
}
#endif

