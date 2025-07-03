/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc_v10.c
 * @brief  File for ADC device version 1.0 routines.
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
 * Construct a ADC_DEVICE_V10 object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkAdcDevGrpIfaceModel10ObjSetImpl_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS                                 status = FLCN_OK;
    CLK_ADC_DEVICE_V10                         *pDev;
    RM_PMU_CLK_CLK_ADC_DEVICE_V10_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_ADC_DEVICE_V10_BOARDOBJ_SET *)pBoardObjDesc;

    // Construct and initialize parent-class object.
    status = clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkAdcDevGrpIfaceModel10ObjSetImpl_V10_exit;
    }

    pDev = (CLK_ADC_DEVICE_V10 *)*ppBoardObj;
    memcpy(&pDev->data, &pTmpDesc->data, 
           sizeof(LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V10));

clkAdcDevGrpIfaceModel10ObjSetImpl_V10_exit:
    return status;
}

/*!
 * Get status from ADC_DEVICE_V10 object.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkAdcDeviceIfaceModel10GetStatusEntry_V10
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    FLCN_STATUS     status = FLCN_OK;

    // Initialize parent-class object.
    status = clkAdcDeviceIfaceModel10GetStatusEntry_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkAdcDeviceIfaceModel10GetStatusEntry_V10_exit;
    }

clkAdcDeviceIfaceModel10GetStatusEntry_V10_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
