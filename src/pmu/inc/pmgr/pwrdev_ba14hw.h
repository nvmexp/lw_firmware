/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba14hw.h
 * @copydoc pwrdev_ba14hw.c
 */

#ifndef PWRDEV_BA14HW_H
#define PWRDEV_BA14HW_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba13hw.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA14HW PWR_DEVICE_BA14HW;

/*!
 * Structure representing a block activity v1.4hw power device.
 */
struct PWR_DEVICE_BA14HW
{
    PWR_DEVICE_BA13HW super;
    /*!
     * Value of energy [mJ] aclwmmulated on this BA device. Needs to be cached
     * in case of multiple channels pointing to same BA device.
     */
    LwU64             energymJ;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_BA14HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_BA14HW");
PwrDevTupleGet          (pwrDevTupleGet_BA14HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_BA14HW");

/* ------------------------- Debug Macros ----------------------------------- */

#endif // PWRDEV_BA14HW_H
