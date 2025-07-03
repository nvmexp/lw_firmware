/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmpwmga100.c
 * @brief   PMU HAL functions for GA100+ for thermal PWM control
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
/* ------------------------- Compile Time Checks ---------------------------- */
ct_assert(DRF_BASE(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW) == 0);
ct_assert(DRF_BASE(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_1_SETTING_NEW) == 1);
ct_assert(DRF_BASE(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_2_SETTING_NEW) == 2);
ct_assert(DRF_BASE(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_3_SETTING_NEW) == 3);

ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW_DONE == 0);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_1_SETTING_NEW_DONE == 0);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_2_SETTING_NEW_DONE == 0);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_3_SETTING_NEW_DONE == 0);

ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW_TRIGGER == 1);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_1_SETTING_NEW_TRIGGER == 1);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_2_SETTING_NEW_TRIGGER == 1);
ct_assert(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK_3_SETTING_NEW_TRIGGER == 1);

ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_PERIOD) == 0);

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
thermPwmSourceDescriptorConstruct_GA100
(
    RM_PMU_PMGR_PWM_SOURCE pwmSource,
    LwU8                   ovlIdxDmem
)
{
    PWM_SOURCE_DESCRIPTOR          *pPwmSrcDesc = NULL;

    union
    {
        PWM_SOURCE_DESCRIPTOR           pwmSrcDesc;
        PWM_SOURCE_DESCRIPTOR_TRIGGER   pwmSrcDescTrigger;
        PWM_SOURCE_DESCRIPTOR_CONFIGURE pwmSrcDescConfigure;
    } descData;

    switch (pwmSource)
    {
        case RM_PMU_PMGR_PWM_SOURCE_THERM_PWM:
        {
            ct_assert(DRF_MASK(LW_CPWR_THERM_PWM0_PERIOD) ==
                      DRF_MASK(LW_CPWR_THERM_PWM1_HI));
            ct_assert(DRF_BASE(LW_CPWR_THERM_PWM0_PERIOD) == 0);
            ct_assert(LW_CPWR_THERM_PWM1_SETTING_NEW_DONE == 0);
            ct_assert(LW_CPWR_THERM_PWM1_NEW_REQUEST_TRIGGER == 1);

            descData.pwmSrcDesc.type          = PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT;
            descData.pwmSrcDesc.addrPeriod    = LW_CPWR_THERM_PWM0;
            descData.pwmSrcDesc.addrDutycycle = LW_CPWR_THERM_PWM1;
            descData.pwmSrcDesc.mask          = DRF_MASK(LW_CPWR_THERM_PWM0_PERIOD);
            descData.pwmSrcDesc.triggerIdx    = DRF_BASE(LW_CPWR_THERM_PWM1_NEW_REQUEST);
            descData.pwmSrcDesc.doneIdx       = DRF_BASE(LW_CPWR_THERM_PWM1_SETTING_NEW);
            descData.pwmSrcDesc.bus           = REG_BUS_CSB;
            descData.pwmSrcDesc.bCancel       = LW_FALSE;
            descData.pwmSrcDesc.bTrigger      = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_1:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_2:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_3:
        {
            ct_assert(DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD) ==
                        DRF_MASK(LW_CPWR_THERM_VID1_PWM_HI));
            LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0);

            descData.pwmSrcDescTrigger.super.type          = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
            descData.pwmSrcDescTrigger.super.addrPeriod    = LW_CPWR_THERM_VID0_PWM(idx);
            descData.pwmSrcDescTrigger.super.addrDutycycle = LW_CPWR_THERM_VID1_PWM(idx);
            descData.pwmSrcDescTrigger.super.mask          = DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD);
            descData.pwmSrcDescTrigger.super.triggerIdx    = idx;
            descData.pwmSrcDescTrigger.super.doneIdx       = idx;
            descData.pwmSrcDescTrigger.super.bus           = REG_BUS_CSB;
            descData.pwmSrcDescTrigger.super.bCancel       = LW_TRUE;
            descData.pwmSrcDescTrigger.super.bTrigger      = LW_TRUE;
            descData.pwmSrcDescTrigger.addrTrigger         = LW_CPWR_THERM_VID_PWM_TRIGGER_MASK;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_0:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_1:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_2:
        case RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_3:
        {
            ct_assert(DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD) ==
                        DRF_MASK(LW_CPWR_THERM_VID2_PWM_IPC_HI_MIN));
            LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_0);

            descData.pwmSrcDescConfigure.super.super.type          = PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE;
            descData.pwmSrcDescConfigure.super.super.addrPeriod    = LW_CPWR_THERM_VID0_PWM(idx);
            descData.pwmSrcDescConfigure.super.super.addrDutycycle = LW_CPWR_THERM_VID2_PWM(idx);
            descData.pwmSrcDescConfigure.super.super.mask          = DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD);
            descData.pwmSrcDescConfigure.super.super.triggerIdx    = idx;
            descData.pwmSrcDescConfigure.super.super.doneIdx       = idx;
            descData.pwmSrcDescConfigure.super.super.bus           = REG_BUS_CSB;
            descData.pwmSrcDescConfigure.super.super.bCancel       = LW_TRUE;
            descData.pwmSrcDescConfigure.super.super.bTrigger      = LW_TRUE;
            descData.pwmSrcDescConfigure.super.addrTrigger         = LW_CPWR_THERM_VID_PWM_TRIGGER_MASK;
            descData.pwmSrcDescConfigure.enableIdx                 = DRF_BASE(LW_CPWR_THERM_VID0_PWM_USE_IPC);
            descData.pwmSrcDescConfigure.bEnable                   = LW_TRUE;
            break;
        }
        default:
        {
            goto thermPwmSourceDescriptorConstruct_GA100_exit;
        }
    }

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &descData.pwmSrcDesc);

thermPwmSourceDescriptorConstruct_GA100_exit:
    return pPwmSrcDesc;
}

/* ------------------------- Static Functions ------------------------------- */
