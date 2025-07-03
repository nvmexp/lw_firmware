/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 */

/* ------------------------- System Includes -------------------------------- */

#include "clk3/fs/clknafll.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(Nafll);


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implementations ------------------------ */

/*!
 * @see         clkRead_FreqSrc
 * @brief       Get the link speed from the previous phase else from hardware
 *              This interface also makes changes to the cached stated of the
 *              Nafll device.
 *
 * @memberof    ClkNafll
 *
 * @param[in]   this the instance of ClkNafll to read the frequency of.
 *
 * @param[out]  pOutput a pointer to the struct to which the freuncy will be
 *              written.
 *
 * @pre         This function should not be called by the clients directly - please
 *              make use of clkReadDomain_<xyz> functions.
 *              Caller of this function *MUST* ensure that it acquires the access lock
 *              of all the domains it is trying to program. @ref CLK_DOMAIN_IS_ACCESS_ENABLED
 *
 * @prereq      Overlay - imem_perfClkAvfs
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkRead_Nafll
(
    ClkNafll    *this,
    ClkSignal   *pOutput,
    LwBool       bActive
)
{
    LwU8                                overrideMode = 0;
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA   lutEntry     = {0};
    CLK_NAFLL_DEVICE                   *pNafllDev = clkNafllDeviceGet(this->id);
    FLCN_STATUS                         status;

    // Short-circuit if the NAFLL device is not supported.
    if (pNafllDev == NULL)
    {
        status = FLCN_OK;
        goto clkRead_Nafll_exit;
    }

    // Reset output
    *pOutput = clkEmpty_Signal;

    // Callwlate the target frequency based on current programmed NDIV
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V20)  || 
        PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V30))
    {
        status = clkNafllLutOverrideGet_HAL(&Clk,
                    pNafllDev, &overrideMode, &lutEntry);
    }
    else
    {
        status = FLCN_ERR_FEATURE_NOT_ENABLED;
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkRead_Nafll_exit;
    }
    pNafllDev->pStatus->regime.lwrrentFreqMHz =
        ((pNafllDev->inputRefClkFreqMHz * lutEntry.lut20.ndiv) /
                (pNafllDev->mdiv * pNafllDev->inputRefClkDivVal * pNafllDev->pStatus->pldiv));

    //
    // Adjust the frequency value based on 1x/2x DVCO.
    // Case 1: When 1x domains are supported(i.e. VF lwrve having 1x
    //         frequency) and 2x DVCO is in use, half the frequency.
    // Case 2: When 1x domains are not supported(i.e. VF lwrve having 2x
    //         frequency) and 1x DVCO is in use, double the frequency.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) &&
        !pNafllDev->dvco.bDvco1x)
    {
        pNafllDev->pStatus->regime.lwrrentFreqMHz /= 2;
    }
    else if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) &&
        pNafllDev->dvco.bDvco1x)
    {
        pNafllDev->pStatus->regime.lwrrentFreqMHz *= 2;
    }
    else
    {
        // Do nothing
    }

    // Parse the programmed regime.
    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
        {
            pNafllDev->pStatus->regime.lwrrentRegimeId =
                LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR;
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
        {
            if (pNafllDev->pStatus->pldiv > CLK_NAFLL_PLDIV_DIV1)
            {
                pNafllDev->pStatus->regime.lwrrentRegimeId =
                    LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN;
            }
            else
            {
                pNafllDev->pStatus->regime.lwrrentRegimeId =
                    LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
        {
            pNafllDev->pStatus->regime.lwrrentRegimeId =
                LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkRead_Nafll_exit;
        }
    }

    // Set target frequency and regime equal to their current values.
    pNafllDev->pStatus->regime.targetFreqMHz  = pNafllDev->pStatus->regime.lwrrentFreqMHz;
    pNafllDev->pStatus->regime.targetRegimeId = pNafllDev->pStatus->regime.lwrrentRegimeId;

    // Do NOT re-callwlate DVCO min frequency.
    pNafllDev->pStatus->dvco.bMinReached = LW_FALSE;

    // Get the current programmed PLDIV
    pNafllDev->pStatus->pldivEngage = LW_TRISTATE_FALSE;
    pNafllDev->pStatus->pldiv = clkNafllPldivGet_HAL(&Clk, pNafllDev);

    // Set the output frequency.
    pOutput->freqKHz = (pNafllDev->pStatus->regime.lwrrentFreqMHz * 1000);

    //
    // 'source' indicates whether a PLL, OSM, or NAFLL is closest to the root
    // of the frequency domain.  Indicate NAFLL on the return.
    //
    pOutput->source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

clkRead_Nafll_exit:
    return status;
}


/*!
 * @see         clkConfig_FreqSrc
 * @brief       Configure this object for the target frequency and set the
 *              clkProgram implementation appropriately.
 *
 *              Specifically, this means that, if we're in phase two or later
 *              and the previous phase has the same frequency, then clkProgram
 *              is set to clkProgram_Nop_FreqSrc (i.e. "Done") since the
 *              programming happened in a previous phase.
 *
 *              Otherwise, it is set to clkProgram_Pending_NafllWrapper ("Pending"),
 *              so that is will get batched up in this phase.
 *
 * @memberof    ClkNafll
 *
 * @param[in]   this the instance of ClkNafll to configure.
 * @param[out]  pOutput a struct to write the genspeed to be configured
 * @param[in]   the phase of the phase array to configure.
 * @param[in]   pTarget a pointer to a struct containing the target genspeed.
 * @param[in]   bHotSwitch not used
 *
 * @pre         This function should not be called by the clients directly.
 *               Caller of this function *MUST* ensure that it acquires the
 *               access lock of all the domains it is trying to program.
 *               @ref CLK_DOMAIN_IS_ACCESS_ENABLED
 *
 * @prereq      Overlay - imem_perfClkAvfs
 *
 * @return      return FLCN_OK, unless the path is not empty, in which case
 *              return FLCN_ERR_ILWALID_PATH.
 */
FLCN_STATUS
clkConfig_Nafll
(
    ClkNafll                *this,
    ClkSignal               *pOutput,
    ClkPhaseIndex            phase,
    const ClkTargetSignal   *pTarget,
    LwBool                   bHotSwitch
)
{
    CLK_NAFLL_DEVICE   *pNafllDev = clkNafllDeviceGet(this->id);
    ClkTargetSignal     target    = *pTarget;
    FLCN_STATUS         status;

    // Short-circuit if the NAFLL device is not supported.
    if (pNafllDev == NULL)
    {
        status = FLCN_OK;
        goto clkConfig_Nafll_exit;
    }

    // Init the output frequency.
    pOutput->freqKHz = (pNafllDev->pStatus->regime.lwrrentFreqMHz * 1000);

    // Config NAFLL dynamic status params based on the input target request.
    status = clkNafllConfig(pNafllDev, pNafllDev->pStatus, &target);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkConfig_Nafll_exit;
    }

    // Update the output frequency.
    pOutput->freqKHz = (pNafllDev->pStatus->regime.targetFreqMHz * 1000);

    //
    // 'source' indicates whether a PLL, OSM, or NAFLL is closest to the root
    // of the frequency domain.  Indicate NAFLL on the return.
    //
    pOutput->source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

clkConfig_Nafll_exit:
    return status;
}


/*!
 * @see         clkProgram_FreqSrc
 * @brief       Calls into NAFLL module to program the required frequency.
 *
 * @memberof    ClkNafll
 *
 * @param[in]   this the instance of ClkNafll to program.
 * @param[in]   The phase of the phase array to set the nafll to.
 *
 * @pre         This function should not be called by the clients directly.
 *              Caller of this function *MUST* ensure that it acquires the
 *              access lock of all the domains it is trying to program.
 *              @ref CLK_DOMAIN_IS_ACCESS_ENABLED
 *
 * @prereq      Overlay - imem_perfClkAvfs
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkProgram_Nafll
(
    ClkNafll       *this,
    ClkPhaseIndex   phase
)
{
    CLK_NAFLL_DEVICE   *pNafllDev;
    FLCN_STATUS         status;

    pNafllDev = clkNafllDeviceGet(this->id);

    // Short-circuit if the NAFLL device is not supported.
    if (pNafllDev == NULL)
    {
        status = FLCN_OK;
        goto clkProgram_Nafll_exit;
    }

    // Program NAFLL based on the callwlated NAFLL configurations.
    status = clkNafllProgram(pNafllDev, pNafllDev->pStatus);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgram_Nafll_exit;
    }

clkProgram_Nafll_exit:
    return status;
}


/*!
 * @see         clkCleanup_FreqSrc
 * @brief       PP-TODO
 *
 * @memberof    ClkNafll
 *
 * @param[in]   this the instance of ClkNafll to clean up.
 *
 * @param[in]   bActive whether this object is part of the path that produces
 *              the final gen speed.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkCleanup_Nafll
(
    ClkNafll   *this,
    LwBool      bActive
)
{
    CLK_NAFLL_DEVICE *pNafllDev = clkNafllDeviceGet(this->id);

    // If this object is supported, set current values equal to target values.
    if (pNafllDev != NULL)
    {
        pNafllDev->pStatus->regime.lwrrentRegimeId = pNafllDev->pStatus->regime.targetRegimeId;
        pNafllDev->pStatus->regime.lwrrentFreqMHz  = pNafllDev->pStatus->regime.targetFreqMHz;
    }

    return FLCN_OK;
}


/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkNafll
 *
 * @param[in]   this        Object to print
 * @param[in]   phaseCount  Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
void
clkPrint_Nafll
(
    ClkNafll        *this,
    ClkPhaseIndex    phaseCount
)
{
    // TODO: Print information using clkNafllDeviceGet(this->id)
}
#endif
