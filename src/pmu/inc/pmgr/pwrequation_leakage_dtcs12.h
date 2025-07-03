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
 * @file  pwrequation_leakage_dtcs12.h
 * @brief @copydoc pwrequation_leakage_dtcs12.c
 */

#ifndef PWREQUATION_LEAKAGE_DTCS12_H
#define PWREQUATION_LEAKAGE_DTCS12_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation_leakage.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Forward Declaration ---------------------------- */
typedef struct PWR_EQUATION_LEAKAGE_DTCS12 PWR_EQUATION_LEAKAGE_DTCS12;

/* ------------------------- Function Prototypes  --------------------------- */
#define PwrEquationLeakageDtcs12Load(fname) FLCN_STATUS (fname)(PWR_EQUATION_LEAKAGE_DTCS12 *pDtcs12)

/* ------------------------- Types Definitions ------------------------------ */
/*!
 * Structure representing a DTCS 1.2 leakage equation entry.
 */
struct PWR_EQUATION_LEAKAGE_DTCS12
{
    /*!
     * PWR_EQUATION_LEAKAGE_DTCS11 super class.
     */
    PWR_EQUATION_LEAKAGE_DTCS11     super;
    /*!
     * VFE index representing IDDQ.
     */
    LwU8                            iddqVfeIdx;
    /*!
     * Therm Channel Index for Tj.
     */
    LwU8                            tjThermChIdx;
    /*!
     * The callwlated IDDQ value from VFE.
     */
    LwU32                           iddqmA;
};

/* ------------------------- Macros ------------------------------------------*/
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12");

/*!
 * PWR_EQUATION_LEAKAGE interfaces
 */
PwrEquationLeakageEvaluatemA (pwrEquationLeakageEvaluatemA_DTCS12)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemA_DTCS12");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_LEAKAGE_DTCS12_H
