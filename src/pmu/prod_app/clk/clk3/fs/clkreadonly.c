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

#include "clk3/clksignal.h"
#include "clk3/fs/clkreadonly.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE__FREQSRC(ReadOnly);


/*******************************************************************************
    Virtual Function Definitions
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @brief       Get the frequency.
 *
 * @details     This reads the input to the readonly, stores that value in the
 *              cache, and then returns the value.
 *
 * @memberof    ClkReadOnly
 *
 * @param[in]   this        Instance of ClkReadOnly from which to read
 * @param[out]  pOutput     Frequency will be written to this struct.
 */
FLCN_STATUS
clkRead_ReadOnly
(
    ClkReadOnly     *this,
    ClkSignal       *pOutput,
    LwBool           bActive
)
{
    // It's okay if we can use the cache.
    FLCN_STATUS status = FLCN_OK;

    //
    // Read the input unless the cache is valid.
    // Note that 'ClkCacheUpdater' may update the cache.
    //
    if (!this->cache.freqKHz)
    {
        // Ensure that the entire cache signal structure is reset, not just the frequency.
        this->cache = clkReset_Signal;

        // Read the input.
        status = clkRead_Wire(&(this->super), &(this->cache), bActive);

        // Make sure the output signal is reset in case of error.
        if (status != FLCN_OK)
        {
            this->cache = clkReset_Signal;
        }
    }

    // Return the signal stored in the cache.
    *pOutput = this->cache;

    // Done
    return status;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @brief       Configure the readonly object.
 *
 * @memberof    ClkReadOnly
 *
 * @details     ClkReadOnly is not configurable, so this will simply return the
 *              cached signal.
 *
 * @param[in]   this            Instance of ClkReadOnly_Object to configure.
 * @param[out]  pOutput         Actual cached clock signal
 * @param[in]   phase           Unused
 * @param[in]   pTarget         Target clock signal frequency and related
 * @param[in]   bHotSwitch      Unused
 *
 * @retval      FLCN_OK                         Success
 * @retval      FLCN_ERR_ILWALID_PATH           Path is not empty.
 * @retval      FLCN_ERR_MISMATCHED_TARGET      Target parameter mismatch
 * @retval      FLCN_ERR_FREQ_NOT_SUPPORTED     Frequency is not within target range.
 */
FLCN_STATUS
clkConfig_ReadOnly
(
    ClkReadOnly             *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS status;

    // Use the cached output if valid.
    if (this->cache.freqKHz)
    {
        *pOutput = this->cache;
    }

    //
    // Read the hardware (input) if the cache is not valid.
    // Swap in the correct IMEM oerlay.
    //
    else
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadPerfDaemon)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkRead_ReadOnly(this, pOutput, bHotSwitch);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            return status;
        }
    }

    // The path must conform to the target parameter.
    if (!clkConforms_SignalPath(pOutput->path, pTarget->super.path))
    {
        return FLCN_ERR_ILWALID_PATH;
    }

    // Other clock signal characteristics must conform to the target parameter.
    if (!CLK_CONFORMS_IGNORING_PATH__SIGNAL(*pOutput, pTarget->super))
    {
        return FLCN_ERR_MISMATCHED_TARGET;
    }

    // Check that the crystal is within the target range.
    if (!LW_WITHIN_INCLUSIVE_RANGE(pTarget->rangeKHz, pOutput->freqKHz))
    {
        return FLCN_ERR_FREQ_NOT_SUPPORTED;
    }

    // All good.
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @brief       Do nothing
 * @memberof    ClkReadOnly
 *
 * @param[in]   this        the instance of ClkReadOnly to program.
 *
 * @param[in]   phase       the phase of the phase array to set the readonly to.
 *
 * @return                  status of the input.
 */
FLCN_STATUS
clkProgram_ReadOnly
(
    ClkReadOnly     *this,
    ClkPhaseIndex    phase
)
{
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @brief       Cleanup the input to the readonly object
 *
 * @details     There are no cleanup actions to be done for a ClkReadOnly object
 *              as it has no registers of its own.  However, it calls the
 *              clkCleanup function of its input object.  The input is always
 *              considered active so that it doesn't power down or gate anything.
 *
 * @memberof    ClkReadOnly
 *
 * @param[in]   this        the instance of ClkReadOnly_Object to clean up.
 *
 * @param[in]   bActive     whether or not this is part of the path current
 *                          providing the clock signal
 *
 * @return                  status of the input object.
 */
FLCN_STATUS
clkCleanup_ReadOnly
(
    ClkReadOnly *this,
    LwBool       bActive
)
{
    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkReadOnly
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_ReadOnly
(
    ClkReadOnly     *this,
    ClkPhaseIndex    phaseCount
)
{
    clkPrint_Signal(&this->cache, "Read-Only");
}
#endif
