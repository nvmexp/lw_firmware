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
 * @author  Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/clk3.h"
#include "clk3/fs/clkrosdm.h"
#include "clk3/generic_dev_trim.h"


/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief       Implicit denominator for SDM callwlations
 *
 * @details     Fractional SDM is in units of 1/8192, so integer callwlations
 *              ilwolving SDM must be done at 8192 times the natural value.
 */
#define SDM_DENOMINATOR 8192U


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(RoSdm);

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*******************************************************************************
    Reading
*******************************************************************************/

/*!
 * @see         ClkFreqSrc_Virtual::clkRead
 * @memberof    ClkRoSdm
 * @brief       Read the coefficients from hardware
 *
 * @details     Determine the current selected input by reading the hardware,
 *              then daisy-chain through the input node to get the frequency.
 *              Set all values of the phase array to align with the current
 *              hardware state.
 *
 *              The 'bActive' member of the phase array, however, is an exception
 *              to this.  'clkRead' sets the 'bActive' member of phase 0 to match
 *              the hardware. For all subsequent phases, however, it is set to
 *              false.
 *
 * @note        This logic assumes analog PLLs as opposed to HPLLs (hybrid PLLs)
 *              which have different registers and fields.  Examples of analog
 *              PLLs supported here are:
 *                  Ampere DRAMPLL PLL90G
 *                  Ampere REFMPLL PLL32G
 *                  Turing DRAMPLL PLL80G
 *                  Turing REFMPLL PLL25G
 *
 * @return      The status returned from calling clkRead on the input
 */
FLCN_STATUS
clkRead_RoSdm
(
    ClkRoSdm      *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    // Read the input.
    {
        FLCN_STATUS status = clkRead_Wire(&this->super.super, pOutput, bActive);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    // Read the coefficients from hardware.
    LwU32 coeffRegData = CLK_REG_RD32(this->super.coeffRegAddr);
    LwU8 mdiv = DRF_VAL(_PTRIM, _SYS_ROPLL_COEFF, _MDIV, coeffRegData);
    LwU8 ndiv = DRF_VAL(_PTRIM, _SYS_ROPLL_COEFF, _NDIV, coeffRegData);

    // Apply NDIV and MDIV without SDM
    LwU32 cfg2RegData = CLK_REG_RD32(this->cfg2RegAddr);
    if (FLD_TEST_DRF(_PVTRIM, _SYS_PLL_CFG2, _SSD_EN_SDM, _NO, cfg2RegData))
    {
        pOutput->freqKHz *= ndiv;
        pOutput->freqKHz /= mdiv;
    }

    // Apply NDIV with Sigma-Delta Modulation (SDM).
    else
    {
        LwU32 ssd0RegData = CLK_REG_RD32(this->ssd0RegAddr);
        LwS16 sdm = DRF_VAL(_PVTRIM, _SYS_PLL_SSD0, _SDM_DIN, ssd0RegData);

        //
        // With SDM, the effective NDIV is ndivAll / SDM_DENOMINATOR, which
        // may be a noninteger.   In effect, callwlations are done in units
        // of SDM_DENOMINATOR so that we may use an integer data type.
        //
        // We know 'ndivAll' will not overflow because:
        // - SDM_DENOMINATOR has 14 bits and ndiv has 15 bits (not including
        //   the sign, so their product has 29 bits (not including sign).
        // - Since each value added into the product has fewer bits, each sum
        //   requires no more than 1 more bit.
        // - Since there are two additions, the total number of bits required
        //   for 'ndivAll' is 31 (not including sign).
        // - We need one bit for the sign, bringing the total to 32 bits.
        //
        // Although 'sdm' is signed, in practice, the effective NDIV is always
        // positive.
        //
        LwS32 ndivAll = ndiv * SDM_DENOMINATOR + SDM_DENOMINATOR / 2 + sdm;
        PMU_HALT_COND(ndivAll > 0);

        // 64-bit arithmetic is required for full precision below.
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            //
            // outputFreq = inputFreq * effective NDIV / MDIV
            //
            // Since 'freqKHz' and 'ndivAll' each have up to 32 bits, the result
            // may have 64 bits.
            //
            // Since 'mdiv' is smaller than SDM_DENOMINATOR, divide by 'mdiv'
            // first to improve precision.
            //
            // In practice, the output frequency in KHz never exceeds 32 bits.
            //
            LwU64 outputAll = pOutput->freqKHz;
            outputAll *= (LwU64) ndivAll;
            outputAll /= mdiv;
            outputAll /= SDM_DENOMINATOR;
            PMU_HALT_COND(outputAll < LW_U32_MAX);
            pOutput->freqKHz = (LwU32) outputAll;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    // Indicate the type of object which is the soruce of the clock signal.
    pOutput->source = this->super.source;

    // Done
    return FLCN_OK;
}

/* -------------------------- Configuration --------------------------------- */
/* -------------------------- Programming ----------------------------------- */
/* -------------------------- Clean-up -------------------------------------- */

/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkRoSdm
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_RoSdm
(
    ClkRoSdm        *this,
    ClkPhaseIndex    phaseCount
)
{
    // Read and print hardware state.
    // (All software state is static const or local to clkRead.)
    CLK_PRINT_REG("Read-Only VCO with SDM support: COEFF", this->super.coeffRegAddr);
    CLK_PRINT_REG("Read-Only VCO with SDM support:  CFG2", this->cfg2RegAddr);
    CLK_PRINT_REG("Read-Only VCO with SDM support:  SSD0", this->ssd0RegAddr);

    // Print the input object information.
    clkPrint_Wire(&this->super.super, phaseCount);
}
#endif
