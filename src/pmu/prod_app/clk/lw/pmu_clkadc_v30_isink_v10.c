/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc_v30_isink_v10.c
 * @brief  File for ISINK V10 ADC V30 routines.
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
 * @brief Construct a ADC_DEVICE_V30_ISINK_V10 object.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    CLK_ADC_DEVICE_V30_ISINK_V10                         *pDev     = NULL;
    FLCN_STATUS                                           status   = FLCN_OK;
    RM_PMU_CLK_CLK_ADC_DEVICE_V30_ISINK_V10_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_ADC_DEVICE_V30_ISINK_V10_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool bFirstConstruct = (*ppBoardObj == NULL);

    // Construct and initialize parent-class object.
    status = clkAdcDevGrpIfaceModel10ObjSetImpl_V30(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10_exit;
    }
    pDev = (CLK_ADC_DEVICE_V30_ISINK_V10 *)*ppBoardObj;

    pDev->infoData = pTmpDesc->data;

    //
    // No state change required across soft reboot, only required at
    // first construct.
    //
    if (bFirstConstruct)
    {
        //
        // Note: We are assuming that the default state of the ADC is powered on
        // and enabled when the ADC device is constructed. For ISINK ADCs,
        // VBIOS devinit does both these operations.
        //
        // Also initialize the state of the client masks disable and hwAccess
        // both to zero as ISINK ADCs are already enabled by VBIOS devinit.
        //
        pDev->super.super.bPoweredOn             = LW_TRUE;
        pDev->super.super.disableClientsMask     = 0U;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
        pDev->super.super.hwRegAccessClientsMask = 0U;
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC
    }

clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10_exit:
    return status;
}
