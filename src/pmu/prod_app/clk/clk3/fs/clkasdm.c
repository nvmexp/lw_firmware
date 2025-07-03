/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */

#include "clk3/clk3.h"
#include "clk3/fs/clkasdm.h"
#include "clk3/generic_dev_trim.h"
#include "osptimer.h"

/* --------------------------- Virtual Table -------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(ASdm);

/* -------------------------- Constants and Structure ----------------------- */
/* -------------------------- Protected Macros ------------------------------ */
/* -------------------------- Protected Function Prototypes ----------------- */
/* -------------------------- Reading --------------------------------------- */

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkASdm
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 *
 *              The bActive memeber of the phase array, however, is an exception
 *              to this.  clkRead sets the bActive member of phase 0 to match
 *              the hardware. For all subsequent phases, however, it is set to
 *              false.
 *
 * @note        This logic assumes analog PLLs as opposed to HPLLs (hybrid PLLs)
 *              which have different registers and fields.  Examples of analog
 *              PLLs supported here are:
 *                  Ada DRAMPLL PLL100G
 *                  Ada REFMPLL PLL32G
 *                  Ampere DRAMPLL PLL90G
 *                  Ampere REFMPLL PLL32G
 *                  Turing DRAMPLL PLL80G
 *                  Turing REFMPLL PLL25G
 *
 * @return      The status returned from calling clkRead on the input
 */
FLCN_STATUS
clkRead_ASdm
(
    ClkASdm     *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    LwU32           cfgRegData;
    LwU32           cfg2RegData;
    LwU32           ssd0RegData;
    FLCN_STATUS     status;
    ClkASdmPhase    asdmPhase;
    ClkAPllPhase    apllPhase;
    ClkPhaseIndex   p;

    // Read PLL coefficients
    status = clkRead_APll(&this->super, pOutput, bActive);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // If the PLL is on, then read the coefficients from hardware, and then
    // read the input.
    //
    cfgRegData = CLK_REG_RD32(this->super.cfgRegAddr);
    cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
    if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _ENABLE, _YES, cfgRegData) &&
        FLD_TEST_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM, _YES, cfg2RegData))
    {
        ssd0RegData = CLK_REG_RD32(this->ssd0RegAddr);
        asdmPhase.sdm = DRF_VAL(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, ssd0RegData);

        apllPhase = this->super.phase[0];

        //
        // outputFreq = (inputFreq * (ndiv * 8192 + 4096 + sdm))/(mdiv * pldiv * 8192)
        // outputFreq = pllOutputFreq + inputFreq(4096 + sdm)/(mdiv * pldiv * 8192)
        // outputFreq = pllOutputFreq + sdm_freq_adjustment
        //
        // The max value for sdm_freq_adjustment is inputFreq as
        // sdmMax = 4096 and mdivMin = pldivMin = 1
        //
        // As such current arithmatic does not overflow LwU32 range but compared
        // to LwU64 evaluation there might be a worst case loss of precision by
        // 1 KHz.
        //
        pOutput->freqKHz = pOutput->freqKHz +
                         ((apllPhase.inputFreqKHz * (4096 + asdmPhase.sdm)) /
                          (apllPhase.mdiv * apllPhase.pldiv * 8192));
    }
    else
    {
        asdmPhase.sdm = (LwS16) LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MIN;
    }

    // Update the phase array
    for (p = 0; p < CLK_MAX_PHASE_COUNT; p++)
    {
        this->phase[p] = asdmPhase;
    }

    // Done
    return status;
}


/* -------------------------- Configuration --------------------------------- */
/* -------------------------- Programming ----------------------------------- */
/* -------------------------- Clean-up -------------------------------------- */
/* -------------------------- Print ----------------------------------------- */

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkASdm
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 *
 */
#if CLK_PRINT_ENABLED
void
clkPrint_ASdm
(
    ClkASdm         *this,
    ClkPhaseIndex    phaseCount
)
{
    // Print the `super` PLL data along with this data instead of daisy-chaining
    // so that the information is better organized for the user.
    LwU8 id = (super.pPllInfoTableEntry ? super.pPllInfoTableEntry->id : p);
    CLK_PRINTF("PLL with SDM support: id=0x%02x source=0x%02x retryCount=%u\n",
            (unsigned) id, (unsigned) this->super.source, (unsigned) this->super.retryCount);

    // Print each programming phase.
    for(ClkPhaseIndex p = 0; p < phaseCount; ++p)
    {
        // Zero for a nonexistent PLDIV.
        // 2 for special DIV2 divider (instead of PLDIV).
        // See the schematic DAG to distinguish between DIV2 v. PLDIV when it prints 2.
        LwU8 pldiv;
        if (this->super.bDiv2Enable)
            pldiv = 2;
        else if (this->super.bPldivEnable)
            pldiv = 0;
        else
            pldiv = this->super.phase[p].pldiv;

        // Cast 'sdm' to (int) to get a sign-extended decimal value.
        // Cast 'sdm' to (unsigned) to get the pure hex value.
        CLK_PRINTF("PLL phase %u: mdiv=%u ndiv=%u pdiv=%u sdm=%d=0x%04x active=%u input=%uKHz\n",
            (unsigned) p,
            (unsigned) this->super.phase[p].mdiv,
            (unsigned) this->super.phase[p].ndiv,
            (unsigned) pldiv,
            (int) this->phase[p].sdm, (unsigned) this->phase[p].sdm,
            (unsigned) this->super.phase[p].bActive,
            (unsigned) this->super.phase[p].inputFreqKHz);
    }

    // Read and print hardware state.
    CLK_PRINT_REG("PLL with SDM support:   CFG", this->super.cfgRegAddr);
    CLK_PRINT_REG("PLL with SDM support: COEFF", this->super.coeffRegAddr);
    CLK_PRINT_REG("PLL with SDM support:  CFG2", this->cfg2RegAddr);
    CLK_PRINT_REG("PLL with SDM support:  SSD0", this->ssd0RegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super, phaseCount);
}
#endif

