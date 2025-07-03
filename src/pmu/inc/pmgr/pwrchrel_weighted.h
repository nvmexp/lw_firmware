/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_weighted.h
 * @brief @copydoc pwrchrel_weighted.c
 */

#ifndef PWRCHREL_WEIGHTED_H
#define PWRCHREL_WEIGHTED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
PwrChRelationshipTupleEvaluate  (pwrChRelationshipTupleEvaluate_WEIGHTED)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipTupleEvaluate_WEIGHTED");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_WEIGHTED_H
