/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrequation_leakage_dtcs13.h
 * @brief @copydoc pwrequation_leakage_dtcs13.c
 */

#ifndef PWREQUATION_LEAKAGE_DTCS13_H
#define PWREQUATION_LEAKAGE_DTCS13_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation_leakage.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Forward Declaration ---------------------------- */
typedef struct PWR_EQUATION_LEAKAGE_DTCS13 PWR_EQUATION_LEAKAGE_DTCS13;

/* ------------------------- Function Prototypes  --------------------------- */
#define PwrEquationLeakageDtcs13Load(fname) FLCN_STATUS (fname)(PWR_EQUATION_LEAKAGE_DTCS13 *pDtcs13)

/* ------------------------- Types Definitions ------------------------------ */
/*!
 * Structure representing a DTCS 1.3 leakage equation entry.
 */
struct PWR_EQUATION_LEAKAGE_DTCS13
{
    /*!
     * PWR_EQUATION_LEAKAGE_DTCS12 super class.
     */
    PWR_EQUATION_LEAKAGE_DTCS12     super;
    /*!
     * Leakage proportion in case GPC-RG is fully resident.
     * Value will be between 0 and 1
     */
    LwUFXP4_12 gpcrgEff;
    /*!
     * Leakage proportion in case GR-RPG is fully resident.
     * Value will be between 0 and 1
     */
    LwUFXP4_12 grrpgEff;
};

/* ------------------------- Macros ------------------------------------------*/
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13");

/*!
 * PWR_EQUATION_LEAKAGE interfaces
 */
PwrEquationLeakageEvaluatemA (pwrEquationLeakageEvaluatemA_DTCS13)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemA_DTCS13");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_LEAKAGE_DTCS13_H
