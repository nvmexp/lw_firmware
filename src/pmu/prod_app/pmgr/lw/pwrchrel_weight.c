/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_weight.c
 * @brief Interface specification for a PWR_CHRELATIONSHIP_WEIGHT.
 *
 * Weighted Power Channel Relationships specify a weight/proporition of another
 * channel's power to use to callwlate the power of a given channel.  These
 * relationships specify a fixed pointe (UFXP 20.12) weight/proportion which
 * will scale the other channel's power.
 *
 *     relationshipPowermW = (weightFxp * channelPowermW + 0x800) >> 12
 *
 * @note The use of the UFXP 20.12 FXP weight will overflow at 1048576 mW.  This
 * will be a safe assumption for the foreseeable future.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel_weight.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WEIGHT PWR_CHRELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_WEIGHT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_WEIGHT_BOARDOBJ_SET *pDescWeight =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_WEIGHT_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHRELATIONSHIP_WEIGHT             *pWeight;
    FLCN_STATUS                            status;

    status = pwrChRelationshipGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pWeight = (PWR_CHRELATIONSHIP_WEIGHT *)*ppBoardObj;

    // Copy in the _WEIGHT type-specific data
    pWeight->weight = pDescWeight->weight;

    return status;
}

/*!
 * _WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrChRelationshipWeightGet
 */
LwSFXP20_12
pwrChRelationshipWeightGet_WEIGHT
(
    PWR_CHRELATIONSHIP *pChRel,
    LwU8                chIdxEval
)
{
    PWR_CHRELATIONSHIP_WEIGHT *pWeight = (PWR_CHRELATIONSHIP_WEIGHT *)pChRel;

    return pWeight->weight;
}

/* ------------------------- Private Functions ------------------------------ */
