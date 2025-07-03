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
 * @file    pwrdev_ina3221.h
 * @copydoc pwrdev_ina3221.c
 */

#ifndef PWRDEV_INA3221_H
#define PWRDEV_INA3221_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev.h"
#include "pmgr/i2cdev.h"
#include "i2c/dev_ina3221.h"
#include "pmu_objtimer.h"

/* ------------------------- Types Definitions ----------------------------- */
#define INA3221_SUMMATION_CONTROL_BITMASK                                   \
        (BIT(DRF_BASE(LWRM_INA3221_MASKENABLE_SUMMATION_CONTROL(0))) |      \
         BIT(DRF_BASE(LWRM_INA3221_MASKENABLE_SUMMATION_CONTROL(1))) |      \
         BIT(DRF_BASE(LWRM_INA3221_MASKENABLE_SUMMATION_CONTROL(2))))

typedef struct PWR_DEVICE_INA3221 PWR_DEVICE_INA3221;

/*!
 * Structure to store data related to one limit index.
 */
typedef struct LIMIT_DATA_INA3221
{
    /*!
     * Limit Index. Indicating which limit this structure is representing.
     * ccs-TODO: limitIdx might be redundant since we already named two
     * LIMIT_DATA_INA3221 struct in PWR_DEVICE_INA3221 critical and warning.
     */
    LwU8            limitIdx;
    /*!
     * Event Mask. Indicating which RM_PMU_THERM_EVENT_MASK_<xyz> this
     * limit can trigger.
     */
    LwU32           eventMask;
} LIMIT_DATA_INA3221;

struct PWR_DEVICE_INA3221
{
    /*!
     * Super PWR_DEVICE structure
     */
    PWR_DEVICE  super;
    /*!
     * Shunt resistor resistance (mOhm) for each INA3221 physical channel.
     */
    RM_PMU_PMGR_PWR_DEVICE_DESC_RSHUNT
                rShuntmOhm[RM_PMU_PMGR_PWR_DEVICE_INA3221_CH_NUM];
    /*!
     * Configuration register 0x00
     */
    LwU16       configuration;
    /*!
     * Mask / Enable register 0x0F
     */
    LwU16       maskEnable;
    /*!
     * Current value linear correction factor M. There is a bias offset on
     * INA3221's current measurement. We are using y=Mx+B to adjust
     * measured data.
     */
    LwUFXP4_12  lwrrCorrectM;
    /*!
     * Current value linear correction factor B. units of Amps.
     */
    LwSFXP4_12  lwrrCorrectB;
    /*!
     * Cached tamper check channel index
     */
    LwU16       tamperIdx;
    /*!
     * Is tampering detected?
     */
    LwBool      bTamperingDetected;
    /*!
     * Provider limit value for INA3221. Note for INA3221, one limit contains
     * only one HW Threshold(i.e. critical or warning).
     */
    LIMIT_DATA_INA3221 limit[LWRM_INA3221_LIMIT_IDX_NUM];

    // Derived parameters

    /*!
     * Number of channels enabled to get reading of shunt voltage register
     */
    LwU8        channelsNum;
    /*!
     * First enabled channel to get valid reading of shunt voltage register
     */
    LwU8        highestProvIdx;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Macro for uninitialized tamper detect channel index. This is the
 * uninitialized value of tamperIdx and we will replace it with the first
 * channel index it sees.
 */
#define PWR_DEVICE_INA3221_TAMPER_IDX_UNINITIALIZED 0xFFFF

/*!
 * Macro to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE class.
 */
#define pwrDevProvNumGet_INA3221(pDev)                                        \
            RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_INA3221(pDev)                                   \
            RM_PMU_PMGR_PWR_DEVICE_INA3221_THRESHOLD_NUM

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_INA3221)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_INA3221");
PwrDevLoad              (pwrDevLoad_INA3221);
PwrDevGetVoltage        (pwrDevGetVoltage_INA3221);
PwrDevGetLwrrent        (pwrDevGetLwrrent_INA3221)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetLwrrent_INA3221");
PwrDevGetPower          (pwrDevGetPower_INA3221)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetPower_INA3221");
PwrDevSetLimit          (pwrDevSetLimit_INA3221)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_INA3221");
PwrDevTupleGet          (pwrDevTupleGet_INA3221)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_INA3221");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_INA3221_H
