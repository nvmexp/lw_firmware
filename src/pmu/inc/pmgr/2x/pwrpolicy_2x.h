/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_2x.h
 * @brief @copydoc pwrpolicy_2x.c
 */

#ifndef PWRPOLICY_2X_H
#define PWRPOLICY_2X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Macros ---------------------------------------- */
#define pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_2X(pModel10, ppBoardObj, size, pDesc)     \
    pwrPolicyGrpIfaceModel10ObjSetImpl(pModel10, ppBoardObj, size, pDesc)

/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Constructor for the PWR_POLICY super-class.
 */
BoardObjGrpIfaceModel10ObjSet (pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_2X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_2X");

PwrPolicyFilter   (pwrPolicyFilter_2X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_2X");
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRPOLICY_2X_H
