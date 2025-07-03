/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba15hw.h
 * @copydoc pwrdev_ba15hw.c
 */

#ifndef PWRDEV_BA15HW_H
#define PWRDEV_BA15HW_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA15HW PWR_DEVICE_BA15HW;

/*!
 * Structure representing a block activity v1.5hw power device.
 */
struct PWR_DEVICE_BA15HW
{
    PWR_DEVICE_BA1XHW super;

    /*!
     * Index into PowerSensorTable pointing to GPUADC device
     */
    LwU8 gpuAdcDevIdx;

    /*!
     * Boolean to control enablement of HW realtime FACTOR_A control
     */
    LwBool bFactorALutEnable;

    /*!
     * Boolean to control enablement of HW realtime LEAKAGE_C control
     */
    LwBool bLeakageCLutEnable;
};

/* ------------------------- Defines ---------------------------------------- */
/*!
 * @copydoc PwrDevTupleGet
 */
#define pwrDevTupleGet_BA15HW      pwrDevTupleGet_BA1XHW

/* ------------------------- Function Prototypes  --------------------------- */
FLCN_STATUS thermPwrDevBaLoad_BA15(PWR_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "thermPwrDevBaLoad_BA15");
void pwrDevBaDmaMode_BA15HW(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevBaDmaMode_BA15HW");
LwBool thermPwrDevBaTrainingIsDmaSet_BA15(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "thermPwrDevBaTrainingIsDmaSet_BA15");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_BA15HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_BA15HW");
PwrDevLoad              (pwrDevLoad_BA15HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_BA15HW");
PwrDevSetLimit          (pwrDevSetLimit_BA15HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_BA15HW");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRDEV_BA15HW_H
