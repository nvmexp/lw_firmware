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
 * @file   pmu_clkadc_v30.c
 * @brief  File for ADC device version 30 routines.
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
 * @brief Construct a ADC_DEVICE_V30 object.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkAdcDevGrpIfaceModel10ObjSetImpl_V30
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    CLK_ADC_DEVICE_V30                         *pDev     = NULL;
    FLCN_STATUS                                 status   = FLCN_OK;
    RM_PMU_CLK_CLK_ADC_DEVICE_V30_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_ADC_DEVICE_V30_BOARDOBJ_SET *)pBoardObjDesc;

    // Construct and initialize parent-class object.
    status = clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkAdcDevGrpIfaceModel10ObjSetImpl_V30_exit;
    }

    pDev = (CLK_ADC_DEVICE_V30 *)*ppBoardObj;
    pDev->infoData = pTmpDesc->data;

clkAdcDevGrpIfaceModel10ObjSetImpl_V30_exit:
    return status;
}

/*!
 * @brief Get status from ADC_DEVICE_V30 object.
 *
 * @copydetails BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
clkAdcDeviceIfaceModel10GetStatusEntry_V30
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS         status = FLCN_OK;
    CLK_ADC_DEVICE_V30 *pDev = (CLK_ADC_DEVICE_V30 *)pBoardObj;

    RM_PMU_CLK_CLK_ADC_DEVICE_V30_BOARDOBJ_GET_STATUS *pAdcGetStatus;

    // Initialize parent-class object.
    status = clkAdcDeviceIfaceModel10GetStatusEntry_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkAdcDeviceIfaceModel10GetStatusEntry_V30_exit;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
    status = clkAdcDeviceRamAssistIsEngaged_HAL(&Clk, &(pDev->super),
                                            &(pDev->statusData.bRamAssistEngaged));
    if (status != FLCN_OK)
    {
        goto clkAdcDeviceIfaceModel10GetStatusEntry_V30_exit;
    }
#endif // PMU_CLK_ADC_RAM_ASSIST

    pAdcGetStatus = (RM_PMU_CLK_CLK_ADC_DEVICE_V30_BOARDOBJ_GET_STATUS *)pPayload;
    pAdcGetStatus->statusData = pDev->statusData;

clkAdcDeviceIfaceModel10GetStatusEntry_V30_exit:
    return status;
}
