/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmpwmgp10x.c
 * @brief   PMU HAL functions for GP10X+ for thermal PWM control
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "therm/objtherm.h"
#include "lib/lib_pwm.h"

#include "config/g_therm_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Public Functions  ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VID_PWM_TRIGGER_MASK)
/*!
 * @brief   Set the VID PWM atomic trigger mask.
 */
void
thermVidPwmTriggerMaskSet_GP10X(void)
{
    LwU32   regValOld;
    LwU32   regValNew;

    regValOld = regValNew = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);

    //
    // Setting new trigger while previous one is pending may lead to PWM
    // corruption. Cancel previous pending trigger and apply new trigger.
    //
    if (FLD_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _0_SETTING_NEW,
                        _PENDING, regValNew))
    {
        regValNew = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                                _0_SETTING_NEW, _DONE, regValNew);
    }
    if (FLD_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _1_SETTING_NEW,
                        _PENDING, regValNew))
    {
        regValNew = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                                _1_SETTING_NEW, _DONE, regValNew);
    }

    if (regValOld != regValNew)
    {
        REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, regValNew);
    }

    // Apply the new trigger.
    regValNew = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                            _0_SETTING_NEW, _TRIGGER, regValNew);
    regValNew = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                            _1_SETTING_NEW, _TRIGGER, regValNew);

    REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, regValNew);
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_VID_PWM_TRIGGER_MASK)

/* ------------------------- Static Functions ------------------------------- */
