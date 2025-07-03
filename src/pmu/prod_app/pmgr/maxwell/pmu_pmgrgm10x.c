/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmgrgm10x.c
 * @brief  Contains all PCB Management (PGMR) routines specific to GM10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "dev_therm.h"
#include "dev_pmgr.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "dbgprintf.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "lib/lib_pwm.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#ifndef LW_PGC6_SCI_FAN_CFG0__SIZE_1
#define LW_PGC6_SCI_FAN_CFG0_0 LW_PGC6_SCI_FAN_CFG0
#endif

#ifndef LW_PGC6_SCI_FAN_CFG1__SIZE_1
#define LW_PGC6_SCI_FAN_CFG1_0 LW_PGC6_SCI_FAN_CFG1
#endif

#ifndef LW_PGC6_SCI_FAN_TEMP_ALERT__SIZE_1
#define LW_PGC6_SCI_FAN_TEMP_ALERT_0 LW_PGC6_SCI_FAN_TEMP_ALERT
#endif

#ifndef LW_PGC6_SCI_FAN_ACT0__SIZE_1
#define LW_PGC6_SCI_FAN_ACT0_0 LW_PGC6_SCI_FAN_ACT0
#endif

#ifndef LW_PGC6_SCI_FAN_ACT1__SIZE_1
#define LW_PGC6_SCI_FAN_ACT1_0 LW_PGC6_SCI_FAN_ACT1
#endif

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */
/*!
 * @brief   Constructs the PWM source descriptor data for requested PWM source.
 *
 * @note    This function assumes that the overlay described by @ref ovlIdxDmem
 *          is already attached by the caller.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] ovlIdxDmem  DMEM Overlay index in which to construct the PWM
 *                        source descriptor
 *
 * @return  PWM_SOURCE_DESCRIPTOR   Pointer to PWM_SOURCE_DESCRIPTOR structure.
 * @return  NULL                    In case of invalid PWM source.
 */
PWM_SOURCE_DESCRIPTOR *
pmgrPwmSourceDescriptorConstruct_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE pwmSource,
    LwU8                   ovlIdxDmem
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = NULL;
    PWM_SOURCE_DESCRIPTOR  pwmSrcDesc;

    switch (pwmSource)
    {
        case RM_PMU_PMGR_PWM_SOURCE_PMGR_FAN:
        {
            ct_assert(DRF_MASK(LW_PMGR_FAN0_PERIOD) == DRF_MASK(LW_PMGR_FAN1_HI));
            ct_assert(DRF_BASE(LW_PMGR_FAN0_PERIOD) == 0);
            ct_assert(LW_PMGR_FAN1_SETTING_NEW_DONE == 0);
            ct_assert(LW_PMGR_FAN1_NEW_REQUEST_TRIGGER == 1);

            pwmSrcDesc.addrPeriod    = LW_PMGR_FAN0;
            pwmSrcDesc.addrDutycycle = LW_PMGR_FAN1;
            pwmSrcDesc.mask          = DRF_MASK(LW_PMGR_FAN0_PERIOD);
            pwmSrcDesc.triggerIdx    = DRF_BASE(LW_PMGR_FAN1_NEW_REQUEST);
            pwmSrcDesc.doneIdx       = DRF_BASE(LW_PMGR_FAN1_SETTING_NEW);
            pwmSrcDesc.bus           = REG_BUS_FECS;
            pwmSrcDesc.bCancel       = LW_FALSE;
            pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_PMGR_PWM:
        {
            ct_assert(DRF_MASK(LW_PMGR_PWM0_PERIOD) == DRF_MASK(LW_PMGR_PWM1_HI));
            ct_assert(DRF_BASE(LW_PMGR_PWM0_PERIOD) == 0);
            ct_assert(LW_PMGR_PWM1_SETTING_NEW_DONE == 0);
            ct_assert(LW_PMGR_PWM1_NEW_REQUEST_TRIGGER == 1);

            pwmSrcDesc.addrPeriod    = LW_PMGR_PWM0;
            pwmSrcDesc.addrDutycycle = LW_PMGR_PWM1;
            pwmSrcDesc.mask          = DRF_MASK(LW_PMGR_PWM0_PERIOD);
            pwmSrcDesc.triggerIdx    = DRF_BASE(LW_PMGR_PWM1_NEW_REQUEST);
            pwmSrcDesc.doneIdx       = DRF_BASE(LW_PMGR_PWM1_SETTING_NEW);
            pwmSrcDesc.bus           = REG_BUS_FECS;
            pwmSrcDesc.bCancel       = LW_FALSE;
            pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0:
        {
            ct_assert(DRF_MASK(LW_PGC6_SCI_FAN_CFG0_PERIOD) ==
                        DRF_MASK(LW_PGC6_SCI_FAN_CFG1_HI));
            ct_assert(DRF_BASE(LW_PGC6_SCI_FAN_CFG0_PERIOD) == 0);
            ct_assert(LW_PGC6_SCI_FAN_CFG1_UPDATE_DONE == 0);
            ct_assert(LW_PGC6_SCI_FAN_CFG1_UPDATE_TRIGGER == 1);

            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_FAN_CFG0_0;
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_FAN_CFG1_0;
            pwmSrcDesc.mask          = DRF_MASK(LW_PGC6_SCI_FAN_CFG0_PERIOD);
            pwmSrcDesc.triggerIdx    = DRF_BASE(LW_PGC6_SCI_FAN_CFG1_UPDATE);
            pwmSrcDesc.doneIdx       = DRF_BASE(LW_PGC6_SCI_FAN_CFG1_UPDATE);
            pwmSrcDesc.bus           = REG_BUS_FECS;
            pwmSrcDesc.bCancel       = LW_TRUE;
            pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        default:
        {
            goto pmgrPwmSourceDescriptorConstruct_GM10X_exit;
        }
    }
    pwmSrcDesc.type = PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT;

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &pwmSrcDesc);

pmgrPwmSourceDescriptorConstruct_GM10X_exit:
    return pPwmSrcDesc;
}

/*!
 * Used to retrieve an actual period and duty cycle of the PWM signal that is
 * being detected by given PWM source.
 *
 * @param[in]  pwmSource     Enum of the PWM source driving the PWM signal
 * @param[out] pPwmDutyCycle Buffer in which to return the duty cycle in raw
 *                           units
 * @param[out] pPwmPeriod    Buffer in which to return the period in raw units
 *
 * @return FLCN_ERR_NOT_SUPPORTED    if invalid pwmSource specified (not
 *                                  supporting PWM detection).
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
pmgrSciPwmParamsActGet_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        LwU32   act0A;
        LwU32   act0B;
        LwU32   act1;

        // Ensure that PWM settings are retrieved atomically.
        act0B = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT0_0);
        do
        {
            act0A = act0B;
            act1  = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT1_0);
            act0B = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT0_0);
        } while (act0A != act0B);

        *pPwmPeriod    = DRF_VAL(_PGC6, _SCI_FAN_ACT0, _PERIOD, act0B);
        *pPwmDutyCycle = DRF_VAL(_PGC6, _SCI_FAN_ACT1, _HI,     act1);

        // Clear an overflow state if detected.
        if (FLD_TEST_DRF(_PGC6, _SCI_FAN_ACT0, _OVERFLOW, _DETECTED, act0B))
        {
            REG_WR32(FECS, LW_PGC6_SCI_FAN_ACT0_0,
                     FLD_SET_DRF(_PGC6, _SCI_FAN_ACT0, _OVERFLOW, _CLEAR,
                                 act0B));
        }

        status = FLCN_OK;
    }

    return status;
}

/*!
 * Used to initialize FAN_TEMP_ALERT register which is used to override
 * fan speed in case of MAXFAN signal assertion.
 *
 * @param[in]  pwmSource        Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>.
 * @param[in]  maxFanDutyCycle  Value of duty cycle to run the fan at max speed.
 */
void
pmgrHwMaxFanInit_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                   maxFanDutyCycle
)
{
    LwU32 tempAlertVal;
    LwU32 configVal;

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        tempAlertVal = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT_0);
        configVal    = REG_RD32(FECS, LW_PGC6_SCI_FAN_CFG1_0);

        // Cancel any pending updates.
        if (FLD_TEST_DRF(_PGC6, _SCI_FAN_CFG1, _UPDATE,
                         _PENDING, configVal))
        {
            REG_WR32(FECS, LW_PGC6_SCI_FAN_CFG1_0,
                FLD_SET_DRF(_PGC6, _SCI_FAN_CFG1, _UPDATE, _DONE, configVal));
        }

        // Setup the config to SCI_FAN_TEMP_ALERT required for HW MAXFAN.
        tempAlertVal = FLD_SET_DRF_NUM(_PGC6, _SCI_FAN_TEMP_ALERT,
                                       _HI, maxFanDutyCycle, tempAlertVal);
        tempAlertVal = FLD_SET_DRF(_PGC6, _SCI_FAN_TEMP_ALERT,
                                   _OVERRIDE, _ENABLE, tempAlertVal);

        // Write the config to SCI_FAN_TEMP_ALERT.
        REG_WR32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT_0, tempAlertVal);

        // Trigger an update for the above configuration to take effect.
        REG_WR32(FECS, LW_PGC6_SCI_FAN_CFG1_0,
            FLD_SET_DRF(_PGC6, _SCI_FAN_CFG1, _UPDATE, _TRIGGER, configVal));
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        tempAlertVal = REG_RD32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT);
        configVal    = REG_RD32(CSB, LW_CPWR_THERM_PWM1);

        // Cancel any pending updates.
        if (FLD_TEST_DRF(_CPWR_THERM, _PWM1, _SETTING_NEW,
                         _PENDING, configVal))
        {
            REG_WR32(CSB, LW_CPWR_THERM_PWM1,
                FLD_SET_DRF(_CPWR_THERM, _PWM1, _SETTING_NEW, _DONE, configVal));
        }

        // Setup the configuration to FAN_TEMP_ALERT, required for HW MAXFAN.
        tempAlertVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PWM_TEMP_ALERT,
                                       _HI, maxFanDutyCycle, tempAlertVal);
        tempAlertVal = FLD_SET_DRF(_CPWR_THERM, _PWM_TEMP_ALERT,
                                   _OVERRIDE, _ENABLE, tempAlertVal);

        // Write the config to LW_CPWR_THERM_PWM_TEMP_ALERT.
        REG_WR32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT, tempAlertVal);

        // Trigger an update for the above configuration to take effect.
        REG_WR32(CSB, LW_CPWR_THERM_PWM1,
            FLD_SET_DRF(_CPWR_THERM, _PWM1, _NEW_REQUEST, _TRIGGER, configVal));
    }
}

/*!
 * @brief Set the force temperature alert condition.
 *
 * @param[in]  bForce  Indicating whether to set/clear forced temp alert.
 */
void
pmgrHwMaxFanTempAlertForceSet_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwBool                  bForce
)
{
    LwU32 reg;

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        reg = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT_0);

        reg = bForce ?
                    FLD_SET_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _FORCE, _YES, reg) :
                    FLD_SET_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _FORCE, _NO, reg);

        REG_WR32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT_0, reg);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        reg = REG_RD32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT);

        reg = bForce ?
                    FLD_SET_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _FAKE, _YES, reg) :
                    FLD_SET_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _FAKE, _NO, reg);

        REG_WR32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT, reg);
    }
}

/*!
 * @brief Check if there is a forced temp alert condition.
 *
 * @return LW_TRUE      if there is a forced temp alert condition.
 *         LW_FALSE     Otherwise.
 */
LwBool
pmgrHwMaxFanTempAlertForceGet_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource
)
{
    LwBool bMaxFanForce = LW_FALSE;
    LwU32  reg;

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        reg = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT_0);
        bMaxFanForce =
            (FLD_TEST_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _FORCE, _YES, reg) &&
             FLD_TEST_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _OVERRIDE, _ENABLE, reg));
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        reg = REG_RD32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT);
        bMaxFanForce =
            (FLD_TEST_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _FAKE, _YES, reg) &&
             FLD_TEST_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _OVERRIDE, _ENABLE, reg));
    }

    return bMaxFanForce;
}
