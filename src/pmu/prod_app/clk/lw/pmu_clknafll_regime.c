/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation. All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_regime.c
 * @brief  Clock code related to the AVFS regime logic and clock programming.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "clk/pmu_clkadc.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Initialize the regime descriptor for the given NAFLL
 *
 * @param[in,out]  pNafllDev        Pointer to the NAFLL device
 * @param[in]      pRegimeDesc      Pointer to the Regime descriptor
 * @param[in]      bFirstConstruct  Flag to indicate if it's the first time
 *                                  construction
 *
 * @return  FLCN_OK                         Regime Logic initialized successfully
 * @return  FLCN_ERR_ILWALID_ARGUMENT       if the given regime descriptor is NULL
 * @return  FLCN_ERROR                      if the given regime-id is invalid or
 * @return  FLCN_ERR_FREQ_NOT_SUPPORTED     FFR frequency is '0MHz'
 */
FLCN_STATUS
clkNafllRegimeInit
(
    CLK_NAFLL_DEVICE        *pNafllDev,
    RM_PMU_CLK_REGIME_DESC  *pRegimeDesc,
    LwBool                   bFirstConstruct
)
{
    FLCN_STATUS            status           = FLCN_OK;
    CLK_NAFLL_REGIME_DESC *pNafllRegimeDesc = &pNafllDev->regimeDesc;

    if (pRegimeDesc == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllRegimeInit_exit;
    }

    pNafllRegimeDesc->nafllId                 = pNafllDev->id;
    pNafllRegimeDesc->targetRegimeIdOverride  =
        pRegimeDesc->targetRegimeIdOverride;
    pNafllRegimeDesc->fixedFreqRegimeLimitMHz =
        pRegimeDesc->fixedFreqRegimeLimitMHz;

    //
    // Initialize parameters which are required to be initialized just for the
    // first time construction.
    //
    if (bFirstConstruct)
    {
        // The regimeId cannot be INVALID. Bail out with FLCN_ERROR if so.
        if (!LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(pRegimeDesc->regimeId))
        {
            status = FLCN_ERROR;
            goto clkNafllRegimeInit_exit;
        }

        // If the regimeID is FFR, then a valid fixedFreqRegimeLimitMHz must be set
        if ((pRegimeDesc->regimeId == LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR) &&
            (pRegimeDesc->fixedFreqRegimeLimitMHz == 0U))
        {
            status = FLCN_ERR_FREQ_NOT_SUPPORTED;
            goto clkNafllRegimeInit_exit;
        }

        // TODO - Delete RM passed regime ID
        pNafllDev->pStatus->regime.lwrrentRegimeId = pRegimeDesc->regimeId;

        //
        // Get the current value of NDIV override set by SW either in FFR regime
        // Note: there is a BIG assumption here that the NAFLL will be set in FFR
        // regime during boot. And so reading the SW set NDIV should give us the
        // boot frequency.
        // An alternate way would be to get the boot frequency from RM. RM will
        // have to read the boot p-state clock and then the nominal frequency in
        // that p-state would be the boot frequency. But that works only when
        // the VBIOS is doing a CPI of that the nominal frequency and using that
        // value to set the NDIV. This is non-trival and maybe not a reliable
        // way, especially if the VBIOS has bugs. For now, assume that devinit
        // will set the NAFLL in FFR regime - ALWAYS.
        //
        LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntry = {0U};

        // Callwlate the target frequency based on current programmed NDIV
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V20) ||
            PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V30))
        {
            status = clkNafllLutOverrideGet_HAL(&Clk,
                        pNafllDev, NULL, &lutEntry);
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V10))
        {
            status = clkNafllLut10OverrideGet_HAL(&Clk,
                        pNafllDev, NULL, &lutEntry.lut10.ndiv, NULL);
        }
        else
        {
            status = FLCN_ERR_FEATURE_NOT_ENABLED;
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllRegimeInit_exit;
        }

        //
        // All LUT_VXX_VF_ENTRY have aligned ndiv as their first member.
        //
        pNafllDev->pStatus->regime.lwrrentFreqMHz =
            ((pNafllDev->inputRefClkFreqMHz * lutEntry.lut10.ndiv) /
             (pNafllDev->mdiv * pNafllDev->inputRefClkDivVal * pNafllDev->pStatus->pldiv));


        //
        // Adjust the frequency value based on 1x/2x DVCO.
        // Case 1: When 1x domains are supported(i.e. VF lwrve having 1x
        //         frequency) and 2x DVCO is in use, half the frequency.
        // Case 2: When 1x domains are not supported(i.e. VF lwrve having 2x
        //         frequency) and 1x DVCO is in use, double the frequency.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && !pNafllDev->dvco.bDvco1x)
        {
            pNafllDev->pStatus->regime.lwrrentFreqMHz /= 2U;
        }
        else if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && pNafllDev->dvco.bDvco1x)
        {
            pNafllDev->pStatus->regime.lwrrentFreqMHz *= 2U;
        }
        else
        {
            // Do nothing
        }
    }

clkNafllRegimeInit_exit:
    return status;
}

/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL dynamic status parameters
 * @param[in]       pTarget     Pointer to the NAFLL target parameters.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 * @return FLCN_ERR_ILWALID_STATE    if the target regime id is invalid
 * @return FLCN_ERR_NOT_SUPPORTED    No supported Nafll Program implementation
 *                                   present.
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkNafllConfig
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
    RM_PMU_CLK_FREQ_TARGET_SIGNAL            *pTarget
)
{
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check the input parameters.
    if ((pNafllDev == NULL) ||
        (pStatus == NULL)   ||
        (pTarget == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllConfig_exit;
    }

    // Sanity check the target frequency
    if (pTarget->super.freqKHz == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllConfig_exit;
    }

    // Sanity check the target frequency
    if (pTarget->super.dvcoMinFreqMHz == 0U)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto clkNafllConfig_exit;
    }

    // Sanity check the target regime.
    if (!LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(pTarget->super.regimeId))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto clkNafllConfig_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
            {
                status = clkNafllConfig_2X(pNafllDev, pStatus, pTarget);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V30))
            {
                status = clkNafllConfig_30(pNafllDev, pStatus, pTarget);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

clkNafllConfig_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}


/*!
 * @brief Helper interface to configure the NAFLL source to the target parameters.
 *
 * @param[in]       pNafllDev   Pointer to the NAFLL device
 * @param[in,out]   pStatus     Pointer to the NAFLL dynamic status parameters
 * @param[in]       pTarget     Pointer to the NAFLL target parameters.
 *
 * @return FLCN_ERR_NOT_SUPPORTED No supported Nafll Program implementation
 *                                present
 * @return other                  Descriptive status code from sub-routines on
 *                                success/failure
 */
FLCN_STATUS
clkNafllProgram
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
            {
                status = clkNafllProgram_2X(pNafllDev, pStatus);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V30))
            {
                status = clkNafllProgram_30(pNafllDev, pStatus);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief   Helper interface to get the regime id for noise aware clk.
 *
 * @param[in]   pClkListItem    LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM *pointer
 * @param[in]   pVoltList       LW2080_CTRL_VOLT_VOLT_RAIL_LIST pointer
 *
 * @return FLCN_OK                   On success
 * @return FLCN_ERR_ILWALID_ARGUMENT Could not find a NAFLL device corresponding
 *                                   to the target clock domain.
 * @return FLCN_ERR_ILWALID_STATE    Found invalid target regime ID for the
 *                                   target clock domain.
 */
FLCN_STATUS
clkNafllTargetRegimeGet
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItem,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST        *pVoltList
)
{
    CLK_NAFLL_DEVICE                     *pNafllDev;
    LwU16                                 targetFreqMHz;
    FLCN_STATUS                           status       = FLCN_OK;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltListRailItem = NULL;
    LwU32                                 voltListIdx;

    // Get primary NAFLL device.
    pNafllDev = clkNafllPrimaryDeviceGet(pClkListItem->clkDomain);
    if (pNafllDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllTargetRegimeGet_exit;
    }

    // Colwert the target frequency from kHz to MHz.
    targetFreqMHz = (LwU16)(pClkListItem->clkFreqKHz / 1000U);

    // Get the rail-item first for the Nafll device's LUT index.
    for (voltListIdx = 0; voltListIdx < pVoltList->numRails; voltListIdx++)
    {
        if (pVoltList->rails[voltListIdx].railIdx == pNafllDev->railIdxForLut)
        {
            pVoltListRailItem = &pVoltList->rails[voltListIdx];
        }
    }

    // Sanity check for the volt rail list item
    if (pVoltListRailItem == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllTargetRegimeGet_exit;
    }

    // Update the target regime.
    pClkListItem->regimeId =
        clkNafllDevTargetRegimeGet(pNafllDev, targetFreqMHz, pClkListItem->dvcoMinFreqMHz, pVoltListRailItem);

    // Sanity check the target regime.
    if (!LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(pClkListItem->regimeId))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllTargetRegimeGet_exit;
    }

clkNafllTargetRegimeGet_exit:
    return status;
}

/*!
 * @brief Given a NAFLL ID - get the current regime
 *
 * @param[in]   nafllId     NAFLL ID
 *
 * @return  the cached value of the regime-id on success
 * @return  @ref LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID if the Nafll device
 *          is NULL for the given ID
 */
LwU8
clkNafllRegimeByIdGet
(
   LwU8  nafllId
)
{
    CLK_NAFLL_DEVICE *pNafllDev = clkNafllDeviceGet(nafllId);

    if (pNafllDev == NULL)
    {
        return LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    }

    return pNafllDev->pStatus->regime.lwrrentRegimeId;
}

/*!
 * @brief Given a NAFLL device - get the current regime
 *
 * @param[in]   pNafllDev Pointer to the NAFLL device
 *
 * @pre         Client MUST attach the clk3x DMEM overlay if
 *              @ref PMU_CLK_CLKS_ON_PMU feature is enabled.
 *
 * @return      the cached value of the regime-id
 */
LwU8
clkNafllRegimeGet
(
   CLK_NAFLL_DEVICE *pNafllDev
)
{
    return pNafllDev->pStatus->regime.lwrrentRegimeId;
}

/*!
 * @brief Given a NAFLL device - get the current target frequency
 *
 * @param[in]   pNafllDev Pointer to the NAFLL device
 *
 * @pre         Client MUST attach the clk3x DMEM overlay if
 *              @ref PMU_CLK_CLKS_ON_PMU feature is enabled.
 *
 * @return      Cached value of the current target frequency
 */
LwU16
clkNafllFreqMHzGet
(
   CLK_NAFLL_DEVICE *pNafllDev
)
{
    return pNafllDev->pStatus->regime.lwrrentFreqMHz;
}

/*!
 * @brief Compute the DVCO Min for all NAFLL devices.
 *
 * @details The DVCO Min will be a function of speedo and voltage. Whenever the
 * set voltage changes we need to call this function so that the DVCO Min can
 * be updated. The set voltage changes in the following two conditions
 * 1. Directed voltage change during a V/F switch
 * 2. Voltage offset is applied by the frequency controller
 *
 * For now, we are going to compute the DVCO Min only during directed voltage
 * switches. CS has agreed to margin for the voltage offsets applied by the
 * frequency controller
 *
 * Based on the agreement with CS, we will compute DVCO min only for the
 * first NAFLL and use the same value for all NAFLLs.
 *
 * @param[in]  pVoltList        The target volt list for which DVCO Min is to be computed
 *
 * @pre Parent func MUST grab PERF DMEM read mutex.
 * @pre Parent func MUST grab CLK 3.0 overlays.
 * @pre Parent func MUST grab VFE overlays.
 *
 * @return FLCN_OK                On success
 * @return FLCN_ERR_ILWALID_STATE if the NAFLL device state is inconsistent
 * @return FLCN_ERROR             if callwlated frequency is zero
 * @return other                  Descriptive error code from sub-routines on
 *                                failure
 */
//! https://confluence.lwpu.com/display/RMPER/Use+single+DVCO+Min+for+all+NAFLL+clock+domains
FLCN_STATUS
clkNafllGrpDvcoMinCompute
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pVoltList
)
{
    CLK_NAFLL_DEVICE           *pNafllDev;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    LwBoardObjIdx               devIdx;
    FLCN_STATUS                 status          = FLCN_OK;
    LwBool                      bFirstNafllDev;
    LwVfeEquIdx                 firstMinFreqIdx;
    LwU32                       voltListIdx;

    // sanity check number of rails to avoid array overflow
    if (LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS < pVoltList->numRails)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllGrpDvcoMinCompute_exit;
    }

    for (voltListIdx = 0; voltListIdx < pVoltList->numRails; voltListIdx++)
    {
        bFirstNafllDev                  = LW_TRUE;
        firstMinFreqIdx                 = PMU_PERF_VFE_EQU_INDEX_ILWALID;
        result.freqMHz                  = 0U;

        // Iterate over all NAFLL devices.
        BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pNafllDev, devIdx, NULL)
        {
            // We only care about one rail at a time
            if (pVoltList->rails[voltListIdx].railIdx != pNafllDev->railIdxForLut)
            {
                continue;
            }

            // Validate the minFreqIdx
            if (pNafllDev->dvco.minFreqIdx == PMU_PERF_VFE_EQU_INDEX_ILWALID)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkNafllGrpDvcoMinCompute_exit;
            }

            if (bFirstNafllDev)
            {
                vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
                vfeVarVal.voltage.varValue = pVoltList->rails[voltListIdx].voltageuV;

                // Parent task will grab the PERF DMEM read semaphore
                status = vfeEquEvaluate(pNafllDev->dvco.minFreqIdx, &vfeVarVal, 1U,
                             LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ, &result);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkNafllGrpDvcoMinCompute_exit;
                }

                //
                // If we have a valid VFE index, we don't expect the computed frequency
                // to be zero. Assert and return a FLCN_ERROR if this happens.
                //
                if (result.freqMHz == 0U)
                {
                        status = FLCN_ERROR;
                    PMU_BREAKPOINT();
                    goto clkNafllGrpDvcoMinCompute_exit;
                }

                firstMinFreqIdx = pNafllDev->dvco.minFreqIdx;
                bFirstNafllDev  = LW_FALSE;
                Clk.clkNafll.dvcoMinFreqMHz[pNafllDev->railIdxForLut] = (LwU16)result.freqMHz;
            }

                // Validate that all NAFLL shares the same VFE equation for the rail
            if (pNafllDev->dvco.minFreqIdx != firstMinFreqIdx)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkNafllGrpDvcoMinCompute_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

clkNafllGrpDvcoMinCompute_exit:
    return status;
}


/*!
 * @brief Helper interface to callwlate the required PLDIV configuration at the
 *        target frequency for a given NAFLL
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 * @return FLCN_OK                On success or programming PLDIV not enabled
 * @return FLCN_ERR_NOT_SUPPORTED If feature/device type is not supported
 * @return other                  Descriptive error code from sub-routines on
 *                                failure
 */
FLCN_STATUS
clkNafllPldivConfig
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
            {
                status = clkNafllPldivConfig_2X(pNafllDev, pStatus);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V30))
            {
                status = clkNafllPldivConfig_30(pNafllDev, pStatus);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Engage/Disengage the PLDIV at the target frequency for a given NAFLL
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   pStatus    Pointer to the dynamic status param of NAFLL device.
 *
 * @return FLCN_ERR_NOT_SUPPORTED If feature/device type is not supported
 * @return other                  Descriptive error code from sub-routines on
 *                                failure
 */
FLCN_STATUS
clkNafllPldivProgram
(
    CLK_NAFLL_DEVICE                         *pNafllDev,
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
            {
                status = clkNafllPldivProgram_2X(pNafllDev, pStatus);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V30))
            {
                status = clkNafllPldivProgram_30(pNafllDev, pStatus);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Get the target regime ID for a given NAFLL, target frequency
 * and voltage
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   targetfreqMHz   Target frequency in MHz
 * @param[in]   dvcoMinFreqMHz  DVCO min frequency in MHz
 * @param[in]   pVoltListItem   Pointer to entry in LW2080_CTRL_VOLT_VOLT_RAIL_LIST
 *
 * @return The target regime ID on success
 * @return @ref LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID on failure
 */
LwU8
clkNafllDevTargetRegimeGet
(
    CLK_NAFLL_DEVICE                       *pNafllDev,
    LwU16                                   targetFreqMHz,
    LwU16                                   dvcoMinFreqMHz,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM   *pVoltListItem
)
{
    LwU8 regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;

    // Sanity check
    if ((targetFreqMHz  == 0U) ||
        (dvcoMinFreqMHz == 0U))
    {
        goto clkNafllDevTargetRegimeGet_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V2X))
            {
                regimeId = clkNafllDevTargetRegimeGet_2X(pNafllDev, targetFreqMHz, dvcoMinFreqMHz, pVoltListItem);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_REGIME_V30))
            {
                regimeId = clkNafllDevTargetRegimeGet_30(pNafllDev, targetFreqMHz, dvcoMinFreqMHz, pVoltListItem);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

clkNafllDevTargetRegimeGet_exit:
    if (regimeId == LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID)
    {
        PMU_BREAKPOINT();
    }
    return regimeId;
}

/*!
 * @brief Given current sw regime id return the appropriate hw regime id
 *
 * @param[in]  swRegimeId       current software regime id
 * @param[out] pHwRegimeId      pointer to current hardware regime id
 *
 * @return  FLCN_OK                     when given sw regime maps to hw regime
 * @return  FLCN_ERR_ILWALID_ARGUMENT   incorrect sw regime id is given
 */
FLCN_STATUS
clkNafllGetHwRegimeBySwRegime
(
   LwU8   swRegimeId,
   LwU8  *pHwRegimeId
)
{
    FLCN_STATUS   status  = FLCN_OK;

    switch (swRegimeId)
    {
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR:
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN:
        {
            *pHwRegimeId = LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ;
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR:
        {
            *pHwRegimeId = LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN;
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR:
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_ABOVE_NOISE_UNAWARE_VMIN:
        case LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM:
        {
            *pHwRegimeId = LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ;
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            break;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
