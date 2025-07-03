/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcs_2x.c
 * @brief  File for CLK_ADC_V20 specific codebase
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Constructor for ADC devices group version 20 (CLK_ADC_V20 *)
 * 
 * @copydetails ClkAdcDevicesIfaceModel10SetHeader
 */
FLCN_STATUS
clkIfaceModel10SetHeaderAdcDevices_2X
(
    BOARDOBJGRP_IFACE_MODEL_10            *pModel10,
    RM_PMU_BOARDOBJGRP    *pHdrDesc,
    LwU16                   size,
    CLK_ADC               **ppAdcs
)
{
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *pAdcSet =
        (RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    CLK_ADC_V20  *pAdcsV20  = NULL;
    FLCN_STATUS   status    = FLCN_OK;

    // Construct the super class object first
    status = clkIfaceModel10SetHeaderAdcDevices_SUPER(pModel10, pHdrDesc, size, ppAdcs);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkConstructAdcDevices_2X_exit;
    }

    // Set CLK_ADC_20 specific parameters here
    pAdcsV20 = (CLK_ADC_V20 *)*ppAdcs;

    pAdcsV20->data = pAdcSet->data.adcsV20;

    boardObjGrpMaskInit_E32(&pAdcsV20->preVoltAdcMask);
    boardObjGrpMaskInit_E32(&pAdcsV20->postVoltOrTempAdcMask);

_clkConstructAdcDevices_2X_exit:
    return status;
}
