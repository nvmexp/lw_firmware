/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba1xhw.h
 * @copydoc pwrdev_ba1xhw.c
 */

#ifndef PWRDEV_BA1XHW_H
#define PWRDEV_BA1XHW_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "pmgr/pwrdev.h"
#include "pmgr/pwrequation.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA1XHW PWR_DEVICE_BA1XHW;

/*!
 * All supported BA Devices before BA13 Devices, which means all
 * BA Devices before PASCAL.
 */
#define PMUCFG_FEATURE_ENABLED_PWR_DEVICE_PRE_BA13X \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA12X))

/*!
 * All supported BA Devices after BA12 Devices, which means all
 * BA Devices from MAXWELL_and_later.
 */
#define PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA12X_AND_LATER      \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA12X)) || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA13X)) || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA14))  || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA15))  || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA16))

/*!
 * All supported BA Devices after BA13 Devices, which means all
 * BA Devices from PASCAL_and_later.
 */
#define PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA13X_THRU_BA15      \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA13X)) || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA14))  || \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA15))

/*!
 * Number of supported limits.
 */
#define PWR_DEVICE_BA1XHW_LIMIT_COUNT               4

/*!
 * Structure representing a block activity v1.0hw power device.
 */
struct PWR_DEVICE_BA1XHW
{
    PWR_DEVICE      super;

    /*!
     * One of the BA10HW design flaws is presence of only single copy of A & C
     * values/parameters. Its RM's responsibility to assure that timing settings
     * for all three windows are identical, so that PMU can update this pair of
     * values only on single window. This field keeps track of primary BA10HW
     * device that has responsibility to update this scaling/offset parameters.
     */
    LwBool          bPrimaryBa10Hw;
    /*!
     * If set device monitors/estimates GPU current [mA], otherwise power [mW]
     */
    LwBool          bLwrrent;
    /*!
     * Index of associated HW BA averaging window
     */
    LwU8            windowIdx;
    /*!
     * Parameter used in computations of scaling A, offset C and thresholds.
     * It represents number of bits that the value needs to be left-shifted
     * before it's programmed into respective HW register. Introduced to
     * increase precision by using all 8-bits available in HW for factorA.
     */
    LwU8            shiftA;
    /*!
     * BA window configuration (precomputed by RM into HW register format)
     */
    LwU32           configuration;
#if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA13X_THRU_BA15 || (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA16)))
    /*!
     * Index to get the voltage from VOLTAGE_RAIL for FactorA/C update.
     */
    LwU8            voltRailIdx;
    /*!
     * EDPp - Content of LW_THERM_PEAKPOWER_CONFIG11(w) as precomputed by RM
     */
    LwU32           edpPConfigC0C1;
    /*!
     * EDPp - Content of LW_THERM_PEAKPOWER_CONFIG12(w) as precomputed by RM
     */
    LwU32           edpPConfigC2Dba;
#endif
    /*!
     * Window specific parameter as precomputed by RM
     */
    LwUFXP20_12     constW;
    /*!
     * Cached GPU core voltage [uV], updated by PERF code on voltage change
     */
    LwU32           voltageuV;
    /*!
     * Pointer to PWR_EQUATION object which provides the corresponding power
     * callwlations for scaling factor "A" and respective shift value used for
     * updating A, C and thresholds.
     */
    PWR_EQUATION   *pScalingEqu;
    /*!
     * Pointer to PWR_EQUATION object which provides the corresponding leakage
     * power callwlations/estimations required for updating factor C.
     */
    PWR_EQUATION   *pLeakageEqu;
};

/* ------------------------- Defines ---------------------------------------- */
/*!
 * Macro to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE class.
 */
#define pwrDevProvNumGet_BA1XHW(pDev)                                          \
            RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_BA1XHW(pDev)                                     \
            RM_PMU_PMGR_PWR_DEVICE_BA1XHW_THRESHOLD_NUM

/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW");
PwrDevLoad              (pwrDevLoad_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_BA1XHW");
PwrDevGetVoltage        (pwrDevGetVoltage_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetVoltage_BA1XHW");
PwrDevGetLwrrent        (pwrDevGetLwrrent_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetLwrrent_BA1XHW");
PwrDevGetPower          (pwrDevGetPower_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetPower_BA1XHW");
PwrDevTupleGet          (pwrDevTupleGet_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_BA1XHW");
PwrDevVoltageSetChanged (pwrDevVoltageSetChanged_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevVoltageSetChanged_BA1XHW");
PwrDevSetLimit          (pwrDevSetLimit_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_BA1XHW");
PwrDevStateSync         (pwrDevStateSync_BA1XHW)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevStateSync_BA1XHW");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrdev_ba13hw.h"
#include "pmgr/pwrdev_ba15hw.h"
#include "pmgr/pwrdev_ba16hw.h"

#endif // PWRDEV_BA1XHW_H
