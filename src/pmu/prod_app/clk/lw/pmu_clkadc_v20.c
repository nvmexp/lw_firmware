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
 * @file   pmu_clkadc_v20.c
 * @brief  File for ADC device version 2.0 routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static BoardObjVirtualTableDynamicCast (s_clkAdcDeviceDynamicCast_V20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcDeviceDynamicCast_V20")
    GCC_ATTRIB_USED();

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Virtual table for CLK_ADC_DEVICE_V20 object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE ClkAdcDeviceV20VirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_clkAdcDeviceDynamicCast_V20)
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct a ADC_DEVICE_V20 object.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkAdcDevGrpIfaceModel10ObjSetImpl_V20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS                                 status = FLCN_OK;
    CLK_ADC_DEVICE_V20                         *pDev;
    RM_PMU_CLK_CLK_ADC_DEVICE_V20_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_ADC_DEVICE_V20_BOARDOBJ_SET *)pBoardObjDesc;

    // Construct and initialize parent-class object.
    status = clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkAdcDevGrpIfaceModel10ObjSetImpl_V20_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDev, *ppBoardObj, CLK, ADC_DEVICE, V20,
                              &status, clkAdcDevGrpIfaceModel10ObjSetImpl_V20_exit);
    pDev->data = pTmpDesc->data;

clkAdcDevGrpIfaceModel10ObjSetImpl_V20_exit:
    return status;
}

/*!
 * @brief Get status from ADC_DEVICE_V20 object.
 *
 * @copydetails BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
clkAdcDeviceIfaceModel10GetStatusEntry_V20
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS                                 status = FLCN_OK;
    CLK_ADC_DEVICE_V20                         *pDev;
    RM_PMU_CLK_CLK_ADC_DEVICE_V20_BOARDOBJ_GET_STATUS *pAdcGetStatus;

    // Initialize parent-class object.
    status = clkAdcDeviceIfaceModel10GetStatusEntry_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkAdcDeviceIfaceModel10GetStatusEntry_V20_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDev, pBoardObj, CLK, ADC_DEVICE, V20,
                                  &status, clkAdcDeviceIfaceModel10GetStatusEntry_V20_exit);

    pAdcGetStatus = (RM_PMU_CLK_CLK_ADC_DEVICE_V20_BOARDOBJ_GET_STATUS *)pPayload;
    pAdcGetStatus->statusData = pDev->statusData;

clkAdcDeviceIfaceModel10GetStatusEntry_V20_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   CLK_ADC_DEVICE_V20 implementation of @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_clkAdcDeviceDynamicCast_V20
(
    BOARDOBJ *pBoardObj,
    LwU8      requestedType
)
{
    CLK_ADC_DEVICE_V20 *pClkAdcDeviceV20 = (CLK_ADC_DEVICE_V20 *)pBoardObj;
    void               *pObject          = NULL;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, V20))
    {
        PMU_BREAKPOINT();
        goto s_clkAdcDeviceDynamicCast_v20_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, BASE):
        {
            CLK_ADC_DEVICE *pClkAdcDevice = &(pClkAdcDeviceV20->super);
            pObject = (void *)pClkAdcDevice;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, V20):
        {
            pObject = (void *)pClkAdcDeviceV20;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_clkAdcDeviceDynamicCast_v20_exit:
    return pObject;
}
