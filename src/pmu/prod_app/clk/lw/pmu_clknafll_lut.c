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
 * @file   pmu_clknafll_lut.c
 * @brief  File for CLK NAFLL and LUT routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

// Check to keep the NAFLL define in sync with the VF_REL define
ct_assert(LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_SEC_MAX ==
          LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_MAX);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydetails ClkNafllLutConstruct()
 *
 * @return FLCN_ERR_NOT_SUPPORTED  if the NAFLL device type is not supported
 */
FLCN_STATUS
clkNafllLutConstruct
(
   CLK_NAFLL_DEVICE             *pNafllDev,
   CLK_LUT_DEVICE               *pLutDevice,
   RM_PMU_CLK_LUT_DEVICE_DESC   *pLutDeviceDesc,
   LwBool                        bFirstConstruct
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V10))
            {
                status = clkNafllLutConstruct_10(pNafllDev, pLutDevice,
                           pLutDeviceDesc, bFirstConstruct);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V20))
            {
                status = clkNafllLutConstruct_20(pNafllDev, pLutDevice,
                           pLutDeviceDesc, bFirstConstruct);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V30))
            {
                status = clkNafllLutConstruct_30(pNafllDev, pLutDevice,
                           pLutDeviceDesc, bFirstConstruct);
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
 * @copydoc ClkNafllLutConstruct()
 */
FLCN_STATUS
clkNafllLutConstruct_SUPER
(
   CLK_NAFLL_DEVICE             *pNafllDev,
   CLK_LUT_DEVICE               *pLutDevice,
   RM_PMU_CLK_LUT_DEVICE_DESC   *pLutDeviceDesc,
   LwBool                        bFirstConstruct
)
{
    FLCN_STATUS status = FLCN_OK;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT)
    LwU8        vfLwrveIdx;
#endif

    if ((pNafllDev      == NULL) ||
        (pLutDevice     == NULL) ||
        (pLutDeviceDesc == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllLutConstruct_SUPER_exit;
    }

    pLutDevice->id = pNafllDev->id;

    //
    // Initialize parameters which are required to be initialized just for the
    // first time construction.
    //
    if (bFirstConstruct)
    {
        pLutDevice->lwrrentTempIndex = CLK_LUT_TEMP_IDX_0;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT)
    for (vfLwrveIdx = 0; vfLwrveIdx < clkDomainsNumSecVFLwrves(); vfLwrveIdx++)
    {
        pLutDevice->bQuickSlowdownForceEngage[vfLwrveIdx] = pLutDeviceDesc->bQuickSlowdownForceEngage[vfLwrveIdx];
    }
#endif

clkNafllLutConstruct_SUPER_exit:
    return status;
}

/*!
 * @brief Get the LUT frequency for a given voltage
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   voltageuV  Voltage in uV
 * @param[in]   voltType   The voltage type @ref LW2080_CTRL_CLK_VOLTAGE_TYPE_<XYZ>
 * @param[out]  pFreqMHz   Pointer in which frequency for the specified voltage
 *                         will be returned.
 *
 * @return FLCN_OK                   Successfully found the frequency value
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 * @return FLCN_ERR_OBJECT_NOT_FOUND Corresponding clock domain object for the
 *                                   given Nafll device not found
 * @return FLCN_ERR_ILWALID_STATE    if call to @ref clkDomain3XProgVoltToFreq
 *                                   fails or returns an invalid output value
 */
FLCN_STATUS
clkNafllLutFreqMHzGet
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU32               voltageuV,
    LwU8                voltType,
    LwU16              *pFreqMHz,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    LW2080_CTRL_CLK_VF_INPUT    clkArbInput;
    LW2080_CTRL_CLK_VF_OUTPUT   clkArbOutput;
    FLCN_STATUS                 status;
    CLK_DOMAIN                 *pDomain;
    CLK_DOMAIN_PROG            *pDomainProg;

    // Sanity check
    if ((pNafllDev == NULL) ||
        (voltageuV < CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) ||
        (voltageuV > CLK_NAFLL_LUT_MAX_VOLTAGE_UV()))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllLutFreqMHzGet_exit;
    }

    pDomain = clkDomainsGetByApiDomain(pNafllDev->clkDomain);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto clkNafllLutFreqMHzGet_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkNafllLutFreqMHzGet_exit;
    }

    LW2080_CTRL_CLK_VF_INPUT_INIT(&clkArbInput);

    //
    // A maximum match may not have been found in the cases where the user has
    // specified a positive voltage delta and the lower voltages maybe no longer
    // be within the VF lwrve.  In those cases, taking the minimum match will
    // pick the frequency values corresponding to the lowest voltage in the VF
    // lwrve.
    //
    clkArbInput.flags   =
        (LwU8)FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
        clkArbInput.flags);
    clkArbInput.value   = voltageuV;

    status = clkDomainProgVoltToFreq(
                pDomainProg,
                pNafllDev->railIdxForLut,
                voltType,
                &clkArbInput,
                &clkArbOutput,
                pVfIterState);
    if ((status != FLCN_OK) ||
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == clkArbOutput.inputBestMatch))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkNafllLutFreqMHzGet_exit;
    }

    if (pFreqMHz != NULL)
    {
        *pFreqMHz = (LwU16)clkArbOutput.value;
    }

clkNafllLutFreqMHzGet_exit:
    return status;
}

/*!
 * @copydetails ClkNafllLutOverride()
 *
 * @return FLCN_ERR_NOT_SUPPORTED if the NAFLL device type is not supported
 */
FLCN_STATUS
clkNafllLutOverride
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideMode,
    LwU16               freqMHz
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V10))
            {
                status = clkNafllLutOverride_10(pNafllDev, overrideMode, freqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V20))
            {
                status = clkNafllLutOverride_20(pNafllDev, overrideMode, freqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V30))
            {
                status = clkNafllLutOverride_30(pNafllDev, overrideMode, freqMHz);
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
 * @brief Update LUT for all the NAFLL devices.
 *
 * @return FLCN_OK All Nafll devices successfully updated
 * @return other   Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkNafllUpdateLutForAll
(
    void
)
{
    FLCN_STATUS         status = FLCN_OK;
    CLK_NAFLL_DEVICE   *pDev;
    LwBoardObjIdx       devIdx;

    // Disable MS group features issuing first access.
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
    {
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
        lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL))
    {
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(clkLibClk3));
    }

    NAFLL_LUT_PRIMARY_ITERATOR_BEGIN(pDev, devIdx)
    {
        CLK_DOMAIN_ACCESS_BEGIN()
        {
            CLK_DOMAIN_IS_ACCESS_ENABLED(pDev->clkDomain)
            {
                status = clkNafllLutProgram(pDev);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkNafllUpdateLutForAll_exit;
                }
            }
            CLK_DOMAIN_IS_ACCESS_DISABLED()
            {
                //
                // We only expect GPC clock to be disable as we are explicitly
                // disabling LPWR features that disable other clock domains.
                //
                if (pDev->clkDomain != LW2080_CTRL_CLK_DOMAIN_GPCCLK)
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    PMU_BREAKPOINT();
                    goto clkNafllUpdateLutForAll_exit;
                }

                //
                // GPC LPWR feature (GPC-RG) will restore latest LUT on its
                // exit post processing sequence. So it is safe to skip the
                // new request of LUT program while GPC is disabled.
                //
            }
        }
        CLK_DOMAIN_ACCESS_END();
    }
    NAFLL_LUT_PRIMARY_ITERATOR_END;

clkNafllUpdateLutForAll_exit:

    // Re-enable MS group features if they were disabled
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
    {
        lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL))
    {
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(clkLibClk3));
    }

    return status;
}

/*!
 * @brief Program the LUT_DEVICE for a given NAFLL device.
 *
 * @param[in]   pNafllDev      Pointer to the NAFLL device
 *
 * @pre         This function should not be called by the clients directly - please
 *              make use of clkNafllUpdateLutForAll instead.
 *              Caller of this function *MUST* ensure that it acquires the
 *              access lock of all the domains it is trying to program.
 *              @ref CLK_DOMAIN_IS_ACCESS_ENABLED
 *
 * @return FLCN_OK The LUT device successfully programmed
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkNafllLutProgram
(
    CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LwU8          nextTempIdx;
    FLCN_STATUS   status;

    // Get the value of the next temperature index
    nextTempIdx = clkNafllLutNextTempIdxGet(pNafllDev);

    NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafllDev)
    {
        //
        // Set up the address write register for the loop to start writing two
        // entries at a time at the lwrrTempIdx
        //
        status = clkNafllLutAddrRegWrite_HAL(&Clk, pNafllDev, nextTempIdx);
        if (status != FLCN_OK)
        {
            goto clkNafllLutProgram_exit;
        }
    }
    NAFLL_LUT_BROADCAST_ITERATOR_END(pNafllDev);

    // Program LUT entries.
    status = clkNafllLutEntriesProgram(pNafllDev);
    if (status != FLCN_OK)
    {
        goto clkNafllLutProgram_exit;
    }

    NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafllDev)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771))
        {
            // Programming ADC code offset as close as possible to new updated LUT switch
            status = clkAdcCalOffsetProgram_HAL(&Clk, pNafllDev->pLogicAdcDevice);
            if (status != FLCN_OK)
            {
                goto clkNafllLutProgram_exit;
            }
        }

        // Update the temperature index
        status = clkNafllTempIdxUpdate(pNafllDev, nextTempIdx);
        if (status != FLCN_OK)
        {
            goto clkNafllLutProgram_exit;
        }

        // Update the init state if not already initialized
        if (!pNafllDev->lutDevice.bInitialized)
        {
            pNafllDev->lutDevice.bInitialized = LW_TRUE;

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PROGRAM_LUT_BEFORE_ENABLING_ADC_WAR_BUG_1629772))
            {
                //
                // Clearing ADC override set for all ADCs as a part of SW WAR for
                // bug 200091478 (SW Bug: 1629772)
                //
                status = clkNafllProgramLutBeforeEnablingAdcWar(pNafllDev, LW_FALSE);
                if (status != FLCN_OK)
                {
                    goto clkNafllLutProgram_exit;
                }
            }

            // Update the quick slowdown operations based on SW/POR capabilities.
            status = clkNafllLutUpdateQuickSlowdown_HAL(&Clk);
            if (status != FLCN_OK)
            {
                goto clkNafllLutProgram_exit;
            }

            // Update the RAM READ_EN bit here
            status = clkNafllLutUpdateRamReadEn_HAL(&Clk, pNafllDev, LW_TRUE);
            if (status != FLCN_OK)
            {
                goto clkNafllLutProgram_exit;
            }
        }
    }
    NAFLL_LUT_BROADCAST_ITERATOR_END(pNafllDev);

clkNafllLutProgram_exit:
    return status;
}

/*!
 * @copydetails ClkNafllLutEntriesProgram()
 *
 * @return FLCN_ERR_NOT_SUPPORTED if the NAFLL device type is not supported
 */
FLCN_STATUS
clkNafllLutEntriesProgram
(
    CLK_NAFLL_DEVICE    *pNafllDev
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V10))
            {
                status = clkNafllLutEntriesProgram_10(pNafllDev);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V20))
            {
                status = clkNafllLutEntriesProgram_20(pNafllDev);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V30))
            {
                status = clkNafllLutEntriesProgram_30(pNafllDev);
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
 * @brief Update the temperature index of the LUT
 *
 * @details Set previousTempIdx to lwrrentTempIdx and cache the latest into
 * lwrrentTempIdx
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   tempIdx         temp index to set
 *
 * @return FLCN_OK                   LUT temperature Index set successfully
 *                                   or nothing changed
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid ID or temperature Idx
 * @return other                     Descriptive error code from sub-routines
 *                                   on failure
 */
FLCN_STATUS
clkNafllTempIdxUpdate
(
    CLK_NAFLL_DEVICE *pNafllDev,
    LwU8              tempIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pNafllDev == NULL) ||
        (tempIdx > CLK_LUT_TEMP_IDX_MAX))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllTempIdxUpdate_exit;
    }

    // Early exit in case the temperature index is already up-to-date
    if (pNafllDev->lutDevice.lwrrentTempIndex == tempIdx)
    {
        goto clkNafllTempIdxUpdate_exit;
    }

    // Call into the HAL for the register updates
    status = clkNafllTempIdxUpdate_HAL(&Clk, pNafllDev, tempIdx);
    if (status != FLCN_OK)
    {
        goto clkNafllTempIdxUpdate_exit;
    }

    // Cache the state
    pNafllDev->lutDevice.prevTempIndex = pNafllDev->lutDevice.lwrrentTempIndex;
    pNafllDev->lutDevice.lwrrentTempIndex = tempIdx;

clkNafllTempIdxUpdate_exit:
    return status;
}

/*!
 * @brief Given a NAFLL device, get the voltage of the rail that determines the LUT
 * lwrve
 *
 * @param[in] pNafllDev  Pointer to the NAFLL device
 *
 * @return The voltage value in microvolts
 */
LwU32
clkNafllLutVoltageGet
(
   CLK_NAFLL_DEVICE *pNafllDev
)
{
    FLCN_STATUS status;
    LwU32      voltageuV = RM_PMU_VOLT_VALUE_0V_IN_UV;
    VOLT_RAIL *pRail = VOLT_RAIL_GET(pNafllDev->railIdxForLut);

    if (pRail == NULL)
    {
        PMU_BREAKPOINT();
        goto clkNafllLutVoltageGet_exit;
    }

    status = voltRailGetVoltage(pRail, &voltageuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    PMU_HALT_COND(voltageuV != RM_PMU_VOLT_VALUE_0V_IN_UV);

clkNafllLutVoltageGet_exit:
    return voltageuV;
}

/*!
 * @brief Helper to callwlate the voltage value for a given vfIdx.
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 * @param[in]   vfIdx      V/F index for which params are required
 * @param[out]  pVoltuV    Voltage (uV) value to be filled back
 */
void
clkNafllLutEntryVoltCallwlate
(
    CLK_NAFLL_DEVICE  *pNafllDev,
    LwU16              vfIdx,
    LwU32             *pVoltuV
)
{
    LW2080_CTRL_CLK_ADC_CAL_V10 *pCalV10 = NULL;

    // Callwlate the corrected voltage
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_HW_CALIBRATION) &&
        pNafllDev->pLogicAdcDevice->bHwCalEnabled)
    {
        // This is HW corrected voltage
        (*pVoltuV) = (vfIdx * CLK_NAFLL_LUT_STEP_SIZE_UV()) +
                     CLK_NAFLL_LUT_MIN_VOLTAGE_UV();
    }
    else
    {
        // Need to correct it in SW
        pCalV10 = (&((CLK_ADC_DEVICE_V10 *)(pNafllDev->pLogicAdcDevice))->data.adcCal);
        (*pVoltuV) = (vfIdx * pCalV10->slope) + pCalV10->intercept;
    }

    // Maintain range sanity and also round down to the nearest stepsize
    (*pVoltuV) = ((*pVoltuV) < CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) ?
                  CLK_NAFLL_LUT_MIN_VOLTAGE_UV()               :
                 ((*pVoltuV) > CLK_NAFLL_LUT_MAX_VOLTAGE_UV()) ?
                  CLK_NAFLL_LUT_MAX_VOLTAGE_UV()               :
                 ((*pVoltuV) / CLK_NAFLL_LUT_STEP_SIZE_UV())   *
                  CLK_NAFLL_LUT_STEP_SIZE_UV();

    return;
}

/*!
 * @brief Helper to find the value of the temperature index that can be
 * used for the next LUT programming.
 *
 * @details Even though there are 5 temperature index that the LUT supports, we usually
 * ping-pong between the 0th and the 1st temperature index. This does mean that
 * we have 3 temperature indexes that are not being used.
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 *
 * @return      nextTempIdx The value of the temperature index that can be used
 *                          for the next update
 */
LwU8
clkNafllLutNextTempIdxGet
(
   CLK_NAFLL_DEVICE  *pNafllDev
)
{
    LwU8 nextTempIdx;

    nextTempIdx = (pNafllDev->lutDevice.lwrrentTempIndex ==
                   CLK_LUT_TEMP_IDX_0) ? CLK_LUT_TEMP_IDX_1 :CLK_LUT_TEMP_IDX_0;

    return nextTempIdx;
}

/*!
 * @copydetails ClkNafllLutVfLwrveGet()
 *
 * @return FLCN_ERR_FEATURE_NOT_ENABLED Requested LUT Feature is not enabled
 * @return FLCN_ERR_NOT_SUPPORTED       NAFLL Type is not supported
 */
FLCN_STATUS
clkNafllLutVfLwrveGet
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_LWRVE       *pLutVfLwrve
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;

    switch (BOARDOBJ_GET_TYPE(pNafllDev))
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        {
            // Using GV105 to test NAFLL V20
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V10))
            {
                status = clkNafllLutVfLwrveGet_10(pNafllDev, pLutVfLwrve);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V20))
            {
                status = clkNafllLutVfLwrveGet_20(pNafllDev, pLutVfLwrve);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_V30))
            {
                status = clkNafllLutVfLwrveGet_30(pNafllDev, pLutVfLwrve);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
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
 * @brief Given a frequency compute the offsets for secondary lwrve
 *
 * @param[in]   pNafllDev         Pointer to the NAFLL device
 * @param[in]   freqMHz           Clock frequency in MHz
 * @param[out]  pClkVfTupleOutput Pointer to VF Tuple for the given frequency
 *
 * @return FLCN_OK                      Successfully computed offset values
 * @return FLCN_ERR_OBJECT_NOT_FOUND    Clock Domain object is not found
 * @return FLCN_ERR_ILWALID_ARGUMENT    Invalid argument passed
 * @return other                        Propagates return values from various calls
 */
FLCN_STATUS
clkNafllLutOffsetTupleFromFreqCompute
(
    CLK_NAFLL_DEVICE                            *pNafllDev,
    LwU16                                        freqMHz,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pClkVfTupleOutput
)
{
    CLK_DOMAIN                         *pDomain;
    CLK_DOMAIN_PROG                    *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT            clkVfInput;
    LW2080_CTRL_CLK_VF_OUTPUT           clkVfOutput;

    LW2080_CTRL_CLK_VF_ITERATION_STATE  vfIterState =
                        LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();
    FLCN_STATUS                         status = FLCN_OK;

    //
    // Attach overlays
    // @note This is a huge DMEM so we might need to keep an eye of already
    //  attached unwanted DMEM from parent and may detach them if needed before
    //  attaching this DMEM.
    //
    OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pDomain = clkDomainsGetByApiDomain(pNafllDev->clkDomain);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutOffsetTupleFromFreqCompute_exit;
        }

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutOffsetTupleFromFreqCompute_exit;
        }

        // Initialize the input parameters.
        LW2080_CTRL_CLK_VF_INPUT_INIT(&clkVfInput);

        // Compute Voltage for the requested frequency
        clkVfInput.value = freqMHz;

        status = clkDomainProgFreqToVolt(
                    pDomainProg,
                    pNafllDev->railIdxForLut,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                    &clkVfInput,
                    &clkVfOutput,
                    NULL);
        if ((status != FLCN_OK) ||
            (LW2080_CTRL_CLK_VF_VALUE_ILWALID == clkVfOutput.inputBestMatch))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkNafllLutOffsetTupleFromFreqCompute_exit;
        }

        // Re-initialize the input parameters.
        LW2080_CTRL_CLK_VF_INPUT_INIT(&clkVfInput);

        // Compute Ndiv and Dvco offset from requested Voltage
        clkVfInput.flags =
            (LwU8)FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
            clkVfInput.flags);
        clkVfInput.value = clkVfOutput.value;

        status = clkDomainProgVoltToFreqTuple(
                    pDomainProg,
                    pNafllDev->railIdxForLut,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
                    &clkVfInput,
                    pClkVfTupleOutput,
                    &vfIterState);
        if ((status != FLCN_OK) ||
            (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pClkVfTupleOutput->inputBestMatch))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkNafllLutOffsetTupleFromFreqCompute_exit;
        }

clkNafllLutOffsetTupleFromFreqCompute_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

