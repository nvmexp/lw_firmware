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
 * @file    pmu_thrmpwmgp10x_gv10x.c
 * @brief   PMU HAL functions for GP10X and GV10X for thermal PWM control
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
thermPwmSourceDescriptorConstruct_GP10X
(
    RM_PMU_PMGR_PWM_SOURCE pwmSource,
    LwU8                   ovlIdxDmem
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = NULL;
    FLCN_STATUS            status      = FLCN_OK;
    PWM_SOURCE_DESCRIPTOR  pwmSrcDesc;

    switch (pwmSource)
    {
        case RM_PMU_PMGR_PWM_SOURCE_THERM_PWM:
        {
            ct_assert(DRF_MASK(LW_CPWR_THERM_PWM0_PERIOD) ==
                      DRF_MASK(LW_CPWR_THERM_PWM1_HI));
            ct_assert(DRF_BASE(LW_CPWR_THERM_PWM0_PERIOD) == 0);
            ct_assert(LW_CPWR_THERM_PWM1_SETTING_NEW_DONE == 0);
            ct_assert(LW_CPWR_THERM_PWM1_NEW_REQUEST_TRIGGER == 1);

            pwmSrcDesc.addrPeriod    = LW_CPWR_THERM_PWM0;
            pwmSrcDesc.addrDutycycle = LW_CPWR_THERM_PWM1;
            pwmSrcDesc.mask          = DRF_MASK(LW_CPWR_THERM_PWM0_PERIOD);
            pwmSrcDesc.triggerIdx    = DRF_BASE(LW_CPWR_THERM_PWM1_NEW_REQUEST);
            pwmSrcDesc.doneIdx       = DRF_BASE(LW_CPWR_THERM_PWM1_SETTING_NEW);
            pwmSrcDesc.bus           = REG_BUS_CSB;
            pwmSrcDesc.bCancel       = LW_FALSE;
            pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_1:
        {
            LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0);

            ct_assert(DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD) ==
                      DRF_MASK(LW_CPWR_THERM_VID1_PWM_HI));
            ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_PERIOD) == 0);
            ct_assert(LW_CPWR_THERM_VID1_PWM_SETTING_NEW_DONE == 0);
            ct_assert(LW_CPWR_THERM_VID1_PWM_SETTING_NEW_TRIGGER == 1);

            pwmSrcDesc.addrPeriod    = LW_CPWR_THERM_VID0_PWM(idx);
            pwmSrcDesc.addrDutycycle = LW_CPWR_THERM_VID1_PWM(idx);
            pwmSrcDesc.mask          = DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD);
            pwmSrcDesc.triggerIdx    = DRF_BASE(LW_CPWR_THERM_VID1_PWM_SETTING_NEW);
            pwmSrcDesc.doneIdx       = DRF_BASE(LW_CPWR_THERM_VID1_PWM_SETTING_NEW);
            pwmSrcDesc.bus           = REG_BUS_CSB;
            pwmSrcDesc.bCancel       = LW_TRUE;
            pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto thermPwmSourceDescriptorConstruct_GP10X_exit;
    }

    pwmSrcDesc.type = PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT;

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &pwmSrcDesc);

thermPwmSourceDescriptorConstruct_GP10X_exit:
    return pPwmSrcDesc;
}

/* ------------------------- Static Functions ------------------------------- */
