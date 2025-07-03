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
 * @file   pmu_clkadcs_1x.c
 * @brief  File for CLK_ADC_V10 specific codebase
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
 * @brief Constructor for ADC devices group version 10 (CLK_ADC_V10)
 * 
 * @copydetails ClkAdcDevicesIfaceModel10SetHeader
 */
FLCN_STATUS
clkIfaceModel10SetHeaderAdcDevices_1X
(
    BOARDOBJGRP_IFACE_MODEL_10            *pModel10,
    RM_PMU_BOARDOBJGRP    *pHdrDesc,
    LwU16                   size,
    CLK_ADC               **ppAdcs
)
{
    FLCN_STATUS   status  = FLCN_OK;

    // Construct the super class object first
    status = clkIfaceModel10SetHeaderAdcDevices_SUPER(pModel10, pHdrDesc, size, ppAdcs);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkConstructAdcDevices_1X_exit;
    }

    // Set CLK_ADC_10 specific parameters here

_clkConstructAdcDevices_1X_exit:
    return status;
}
