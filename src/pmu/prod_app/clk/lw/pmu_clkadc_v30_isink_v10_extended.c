/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc_v30_isink_v10_extended.c
 * @brief  File for ISINK V10 ADC V30 routines applicable only to DGPUs and
 *         contains all the extended support to the ADC base infra.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @Copydoc clkAdcCalCoeffCache
 */
FLCN_STATUS
clkAdcV30IsinkCalCoeffCache_V10
(
    CLK_ADC_DEVICE *pAdcDev
)
{
    CLK_ADC_DEVICE_V30              *pAdcDevV30 = (CLK_ADC_DEVICE_V30 *)(pAdcDev);
    CLK_ADC_DEVICE_V30_ISINK_V10    *pDev       = (CLK_ADC_DEVICE_V30_ISINK_V10 *)(pAdcDev);
    LwS32           tempCoefficientNum          = 0;
    LwS32           tempCoefficientDen          = 0;
    LwS32           voltTempCoefficientNum      = 0;
    LwS32           voltTempCoefficientDen      = 0;
    OSTASK_OVL_DESC ovlDescList[]               = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLwF32)
        };

    //
    // Callwlate & cache tempCoefficient and voltTempCoeffient based on
    // the fused values for high/low temperature errors.
    //
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        tempCoefficientNum = pAdcDevV30->infoData.highTempLowVoltErr - pAdcDevV30->infoData.lowTempLowVoltErr;
        tempCoefficientDen = pDev->infoData.highTemp - pDev->infoData.lowTemp;

        pAdcDevV30->statusData.tempCoefficient = lwF32Div(lwF32ColwertFromS32(tempCoefficientNum),
                                                          lwF32ColwertFromS32(tempCoefficientDen));

        voltTempCoefficientNum = ((pAdcDevV30->infoData.highTempHighVoltErr - pAdcDevV30->infoData.highTempLowVoltErr) -
                                  (pAdcDevV30->infoData.lowTempHighVoltErr - pAdcDevV30->infoData.lowTempLowVoltErr));

        //
        // Dividing voltTempCoefficientDen by 1000 to have it contained
        // in 32 bits. Essentially the volt difference would be in mV
        // instead of uV.
        //
        voltTempCoefficientDen = ((pDev->infoData.highVoltuV - pDev->infoData.lowVoltuV) *
                                  (pDev->infoData.highTemp - pDev->infoData.lowTemp) / 1000);

        pAdcDevV30->statusData.voltTempCoefficient = lwF32Div(lwF32ColwertFromS32(voltTempCoefficientNum),
                                                              lwF32ColwertFromS32(voltTempCoefficientDen));
        //
        // Now divide voltTempCoefficient by 1000 to rescale the volt in
        // voltTempCoefficientDento uV/degree celsius from mV/degree celsius.
        //
        pAdcDevV30->statusData.voltTempCoefficient = lwF32Div(pAdcDevV30->statusData.voltTempCoefficient, 1000.0f);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @brief Compute the ADC code offset to inlwlcate voltage and temperature based
 *        evaluations for calibration
 *
 * @param[in]   pAdcDev          Pointer to the ADC device
 * @param[in]   bTempChange      Boolean to indicate if this is called right after
 *     a temperature change. When set to FALSE, it is understood that this function
 *     is called prior to an anticipated voltage change.
 * @param[in]   pTargetVoltList  List of target voltages for every supported
 *     voltage rail when called prior to an anticipated voltage change. This is a
 *     no-op when this function is called right after a temperature change, i.e.,
 *     when bTempChange = TRUE.
 *
 * @return FLCN_OK
 *      Successful computation of ADC offset
 * @return FLCN_ERR_ILWALID_STATE
 *      Invalid input arguments
 * @return other
 *      Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcV30IsinkComputeCodeOffset_V10
(
    CLK_ADC_DEVICE                     *pAdcDev,
    LwBool                              bTempChange,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pTargetVoltList
)
{
    CLK_ADC_V20                     *pAdcsV20   = (CLK_ADC_V20 *)CLK_ADCS_GET();
    CLK_ADC_DEVICE_V30              *pAdcDevV30 = (CLK_ADC_DEVICE_V30 *)pAdcDev;
    CLK_ADC_DEVICE_V30_ISINK_V10    *pDev       = (CLK_ADC_DEVICE_V30_ISINK_V10 *)(pAdcDev);
    LwU32           lwrrVoltuV                  = RM_PMU_VOLT_VALUE_0V_IN_UV;
    FLCN_STATUS     status                      = FLCN_OK;
    LwF32           lwrrTempF32                 = 0;
    LwS32           lwrrTemp                    = 0;
    LwF32           adcCodeCorrectionOffset     = 0; 
    LwF32           tempBasedOffsetCorrection;
    LwF32           voltTempBasedGainCorrection;
    OSTASK_OVL_DESC ovlDescList[]               = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLwF32)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Target voltage list cannot be NULL for a voltage change call
        if (!bTempChange && (pTargetVoltList == NULL))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkAdcV30IsinkComputeCodeOffset_V10_exit;
        }

        // Check if ADC code correction offset is being overridden.
        if (pAdcDevV30->infoData.adcCodeCorrectionOffset != 0)
        {
            adcCodeCorrectionOffset =
                lwF32ColwertFromS32(pAdcDevV30->infoData.adcCodeCorrectionOffset);
        }
        else
        {
            // Read the current temperature using the temperature VFE variable index
            status = vfeVarEvaluate(pAdcsV20->data.tempVfeVarIdx, &lwrrTempF32);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkAdcV30IsinkComputeCodeOffset_V10_exit;
            }

            // Colwert floating point temperature value to a signed 32bit integer
            lwrrTemp = lwF32ColwertToS32(lwrrTempF32);

            //
            // When called for a temperature change we read the current voltage and when
            // called from for a voltage change, we pick voltage from the input target
            // voltage list.
            //
            if (bTempChange)
            {
                // Get the voltage rail on which the concerned ADC is
                VOLT_RAIL *pRail = VOLT_RAIL_GET(pAdcDev->voltRailIdx);

                status = voltRailGetVoltage(pRail, &lwrrVoltuV);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkAdcV30IsinkComputeCodeOffset_V10_exit;
                }
            }
            else
            {
                lwrrVoltuV = pTargetVoltList->rails[pAdcDev->voltRailIdx].voltageuV;
            }

            // [1] Callwlate temperature based offset correction that we need to apply
            tempBasedOffsetCorrection = lwF32Mul(pAdcDevV30->statusData.tempCoefficient,
                                                (LwF32)(lwrrTemp - pDev->infoData.refTemp));

            // [2] Callwlate (V,T) based gain correction that we need to apply
            voltTempBasedGainCorrection = lwF32Mul(pAdcDevV30->statusData.voltTempCoefficient,
                (LwF32)(lwrrTemp - pDev->infoData.refTemp) * (lwrrVoltuV - pDev->infoData.refVoltuV));

            // ADC code correction offset is the summation of 1 and 2 above
            adcCodeCorrectionOffset = tempBasedOffsetCorrection + voltTempBasedGainCorrection;
        }

        // Cap the ADC code correction offset with the min/max reference value
        if (adcCodeCorrectionOffset <
                lwF32ColwertFromS32(pDev->infoData.adcCodeCorrectionOffsetMin))
        {
            pAdcDevV30->targetAdcCodeCorrectionOffset =
                    pDev->infoData.adcCodeCorrectionOffsetMin;
        }
        else if (adcCodeCorrectionOffset >
                    lwF32ColwertFromS32(pDev->infoData.adcCodeCorrectionOffsetMax))
        {
            pAdcDevV30->targetAdcCodeCorrectionOffset =
                    pDev->infoData.adcCodeCorrectionOffsetMax;
        }
        else
        {
            pAdcDevV30->targetAdcCodeCorrectionOffset =
                    (LwS8)lwF32ColwertToS32(adcCodeCorrectionOffset);
        }

        // Check if the new offset is same as the current offset programmed
        if (pAdcDevV30->targetAdcCodeCorrectionOffset == pAdcDevV30->statusData.adcCodeCorrectionOffset)
        {
            //
            // We need not program the new offset in such a case. Explicitly set the
            // status to FLCN_OK and return early.
            //
            status = FLCN_OK;
            goto clkAdcV30IsinkComputeCodeOffset_V10_exit;
        }

        //
        // Set the corresponding mask of ADCs depending on whether this is a
        // temperature/voltage change and also if the offset is increasing/decreasing
        // in case of a voltage change.
        //
        if (bTempChange) // Temperature change
        {
            boardObjGrpMaskBitSet(&pAdcsV20->postVoltOrTempAdcMask, BOARDOBJ_GET_GRP_IDX(&pAdcDev->super));
        }
        else // Voltage change
        {
            if (pAdcDevV30->targetAdcCodeCorrectionOffset < pAdcDevV30->statusData.adcCodeCorrectionOffset)
            {
                //
                // If the new offset < old offset, it's safer to apply it before the voltage change
                // as the offset translates proportionally to frequency.
                //
                boardObjGrpMaskBitSet(&pAdcsV20->preVoltAdcMask, BOARDOBJ_GET_GRP_IDX(&pAdcDev->super));
            }
            else if (pAdcDevV30->targetAdcCodeCorrectionOffset > pAdcDevV30->statusData.adcCodeCorrectionOffset)
            {
                //
                // If the new offset > old offset, it's safer to apply it after the voltage change
                // as the offset translates proportionally to frequency. 
                //
                boardObjGrpMaskBitSet(&pAdcsV20->postVoltOrTempAdcMask, BOARDOBJ_GET_GRP_IDX(&pAdcDev->super));
            }
            // New offset is same as what is already programmed
            else
            {
                //
                // Do nothing!
                // This case is already taken care of above, we should ideally never
                // reach here.
                //
                goto clkAdcV30IsinkComputeCodeOffset_V10_exit;
            }
        }

clkAdcV30IsinkComputeCodeOffset_V10_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
