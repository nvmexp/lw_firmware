/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmpwmgk10x.c
 * @brief   PMU HAL functions for GK10X+ for thermal PWM control
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------ Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_ACT_GET)
/*!
 * Used to retrieve an actual period and duty cycle of the PWM signal that is
 * being detected by given PWM source.
 *
 * @param[in]  pwmSource     PWM source enum
 * @param[out] pPwmDutyCycle Buffer in which to return the duty cycle in raw
 *                           units
 * @param[out] pPwmPeriod    Buffer in which to return the period in raw units
 *
 * @return FLCN_ERR_NOT_SUPPORTED    if invalid pwmSource specified (not
 *                                  supporting PWM detection).
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
thermPwmParamsActGet_GM10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        LwU32   act0A;
        LwU32   act0B;
        LwU32   act1;

        // Ensure that PWM settings are retrieved atomically.
        act0B = REG_RD32(CSB, LW_CPWR_THERM_PWM_ACT0);
        do
        {
            act0A = act0B;
            act1  = REG_RD32(CSB, LW_CPWR_THERM_PWM_ACT1);
            act0B = REG_RD32(CSB, LW_CPWR_THERM_PWM_ACT0);
        } while (act0A != act0B);

        *pPwmPeriod    = DRF_VAL(_CPWR_THERM, _PWM_ACT0, _PERIOD, act0B);
        *pPwmDutyCycle = DRF_VAL(_CPWR_THERM, _PWM_ACT1, _HI,     act1);

        // Clear an overflow state if detected.
        if (FLD_TEST_DRF(_CPWR_THERM, _PWM_ACT0, _OVERFLOW, _DETECTED, act0B))
        {
            REG_WR32(CSB, LW_CPWR_THERM_PWM_ACT0,
                     FLD_SET_DRF(_CPWR_THERM, _PWM_ACT0, _OVERFLOW, _CLEAR,
                                 act0B));
        }

        status = FLCN_OK;
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_ACT_GET)

/* ------------------------ Static Functions ------------------------------- */
