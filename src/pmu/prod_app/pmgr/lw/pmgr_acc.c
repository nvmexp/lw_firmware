/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pmgr_acc.c
 * @brief Library of functions used to manuipulate aclwmulators used across
 *        PMGR.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objpmgr.h"
#include "pmgr/pmgr_acc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc pmgrAclwpdate
 */
void pmgrAclwpdate
(
    PMGR_ACC         *pAcc,
    LWU64_TYPE       *pSrcLwrr
)
{
    LwU64 result;

    // Diff between current aclwmulator and last aclwmulator value.
    lw64Sub(&result, &pSrcLwrr->val, &pAcc->srcLast);

    // Update the last source aclwmulator value
    pAcc->srcLast = pSrcLwrr->val;

    //
    // Scale the aclwmulator diff, colwerting from source units to destination
    // units. Colwersion is given by the scaleFactor. Scaling result back down
    // to no fractional bits.
    //
    result = multUFXP64(pAcc->shiftBits, result, pAcc->scaleFactor);

    // Add the colwerted aclwmulator diff to dstLast
    lw64Add(&pAcc->dstLast, &pAcc->dstLast, &result);
}

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/* ------------------------- Static Functions ------------------------------- */
