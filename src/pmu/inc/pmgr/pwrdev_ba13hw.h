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
 * @file    pwrdev_ba13hw.h
 * @copydoc pwrdev_ba13hw.c
 */

#ifndef PWRDEV_BA13HW_H
#define PWRDEV_BA13HW_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA13HW PWR_DEVICE_BA13HW;

/*!
 * Structure representing a block activity v1.35hw power device.
 */
struct PWR_DEVICE_BA13HW
{
    PWR_DEVICE_BA1XHW super;
};

/* ------------------------- Defines ---------------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define pwrDevIfaceModel10SetImpl_BA13HW pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW

/*!
 * @copydoc PwrDevTupleGet
 */
#define pwrDevTupleGet_BA13HW      pwrDevTupleGet_BA1XHW

/* ------------------------- Function Prototypes  --------------------------- */
FLCN_STATUS thermPwrDevBaLoad_BA13(PWR_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "thermPwrDevBaLoad_BA13");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrdev_ba14hw.h"

#endif // PWRDEV_BA13HW_H

