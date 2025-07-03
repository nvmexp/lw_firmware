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
 * @file    perf_cf_sensor_pmu_fb.c
 * @copydoc perf_cf_sensor_pmu_fb.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_pmu_fb.h"
#include "pmu_objpmu.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void s_perfCfSensorPmuFbCntGet(PERF_CF_SENSOR_PMU_FB *pSensorPmuFb, LwU64 *pMultiCount, LwU64 *pDivCount)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfSensorPmuFbCntGet");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_PMU_FB *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_PMU_FB *)pBoardObjDesc;
    PERF_CF_SENSOR_PMU_FB  *pSensorPmuFb;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS             status          = FLCN_OK;
    CLK_DOMAIN             *pDomain;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_PMU(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit;
    }
    pSensorPmuFb = (PERF_CF_SENSOR_PMU_FB *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pSensorPmuFb->scale            != pDescSensor->scale)          ||
            (pSensorPmuFb->clkDomIdxMulti   != pDescSensor->clkDomIdxMulti) ||
            (pSensorPmuFb->clkDomIdxDiv     != pDescSensor->clkDomIdxDiv))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit;
        }
    }

    // Set member variables.
    pSensorPmuFb->scale             = pDescSensor->scale;
    pSensorPmuFb->clkDomIdxMulti    = pDescSensor->clkDomIdxMulti;
    pSensorPmuFb->clkDomIdxDiv      = pDescSensor->clkDomIdxDiv;

    if (pSensorPmuFb->scale == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit;
    }

    // Get clock API domains from indexes.
    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pSensorPmuFb->clkDomIdxMulti);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit;
    }
    pSensorPmuFb->clkApiDomainMulti = clkDomainApiDomainGet(pDomain);

    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pSensorPmuFb->clkDomIdxDiv);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit;
    }
    pSensorPmuFb->clkApiDomainDiv = clkDomainApiDomainGet(pDomain);    

perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PMU_FB
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_PMU_FB_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PMU_FB_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_PMU_FB *pSensorPmuFb = (PERF_CF_SENSOR_PMU_FB *)pBoardObj;
    FLCN_STATUS status      = FLCN_OK;
    LwU32       idleDiff32;
    LwU64       idleDiff;
    LwU64       multiCount;
    LwU64       multiDiff;
    LwU64       divCount;
    LwU64       divDiff;
    LwU64       scale       = pSensorPmuFb->scale;

    // Intentionally skipping its parent's (PMU) get status.
    status = perfCfSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PMU_FB_exit;
    }

    //
    // Read the 3 HW counters and scale the difference.
    //

    status = pmuIdleCounterDiff_HAL(&Pmu,
        pSensorPmuFb->super.counterIdx,
        &idleDiff32,
        &pSensorPmuFb->super.last);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PMU_FB_exit;
    }

    s_perfCfSensorPmuFbCntGet(pSensorPmuFb, &multiCount, &divCount);

    lw64Sub(&multiDiff, &multiCount, &pSensorPmuFb->lastMulti);
    pSensorPmuFb->lastMulti = multiCount;

    lw64Sub(&divDiff, &divCount, &pSensorPmuFb->lastDiv);
    pSensorPmuFb->lastDiv = divCount;

    // If the divisor clock counter has not incremented at all, set it to 1 to avoid divide by 0.
    if (divDiff == 0)
    {
        divDiff = 1;
    }

    //
    // The xyzDiffs should be 32 bits. The ratio between multiDiff and divDiff
    // should be less than ~10. Scale is FXP20.12. So, each step of this
    // sequence should fit in 64 bits without sacrificing resolution.
    //

    idleDiff = idleDiff32;
    lwU64Mul(&idleDiff, &idleDiff, &multiDiff);
    lwU64Div(&idleDiff, &idleDiff, &divDiff);
    lwU64Mul(&idleDiff, &idleDiff, &scale);
    lw64Lsr(&idleDiff, &idleDiff, 12);
    lw64Add(&pSensorPmuFb->super.reading, &pSensorPmuFb->super.reading, &idleDiff);

    pStatus->super.last = pSensorPmuFb->super.last;
    LwU64_ALIGN32_PACK(&pStatus->lastMulti,             &pSensorPmuFb->lastMulti);
    LwU64_ALIGN32_PACK(&pStatus->lastDiv,               &pSensorPmuFb->lastDiv);
    LwU64_ALIGN32_PACK(&pStatus->super.super.reading,   &pSensorPmuFb->super.reading);

perfCfSensorIfaceModel10GetStatus_PMU_FB_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PMU_FB
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PMU_FB *pSensorPmuFb = (PERF_CF_SENSOR_PMU_FB *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    // Apply FB HW settings first.
    status = pmuIdleCounterFbInit_HAL(&Pmu);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PMU_FB_exit;
    }

    // Apply PMU HW settings and get latest HW reading.
    status = perfCfSensorLoad_PMU(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PMU_FB_exit;
    }

    // Get latest clock counter readings.
    s_perfCfSensorPmuFbCntGet(pSensorPmuFb, &pSensorPmuFb->lastMulti, &pSensorPmuFb->lastDiv);

perfCfSensorLoad_PMU_FB_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

/*!
 * @brief   Get multi and div clock counter values.
 */
static void
s_perfCfSensorPmuFbCntGet
(
    PERF_CF_SENSOR_PMU_FB  *pSensorPmuFb,
    LwU64                  *pMultiCount,
    LwU64                  *pDivCount
)
{
    *pMultiCount = clkCntrRead(pSensorPmuFb->clkApiDomainMulti, LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED);
    *pDivCount   = clkCntrRead(pSensorPmuFb->clkApiDomainDiv,   LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED);
}
