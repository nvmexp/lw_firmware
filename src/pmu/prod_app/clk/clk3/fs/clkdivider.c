/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
 * @author  Onkar Bimalkhedkar
 */

#include "clk3/fs/clkdivider.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE__FREQSRC(Divider);


/*******************************************************************************
    Virtual Function Implementations
*******************************************************************************/

/*!
 * @brief       Get the frequency in KHz of the divider.
 * @memberof    ClkDivider
 *
 * @details     This reads the input to the divider, divides the
 *              value and then sets pOutput->freqKHz accordingly.
 *
 * @param[in]   this        Tnstance from which to read the frequency
 * @param[out]  pOutput     Struct to which the freuncy will be written
 * @param[in]   bActive     Unused
 */
FLCN_STATUS
clkRead_Divider
(
    ClkDivider       *this,
    ClkSignal        *pOutput,
    LwBool            bActive
)
{
    FLCN_STATUS status;

    status = clkRead_Wire(&this->super, pOutput, bActive);
    pOutput->freqKHz /= this->divider;
    return status;
}


/*!
 * @brief       Configure the divider object.
 * @memberof    ClkDivider
 *
 * @details     ClkDivider is not configurable, so this simply configures
 *              the input to the divider.
 *
 * @param[in]   this        instance of ClkDivider to configure.
 * @param[out]  pOutput     actual frequency to which the divider is configured
 * @param[in]   phase       Unused
 * @param[in]   pTarget     Target frequency to set the divider to.
 * @param[in]   bHotSwitch  Unused
 */
FLCN_STATUS
clkConfig_Divider
(
    ClkDivider              *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS     status;
    ClkTargetSignal target;

    target = *pTarget;
    CLK_MULTIPLY__TARGET_SIGNAL(target, this->divider);
    status = clkConfig_Wire(&this->super, pOutput, phase, &target, bHotSwitch);
    pOutput->freqKHz /= this->divider;

    return status;
}

