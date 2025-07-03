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
 * @file    pwrdev_ba16hw.h
 * @copydoc pwrdev_ba16hw.c
 */

#ifndef PWRDEV_BA16HW_H
#define PWRDEV_BA16HW_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA16HW PWR_DEVICE_BA16HW;

/*!
 * Structure representing a block activity v1.6hw power device.
 */
struct PWR_DEVICE_BA16HW
{
    /*!
     * Must always be the first element
     */
    PWR_DEVICE_BA1XHW super;
    /*!
     * Index into PowerSensorTable pointing to GPUADC device
     */
    LwU8              gpuAdcDevIdx;
    /*!
     * Boolean to control enablement of HW realtime FACTOR_A control
     */
    LwBool            bFactorALutEnableLW;

    /*!
     * Boolean to control enablement of HW realtime LEAKAGE_C control
     */
    LwBool            bLeakageCLutEnableLW;
    /*!
     * HW realtime FACTOR_A/LEAKAGE_C controls for MSVDD rail
     */
    LwBool            bFactorALutEnableMS;
    LwBool            bLeakageCLutEnableMS;
    /*!
     * Boolean to enable BA operation in FBVDD Mode.
     */
    LwBool            bFBVDDMode;
    /*!
     * Pointer to PWR_EQUATION object which provides the corresponding power
     * callwlations for scaling factor "A" and respective shift value used for
     * updating A, C and thresholds for MSVDD rail.
     */
    PWR_EQUATION     *pScalingEquMS;
    /*!
     * Pointer to PWR_EQUATION object which provides the corresponding leakage
     * power callwlations/estimations required for updating factor C for MSVDD
     * rail.
     */
    PWR_EQUATION     *pLeakageEquMS;
    /*!
     * Parameter used in computations of scaling A, offset C and thresholds.
     * It represents number of bits that the value needs to be left-shifted
     * before it's programmed into respective HW register. Introduced to
     * increase precision by using all 8-bits available in HW for factorA.
     *
     * @note shiftA for LWVDD rail is in @ref PWR_DEVICE_BA1XHW::shiftA
     * structure
     */
    LwU8              shiftAMS;
    /*!
     * Cached GPU MSVDD voltage
     */
    LwU32             voltageMSuV;
    /*!
     * Is BA source LWVDD enabled?
     */
     LwBool           bMonitorLW;
    /*!
     * Is BA source MSVDD enabled?
     */
     LwBool           bMonitorMS;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
FLCN_STATUS pwrDevPatchBaWeightsBug200734888_BA16HW(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevPatchBaWeightsBug200734888_BA16HW");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_BA16HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_BA16HW");
PwrDevLoad              (pwrDevLoad_BA16HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_BA16HW");
PwrDevSetLimit          (pwrDevSetLimit_BA16HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_BA16HW");
PwrDevTupleGet          (pwrDevTupleGet_BA16HW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_BA16HW");
PwrDevStateSync         (pwrDevStateSync_BA16HW)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevStateSync_BA16HW");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRDEV_BA16HW_H
