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
 * @author  Lawrence Chang
 * @author  Manisha Choudhury
 */

/* ------------------------- System Includes -------------------------------- */

#include "osptimer.h"

/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkmux.h"


/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief       Poll delay time to synchronize a glitchless mux.
 * @private
 *
 * @details     Leon Teng said that, for GA10x, we need to wait 1us before
 *              polling the _SWITCH_DIVIDER_DONE field since it may be 1
 *              before the multiplexer even begins to synchronize.
 */
 #define SWDIV_POLL_DELAY_NS 1000

/*!
 * @brief       Maximum time to synchronize a glitchless mux.
 * @private
 *
 * @details     Leon Teng said that, for GA10x switch dividers, 20-30 XTAL
 *              cycles is the maximum time for the multiplexer to synchronize
 *              and complete the switch.  At 27MHz, that would be 1.2us, so
 *              10us is very safe.  See bug 2664354.
 *
 *              Raj Jayakar said that, for GP100, 170 cycles is the maximum
 *              for the multiplexer to synchronize and complete the switch.
 *              At 27MHz, that would be 6.3us, so 10us is very safe.
 */
 #define MAX_TIMEOUT_NS 10000


/* ------------------------- Type Definitions ------------------------------- */

typedef struct ClkProgram_IsSynced_Arg_Mux
{
    const ClkMux    *this;
    ClkPhaseIndex    phase;
} ClkProgram_IsSynced_Arg_Mux;


/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(Mux);


/* ------------------------- Prototypes ------------------------------------- */

static LW_INLINE ClkSignalNode clkRead_FindMatch_Mux(const ClkMux *this, LwU32 muxRegData);
LwBool clkProgram_IsSynced_Mux(void *pArgs)     // pArgs is ClkProgram_IsSynced_Arg_Mux*
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_IsSynced_Mux");


/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @memberof    ClkMux
 * @brief       Find a match in the field value map.
 * @see         ClkMux::muxValueMap
 *
 * @details     This function uses 'muxValueMap' to translate the data read
 *              from the mux register into the node of the selected input.
 *
 *              If there is no match, CLK_SIGNAL_NODE_INDETERMINATE is
 *              returned.
 *
 * @param[in]   this            This ClkMux object
 * @param[in]   muxRegData      Register data to find
 *
 * @return                                      Node of the matching element.
 * @retval      CLK_SIGNAL_NODE_INDETERMINATE   There is no match.
 */
static LW_INLINE LwU8
clkRead_FindMatch_Mux
(   const ClkMux *this,
    LwU32 muxRegData
)
{
    LwU8 i;
    LwU8 count = this->count;

    // Search the map.
    for (i = 0; i < count; ++i)
    {
        if (CLK_MATCHES__FIELDVALUE(this->muxValueMap[i], muxRegData))
        {
            return i;
        }
    }

    // There is no match.
    return CLK_SIGNAL_NODE_INDETERMINATE;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @brief       read the frequency of the active input from hardware
 *
 * @details     This function read the mux's register to determine which of
 *              its input nodes is selected.  It then reads that input node
 *              and returns its clock signal.
 *
 * @return      Unless there is an error with this mux, return the status
 *              returned by the input node when clkRead was called on it.
 */
FLCN_STATUS clkRead_Mux
(
    ClkMux      *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    FLCN_STATUS     status;
    LwU32           muxRegData;
    ClkSignalNode   node;       // Index of the input node
    LwU8            i;          // Iterator
    ClkPhaseIndex   p;          // Iterator

    //
    // If a cycle has been detected, we need to break the relwrsive chain of
    // calls.
    //
    if (this->super.bCycle)
    {
        //
        // If this is part of the active path, and a cycle is detected, then
        // there is a big problem.  The hardware is in a bad state.
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

    // Read the value of the register and get the node from it
    muxRegData = CLK_REG_RD32(this->muxRegAddr);
    node = clkRead_FindMatch_Mux(this, muxRegData);

    //
    // If value in the register is not in the map, either it is gated, or the
    // hardware is lwrrently in an invalid state.
    //
    if (node == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        if (!CLK_MATCHES__FIELDVALUE(this->muxGateField, muxRegData))
        {
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    // Make sure the input is hooked up to something
    else
    {
        if (this->input[node] == NULL)
        {
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    //
    // Everything turned out ok.  Read all of the input objects except the one
    // selected by this mux.  Each of these inputs is inactive whether or not
    // this mux is active.
    //
    // The values returned in 'pOutput' is discarded and errors are ignored.
    // The purpose of these calls is to update the various ClkFreqSrc data
    // structures to reflect the hardware state.
    //
    this->super.bCycle = LW_TRUE;
    for (i = 0; i < this->count; i++)
    {
        ClkFreqSrc *pInput = this->input[i];
        if (pInput != NULL && i != node)
        {
            clkRead_FreqSrc_VIP(pInput, pOutput, LW_FALSE);
        }
    }

    //
    // If there is no active input, we're done.  This can happen if the VBIOS
    // devinit and/or hardware initialize the mux to an input that is not
    // supported in the Clocks 3.x schematic DAG.  That's okay, but after the
    // first time we program it, we won't switch it back to that path.
    //
    if (node == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        status = FLCN_OK;
    }

    //
    // Read the input selected by this mux (if any).  That input is active only
    // if this mux is along the active path.
    //
    else
    {
        status = clkRead_FreqSrc_VIP(this->input[node], pOutput, bActive);
    }

    this->super.bCycle = LW_FALSE;

    // Set up the phase array as well
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p].inputIndex = node;
    }

    // Add this mux to the output path
    if (node == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        *pOutput = clkEmpty_Signal;
    }

    else
    {
        pOutput->path = CLK_PUSH__SIGNAL_PATH(pOutput->path, node);
    }

    return status;
}


/*******************************************************************************
    Configuration
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @brief       configure the mux to be programmed
 *
 * @details     This function pops a node off of the target signal path and
 *              uses it to set the active input node.  It then calls clkConfig
 *              on that node and returns the result.
 *
 *              The pTarget parameter must contain a signal path with a non
 *              indeterminate node, and the node must also correspond to a mux
 *              input that is actually connected to something.  If these
 *              criteria are not met, the function will return
 *              FLCN_ERR_ILWALID_PATH.
 *
 * @return      If the node at the head of the path was indeterminate or
 *              corresponds to a disconnected input, return FLCN_ERR_ILWALID_PATH.
 *              Otherwise, return the status of the input.
 */
FLCN_STATUS clkConfig_Mux
(
    ClkMux                  *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    FLCN_STATUS status;
    ClkTargetSignal inputTarget;
    ClkPhaseIndex p;
    LwBool bInputHotSwitch;

    // The index of the active node
    ClkSignalNode node;

    // Check for cycles
    if (this->super.bCycle)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_CYCLE_DETECTED;
    }

    // Get value off of top of stack
    node = CLK_HEAD__SIGNAL_PATH(pTarget->super.path);

    // If the node is indeterminate (i.e. don't care), then keep it unchanged.
    if (node == CLK_SIGNAL_NODE_INDETERMINATE)
    {
        node = this->phase[phase-1].inputIndex;
    }

    // Error if the node does not exist.
    if (node > this->count || !this->input[node])
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_PATH;
    }

    // Error if this is a glitchy mux and we intend to switch it while active
    if (bHotSwitch && this->glitchy &&
        node != this->phase[phase-1].inputIndex)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_PATH;
    }

    //
    // Update the phase array for this and future phases.
    // This mux is unused and perhaps can be gated.
    //
    for (p = phase; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p].inputIndex = node;
    }

    //
    // Check to see if input will be active while it is being programmed.
    // The input will be active if the mux is active and has the same input
    // this phase as it did the previous phase.
    //
    bInputHotSwitch = bHotSwitch &&
        (this->phase[phase].inputIndex == this->phase[phase - 1].inputIndex);

    inputTarget = *pTarget;
    inputTarget.super.path = CLK_POP__SIGNAL_PATH(pTarget->super.path);

    this->super.bCycle = LW_TRUE;
    status = clkConfig_FreqSrc_VIP(
        this->input[node],
        pOutput,
        phase,
        &inputTarget,
        bInputHotSwitch);
    this->super.bCycle = LW_FALSE;

    // Add this node to the signal path.
    pOutput->path = CLK_PUSH__SIGNAL_PATH(pOutput->path, node);

    // Done
    return status;
}


/*******************************************************************************
    Programming
*******************************************************************************/

/*!
 * @brief       Program the multiplexer
 * @see         ClkFreqSrc_Virtual::clkProgram
 *
 * @details     This programs the mux in several different steps.
 *
 *              1.  Program the input of the mux.
 *
 *              2.  Program the mux itself by setting its register to the input
 *                  specified in the phase array.  This is the "active" input.
 *
 *              3.  If the mux has a status register, keep checking the status
 *                  register of the mux until it has synced up, or timed out.
 *
 * @param[in]   this    The mux whose input is to be programmed
 * @param[in]   phase   The phase to program the input to.
 */
FLCN_STATUS clkProgram_Mux
(
    ClkMux          *this,
    ClkPhaseIndex    phase
)
{
    ClkFreqSrc     *pInputObject;
    FLCN_STATUS     status;
    LwU32           muxRegDataOld;
    LwU32           muxRegDataNew;
    LwU8            inputIndex;
    ClkProgram_IsSynced_Arg_Mux arg =
    {   .this     = this,
        .phase    = phase
    };

    // clkConfig should prevent cycles this from being programmed.
    PMU_HALT_COND(!this->super.bCycle);

    // Get the input node number.  clkConfig should prevent an invalid index.
    inputIndex = this->phase[phase].inputIndex;
    PMU_HALT_COND(inputIndex < this->count);

    //
    // Get input object.  Assuming that clkConfig was exelwted successfully,
    // 'inputIndex' was selected so that this pointer cannot be NULL.
    // Nonetheless, if this is NULL, the PMU halts (per CL 23619977), so
    // there is no point in asserting here.  ClkFreqSrc objects are generally
    // less than 256 bytes in size.
    //
    pInputObject = this->input[inputIndex];

    // Program the active input.
    this->super.bCycle = LW_TRUE;
    status = clkProgram_FreqSrc_VIP(pInputObject, phase);
    this->super.bCycle = LW_FALSE;
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Ungate the multiplexer and switch it to the active input.
    //
    // Do not write the register unless the value has changed.  This is important
    // because register writes can trigger actions in the hardware,  For example,
    // a write to LW_PVTRIM_SYS_DISPPLL_CFG on GA10x causes the HPLL to lose it
    // lock (per bug 3262218/123).
    //
    muxRegDataOld = muxRegDataNew = CLK_REG_RD32(this->muxRegAddr);
    muxRegDataNew = CLK_APPLY__FIELDVALUE(this->muxValueMap[inputIndex], muxRegDataNew);
    if (muxRegDataOld != muxRegDataNew)
    {
        CLK_REG_WR32(this->muxRegAddr, muxRegDataNew);
    }

    // There is no need to wait on this multiplexer.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_SWDIV_SUPPORTED)
    if (this->muxDoneField.mask == 0x00000000u)
#else
    if (this->statusRegAddr == 0x00000000u)
#endif
    {
        return FLCN_OK;
    }

    // Wait for synchronization to begin.
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_SWDIV_SUPPORTED)
    OS_PTIMER_SPIN_WAIT_NS(SWDIV_POLL_DELAY_NS);
#endif

    //
    // Wait up to 10us for the multiplexer to sync.  1-2 us is typical.
    //
    // Exit on success.
    //
    // NOTE: 'lwrtosLwrrentTaskDelayTicks' allows other tasks to run while this
    // task waits, but the typical delay is 30us, which is too much.
    //
    if (OS_PTIMER_SPIN_WAIT_NS_COND(clkProgram_IsSynced_Mux, &arg, MAX_TIMEOUT_NS))
    {
        return FLCN_OK;
    }

    // Check one more time to make sure we're not caught in a race condition.
    if (clkProgram_IsSynced_Mux(&arg))
    {
        return FLCN_OK;
    }

    // Failed to sync.
    return FLCN_ERR_TIMEOUT;
}


/*!
 * @brief       Has the mux synced to the target?
 * @see         OS_PTIMER_COND_FUNC
 * @memberof    ClkMux
 *
 * @details     This function returns true if the multiplexer has finished
 *              synchronization.  Essentially, this means that the mux has
 *              completed the switch.
 *
 *              The field to check differs in SWDIVs (GA10x and after) v. older
 *              OSMs and LDIVs.  See source code comments for details.
 *
 * @note        This function's signature conforms to OS_PTIMER_COND_FUNC
 *              so that it can be called by OS_PTIMER_SPIN_WAIT_NS_COND.
 *              As such, it must have an entry in pmu_sw/build/Analyze.pm
 *              for $fpMappings.
 *
 * @warning     pArgs is typecast to a ClkProgram_IsSynced_Arg_Mux pointer.
 *
 * @param[in]   pArgs   Pointer to this ClkProgram_IsSynced_Arg_Mux object
 *
 * @retval      true    The mux has switched.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_SWDIV_SUPPORTED)
LwBool clkProgram_IsSynced_Mux(void *pArgs)
{
    // The argument points to the object.
    const ClkProgram_IsSynced_Arg_Mux *pArgsTmp = pArgs;

    // Read the current value of the mux register.
    LwU32 muxRegData = CLK_REG_RD32(pArgsTmp->this->muxRegAddr);

    // True iff the mux has successfully switched
    return CLK_MATCHES__FIELDVALUE(pArgsTmp->this->muxDoneField, muxRegData);
}

#else
LwBool clkProgram_IsSynced_Mux(void *pArgs)
{
    // The argument points to the object.
    const ClkProgram_IsSynced_Arg_Mux *pArgsTmp = pArgs;

    // Read the current value of the status register.
    LwU32 statusRegData = CLK_REG_RD32(pArgsTmp->this->statusRegAddr);

    // Index of the active input for this phase
    LwU8 inputIndex = pArgsTmp->this->phase[pArgsTmp->phase].inputIndex;

    // True iff the mux has successfully switched
    return CLK_MATCHES__FIELDVALUE(pArgsTmp->this->muxValueMap[inputIndex], statusRegData);
}
#endif


/*******************************************************************************
    Clean-Up
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @brief       cleanup the mux
 *
 * @details     The mux it not active, it's register is programmed to its gate
 *              value to save power.
 *
 *              The mux also iterates through its input array and calls
 *              clkCleanup on all of its inputs.
 *
 * @return      Unless a cycle was detected, return the worst status of all of
 *              the input nodes.
 */
FLCN_STATUS clkCleanup_Mux
(
    ClkMux  *this,
    LwBool   bActive
)
{
    // Keep track to the worst status encountered so far
    FLCN_STATUS inputStatus;
    FLCN_STATUS worstStatus;

    // Iterators
    LwU8 activeIndex;
    ClkPhaseIndex p;
    LwU8 cleanIndex;

    // Make sure this isn't being called in a cycle
    if (this->super.bCycle)
    {
        //
        // Cycles are only a problem if they are part of the active clock
        // path.  The clock DAG has cycles built into it, and because
        // clkCleanup traverses the entire DAG it will always encounter these
        // cycles as part of its normal operation.
        //
        if (bActive)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_CYCLE_DETECTED;
        }
        else
        {
            return FLCN_OK;
        }
    }

    activeIndex = this->phase[CLK_MAX_PHASE_COUNT - 1].inputIndex;
    worstStatus = FLCN_OK;

    this->super.bCycle = LW_TRUE;

    // Iterate through all input indices
    for (cleanIndex = 0; cleanIndex < this->count; cleanIndex++)
    {
        // The input is active only if this mux is active.
        LwBool bInputActive = bActive && (cleanIndex == activeIndex);

        // Get the input to be cleaned
        ClkFreqSrc *pInput = this->input[cleanIndex];

        // Clean the input if there is something there
        if (pInput != 0)
        {
            inputStatus = clkCleanup_FreqSrc_VIP(pInput, bInputActive);
            worstStatus = worstStatus == FLCN_OK && inputStatus != FLCN_OK ?
                inputStatus : worstStatus;
        }
    }

    this->super.bCycle = LW_FALSE;

    //
    // Gate this mux to save power its not active and an error was not
    // encountered during programming.
    //
    if (!bActive && this->muxGateField.mask)
    {
        LwU32 muxRegDataOld;
        LwU32 muxRegDataNew;
        muxRegDataOld = muxRegDataNew = CLK_REG_RD32(this->muxRegAddr);
        muxRegDataNew = CLK_APPLY__FIELDVALUE(this->muxGateField, muxRegDataNew);
        if (muxRegDataOld != muxRegDataNew)
        {
            CLK_REG_WR32(this->muxRegAddr, muxRegDataNew);
        }
    }

    //
    // Update the mux's phase array to reflect the last phase, which reflects
    // the hardware state.  This is an essential post-condition for clkCleanup
    //
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    return worstStatus;
}


/*******************************************************************************
    Print
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkMux
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_Mux
(
    ClkMux          *this,
    ClkPhaseIndex    phaseCount
)
{
    // Print the input for each programming phase.
    CLK_PRINTF("MUX Active Input:");
    for (ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        CLK_PRINTF(" phase[%u]=%u", (unsigned) p, (unsigned) this->phase[p].inputIndex);
    }
    CLK_PRINTF("\n");

    // Read and print hardware state.
    CLK_PRINT_REG("MUX", this->muxRegAddr);
#if ! PMUCFG_FEATURE_ENABLED(PMU_CLK_SWDIV_SUPPORTED)
    CLK_PRINT_REG("MUX", this->statusRegAddr);          // Often unused
#endif

    // Print information for each active input object in input-index order.
    for (LwU8 i = 0; i < this->count ; ++i)
    {
        // Determine how many phases (if any) have this input as active.
        ClkPhaseIndex inputPhaseCount = 0;
        for(ClkPhaseIndex p = 0; p < phaseCount; ++p)
        {
            ++inputPhaseCount;
        }

        // Print heading before printing the input for clarity.  (The phase count
        // is not actually all that useful.)  Skip any input which is not active
        // in at least one phase.
        if (inputPhaseCount)
        {
            CLK_PRINTF("MUX Input %u: active in %u phases\n", (unsigned) i, (unsigned) inputPhaseCount);
            clkPrint_FreqSrc_VIP(this->input[i], phaseCount);
        }
    }
}
#endif
