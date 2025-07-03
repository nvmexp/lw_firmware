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
 * @file    perf_cf_sensor_clkcntr.c
 * @copydoc perf_cf_sensor_clkcntr.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_clkcntr.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_CLKCNTR *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_CLKCNTR *)pBoardObjDesc;
    PERF_CF_SENSOR_CLKCNTR *pSensorClkcntr;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS             status          = FLCN_OK;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR_exit;
    }
    pSensorClkcntr = (PERF_CF_SENSOR_CLKCNTR *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pSensorClkcntr->clkDomIdx != pDescSensor->clkDomIdx)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR_exit;
        }
    }

    // Set member variables.
    pSensorClkcntr->clkDomIdx = pDescSensor->clkDomIdx;

perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_CLKCNTR
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_CLKCNTR_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_CLKCNTR_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_CLKCNTR *pSensorClkcntr =
        (PERF_CF_SENSOR_CLKCNTR *)pBoardObj;
    LwU64       clkCntr;
    FLCN_STATUS status  = FLCN_OK;

    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_CLKCNTR_exit;
    }

    clkCntr = clkCntrRead(pSensorClkcntr->clkApiDomain, LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED);
    LwU64_ALIGN32_PACK(&(pStatus->super.reading), &clkCntr);

perfCfSensorIfaceModel10GetStatus_CLKCNTR_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_CLKCNTR
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_CLKCNTR *pSensorClkcntr =
        (PERF_CF_SENSOR_CLKCNTR *)pSensor;
    FLCN_STATUS status = FLCN_OK;
    CLK_DOMAIN *pDomain;

    status = perfCfSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_CLKCNTR_exit;
    }

    // Validate clock domain index. All boardobjgrps have been constructed at load.
    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pSensorClkcntr->clkDomIdx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfSensorLoad_CLKCNTR_exit;
    }

    // Get clock API domain from index.
    pSensorClkcntr->clkApiDomain = clkDomainApiDomainGet(pDomain);

perfCfSensorLoad_CLKCNTR_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
