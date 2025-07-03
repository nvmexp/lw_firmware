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
 */

#include "clk3/fs/clkwire.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE__FREQSRC(Wire);


/*******************************************************************************
    Virtual Function Implementations
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @brief       Get the frequency in KHz of the input to the wire.
 *
 * @memberof    ClkWire
 *
 * @param[in]   this        the instance of ClkWire_Object to read the frequency
 *                          of.
 *
 * @param[out]  pOutput     a pointer to the struct to which the frequency will
 *                          be written.
 *
 * @param[in]   bActive     whether or not this object is part of the path that
 *                          is lwrrently generating the clock signal.
 *
 * @return                  status of the input device.
 */
FLCN_STATUS
clkRead_Wire
(
    ClkWire     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    FLCN_STATUS status;
    if (this->super.bCycle)
    {
        //
        // If this is part of the active path, and a cycle is detected, then
        // there is a big problem.  The hardware is in a bad state
        //
        if (bActive)
        {
            return FLCN_ERR_CYCLE_DETECTED;
        }

        //
        // There is a cycle but it isn't part of the active path.  This is
        // expected to happen as there are some cycles built into the clock
        // topology.
        //
        else
        {
            return FLCN_OK;
        }
    }

    // Set the flag before daisy-chaining so that lwcles are detected.
    this->super.bCycle = LW_TRUE;
    status = clkRead_FreqSrc_VIP(this->pInput, pOutput, bActive);
    this->super.bCycle = LW_FALSE;

    // Done
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @brief       Configure the the the wire object.
 *
 * @details     This simply relays all of the information to the input of the
 *              wire object.
 *
 * @memberof    ClkWire
 *
 * @param[in]   this        the instance of ClkWire_Object to configure.
 *
 * @param[out]  pOutput     a pointer to the struct to which the actual
 *                          frequency will be written
 *
 * @param[in]   phase       the index of the phase array to configure
 *
 * @param[in]   pTarget     a pointer to a ClkTargetSignal that defines the
 *                          target frequency to try to configure the object to.
 *
 * @param[in]   bHotSwitch  Whether or not this object will be programmed while
 *                          it is part of the path that is lwrrently generating
 *                          the clock singal during the given phase.
 *
 * @return                  status of the input device.
 */
FLCN_STATUS
clkConfig_Wire
(
    ClkWire                 *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS status;

    // Fail if the target path represents a cycle through the schematic.
    if (this->super.bCycle)
    {
        return FLCN_ERR_CYCLE_DETECTED;
    }

    // Configure the onput setting the flag to detect cycles.
    this->super.bCycle = LW_TRUE;
    status = clkConfig_FreqSrc_VIP(
        this->pInput,
        pOutput,
        phase,
        pTarget,
        bHotSwitch);
    this->super.bCycle = LW_FALSE;

    // Done
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @brief       Set the phase of the multiplier
 *
 * @details     The wire has no registers of its own to configure, so
 *              this just calls program on the input of the ClkWire object.
 *
 * @memberof    ClkWire
 *
 * @param[in]   this        the instance of ClkWire_Object to program.
 *
 * @param[in]   phase       the phase of the phase array to set the wire to.
 *
 * @return                  status of the input.
 */
FLCN_STATUS
clkProgram_Wire
(
    ClkWire         *this,
    ClkPhaseIndex    phase
)
{
    FLCN_STATUS status;

    //
    // If there is a cycle, clkConfig should have prevented this from being
    // programmed in the first place.
    //
    PMU_HALT_COND(!this->super.bCycle);

    // Program the input using the flag to make sure there are no cycles.
    this->super.bCycle = LW_TRUE;
    status = clkProgram_FreqSrc_VIP(this->pInput, phase);
    this->super.bCycle = LW_FALSE;

    // Done.
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @brief       Cleanup the input to the wire object
 *
 * @details     There are no cleanup actions to be done for a ClkWire object as
 *              it has no registers of its own, however, it will call the
 *              Cleanup function of its input object.
 *
 * @memberof    ClkWire
 *
 * @param[in]   this        the instance of ClkWire_Object to clean up.
 *
 * @param[in]   bActive     whether or not this is part of the path current
 *                          providing the clock signal
 *
 * @return                  status of the input object.
 */
FLCN_STATUS
clkCleanup_Wire
(
    ClkWire     *this,
    LwBool       bActive
)
{
    FLCN_STATUS status;

    if (this->super.bCycle)
    {
        //
        // Cycles are only a problem if they are part of the active clock path.
        // The clock DAG has cycles built into it, and because clkCleanup
        // traverses the entire DAG it will always encounter these cycles as
        // part of its normal operation.
        //
        if (bActive)
        {
            return FLCN_ERR_CYCLE_DETECTED;
        }
        else
        {
            return FLCN_OK;
        }
    }

    // Clean up the input using the flag to detect cycles in the schematic.
    this->super.bCycle = LW_TRUE;
    status = clkCleanup_FreqSrc_VIP(this->pInput, bActive);
    this->super.bCycle = LW_FALSE;

    // Done.
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkWire
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 *
 */
#if CLK_PRINT_ENABLED
void
clkPrint_Wire
(
    ClkWire         *this,
    ClkPhaseIndex    phaseCount
)
{
    clkPrint_FreqSrc_VIP(this->pInput, phaseCount);
}
#endif
