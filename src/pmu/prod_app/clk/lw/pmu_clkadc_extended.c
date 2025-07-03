/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc_extended.c
 * @brief  File for ADC CLK routines applicable only to DGPUs and contains all
 *         the extended support to the ADC base infra.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objlpwr.h"
#include "lwostmrcallback.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBAdcVoltageSample;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
#define ADC_VOLTAGE_SAMPLE_PERIOD_US 1000000U
#define TIME_NS_TO_US                1000U
#define TIMER_FREQ_MHZ               27U

/* ------------------------- Private Functions ------------------------------ */

static OsTmrCallbackFunc (s_clkAdcVoltageSampleOsCallback)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "s_clkAdcVoltageSampleOsCallback")
    GCC_ATTRIB_USED();
static FLCN_STATUS s_clkAdcVoltageRead(CLK_ADC_DEVICE *pAdcDev, CLK_ADC_ACC_SAMPLE *pAdcAccSample)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "s_clkAdcVoltageRead");
static FLCN_STATUS s_clkAdcCalcNumSamplesDiff_PTIMER(CLK_ADC_ACC_SAMPLE  *pAdcAccSample, LwU32 *pNumSamplesDiff)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "s_clkAdcCalcNumSamplesDiff_PTIMER");
static FLCN_STATUS s_clkAdcAccLatch(CLK_ADC_DEVICE  *pAdcDev, LwBool bLatchAcc)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "s_clkAdcAccLatch")
    GCC_ATTRIB_USED();
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
static LwBool s_clkAdcsVoltRailSanityCheck(BOARDOBJGRPMASK_E32 *pAdcMask)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "s_clkAdcsVoltRailSanityCheck")
    GCC_ATTRIB_USED();
#endif // PMU_CLK_ADC_RAM_ASSIST

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc clkAdcsLoad()
 */
FLCN_STATUS
clkAdcsLoad_EXTENDED
(
    LwU32 actionMask
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK,
                      _ADC_CALLBACK, _YES, actionMask)) &&
        (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_VOLTAGE_SAMPLE_CALLBACK)))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Enable access to all the ADCs before triggering the voltage sampling
            clkAdcHwAccessSync(LW_TRUE, LW2080_CTRL_CLK_ADC_CLIENT_ID_INIT);

            //
            // Initialize the ADC aclwmulator and force trigger the callback
            // function to initialize the SW state at time ZERO. It makes sense
            // to do that only if the ADCs are already powered on and enabled.
            // For Turing and prior, ADCs are powered on/enabled only when the
            // NAFLL regime needed the ADCs to be on. Starting Ampere we always
            // keep the ADCs powered on - @ref PMU_PERF_ADC_DEVICES_ALWAYS_ON.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
            {
                // Initialize the ADC Aclwmulator
                status = clkAdcsAccInit();
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkAdcsLoad_EXTENDED_exit_attachment;
                }

                clkAdcVoltageSampleCallback(NULL, 0, 0);
            }
clkAdcsLoad_EXTENDED_exit_attachment:
            lwosNOP();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            goto clkAdcsLoad_EXTENDED_exit;
        }

            // Now schedule the periodic callback only for the first time
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBAdcVoltageSample))
            {
                osTmrCallbackCreate(&OsCBAdcVoltageSample,                           // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,          // type
                    OVL_INDEX_IMEM(libClkVolt),                                      // ovlImem
                    (OsTmrCallbackFuncPtr)OS_LABEL(s_clkAdcVoltageSampleOsCallback), // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                           // queueHandle
                    ADC_VOLTAGE_SAMPLE_PERIOD_US,                                    // periodNormalus
                    ADC_VOLTAGE_SAMPLE_PERIOD_US,                                    // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                                // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                            // taskId
                osTmrCallbackSchedule(&OsCBAdcVoltageSample);
            }
            else
            {
                //Update callback period
                osTmrCallbackUpdate(&OsCBAdcVoltageSample,
                      ADC_VOLTAGE_SAMPLE_PERIOD_US, ADC_VOLTAGE_SAMPLE_PERIOD_US,
                      OS_TIMER_RELAXED_MODE_USE_NORMAL,
                      OS_TIMER_UPDATE_USE_BASE_LWRRENT);
            }
#else
            osTimerScheduleCallback(
                &PerfOsTimer,                                            // pOsTimer
                PERF_OS_TIMER_ENTRY_ADC_VOLTAGE_SAMPLE_CALLBACK,         // index
                lwrtosTaskGetTickCount32(),                              // ticksPrev
                ADC_VOLTAGE_SAMPLE_PERIOD_US,                            // usDelay
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC),// flags
                clkAdcVoltageSampleCallback,                             // pCallback
                NULL,                                                    // pParam
                OVL_INDEX_IMEM(libClkVolt));                             // ovlIdx
#endif
    }

clkAdcsLoad_EXTENDED_exit:
    return status;
}

/*!
 * @brief Program ADC to input SW override code and mode of operation.
 *
 * @param[in]   pAdcDevice      Pointer to the ADC device
 * @param[in]   overrideCode    SW override ADC code.
 * @param[in]   overrideMode    SW override mode of operation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT If pAdcDevice is NULL
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcProgram
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwU8            overrideCode,
    LwU8            overrideMode
)
{
    FLCN_STATUS status;

    if (pAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcProgram_exit;
    }

    status = clkAdcCodeOverrideSet_HAL(&Clk, pAdcDevice, overrideCode, overrideMode);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkAdcProgram_exit;
    }

    // Cache the override code and mode of operation in the ADC device
    pAdcDevice->overrideCode = overrideCode;
    pAdcDevice->overrideMode = overrideMode;

clkAdcProgram_exit:
    return status;
}

/*!
 * @brief Program ADC group with requested voltage and mode of operation.
 *
 * @param[in] LW2080_CTRL_CLK_ADC_SW_OVERRIDE_LIST  Pointer to the ADC SW
 *                                                  override list
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT If:
 *             1. pAdcSwOverrideList is NULL
 *             2. The input volt rail is not a subset of the VOLT_RAIL mask
 *             3. The input voltage value is out of bounds
 * @return other  Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkAdcsProgram_IMPL
(
    LW2080_CTRL_CLK_ADC_SW_OVERRIDE_LIST   *pAdcSwOverrideList
)
{
    VOLT_RAIL              *pVoltRail;
    BOARDOBJGRPMASK_E32     tmpVoltRailsMask;
    FLCN_STATUS             status;
    LwBoardObjIdx           voltRailIdx;
    LwU8                    overrideCode;

    if (pAdcSwOverrideList == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsProgram_exit;
    }

    // 1. Import the volt rail mask.
    boardObjGrpMaskInit_E32(&tmpVoltRailsMask);
    status = boardObjGrpMaskImport_E32(&tmpVoltRailsMask,
                &(pAdcSwOverrideList->voltRailsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkAdcsProgram_exit;
    }

    // 2. Sanity check that the input volt rail mask is subset of VOLT_RAIL mask.
    BOARDOBJGRP_E32 *pBoardObjGrpE32 = &(VOLT_RAILS_GET()->super);
    if (!boardObjGrpMaskIsSubset(&tmpVoltRailsMask, &(pBoardObjGrpE32->objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsProgram_exit;
    }

    // 3. Update the rail specific voltage delta params.
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pVoltRail, voltRailIdx, &tmpVoltRailsMask.super)
    {
        CLK_ADC_DEVICE *pAdcDev;
        LwBoardObjIdx    devIdx;

        // Sanity check for allowed voltage range.
        if ((pAdcSwOverrideList->volt[voltRailIdx].voltageuV <
                CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) ||
            (pAdcSwOverrideList->volt[voltRailIdx].voltageuV >
                CLK_NAFLL_LUT_MAX_VOLTAGE_UV()))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkAdcsProgram_exit;
        }

        // Compute the overrideCode to set
        overrideCode = CLK_NAFLL_LUT_IDX(pAdcSwOverrideList->volt[voltRailIdx].voltageuV);

        //
        // Program ADC overrides for NAFLL ADCs. ISINK ADCs don't have this
        // capability due to L3 write priv protection. If new ADC device types
        // other than NAFLL and ISINK are added in future then this code should
        // be revisited as we're looping over NAFLL ADCs only.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, devIdx,
            &Clk.clkAdc.pAdcs->nafllAdcDevicesMask.super)
        {
            //
            // Program all ADCs monitoring the requested voltage rail and operating
            // under requested mode of operation.
            //
            if ((pAdcDev->voltRailIdx  == BOARDOBJ_GRP_IDX_TO_8BIT(voltRailIdx)) &&
                (pAdcDev->overrideMode == pAdcSwOverrideList->volt[voltRailIdx].overrideMode))
            {
                status = clkAdcProgram(pAdcDev,
                            overrideCode,
                            pAdcDev->overrideMode);
                // return on first failure
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkAdcsProgram_exit;
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    BOARDOBJGRP_ITERATOR_END;

clkAdcsProgram_exit:
    return status;
}

/*!
 * @brief Sets or clears the voltage override for a given ADC device
 *
 * @param[in]   pAdcDevice  Pointer to the ADC device
 * @param[in]   voltageuV   voltage value in uV to override with
 * @param[in]   clientId    Client-Id that is requesting enable or disable
 * @param[in]   bSet        LW_TRUE if set. LW_FALSE if clear
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT If
 *             1. pAdcDevice is NULL
 *             2. The input voltage value is out of bounds
 * @return other  Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkAdcPreSiliconSwOverrideSet
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwU32           voltageuV,
    LwU8            clientId,
    LwBool          bSet
)
{
    FLCN_STATUS status;
    LwU8        overrideCode = CLK_ADC_OVERRIDE_CODE_ILWALID;
    LwU8        overrideMode = LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_HW_REQ;

    if (pAdcDevice == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (bSet)
    {
        if ((voltageuV < CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) ||
            (voltageuV > CLK_NAFLL_LUT_MAX_VOLTAGE_UV()))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
        // Compute the overrideCode to set
        overrideCode = CLK_NAFLL_LUT_IDX(voltageuV);
        overrideMode = LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_SW_REQ;
    }

    status = clkAdcCodeOverrideSet_HAL(&Clk, pAdcDevice, overrideCode, overrideMode);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (!pAdcDevice->bPoweredOn &&
        (pAdcDevice->disableClientsMask != 0U))
    {
        status = clkAdcPowerOnAndEnable(pAdcDevice,
                                        clientId,
                                        LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
                                        LW_TRUE);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    // Cache the override code in the ADC device
    pAdcDevice->overrideCode = overrideCode;

    return  status;
}

/*!
 * @brief Gets the instantaneous ADC code value from the ADC_MONITOR register
 *        for a given ADC device
 *
 * @param[in]   pAdcDevice  Pointer to the ADC device
 * @param[out]  pInstCode   Pointer to the instantaneous ADC code value
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT If pAdcDevice is NULL
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcInstCodeGet
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwU8           *pInstCode
)
{
    if (pAdcDevice == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Return early in case register access to the ADC is restricted
    if (!CLK_ADC_IS_HW_ACCESS_ENABLED(pAdcDevice))
    {
        return FLCN_OK;
    }

    return clkAdcInstCodeGet_HAL(&Clk, pAdcDevice, pInstCode);
}

/*!
 * @brief Compute the ADC code offset to inlwlcate temperature or voltage based
 *        evaluations for calibration.
 *
 * This function only computes and caches the offset, the actual programming
 * happens later as a part of @ref clkNafllLutProgram for V20 and as a part of
 * @ref clkAdcsProgramCodeOffset for V30.
 *
 * @param[in]   bVFEEvalRequired    VF Lwrve re-evaluation required or not
 * @param[in]   bTempChange      Boolean to indicate if this is called right after
 *     a temperature change. When set to FALSE, it is understood that this function
 *     is called prior to an anticipated voltage change.
 * @param[in]   pTargetVoltList  List of target voltages for every supported
 *     voltage rail when called prior to an anticipated voltage change. This is a
 *     no-op when this function is called right after a temperature change, i.e.,
 *     when bTempChange = TRUE.
 *
 * @return FLCN_OK All ADC devices have successfully computed the offsets
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcsComputeCodeOffset
(
    LwBool                              bVFEEvalRequired,
    LwBool                              bTempChange,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pTargetVoltList
)
{
    CLK_ADC_DEVICE  *pAdcDev;
    LwBoardObjIdx    devIdx;
    FLCN_STATUS      status  = FLCN_OK;

    // Callwlate ADC Calibration offset
    BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, devIdx, NULL)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION)
        // If dynamic calibration is disabled, no action to perform
        if (!pAdcDev->bDynCal)
        {
            continue;
        }
#endif // PMU_PERF_ADC_RUNTIME_CALIBRATION

        switch (pAdcDev->super.type)
        {
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
            {
                // Reference Bug - http://lwbugs/200423771
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771) &&
                    PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V20_EXTENDED))
                {
                    status = clkAdcComputeCodeOffset_V20(pAdcDev, bVFEEvalRequired);
                    // return on first failure
                    if (status != FLCN_OK)
                    {
                        goto clkAdcsComputeCodeOffset_exit;
                    }
                }
                break;
            }
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION) &&
                    PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30_EXTENDED))
                {
                    status = clkAdcComputeCodeOffset_V30(pAdcDev, bTempChange, pTargetVoltList);
                    // return on first failure
                    if (status != FLCN_OK)
                    {
                        goto clkAdcsComputeCodeOffset_exit;
                    }
                }
                break;
            }
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION) &&
                    PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30_ISINK_V10_EXTENDED))
                {
                    status = clkAdcV30IsinkComputeCodeOffset_V10(pAdcDev, bTempChange, pTargetVoltList);
                    // return on first failure
                    if (status != FLCN_OK)
                    {
                        goto clkAdcsComputeCodeOffset_exit;
                    }
                }
                break;
            }
            default:
            {
                // Do Nothing
                break;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkAdcsComputeCodeOffset_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Program the ADC code offset to inlwlcate temperature or voltage based
 *        evaluations for calibration.
 *
 * This function only programs the ADC calibration code offset and the callwlation
 * happens as a part of clkAdcsComputeCodeOffset.
 *
 * @param[in]   bPreVoltOffsetProg  Boolean to indicate if this is called prior
 *     to an anticipated voltage change. When set to FALSE, it is understood
 *     that the function is called post a voltage or temperature change.
 *
 * @return FLCN_OK All ADC devices have successfully programmed the offset
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcsProgramCodeOffset
(
    LwBool  bPreVoltOffsetProg
)
{
    CLK_ADC_V20        *pAdcsV20   = (CLK_ADC_V20 *)CLK_ADCS_GET();
    FLCN_STATUS         status     = FLCN_OK;
    CLK_ADC_DEVICE     *pAdcDev;
    BOARDOBJGRPMASK_E32 adcMask;
    LwBoardObjIdx       devIdx;

    // Return early if ADC runtime calibration is not enabled
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION))
    {
        return status;
    }

    // Initialize the mask to 0
    boardObjGrpMaskInit_E32(&adcMask);

    //
    // Re-initialize the mask of ADCs to run on depending on when this function
    // is called - just prior to a voltage change or post a temperature/voltage
    // change.
    //
    if (bPreVoltOffsetProg)
    {
        status = boardObjGrpMaskCopy(&adcMask, &pAdcsV20->preVoltAdcMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkAdcsProgramCodeOffset_exit;
        }
    }
    else
    {
        status = boardObjGrpMaskCopy(&adcMask, &pAdcsV20->postVoltOrTempAdcMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkAdcsProgramCodeOffset_exit;
        }
    }

    // Callwlate ADC Calibration offset
    BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, devIdx, &adcMask.super)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION)
        // If dynamic calibration is disabled, no action to perform
        if (!pAdcDev->bDynCal)
        {
            continue;
        }
#endif // PMU_PERF_ADC_RUNTIME_CALIBRATION

        switch (pAdcDev->super.type)
        {
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION))
                {
                    status = clkAdcProgramCodeOffset_V30(pAdcDev, bPreVoltOffsetProg);
                    // return on first failure
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto _clkAdcsProgramCodeOffset_exit;
                    }
                }
                break;
            }
            default:
            {
                // Do Nothing
                break;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

_clkAdcsProgramCodeOffset_exit:
    return status;
}

/*!
 * @brief Get the ADC voltage value and the related parameters for all the ADCs specified in the
 * given mask.
 *
 * This interface intends to read the sensed voltage as reported by ADCs- it acts as a wrapper to
 * @ref s_clkAdcVoltageRead and facilitates fetching ADC sensed voltage related data for multiple
 * ADCs at the same time (as specified in the input mask).
 *
 * NOTE1: This interface is only meant for internal usage with CLK_ADC code. All the clients external
 *        to that  *MUST* make use of CLK_ADCS_VOLTAGE_READ_EXTERNAL macro. This macro essentially
 *        calls the function below but ensures some of the parameters meant for internal usage are
 *        left untouched by the clients in all cases.
 *
 * NOTE2: When bOnlyUpdateAccSampleCache is set to true we do not care about the input pAdcAccSample
 *        as we'd just update the internal cached values in that case.
 *
 * @param[in]      pAdcMask                  Pointer to mask of ADCs for which we need to get ADC_ACC_SAMPLE
 * @param[in,out]  pAdcAccSample             Pointer to the list of ADC aclwmulator sample struct
 * @param[in]      bOnlyUpdateAccSampleCache Boolean to indicate that we just want to update the
 *     internal cache for ADC aclwmulator sample (CLK_ADC_DEVICE.adcAccSample) with the current
 *     readings instead of passing back the updated data via pAdcAccSample input parameter.
 * @param[in]      clientId                  Id of the client requesting the operation
 *
 * @return FLCN_OK                   Operation completed successfully or if
 *                                   nothing changed
 * @return FLCN_ERR_ILWALID_ARGUMENT If adcMask is zero or pAdcAccSampleList is NULL
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcsVoltageReadWrapper
(
    BOARDOBJGRPMASK_E32    *pAdcMask,
    CLK_ADC_ACC_SAMPLE     *pAdcAccSample,
    LwBool                  bOnlyUpdateAccSampleCache,
    LwU8                    clientId
)
{
    CLK_ADC_DEVICE *pAdcDev        = NULL;
    LwBoardObjIdx   adcDevIdx;
    FLCN_STATUS     status         = FLCN_OK;
    LwBool          bMsGrpDisabled = LW_FALSE;

    //
    // If MSCG is supported, disable it before issuing first access.
    // This request can not be initiated from a LPWR context so avoid
    // it if the client id is another LPWR feature i.e. GRRG for now
    //
    // This client check is a temporary WAR and the clean-up is tracked
    // in bug 200646402
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
        (clientId != LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG))
    {
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
        lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        bMsGrpDisabled = LW_TRUE;
    }

    //
    // Sanity check the mask
    // Need to avoid using boardObjGrp interfaces since this is a resident
    // interface for MSCG to work. And boardObjGrp interfaces are not resident.
    //
    if ((pAdcMask->super.pData[0] == 0U) ||
        ((Clk.clkAdc.super.objMask.super.pData[0] & pAdcMask->super.pData[0]) 
          != pAdcMask->super.pData[0]))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT; 
        goto clkAdcsVoltageReadWrapper_error_exit;
    }

    //
    // Sanity check the input pointer to ADC sample list when we actually want
    // to use it. i.e., when this call is not made to update the internal cache.
    //
    if (!bOnlyUpdateAccSampleCache && (pAdcAccSample == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT; 
        goto clkAdcsVoltageReadWrapper_error_exit;
    }

    appTaskCriticalEnter();
    {
        //
        // Bail-out early, in case access to HW registers is disabled and no
        // HW access handling per device further down the stack i.e. GA10X+.
        // The caller will anyways have the SW cached value of the voltage
        // for the given ADC devices.
        //
        if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC) &&
            (Clk.clkAdc.adcAccessDisabledMask != 0U))
        {
            goto clkAdcVoltageReadWrapper_exit;
        }

        // Latch all ADCs
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_ACC_BATCH_LATCH_SUPPORTED))
        {
            BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
            {
                status = s_clkAdcAccLatch(pAdcDev, LW_TRUE);
                if (status != FLCN_OK)
                {
                    goto clkAdcVoltageReadWrapper_exit;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }

        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
        {
            if (bOnlyUpdateAccSampleCache)
            {
                status = s_clkAdcVoltageRead(pAdcDev, &pAdcDev->adcAccSample);
            }
            else
            {
                status = s_clkAdcVoltageRead(pAdcDev, pAdcAccSample + adcDevIdx);
            }
            if (status != FLCN_OK)
            {
                goto clkAdcVoltageReadWrapper_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        // Unlatch all ADCs
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_ACC_BATCH_LATCH_SUPPORTED))
        {
            BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
            {
                status = s_clkAdcAccLatch(pAdcDev, LW_FALSE);
                if (status != FLCN_OK)
                {
                    goto clkAdcVoltageReadWrapper_exit;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }
    }

clkAdcVoltageReadWrapper_exit:
    appTaskCriticalExit();

clkAdcsVoltageReadWrapper_error_exit:
    // Re-enable MS Group if it was disabled
    if (bMsGrpDisabled)
    {
        lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
/*!
 * @brief Interface to enable/disable RAM_ASSIST ADC_CTRL for all the ADCs
 * specified in the given mask.
 *
 * @param[in]  pAdcMask        Pointer to mask of ADCs
 * @param[in]  bEnable         Boolean to indicate enable/disable
 *
 * @return FLCN_OK                   Operation completed successfully or if
 *                                   nothing changed
 * @return FLCN_ERR_NOT_SUPPORTED    In case the feature is not supported
 * @return FLCN_ERR_ILWALID_ARGUMENT If any sanity check fails for any in-param
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 *
 * TODO: @kwadhwa - add GH20X support in Features.pm once the required registers
 *       get added to the manuals.
 */
FLCN_STATUS
clkAdcsRamAssistCtrlEnable
(
    BOARDOBJGRPMASK_E32  *pAdcMask,
    LwBool                bEnable
)
{
    FLCN_STATUS     status   = FLCN_OK;
    CLK_ADC_DEVICE *pAdcDev  = NULL;
    LwBoardObjIdx   adcDevIdx;

    //
    // Sanity check if all the ADCs are powered-on, enabled and belong to the
    // same voltage rail. Additionally, the input ADC mask must be subset of
    // NAFLL ADC Devices mask.
    //
    if ((!clkAdcsEnabled(*pAdcMask)) ||
        (!s_clkAdcsVoltRailSanityCheck(pAdcMask)))
    {    
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsRamAssistCtrlEnable_exit;
    }

    // Elevate to critical to avoid inconsistent state across GPCs
    appTaskCriticalEnter();
    {
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
        {
            status = clkAdcDeviceRamAssistCtrlEnable_HAL(&Clk, pAdcDev, bEnable);
            if (status != FLCN_OK)
            {
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    appTaskCriticalExit();

clkAdcsRamAssistCtrlEnable_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Program the RAM_ASSIST VCrit threshold values for all the ADCs specified in the
 * given mask.
 *
 * @param[in]  pAdcMask           Pointer to mask of ADCs
 * @param[in]  vcritThreshLowuV   RAM_ASSIST Vcrit Low threshold value in microvolts to program
 * @param[in]  vcritThreshHighuV  RAM_ASSIST Vcrit High threshold value in microvolts to program
 *
 * @return FLCN_OK                   Operation completed successfully or if
 *                                   nothing changed
 * @return FLCN_ERR_NOT_SUPPORTED    In case the feature is not supported
 * @return FLCN_ERR_ILWALID_ARGUMENT If any sanity check fails for any in-param
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 *
 * TODO: @kwadhwa - add GH20X support in Features.pm once the required registers
 *       get added to the manuals.
 */
FLCN_STATUS
clkAdcsProgramVcritThresholds
(
    BOARDOBJGRPMASK_E32  *pAdcMask,
    LwU32                 vcritThreshLowuV,
    LwU32                 vcritThreshHighuV
)
{
    FLCN_STATUS     status   = FLCN_OK;
    CLK_ADC_DEVICE *pAdcDev  = NULL;
    LwBoardObjIdx   adcDevIdx;

    //
    // Sanity check if all the ADCs are powered-on, enabled and belong to the
    // same voltage rail. Additionally, the input ADC mask must be subset of
    // NAFLL ADC Devices mask.
    //
    if ((!clkAdcsEnabled(*pAdcMask)) ||
        (!s_clkAdcsVoltRailSanityCheck(pAdcMask)))
    {    
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsProgramVcritThresholds_exit;
    }

    // Sanity check the input parameters
    if ((!CLK_ADC_IS_VOLTAGE_IN_POR_RANGE(vcritThreshLowuV)) ||
        (!CLK_ADC_IS_VOLTAGE_IN_POR_RANGE(vcritThreshHighuV)) ||
        (vcritThreshLowuV  >= vcritThreshHighuV))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsProgramVcritThresholds_exit;
    }

    // Elevate to critical to avoid inconsistent state across GPCs
    appTaskCriticalEnter();
    {
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
        {
            status = clkAdcDeviceProgramVcritThresholds_HAL(&Clk, pAdcDev, vcritThreshLowuV, vcritThreshHighuV);
            if (status != FLCN_OK)
            {
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    appTaskCriticalExit();

clkAdcsProgramVcritThresholds_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Interface to check if RAM Assist is Engaged for all the ADCs
 * specified in the given mask.
 *
 * @param[in]  pAdcMask                  Pointer to mask of ADCs
 * @param[in]  pRamAssistEngagedMask     Pointer to RAM Assist Engaged Mask
 *
 * @return FLCN_OK                   Operation completed successfully or if
 *                                   nothing changed
 * @return FLCN_ERR_NOT_SUPPORTED    In case the feature is not supported
 * @return FLCN_ERR_ILWALID_ARGUMENT If any sanity check fails for any in-param
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcsRamAssistIsEngaged
(
    BOARDOBJGRPMASK_E32  *pAdcMask,
    LwU32                *pRamAssistEngagedMask
)
{
    FLCN_STATUS     status   = FLCN_OK;
    CLK_ADC_DEVICE *pAdcDev  = NULL;
    LwBool          bEngaged = LW_FALSE;
    LwBoardObjIdx   adcDevIdx;

    //
    // Sanity check if all the ADCs are powered-on, enabled and belong to the
    // same voltage rail. Additionally, the input ADC mask must be subset of
    // NAFLL ADC Devices mask.
    //
    if ((!clkAdcsEnabled(*pAdcMask)) ||
        (!s_clkAdcsVoltRailSanityCheck(pAdcMask)))
    {    
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcsRamAssistIsEngaged_exit;
    }

    *pRamAssistEngagedMask = 0;
    // Elevate to critical to avoid inconsistent state across GPCs
    appTaskCriticalEnter();
    {
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
        {
            bEngaged = LW_FALSE;
            status   = clkAdcDeviceRamAssistIsEngaged_HAL(&Clk, pAdcDev, &bEngaged);
            if (status != FLCN_OK)
            {
                break;
            }
            if (bEngaged)
            {
                *pRamAssistEngagedMask |= LWBIT(adcDevIdx);
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    appTaskCriticalExit();

clkAdcsRamAssistIsEngaged_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}
#endif // PMU_CLK_ADC_RAM_ASSIST

/*!
 * @brief Program latch ADC aclwmulator values before reading them.
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 * @param[in]   bLatchAcc   Boolean to indicate if CAPTURE_ACC is set
 *
 * @return FLCN_OK                      Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT    In case ADC device is NULL
 * @return FLCN_ERR_ILWALID_STATE       In case the ADC is not powered-on/enabled
 */
static FLCN_STATUS
s_clkAdcAccLatch
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bLatchAcc
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pAdcDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_clkAdcAccLatch_exit;
    }

    // Check for the HW register access first - return early if access disabled
    if (!CLK_ADC_IS_HW_ACCESS_ENABLED(pAdcDev))
    {
        goto s_clkAdcAccLatch_exit;
    }

    //
    // Return early if the ADC is not already powered-on/enabled
    // (a preceding SET_CONTROL)
    //
    if ((!pAdcDev->bPoweredOn) || (pAdcDev->disableClientsMask != 0U))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto s_clkAdcAccLatch_exit;
    }

    //
    // Program CAPTURE_ACC to latch/unlatch aclwmulator values.
    // ISINK ADCs don't have this capability due to L3 write priv protection.
    // VBIOS devinit sets the continous capture mode for them.
    //
    if (BOARDOBJ_GET_TYPE(pAdcDev) !=
            LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
    {
        status = clkAdcAccCapture_HAL(&Clk, pAdcDev, bLatchAcc);
        if (status != FLCN_OK)
        {
            goto s_clkAdcAccLatch_exit;
        }
    }

s_clkAdcAccLatch_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Refresh/Get the ADC voltage value and all related parameters
 *
 * Serves as a helper for @ref clkAdcVoltageReadWrapper and should not be called
 * into directly for getting voltage readings.
 *
 * @param[in]      pAdcDev       Pointer to the ADC device
 * @param[in,out]  pAdcAccSample Pointer to the Aclwmulator sample struct
 *
 * @return FLCN_OK                   Operation completed successfully or if
 *                                   nothing changed
 * @return FLCN_ERR_ILWALID_ARGUMENT If the ADC device/sample pointer is NULL
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
static FLCN_STATUS
s_clkAdcVoltageRead
(
    CLK_ADC_DEVICE     *pAdcDev,
    CLK_ADC_ACC_SAMPLE *pAdcAccSample
)
{
    FLCN_STATUS status;
    LwU32       accLwrr;
    LwU32       accLast;
    LwU32       accDiff;
    LwU32       numSamplesLwrr;
    LwU32       numSamplesLast;
    LwU32       numSamplesDiff;
    LwU32       sampledVoltageuV;

    if (pAdcDev == NULL || pAdcAccSample == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_clkAdcVoltageRead_exit;
    }

    // Read back the cached value for the ADC first
    accLast = pAdcAccSample->adcAclwmulatorVal;
    numSamplesLast = pAdcAccSample->adcNumSamplesVal;

    //
    // The register read only goes thru' if the device's HW register access
    // is enabled
    //
    if (CLK_ADC_IS_HW_ACCESS_ENABLED(pAdcDev))
    {
        // Now the register reads
        status = clkAdcVoltageParamsRead_HAL(&Clk, pAdcDev, pAdcAccSample);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkAdcVoltageRead_exit;
        }
        accLwrr = pAdcAccSample->adcAclwmulatorVal;
        numSamplesLwrr = pAdcAccSample->adcNumSamplesVal;
    }
    else
    {
        status = FLCN_OK;
        accLwrr = pAdcDev->adcAccSample.adcAclwmulatorVal;
        numSamplesLwrr = pAdcDev->adcAccSample.adcNumSamplesVal;
        pAdcAccSample->adcAclwmulatorVal = accLwrr;
        pAdcAccSample->adcNumSamplesVal  = numSamplesLwrr;
    }

    // Callwlate the num of samples
    accDiff = accLwrr - accLast;
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_IS_SENSED_VOLTAGE_PTIMER_BASED))
    {
        status = s_clkAdcCalcNumSamplesDiff_PTIMER(pAdcAccSample, &numSamplesDiff);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkAdcVoltageRead_exit;
        }
    }
    else
    {
        numSamplesDiff = numSamplesLwrr - numSamplesLast;
    }

    // Now for the actual voltage
    if (numSamplesDiff != 0)
    {
        pAdcAccSample->sampledCode = (LwU8)(accDiff / numSamplesDiff);
        sampledVoltageuV = (pAdcAccSample->sampledCode == 0) ? 0U : 
                            ((pAdcAccSample->sampledCode * 
                                  clkAdcStepSizeUvGet_HAL(&Clk, pAdcDev)) +
                              clkAdcMilwoltageUvGet_HAL(&Clk, pAdcDev));
    }
    else
    {
        pAdcAccSample->sampledCode = 0U;
        sampledVoltageuV = 0U;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_HW_CALIBRATION) &&
        pAdcDev->bHwCalEnabled)
    {
        // This is HW corrected voltage
        pAdcAccSample->correctedVoltageuV = sampledVoltageuV;
        // Setting to LW_U32_MAX to suggest un-supported
        pAdcAccSample->actualVoltageuV    = LW_U32_MAX;
    }
    else // GP100-only
    {
        CLK_ADC_DEVICE_V10 *pAdcDevV10    = (CLK_ADC_DEVICE_V10 *)(pAdcDev);
        pAdcAccSample->actualVoltageuV    = sampledVoltageuV;
        pAdcAccSample->correctedVoltageuV = (pAdcAccSample->sampledCode *
                                                pAdcDevV10->data.adcCal.slope) +
                                            pAdcDevV10->data.adcCal.intercept;
    }

s_clkAdcVoltageRead_exit:
    return status;
}

/*!
 * @brief Callwlate the number of aclwmulator samples based on PTIMER
 *
 * It also updates the last timestamp value for the aclwmulator sample
 *
 * @param[in,out]  pAdcAccSample   Pointer to the Aclwmulator sample struct
 * @param[in,out]  pNumSamplesDiff Pointer to the num sample diff variable
 *
 * @return  FLCN_OK                    If operation completed successfully
 * @return  FLCN_ERR_ILWALID_ARGUMENT  In case the feature is not supported or
                                       if any of the in-params is 'NULL'
 */
static FLCN_STATUS
s_clkAdcCalcNumSamplesDiff_PTIMER
(
    CLK_ADC_ACC_SAMPLE  *pAdcAccSample,
    LwU32               *pNumSamplesDiff
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU64       timeNsLwrr;
    LwU64       timeNsLast;
    LwU64       timeNsDiff64;
    LwU64       divVal = (LwU64)TIME_NS_TO_US;
    LwU64       mulVal = (LwU64)TIMER_FREQ_MHZ;

    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_IS_SENSED_VOLTAGE_PTIMER_BASED) ||
        (pAdcAccSample == NULL) || 
        (pNumSamplesDiff == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkAdcCalcNumSamplesDiff_PTIMER_exit;
    }

    // Get the current timestamp
    osPTimerTimeNsLwrrentGetAsLwU64(&timeNsLwrr);
    RM_FLCN_U64_UNPACK(&timeNsLast, &pAdcAccSample->timeNsLast);

    //
    // numSamplesDiff64 = timeNsDiff * PTIMER Freq (27 MHz)
    // numSamplesDiff64 = timeDiff * 10^(-9) * 27 * 10^6
    // numSamplesDiff64 = timeDiff * 27 / 10^3
    //
    lw64Sub(&timeNsDiff64, &timeNsLwrr, &timeNsLast);
    lwU64Mul(&timeNsDiff64, &timeNsDiff64, &mulVal);
    lwU64Div(&timeNsDiff64, &timeNsDiff64, &divVal);

    *pNumSamplesDiff = (LwU32)timeNsDiff64;

    // Cache the timestamp of the sample
    RM_FLCN_U64_PACK(&pAdcAccSample->timeNsLast, &timeNsLwrr);

s_clkAdcCalcNumSamplesDiff_PTIMER_exit:
    return status;
}

/*!
 * @brief Helper interface which synchronizes access to the ADC registers.
 *
 * @details This interface will be called by features like PG and GCX before gating
 * the clocks, and after restoring them.
 *
 * @param[in] bAccessEnable  Boolean indicating whether access is to enabled
 *                           or disabled.
 * @param[in] clientId       ID of the client that wants to enable/disable the
 *                           access.
 */
void
clkAdcHwAccessSync
(
    LwBool bAccessEnable,
    LwU8   clientId
)
{
    // Elevate to critical to synchronize protected the SW counter state.
    appTaskCriticalEnter();
    {
        if (bAccessEnable)
        {
            Clk.clkAdc.adcAccessDisabledMask &= ~LWBIT_TYPE(clientId, LwU32);
        }
    }
    appTaskCriticalExit();

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC) &&
        (clientId != LW2080_CTRL_CLK_ADC_CLIENT_ID_INIT))
    {
        if (clkAdcDeviceHwRegisterAccessSync(&Clk.clkAdc.super.objMask, clientId, bAccessEnable) != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return;
        }
    }

    // Elevate to critical to synchronize protected the SW counter state.
    appTaskCriticalEnter();
    {
        if (!bAccessEnable)
        {
            Clk.clkAdc.adcAccessDisabledMask |= LWBIT_TYPE(clientId, LwU32);
        }
    }
    appTaskCriticalExit();
}

/*!
 * @ref   OsTimerCallback
 *
 * @brief OS_TIMER callback which implements ADC voltage sampling
 *
 * @note  Scheduled from @ref perfPreInit().
 */
void
clkAdcVoltageSampleCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_clkAdcVoltageSampleOsCallback(NULL);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @ref   OsTimerOsCallback
 *
 * @brief OS_TMR callback which implements ADC voltage sampling
 *
 * @note  Scheduled from @ref perfPreInit().
 */
static FLCN_STATUS
s_clkAdcVoltageSampleOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Proceed only if all the ADCs have been powered on/enabled or we have
        // Hw access handling per device further down the stack i.e. GA10X+
        //
        if ((CLK_IS_AVFS_SUPPORTED() &&
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC))) ||
             clkAdcsEnabled(Clk.clkAdc.super.objMask))
        {
            // Update sampled voltage data for all the supported ADC device indices
            status = clkAdcsVoltageReadWrapper(&Clk.clkAdc.super.objMask, NULL, 
                         LW_TRUE, LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Initializes all supported ADC Device Aclwmulators
 */
FLCN_STATUS
clkAdcsAccInit
(
    void
)
{
    CLK_ADC_DEVICE *pAdcDev;
    LwBoardObjIdx   adcDevIdx;
    FLCN_STATUS     status  = FLCN_OK;

    //
    // Initialize all supported NAFLL ADC device Aclwmulators. ISINK ADC device
    // Aclwmulators are initialized in VBIOS devinit as they are L3 write
    // protected. If new ADC device types other than NAFLL and ISINK are added
    // in future then this code should be revisited as we're looping over NAFLL
    // ADCs only.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx,
                                &Clk.clkAdc.pAdcs->nafllAdcDevicesMask.super)
    {
        // Refresh voltage and related parameters
        status = clkAdcAccInit_HAL(&Clk, pAdcDev, &pAdcDev->adcAccSample);
        if (status != FLCN_OK)
        {
            goto clkAdcsAccInit_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkAdcsAccInit_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
/*!
 * @brief Static helper to check if all the ADCs specified in the given mask
 * belong to the same Voltage rail or not. Additionally, the input ADC mask
 * must be subset of NAFLL ADC Devices mask.
 *
 * @param[in]  pAdcMask           Pointer to mask of ADCs
 *
 * @return LW_TRUE  If sanity check passed.
 * @return LW_FALSE Otherwise
 */
static LwBool
s_clkAdcsVoltRailSanityCheck
(
    BOARDOBJGRPMASK_E32    *pAdcMask
)
{
    CLK_ADC_DEVICE *pAdcDev  = NULL;
    LwU8            railIdx  = LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID;
    LwBoardObjIdx   adcDevIdx;

    // Sanity check the input ADC mask
    if (!boardObjGrpMaskIsSubset(pAdcMask, &(Clk.clkAdc.pAdcs->nafllAdcDevicesMask)))
    {
        return LW_FALSE;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
    {
        // Sanity check individual ADC devices
        if (pAdcDev->voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
        {
            return LW_FALSE;
        }

        // First call
        if (LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID == railIdx)
        {
            railIdx = pAdcDev->voltRailIdx;
        }
        // Subsequent calls
        else if (railIdx != pAdcDev->voltRailIdx)
        {
            return LW_FALSE;
        }
        // All good
        else
        {
            // Do nothing
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return LW_TRUE;
}
#endif // PMU_CLK_ADC_RAM_ASSIST
