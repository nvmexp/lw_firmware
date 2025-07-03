/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies_2x.h
 * @brief Structure specification for the Power Policy functionality container -
 * all as specified by the Power Policy Table 2.X.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Policy_Table_2.X
 *
 * All of the actual functionality is found in @ref pwrpolicy.c.
 */

#ifndef PWRPOLICIES_2X_H
#define PWRPOLICIES_2X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicies.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICIES PWR_POLICIES_2X;

/* ------------------------- Macros ----------------------------------------- */
#define pwrPoliciesProcessPerfStatus_2X(pStatus)

/* ------------------------- Macros ---------------------------------------- */
#define pwrPoliciesConstructHeader_2X(ppPolicies, pMon, pHdr)                 \
    pwrPoliciesConstructHeader(ppPolicies, pMon, pHdr)
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
PwrPoliciesEvaluate (pwrPoliciesEvaluate_2X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrpoliciesEvaluate_2X");

#endif // PWRPOLICIES_2X_H

