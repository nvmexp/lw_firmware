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
 * @file   pmu_clkadc_v20_extended.c
 * @brief  File for ADC device version 2.0 routines applicable only to DGPUs and
 *          contains all the extended support to the ADC base infra.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Compute the ADC code offset to inlwlcate temperature based evaluations
 *        for calibration
 * 
 * Evaluate the VFE Index and cache the callwlated offset.
 *
 * @param[in]   pAdcDevice          Pointer to the ADC device
 * @param[in]   bVFEEvalRequired    VF Lwrve re-evaluation required or not
 *
 * @return FLCN_OK VFE Index is marked invalid or VFE equation evaluation is
 *                 successful
 * @return other   Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcComputeCodeOffset_V20
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool           bVFEEvalRequired
)
{
    CLK_ADC_DEVICE_V20        *pAdcDevV20 = (CLK_ADC_DEVICE_V20 *)(pAdcDev);
    FLCN_STATUS                status     = FLCN_OK;
    RM_PMU_PERF_VFE_EQU_RESULT result;

    // Return early if calibration type is not V20 or offset VFE Index is invalid
    if ((pAdcDevV20->data.calType != LW2080_CTRL_CLK_ADC_CAL_TYPE_V20) ||
        (pAdcDevV20->data.adcCal.calV20.offsetVfeIdx == PMU_PERF_VFE_EQU_INDEX_ILWALID))
    {
        // Ilwalidate the offset value
        pAdcDevV20->statusData.offset = LW_S32_MAX;
        goto clkAdcComputeCodeOffset_V20_exit;
    }

    // Evaluate VFE lwrve Idx
    if (bVFEEvalRequired)
    {
        status = vfeEquEvaluate(pAdcDevV20->data.adcCal.calV20.offsetVfeIdx, NULL, 0,
                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_ADC_CODE, &result);
        if (status != FLCN_OK)
        {
            // Ilwalidate the offset value
            pAdcDevV20->statusData.offset = LW_S32_MAX;
            PMU_BREAKPOINT();
            goto clkAdcComputeCodeOffset_V20_exit;
        }

        // Store back value to ADC_DEVICE_V20 object
        pAdcDevV20->statusData.offset = result.adcCode;
    }

clkAdcComputeCodeOffset_V20_exit:
    return status;
}
