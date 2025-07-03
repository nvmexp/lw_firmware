/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2018 by LWPU Corporation.  All rights reserved.  All
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
 */

#include "clk3/fs/clkmultiplier.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE__FREQSRC(Multiplier);


/*******************************************************************************
    Virtual Function Implementations
*******************************************************************************/

/*!
 * @brief       Get the frequency in KHz of the multiplier.
 * @memberof    ClkMultiplier
 *
 * @details     This reads the input to the multiplier, multiplies the
 *              value and then sets pOutput->freqKHz accordingly.
 *
 * @param[in]   this        Tnstance from which to read the frequency
 * @param[out]  pOutput     Struct to which the freuncy will be written
 * @param[in]   bActive     Unused
 */
FLCN_STATUS
clkRead_Multiplier
(
    ClkMultiplier       *this,
    ClkSignal           *pOutput,
    LwBool               bActive
)
{
    FLCN_STATUS status;

    status = clkRead_Wire(&this->super, pOutput, bActive);
    pOutput->freqKHz *= this->multiplier;
    return status;
}


/*!
 * @brief       Configure the multiplier object.
 * @memberof    ClkMultiplier
 *
 * @details     ClkMultipler is not configurable, so this simply configures
 *              the input to the multiplier.
 *
 * @param[in]   this        instance of ClkMultiplier to configure.
 * @param[out]  pOutput     actual frequency to which the multiplier is configured
 * @param[in]   phase       Unused
 * @param[in]   pTarget     Target frequency to set the multiplier to.
 * @param[in]   bHotSwitch  Unused
 */
FLCN_STATUS
clkConfig_Multiplier
(
    ClkMultiplier           *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS status;
    ClkTargetSignal target;

    target = *pTarget;
    CLK_DIVIDE__TARGET_SIGNAL(target, this->multiplier);
    status = clkConfig_Wire(&this->super, pOutput, phase, &target, bHotSwitch);
    pOutput->freqKHz *= this->multiplier;

    return status;
}

