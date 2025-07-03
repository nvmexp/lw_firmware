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
 * @author  Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkhpll.h"
#include "clk3/generic_dev_trim.h"


/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief       Delay between power-up and programming.
 */
#define DELAY_READY_NS 5000U

/*!
 * @brief       Maximum time to wait for HPLL to lock
 * @see         Bug 200677168/218
 *
 * @details     According to section 5 of HPLL16G_SSD_DYN_B1.doc, this value
 *              is a function of MDIV.  As of GA10x, the POR is MDIV=1, so the
 *              typical lock time is 30us and the maximum lock time is 50us.
 *              Pranav Lodaya suggested that we use 100us for margin.
 *
 *              However, per bug 200677168, we've determined that, under stress,
 *              there can be a significant amount of backlog in the bus.  As such,
 *              we use 1000us.
 */
#define LOCK_TIME_MAX_NS 1000000U


/*!
 * @brief       Number of times to power cycle the HPLL on lock failure
 */
#define RETRY_COUNT_MAX 5U


/*!
 * @brief       Number of NDIV steps
 * @see         hw/PLL_studies/scripts/HPLL.pl
 * @see         Section 8 of HPLL16G_SSD_DYN_B1.doc
 */
#define NDIV_FRAC_STEP_COUNT 4U


/*!
 * @brief       Minimum NDIV step size
 * @see         hw/PLL_studies/scripts/HPLL.pl
 * @see         Section 8 of HPLL16G_SSD_DYN_B1.doc
 *
 * @note        NDIV_FRAC_STEP_TIMER should be initialized to 3.
 */
#define NDIV_FRAC_STEP_MIN 1000U


/*!
 * @brief       Implicit denominator for NDIV callwlations
 *
 * @details     Fractional NDIV is in units of 1/8192, so integer callwlations
 *              ilwolving NDIV must be done at 8192 times the natural value.
 */
#define NDIV_DENOMINATOR 8192U


/*!
 * @brief       Limits for HPLL per HPLL16G_SSD_DYN_B1.doc etc.
 * @see         Bug 2557340/48
 * @see         Sec 5 in hw/libs/common/analog/lwpu/doc/HPLL16G_SSD_DYN_B1.doc
 * @see         6.4.2 in hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_IAS.docx
 *
 * @note        Since MDIV, NDIV, PLDIV, and REF are a single value, clkConfig
 *              has no loops (i.e. O(1)).  In legacy code, these values were
 *              ranges, which caused clkConfig to have O(n^4) complexity.
 *
 * @note        There is no point in having active logic to check the
 *              update range since it the update rate is reference / MDIV,
 *              both of which are fixed.  Specifically, the update rate is
 *              27MHz / 1 = 27MHz.  HPLL16G_SSD_DYN_B1 requires it to be between
 *              19MHz and 38MHz.
 *
 * @note        Similarly, there is no point in having active logic for both
 *              the VCO frequency and NDIV since VCO = reference / NDIV * NDIV,
 *              and MDIV is a fixed value.  It's more colwenient to use NDIV
 *              than VCO in the clkConfig logic.  This table compares the
 *              NDIV values in HPLL16G_SSD_DYN_B1:
 *                      Doc     Actual      Callwlation
 *              Min      20         30      27MHz /  800MHz rounded up
 *              Max     255         60      27MHz / 1620MHz rounded down
 */
//!{
#define TARGET_REF_KHZ            27000U    // Reference (input) frequency
#define TARGET_VCO_MIN_KHZ       800000U    // Reference / MDIV * NDIV
#define TARGET_VCO_MAX_KHZ      1620000U    // Reference / MDIV * NDIV
#define TARGET_MDIV                   1U    // Fixed per bug 2557340/48
#define TARGET_NDIV_MIN   LW_UNSIGNED_DIV_CEIL(TARGET_VCO_MIN_KHZ, TARGET_REF_KHZ)  //  20 in doc
#define TARGET_NDIV_MAX                      (TARGET_VCO_MAX_KHZ / TARGET_REF_KHZ)  // 255 in doc
//!}


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(HPll);

const ClkHPllPhase clkReset_Phase =
{
    .bHotSwitch = LW_FALSE,
    .mdiv       = 0,
    .ndiv       = 0,
    .pldiv      = 0
};


/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */

//
// Functions here that are inlined are called only once.  This reduces stack
// pressure without increasing the IMEM size.
//
LwBool clkProgram_IsLocked_HPll(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_IsLocked_HPll");

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implementations ------------------------ */

/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkHPll
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 *
 *              The bActive member of the phase array, however, is an exception
 *              to this.  clkRead sets the bActive member of phase 0 to match
 *              the hardware. For all subsequent phases, however, it is set to
 *              false.
 *
 * @return      The status returned from calling clkRead on the input
 */
FLCN_STATUS
clkRead_HPll
(
    ClkHPll     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    FLCN_STATUS  status;

    // Initialize to the reset state (mostly zeroes).
    ClkHPllPhase hardwarePhase = clkReset_Phase;

    // If this PLL is not running, set coefficients to be zero
    LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
    if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _NO, cfgRegData))
    {
        *pOutput      = clkReset_Signal;
        status        = FLCN_OK;
    }

    //
    // If the PLL is on, then read the coefficients from hardware, and then
    // read the input (reference frequency), and do the callwlations.
    //
    else
    {
        //
        // Analog PLLs have one register for coefficients, Hybrids have two.
        // MDIV and PLDIV are in the same register/field for all PLL types.
        //
        LwU32 coeffRegData  = CLK_REG_RD32(this->coeffRegAddr);
        hardwarePhase.mdiv  = DRF_VAL(_PTRIM, _SYS_PLL_COEFF, _MDIV,  coeffRegData);
        hardwarePhase.pldiv = DRF_VAL(_PTRIM, _SYS_PLL_COEFF, _PLDIV, coeffRegData);

        //
        // NDIV is the feedback divider, so in effect, it is a multiplier.
        // NDIV can be set to fractional values.  _NDIV is the integer portion.
        // _NDIV_FRAC has an implicit denominator of 8192.  In short:
        // NDIVeff = NDIV_INT[7:0] + (1/8192) x NDIV_FRAC[15:0]
        //
        LwU32 cfg4RegData  = CLK_REG_RD32(this->cfg4RegAddr);
        LwU16 ndivFrac     = DRF_VAL(_PTRIM, _SYS_HPLL_CFG4, _NDIV_FRAC, cfg4RegData);
        hardwarePhase.ndiv = DRF_VAL(_PTRIM, _SYS_HPLL_CFG4, _NDIV,      cfg4RegData);

        // Get the input clock signal.
        status = clkRead_Wire(&this->super, pOutput, bActive);

        // 64-bit arithmetic is required for full precision below.
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            //
            // Callwlate the output frequency keeping in mind that 'n' is not an integer:
            //      output = input / m * n / p
            // where 'output' is the PLL output frequency (outputFreqKHz) and
            // 'input' is the reference frequency (pOutput->freqKHz).
            //
            // Since 'n' has integer (NDIV_INT) and fractional (NDIV_FRAC) parts:
            //      output = input * (NDIV_INT + NDIV_FRAC/NDIV_DENOMINATOR)
            //      output = input * (NDIV_INT * NDIV_DENOMINATOR +NDIV_FRAC) / NDIV_DENOMINATOR
            //
            // 'outputFreqKHz' is the frequency times 8192 (NDIV_DENOMINATOR)
            // until the division.  This is done to accommodate fractional NDIV.
            //
            // NDIV_FRAC is not supported by clkConfig/clkProgram as of GA10x.
            // clkRead supports it since it is a valid hardware state and we plan
            // to fully support NDIV_FRAC in the near future.
            //
            //
            // We know 'outputFreqKHz' will not overflow because:
            //
            //      NDIV_DENOMINATOR is 14 bits, and
            //      hardwarePhase.ndiv is 8 bits, so
            //      their product is 22 bits.
            //
            //      ndivFrac is 16 bits,
            //      which is less than 22 bits, so
            //      the NDIV sum is at most 23 bits.
            //
            //      pOutput->freqKHz is 32 bits, and
            //      the NDIV sum is at most 23 bits, so
            //      their product is 55 bits,
            //      which is less than 64 bits of 'outputFreqKHz'.
            //
            LwU64 outputFreqKHz = pOutput->freqKHz;
            outputFreqKHz *= hardwarePhase.ndiv * NDIV_DENOMINATOR + ndivFrac;
            outputFreqKHz /= hardwarePhase.mdiv;
            outputFreqKHz /= hardwarePhase.pldiv;
            outputFreqKHz /= NDIV_DENOMINATOR;          // Units for ndivFrac
            pOutput->freqKHz = (LwU32) outputFreqKHz;
            PMU_HALT_COND(outputFreqKHz < LW_U32_MAX);     // We're fast, but not that fast. :)
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    // Update the phase array.
    ClkPhaseIndex p;
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = hardwarePhase;
    }

    //
    // 'source' indicates which sort of object is closest to the root:
    // PLL, NAFLL, XTAL, OSM; and if it is a PLL, whether it is the domain-
    // specific PLL (e.g. DISPPLL or HUBPLL) or one of the SPPLLs.  As such,
    // on the return, this logic overrides any previous 'source' setting
    // from a child in the schematic dag.
    //
    // In practice, HPLLs are domain-specific, so 'this->source' is generally
    // LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL
    //
    pOutput->source = this->source;

    // Done
    return status;
}


/*******************************************************************************
    Configuration
*******************************************************************************/

/*!
 * @memberof    ClkHPll
 * @brief       Configure the hardware to be programmed to a certain frequency
 * @see         ClkFreqSrc_Virtual::clkConfig
 * @see         Bug 2557340/48
 * #see         Bug 2789595
 * @see         Sec 5 in hw/libs/common/analog/lwpu/doc/HPLL16G_SSD_DYN_B1.doc
 * @see         6.4.2 in hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_IAS.docx
 *
 * @details     The logic herein assumes that the following are fixed points:
 *              - Reference (input) frequency per TARGET_REF_KHZ,
 *              - MDIV per TARGET_MDIV, and
 *              - PLDIV per this->pdivTarget.
 *
 *              Per dislwssion in bug 2789595, there is an issue with undershoot
 *              when NDIV_FRAC is programmed to a positive value.  When dynamic
 *              ramp is engaged on the subsequent programming, NDIV_FRAC is zeroed
 *              by hardware.  This is bad if we're increasing frequencies (or more
 *              precisely, if the target is higher than the HPLL output frequency).
 *
 *              There is an analogous issue with overshoot if we use a negative
 *              value in NDIV_RRAC.
 *
 *              The consensus is to disable the NDIV_FRAC feature and always
 *              use zero at the cost of some power.
 */
FLCN_STATUS
clkConfig_HPll
(
    ClkHPll               *this,
    ClkSignal             *pOutput,
    ClkPhaseIndex          phase,
    const ClkTargetSignal *pTarget,
    LwBool                 bHotSwitch
)
{
    // Initialize to the reset state (mostly zeroes).
    this->phase[phase] = clkReset_Phase;

    // clkProgram will need to know.
    this->phase[phase].bHotSwitch = bHotSwitch;

    //
    // First configure the input to the correct reference frequency.
    // This is generally a NOOP (other than the sanity check) since the input
    // is generally XTAL.
    //
    ClkTargetSignal inputTarget;
    CLK_ASSIGN_FROM_FREQ__TARGET_SIGNAL(inputTarget, TARGET_REF_KHZ, *pTarget);
    FLCN_STATUS status = clkConfig_Wire(&this->super, pOutput, phase, &inputTarget, bHotSwitch);
    if (status != FLCN_OK)
    {
        return status;
    }

    // MDIV and PLDIV are constant.
    this->phase[phase].mdiv  = TARGET_MDIV;
    this->phase[phase].pldiv = this->pdivTarget;

    //
    // We can not program MDIV or PLDIV during a hot switch (i.e. while the
    // HPLL is running and being used).
    //
    if (bHotSwitch                                                  &&
       (this->phase[phase].mdiv  != this->phase[phase - 1].mdiv     ||
        this->phase[phase].pldiv != this->phase[phase - 1].pldiv))
    {
        CLK_PRINT_ALL(phase);
        CLK_PRINTF("HPLL Illegal Hot-Switch: MDIV=%u=>%u PLDIV=%u=>%u\n",
            this->phase[phase - 1].mdiv,  this->phase[phase].mdiv,
            this->phase[phase - 1].pldiv, this->phase[phase].pldiv);
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    // 64-bit arithmetic is required for full precision below.
    // Use a local block to avoid 64-bit locals from escaping.
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            //
            // Callwlate NDIV per this algebra:
            //      target = input / m * n / p
            //      target / input * m * p = n
            //      target * m * p / input = n
            //
            // 'pTarget->super.freqKHz' is 'target'.
            //
            // 'pOutput->freqKHz' is 'input' at this point.
            //
            // 'ndivPrecise' is 'n * 8192' == 'n * NDIV_DENOMINATOR' so that the
            // division operation is done in high precision.
            //
            // Per bug 2789595, NDIV_FRAC is not supported and therefore always
            // programmed to zero.  In effect, the extra bits of 'ndivPrecise' are
            // discarded.
            //
            // We know 'ndivPrecise' will not overflow because:
            //      'freqKHz' is 32 bits,
            //      'mdiv' is 8 bits,
            //      'pldiv' is 8 bits, and
            //      NDIV_DENOMINATOR is 14 bits, so
            //      their product is 62 bits.
            //
            LwU64 ndivPrecise = pTarget->super.freqKHz;     // target
            ndivPrecise *= this->phase[phase].mdiv;         // * m
            ndivPrecise *= this->phase[phase].pldiv;        // * p
            ndivPrecise *= NDIV_DENOMINATOR;                // high-precision
            ndivPrecise /= pOutput->freqKHz;                // / input

            // Discard the high-precision denominator, rounding to the nearest integer.
            ndivPrecise = LW_UNSIGNED_ROUNDED_DIV(ndivPrecise, NDIV_DENOMINATOR);

            //
            // Make sure the result is in range, not just for the hardware, but
            // also for 'ndiv' in the phase array.
            //
            if (ndivPrecise < (LwU64) TARGET_NDIV_MIN   ||
                ndivPrecise > (LwU64) TARGET_NDIV_MAX)
            {
                status = FLCN_ERR_FREQ_NOT_SUPPORTED;
                CLK_PRINT_ALL(phase);
                CLK_PRINTF("HPLL NDIV Range Assertion: %llu < %llu < %llu\n",
                    (LwU64) TARGET_NDIV_MIN, ndivPrecise, (LwU64) TARGET_NDIV_MAX);
                PMU_BREAKPOINT();
            }

            // Looks good.  Save into the phase array.
            else
            {
                this->phase[phase].ndiv = (LwU8) ndivPrecise;
                status = FLCN_OK;
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    // Was there an error in the 64-bit logic block?
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Callwlate the actual output frequency.
    //
    // We know we're not going to overflow 32-bits since the result of the
    // first multiplication is at most 27000 * 60 = 1620000.
    //
    LwU32 outputFreq = pOutput->freqKHz;       // Reference (input) frequency
    outputFreq *= this->phase[phase].ndiv;
    outputFreq /= this->phase[phase].mdiv;
    outputFreq /= this->phase[phase].pldiv;
    pOutput->freqKHz = (LwU32) outputFreq;

    // Copy this phase to all subsequent phases.
    ClkPhaseIndex p;
    for (p = phase; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = this->phase[phase];
    }

    //
    // 'source' indicates which sort of object is closest to the root:
    // PLL, NAFLL, XTAL, OSM; and if it is a PLL, whether it is the domain-
    // specific PLL (e.g. DISPPLL or HUBPLL) or one of the SPPLLs.  As such,
    // on the return, this logic overrides any previous 'source' setting
    // from a child in the schematic dag.
    //
    // In practice, HPLLs are domain-specific, so 'this->source' is generally
    // LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL
    //
    pOutput->source = this->source;

    // Done
    return status;
}


/*******************************************************************************
    Programming
*******************************************************************************/

/*!
 * @memberof    ClkHPll
 * @brief       Program the pll
 * @see         ClkFreqSrc_Virtual::clkProgram
 * @see         Confluence: display/RMCLOC/Dynamic+Ramping
 * @see         HPLL16G_SSD_DYN_B1.doc section 9.1
 *
 * details      The basic programming steps per HPLL16G_SSD_DYN_B1.doc are:
 *              1.  Check for PLL lock else return
 *              2.  Program  _CFG2_FRAC_STEP
 *              3.  Program to registers _CFG4_NDIV and _CFG4_NDIV_FRAC.
 *              4.  PLL_LOCK will deassert and initiate the frequency ramp.
 *              5.  Wait for _CFG_PLL_LOCK_TRUE before making another dynamic
 *                  frequency change.
 *
 *              This implementation uses dynamic ramp if the HPLL is enabled
 *              and locked.  This handles the hot-switch case, but it also
 *              works in the cold-switch case so long as MDIV and PLDIV are
 *              not changing.
 *
 *              If MDIV or PLDIV are changing, or if the HPLL is powered down
 *              or not locked, this implementation goes through the power-up
 *              sequence, unless this is a hot-switch. In the case of hot-switch
 *              if issues FLCN_ERR_ILWALID_STATE.  After the power-up, it
 *              programs all the coefficients.
 *
 *              If the HPLL fails to lock to the new frequency, it is powered
 *              down and the programming sequence is retried up to five times
 *              if this is a cold switch.  In older chips, the PLL would fail
 *              to lock very rarely, and one retry is generally adequate.  If
 *              it fails five times, this function returns FLCN_ERR_TIMEOUT.
 *
 * @note        This implementation is inadequate for MPLL/REFMPLL since we must
 *              query LW_PTRIM_SYS_MEM_PLL_STATUS (instead of LW_PTRIM_SYS_PLL_CFG).
 *              The field definitions (not just the address) differ.
 *
 * @param[in]   this            Frequency source object pointer
 * @param[in]   phase           The phase to program the hardware to
 *
 * @retval      FLCN_ERR_ILWALID_STATE  Illegal hot-switch
 * @retval      FLCN_ERR_TIMEOUT        PLL failed to lock five times.
 */
FLCN_STATUS
clkProgram_HPll
(
    ClkHPll *this,
    ClkPhaseIndex phase
)
{
    FLCN_STATUS status;

    // Program the input (reference frequency)
    status = clkProgram_Wire(&this->super, phase);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // If there is a timeout, try again several times unless the HPLL is being
    // hot-switched since retrying means power-cycling the HPLL. Bug 1338203
    //
    LwU8 retryRemaining = (this->phase[phase].bHotSwitch? 1 : RETRY_COUNT_MAX);

    //
    // The retry loop is exited by 'break' on success or 'return' on failure.
    //
    while (LW_TRUE)
    {
        //
        // Read the current configuration.  The config register is read/written
        // inside the retry loop to ensure that there are no pending register
        // transitions after PLL power down.  The additional delay can't hurt.
        //
        LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

        //
        // Dynamically ramp only if all prerequisites are met:
        // - The HPLL is already running,
        // - The HPLL is not in sync mode,
        // - MDIV is not changing, and
        // - PLDIV is not changing.
        //
        // NDIV_FRAC is always zero per bug 2789595.
        //
        // There is no point in checking _PLL_LOCK since it is locked when this
        // function exited on the previous call (unless it timed out).  Since
        // _PLL_LOCK is latched in hardware, it goes to _FALSE only when this
        // register is written.
        //
        if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_ON, cfgRegData) &&
            FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _YES,      cfgRegData) &&
            this->phase[phase].mdiv  == this->phase[phase - 1].mdiv               &&
            this->phase[phase].pldiv == this->phase[phase - 1].pldiv)
        {
            //
            // hw/PLL_studies/scripts/HPLL.pl, which is referenced in section 8
            // of HPLL16G_SSD_DYN_B1.doc, shows the FRAC_STEP callwlations.
            //
            // However, since NDIV_FRAC is always zero, we use the minimum.
            //
            // We decided to not use NDIV_FRAC, or more specifically, always use
            // zero, because of an undershoot issue per bug 2789595.
            //
            // According to Tao Liu, FRAC_STEP and HPLL.pl apply only to the last
            // transition of a 3-transition dynamic-ramp sequence, where the
            // intermediate transitions use non-fractional NDIV values.  For
            // example, if NDIV is changing from 20.9 to 50.6, the hardware-
            // controlled sequence is this:  20.9 => 20.0 => 50.0 => 50.6.
            //
            // FRAC_STEP is generally 1/4 the delta (0.6 in this example) to
            // avoid overshoot, but with a minimum of 1000/8192.  1/4 means that
            // the last transition takes four steps.
            //
            // In the first transition, there is an undershoot of up to 26.997MHz.
            // assuming the reference (input) frequency is 27MHz and the maximum
            // delta is 8191/8192.  In the example, the first delta is 0.9,
            // so the undershoot would be 24.3MHz.
            //
            LwU32 cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
            cfg2RegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_HPLL_CFG2, _FRAC_STEP,
                            NDIV_FRAC_STEP_MIN, cfg2RegData);
            CLK_REG_WR32(this->cfg2RegAddr, cfg2RegData);

            //
            // Read back the register to make sure the write has been received
            // by the HPLL before updating the coefficients.  The data is
            // irrelevant; this is a timing issue.  See timeout failure condition
            // described in bug 200677168/163.
            //
            CLK_REG_RD32(this->cfg2RegAddr);
        }

        //
        // Dynamic ramping is the only way we can program the HPLL while
        // it is in use.  Fail if this is a hot-switch.
        //
        else if (this->phase[phase].bHotSwitch)
        {
            CLK_PRINT_ALL(phase);
            CLK_PRINTF("HPLL Hot-Switch w/o Dyn Ramp: CFG addr=0x%08x data=0x%08x MDIV=%u=>%u PLDIV=%u=>%u\n",
                this->cfgRegAddr, cfgRegData,
                this->phase[phase - 1].mdiv,  this->phase[phase].mdiv,
                this->phase[phase - 1].pldiv, this->phase[phase].pldiv);
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_STATE;
        }

        //
        // Program in the cases where dynamic ramping can not be used.
        //
        else
        {
            //
            // Make sure the HPLL is powered on and disabled.  Also disable sync
            // mode since this is a cold switch.
            //
            cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_ON, cfgRegData);
            cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE,  cfgRegData);
            cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,       cfgRegData);
            CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

            //
            // Read back the register to make sure the write has been received
            // by the HPLL before updating the coefficients.  The data is
            // irrelevant; this is a timing issue.  See timeout failure condition
            // described in bug 200677168/163.
            //
            CLK_REG_RD32(this->cfgRegAddr);

            //
            // Starting with the PLLs used on Pascal, there must be a delay of
            // 5us between power up and programming the coefficients and enablement.
            // OS_TIMER_WAIT_US has a typical minimum delay of 30us, so it's not
            // used here.
            //
            OS_PTIMER_SPIN_WAIT_NS(DELAY_READY_NS);

            //
            // Update the MDIV and PLDIV coefficients, which can be changed
            // only while the HPLL is disabled.  For this reason, clkConfig
            // errors out if we try to change them in a hot switch.
            //
            // The coefficient registers are read/written inside the retry loop
            // to ensure that there are no pending register transactions after
            // PLL power up.  The additional delay can't hurt.
            //
            LwU32 coeffRegData = CLK_REG_RD32(this->coeffRegAddr);
            coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_PLL_COEFF, _MDIV,  this->phase[phase].mdiv,  coeffRegData);
            coeffRegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_PLL_COEFF, _PLDIV, this->phase[phase].pldiv, coeffRegData);
            CLK_REG_WR32(this->coeffRegAddr, coeffRegData);

            //
            // Read back the register to make sure the write has been received
            // by the HPLL before updating NDIV.  The data is irrelevant; this
            // is a timing issue.  See timeout failure condition described in
            // bug 200677168/163.
            //
            CLK_REG_RD32(this->coeffRegAddr);
        }

        //
        // On hybrid PLLs, NDIV may be fractional.  If the HPLL is powered on
        // and enabled, this register write deasserts PLL_LOCK.
        //
        LwU32 cfg4RegData = CLK_REG_RD32(this->cfg4RegAddr);
        cfg4RegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_HPLL_CFG4, _NDIV, this->phase[phase].ndiv, cfg4RegData);
        cfg4RegData = FLD_SET_DRF_NUM(_PTRIM, _SYS_HPLL_CFG4, _NDIV_FRAC, 0, cfg4RegData);
        CLK_REG_WR32(this->cfg4RegAddr, cfg4RegData);

        //
        // Read back the register to make sure the write has been received by the
        // HPLL before enabling it.  The data is irrelevant; this is a timing
        // issue.  See timeout failure condition described in bug 200677168/163.
        //
        CLK_REG_RD32(this->cfg4RegAddr);

        // Ready to enable the HPLL if not already.
        if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _NO, cfgRegData))
        {
            cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _YES, cfgRegData);
            CLK_REG_WR32(this->cfgRegAddr, cfgRegData);
        }

        //
        // Read back the register to make sure the write has been received by the
        // HPLL before starting the timeout period.  This is a timing issue.
        // We don't actually expect it to be locked this quickly, but if it is,
        // go ahead and exit the retry loop.  See timeout failure condition
        // described in bug 200677168/163.
        //
        if (clkProgram_IsLocked_HPll(this))
        {
            break;
        }

        //
        // Wait for the HPLL to lock.  Exit on success.
        //
        // TODO: Consider using OS_TIMER_WAIT_US, which has a typical delay of
        // 30us, but has the advantage of allowing other tasks to run while we
        // wait.  It is a wrapper of lwrtosLwrrentTaskDelayTicks.
        //
        if (OS_PTIMER_SPIN_WAIT_NS_COND(clkProgram_IsLocked_HPll, this, LOCK_TIME_MAX_NS))
        {
            break;
        }

        // Check one more time to make sure we're not caught in a race condition.
        if (clkProgram_IsLocked_HPll(this))
        {
            break;
        }

        //
        // We timed out.  Give up if we've run out of retries.  Leave the HPLL
        // powered-up and enabled in the off chance is locks a but later on.
        //
        retryRemaining--;
        if (retryRemaining == 0)
        {
            CLK_PRINT_ALL(phase);
            CLK_PRINTF("HPLL Timeout: CFG addr=0x%08x data=0x%08x  CFG4 addr=0x%08x data=0x%08x  MDIV=%u=>%u PLDIV=%u=>%u\n",
                this->cfgRegAddr,  cfgRegData,
                this->cfg4RegAddr, cfg4RegData,
                this->phase[phase - 1].mdiv,  this->phase[phase].mdiv,
                this->phase[phase - 1].pldiv, this->phase[phase].pldiv);
            PMU_BREAKPOINT();
            return FLCN_ERR_TIMEOUT;
        }

        //
        // We timed out, so prepare to retry by disabling the PLL and powering
        // it off.  Also make sure sync mode is disabled.
        //
        cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ,      _POWER_OFF, cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE,   cfgRegData);
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE,    _NO,        cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        //
        // Read back the register to make sure the write has been received by the
        // HPLL before powering it back on.  The data is irrelevant; this is a timing
        // issue.  See timeout failure condition described in bug 200677168/163.
        //
        CLK_REG_RD32(this->cfgRegAddr);
    }

    // The PLL is locked, so enable SYNC mode.
    LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);
    cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _ENABLE, cfgRegData);
    CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

    // Done
    return FLCN_OK;
}


/*!
 * @brief       Is PLL Locked?
 * @see         OS_PTIMER_COND_FUNC
 * @memberof    ClkHPll
 *
 * @details     This function returns true if the PLL has locked onto the
 *              programmed frequency.
 *
 * @note        This function's signature conforms to OS_PTIMER_COND_FUNC
 *              so that it can be called by OS_PTIMER_SPIN_WAIT_NS_COND.
 *              As such, it must have an entry in pmu_sw/build/Analyze.pm
 *              for $fpMappings.
 *
 * @warning     pArgs is typecast to a ClkHPll pointer.
 *
 * @param[in]   pArgs   Pointer to this ClkHPll object
 *
 * @retval      true    The PLL has locked.
 */
LwBool clkProgram_IsLocked_HPll(void *pArgs)
{
    // The argument points to the PLL object.
    const ClkHPll *this = pArgs;

    // Read the current value of the cfg register
    LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

    // True iff the lock has been acquired.
    return FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _PLL_LOCK, _TRUE, cfgRegData);
}


/*******************************************************************************
    Clean-Up
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkCleanup
 * @memberof    ClkHPll
 * @brief       Power down the PLL if no longer used.
 *
 * @details     clkCleanup functions usually have the post condition that
 *              the last element of the phase array is copied to all other
 *              phases of the phase array.  This is true for this function
 *              as well with the exception of the bActive member of the phase
 *              struct.  The bActive member is set to false for all phases
 *              except phase 0 where it is set to the value of the bActive
 *              input.
 */
FLCN_STATUS
clkCleanup_HPll
(
    ClkHPll *this,
    LwBool bActive
)
{
    FLCN_STATUS status;
    ClkPhaseIndex p;

    // If this PLL is not part of the active path, turn it off.
    if (!bActive)
    {
        // Read the configuration register to preserver other flags
        LwU32 cfgRegData = CLK_REG_RD32(this->cfgRegAddr);

        // Disable sync mode.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _SYNC_MODE, _DISABLE, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        // Disable the HPLL.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _NO, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);

        // Power down HPLL.
        cfgRegData = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ, _POWER_OFF, cfgRegData);
        CLK_REG_WR32(this->cfgRegAddr, cfgRegData);
    }

    // Cleanup the input as needed.
    status = clkCleanup_Wire(&this->super, bActive);

    //
    // Update the pll's phase array to reflect the last phase
    // Only phase 0 can have the bActive flag set, however
    //
    for (p = 0; p < CLK_MAX_PHASE_COUNT - 1; p++)
    {
        this->phase[p] = this->phase[CLK_MAX_PHASE_COUNT - 1];
    }

    return status;
}


/*******************************************************************************
    Print
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkHPll
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_HPll
(
    ClkHPll         *this,
    ClkPhaseIndex    phaseCount
)
{
    // Print each programming phase.
    for(ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        CLK_PRINTF("HPLL phase %u: mdiv=%u ndiv=%u pdiv=%u hot=%uKHz\n",
            (unsigned) p,
            (unsigned) this->phase[p].mdiv,
            (unsigned) this->phase[p].ndiv,
            (unsigned) this->phase[p].pldiv,
            (unsigned) this->phase[p].bHotSwitch);
    }

    // Print current hardware state.
    CLK_PRINT_REG("HPLL:   CFG", this->cfgRegAddr);
    CLK_PRINT_REG("HPLL: COEFF", this->coeffRegAddr);
    CLK_PRINT_REG("HPLL:  CFG2", this->cfg2RegAddr);
    CLK_PRINT_REG("HPLL:  CFG4", this->cfg4RegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif // CLK_PRINT_ENABLED
