/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrviolation.c
 * @brief  Interface specification for a PWR_VIOLATION.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/thrmintr.h"
#include "pmgr/pwrviolation.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Prototypes--------------------- */
static PwrViolationLoad     (s_pwrViolationsLoad)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrViolationsLoad");
static PwrViolationEvaluate (s_pwrViolationEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrViolationEvaluate");
static LwUFXP4_12 s_pwrViolationTimerGet(PWR_VIOLATION *pViolation);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrViolationIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_VIOLATION_PROPGAIN);
            return pwrViolationGrpIfaceModel10ObjSetImpl_PROPGAIN(pModel10, ppBoardObj,
                sizeof(PWR_VIOLATION_PROPGAIN), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrViolationGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_VIOLATION_BOARDOBJ_SET  *pSet;
    PWR_VIOLATION                           *pViolation;
    FLCN_STATUS                              status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pViolation = (PWR_VIOLATION *)*ppBoardObj;
    pSet       = (RM_PMU_PMGR_PWR_VIOLATION_BOARDOBJ_SET *)pBoardObjDesc;

    pViolation->thrmIdx              = pSet->thrmIdx;
    pViolation->violation.violTarget = pSet->violTarget;

    //
    // It is important to divide base period [ns] with the time of OS tick
    // before we apply sampleMult to assure that resulting period in ticks
    // is proportional to the sampleMult without rounding error.
    //
    pViolation->ticksPeriod =
        (((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->baseSamplePeriodms *
         1000 / LWRTOS_TICK_PERIOD_US) *
        pSet->sampleMult;

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrViolationIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_VIOLATION_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_VIOLATION  *pViolation;
    FLCN_STATUS     status;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pViolation = (PWR_VIOLATION *)pBoardObj;
    pGetStatus = (RM_PMU_PMGR_PWR_VIOLATION_BOARDOBJ_GET_STATUS *)pPayload;

    // copyout VIOLATION class specific param
    pGetStatus->violLwrrent  = pViolation->violation.violLwrrent;

    return status;
}

/*!
 * @copydoc PwrViolationsLoad
 */
FLCN_STATUS
pwrViolationLoad_SUPER
(
    PWR_VIOLATION  *pViolation,
    LwU32           ticksNow
)
{
    pViolation->ticksNext = ticksNow + pViolation->ticksPeriod;

    //
    // s_pwrViolationTimerGet gets the current Ptimer and current therm event/
    // therm monitor timer and also returns the current violation rate.
    // During load, we discard the return value as it is invalid and the
    // timers are now initialized.
    //
    (void)s_pwrViolationTimerGet(pViolation);

    return FLCN_OK;
}

/*!
 * @copydoc PwrViolationEvaluate
 */
FLCN_STATUS
pwrViolationEvaluate_SUPER
(
    PWR_VIOLATION  *pViolation
)
{
    pViolation->violation.violLwrrent = s_pwrViolationTimerGet(pViolation);

    return FLCN_OK;
}

/*!
 * @brief   Public function to load all power violations.
 *
 * @param[in]   ticksNow    OS ticks timestamp to synchronize all load() code
 *
 * @return  TODO
 */
FLCN_STATUS
pwrViolationsLoad
(
    LwU32 ticksNow
)
{
    PWR_VIOLATION  *pViolation;
    LwBoardObjIdx  idx;
    FLCN_STATUS     status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_VIOLATION, pViolation, idx, NULL)
    {
        status = s_pwrViolationsLoad(pViolation, ticksNow);
        if (status != FLCN_OK)
        {
            goto pwrViolationsLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

pwrViolationsLoad_exit:
    return status;
}

/*!
 * Builds a bit-mask of expired PWR_VIOLATION objects while updating their new
 * expiration information.
 *
 * @param[in] ticksNow  OS ticks time at which we evaluate violation expiration.
 * @param[in] pM32      Pointer of mask to be updated with expiration info.
 *
 * @return  Set if at least one PWR_VIOLATION object has expired.
 *          Used by the caller to avoid checking if mask is zero.
 */
LwBool
pwrViolationsExpiredMaskGet
(
    LwU32                ticksNow,
    BOARDOBJGRPMASK_E32 *pM32
)
{
    PWR_VIOLATION  *pViolation;
    LwBoardObjIdx   idx;
    LwBool          bResult = LW_FALSE;

    boardObjGrpMaskInit_E32(pM32);

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_VIOLATION, pViolation, idx, NULL)
    {
        // Overflow (wrap-around) safe check of the power policy expiration.
        if (!(OS_TMR_IS_BEFORE(ticksNow, pViolation->ticksNext)))
        {
            // If policy expired more than once we'll process it only once.
            LwU32 expiredCount = 1 +
                (ticksNow - pViolation->ticksNext) / pViolation->ticksPeriod;
            pViolation->ticksNext += (expiredCount * pViolation->ticksPeriod);

            boardObjGrpMaskBitSet(pM32, idx);

            bResult = LW_TRUE;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return bResult;
}

/*!
 * TODO: Document (this is main entry point)
 */
FLCN_STATUS
pwrViolationsEvaluate
(
    BOARDOBJGRPMASK *pMask
)
{
    PWR_VIOLATION  *pViolation;
    LwBoardObjIdx   idx;
    FLCN_STATUS     status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_VIOLATION, pViolation, idx, pMask)
    {
        status = s_pwrViolationEvaluate(pViolation);
        if (status != FLCN_OK)
        {
            PMU_HALT();
            goto pwrViolationsEvaluate_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

pwrViolationsEvaluate_exit:
    return status;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrViolationIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_VIOLATION_PROPGAIN);
            return pwrViolationIfaceModel10GetStatus_PROPGAIN(pModel10, pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Top-level dispatcher for the PWR violation load() call.
 *
 * @copydoc PwrViolationsLoad
 */
static FLCN_STATUS
s_pwrViolationsLoad
(
    PWR_VIOLATION  *pViolation,
    LwU32           ticksNow
)
{
    switch (BOARDOBJ_GET_TYPE(pViolation))
    {
        case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_VIOLATION_PROPGAIN);
            return pwrViolationLoad_PROPGAIN(pViolation, ticksNow);
    }

    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Top-level dispatcher for the PWR violation evaluate() call.
 *
 * @copydoc PwrViolationEvaluate
 */
static FLCN_STATUS
s_pwrViolationEvaluate
(
    PWR_VIOLATION  *pViolation
)
{
    switch (BOARDOBJ_GET_TYPE(pViolation))
    {
        case LW2080_CTRL_PMGR_PWR_VIOLATION_TYPE_PROPGAIN:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_VIOLATION_PROPGAIN);
            return pwrViolationEvaluate_PROPGAIN(pViolation);
    }

    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Helper function to get the current timers and violation rate
 */
static LwUFXP4_12
s_pwrViolationTimerGet
(
    PWR_VIOLATION *pViolation
)
{
    if (pViolation->thrmIdx.thrmIdxType ==
        LW2080_CTRL_PMGR_PWR_VIOLATION_THERM_INDEX_TYPE_EVENT)
    {
        return libPmgrViolationLwrrGetEvent(&pViolation->violation,
                   pViolation->thrmIdx.index.thrmEvent);
    }
    else if (pViolation->thrmIdx.thrmIdxType ==
        LW2080_CTRL_PMGR_PWR_VIOLATION_THERM_INDEX_TYPE_MONITOR)
    {
        return libPmgrViolationLwrrGetMon(&pViolation->violation,
                   pViolation->thrmIdx.index.thrmMon);
    }
    else
    {
        // Invalid therm index type, returning 0 value.
        PMU_HALT();
        return 0;
    }
}
