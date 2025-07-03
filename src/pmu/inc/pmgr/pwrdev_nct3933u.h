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
 * @file    pwrdev_nct3933u.h
 * @copydoc pwrdev_nct3933u.c
 */

#ifndef PWRDEV_NCT3933U_H
#define PWRDEV_NCT3933U_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev.h"
#include "pmgr/i2cdev.h"
#include "i2c/dev_nct3933u.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_NCT3933U PWR_DEVICE_NCT3933U;

struct PWR_DEVICE_NCT3933U
{
    /*!
     * Super PWR_DEVICE structure
     */
    PWR_DEVICE super;
    /*!
     * Default limit of the sensor (mA)
     */
    LwU32      defaultLimitmA;
    /*!
     * Value of register CR04h in NCT3933U
     */
    LwU8       reg04val;
    /*!
     * Value of register CR05h in NCT3933U
     */
    LwU8       reg05val;
    /*!
     * Step size in mA of one single step of circuit
     */
    LwU16      stepSizemA;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Macro to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE class.
 */
#define pwrDevProvNumGet_NCT3933U(pDev)                                       \
            RM_PMU_PMGR_PWR_DEVICE_NCT3933U_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_NCT3933U(pDev)                                  \
            RM_PMU_PMGR_PWR_DEVICE_NCT3933U_THRESHOLD_NUM

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U");
PwrDevLoad              (pwrDevLoad_NCT3933U)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_NCT3933U");
PwrDevSetLimit          (pwrDevSetLimit_NCT3933U)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_NCT3933U");
PwrDevGetLimit          (pwrDevGetLimit_NCT3933U)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevGetLimit_NCT3933U");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_NCT3933U_H
