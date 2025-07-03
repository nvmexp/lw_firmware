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
 * @file  pwrchrel_weight.h
 * @brief @copydoc pwrchrel_weight.c
 */

#ifndef PWRCHREL_WEIGHT_H
#define PWRCHREL_WEIGHT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP_WEIGHT PWR_CHRELATIONSHIP_WEIGHT;

struct PWR_CHRELATIONSHIP_WEIGHT
{
    /*!
     * @copydoc PWR_CHRELATIONSHIP
     */
    PWR_CHRELATIONSHIP super;

    /*!
     * Weight to apply to the value read from the power channel.
     */
    LwSFXP20_12        weight;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChRelationshipGrpIfaceModel10ObjSet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_WEIGHT");
PwrChRelationshipWeightGet  (pwrChRelationshipWeightGet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipWeightGet_WEIGHT");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_WEIGHT_H
