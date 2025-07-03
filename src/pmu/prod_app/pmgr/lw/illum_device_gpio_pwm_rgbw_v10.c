/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_device_gpio_pwm_rgbw_v10.c
 * @copydoc illum_device_gpio_pwm_rgbw_v10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device_gpio_pwm_rgbw_v10.h"
#include "pmu_objpmgr.h"
#include "pmu_objdisp.h"
#include "pmu/pmuifpmgrpwm.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_ILLUM_DEVICE_GPIO_PWM_RGBW_V10 *pDescIllumDeviceGPRGBWV10 =
        (RM_PMU_PMGR_ILLUM_DEVICE_GPIO_PWM_RGBW_V10 *)pBoardObjDesc;
    LwBool                             bFirstConstruct = (*ppBoardObj == NULL);
    OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwmConstruct)
        };
    ILLUM_DEVICE_GPIO_PWM_RGBW_V10    *pIllumDeviceGPRGBWV10;
    FLCN_STATUS             status;

    status = illumDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10_exit;
    }
    pIllumDeviceGPRGBWV10 = (ILLUM_DEVICE_GPIO_PWM_RGBW_V10 *)*ppBoardObj;

    // Set member variables.
    pIllumDeviceGPRGBWV10->gpioPinRed        = pDescIllumDeviceGPRGBWV10->gpioPinRed;
    pIllumDeviceGPRGBWV10->gpioPinGreen      = pDescIllumDeviceGPRGBWV10->gpioPinGreen;
    pIllumDeviceGPRGBWV10->gpioPinBlue       = pDescIllumDeviceGPRGBWV10->gpioPinBlue;
    pIllumDeviceGPRGBWV10->gpioPinWhite      = pDescIllumDeviceGPRGBWV10->gpioPinWhite;
    pIllumDeviceGPRGBWV10->pwmSourceRed      = pDescIllumDeviceGPRGBWV10->pwmSourceRed;
    pIllumDeviceGPRGBWV10->pwmSourceGreen    = pDescIllumDeviceGPRGBWV10->pwmSourceGreen;
    pIllumDeviceGPRGBWV10->pwmSourceBlue     = pDescIllumDeviceGPRGBWV10->pwmSourceBlue;
    pIllumDeviceGPRGBWV10->pwmSourceWhite    = pDescIllumDeviceGPRGBWV10->pwmSourceWhite;
    pIllumDeviceGPRGBWV10->rawPeriod         = pDescIllumDeviceGPRGBWV10->rawPeriod;

    if (bFirstConstruct)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Construct PWM source descriptor data.
            pIllumDeviceGPRGBWV10->pPwmSrcDescRed =
                pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pDescIllumDeviceGPRGBWV10->pwmSourceRed);
            if (pIllumDeviceGPRGBWV10->pPwmSrcDescRed == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto illumDeviceConstructImpl_GPIO_PWM_RGBW_V10_ovl_detach;
            }

            pIllumDeviceGPRGBWV10->pwmSourceRed = pDescIllumDeviceGPRGBWV10->pwmSourceRed;
            pIllumDeviceGPRGBWV10->pPwmSrcDescGreen =
                pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pDescIllumDeviceGPRGBWV10->pwmSourceGreen);
            if (pIllumDeviceGPRGBWV10->pPwmSrcDescGreen == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto illumDeviceConstructImpl_GPIO_PWM_RGBW_V10_ovl_detach;
            }

            pIllumDeviceGPRGBWV10->pwmSourceGreen = pDescIllumDeviceGPRGBWV10->pwmSourceGreen;

            pIllumDeviceGPRGBWV10->pPwmSrcDescBlue =
                pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pDescIllumDeviceGPRGBWV10->pwmSourceBlue);
            if (pIllumDeviceGPRGBWV10->pPwmSrcDescBlue == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto illumDeviceConstructImpl_GPIO_PWM_RGBW_V10_ovl_detach;
            }

            pIllumDeviceGPRGBWV10->pwmSourceBlue = pDescIllumDeviceGPRGBWV10->pwmSourceBlue;

            pIllumDeviceGPRGBWV10->pPwmSrcDescWhite =
                pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pDescIllumDeviceGPRGBWV10->pwmSourceWhite);
            if (pIllumDeviceGPRGBWV10->pPwmSrcDescWhite == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto illumDeviceConstructImpl_GPIO_PWM_RGBW_V10_ovl_detach;
            }

            pIllumDeviceGPRGBWV10->pwmSourceWhite = pDescIllumDeviceGPRGBWV10->pwmSourceWhite;

illumDeviceConstructImpl_GPIO_PWM_RGBW_V10_ovl_detach:
            lwosNOP();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10_exit;
        }
    }
    else
    {
        if ((pIllumDeviceGPRGBWV10->pwmSourceRed != pDescIllumDeviceGPRGBWV10->pwmSourceRed)     ||
            (pIllumDeviceGPRGBWV10->pwmSourceGreen != pDescIllumDeviceGPRGBWV10->pwmSourceGreen) ||
            (pIllumDeviceGPRGBWV10->pwmSourceBlue != pDescIllumDeviceGPRGBWV10->pwmSourceBlue)   ||
            (pIllumDeviceGPRGBWV10->pwmSourceWhite != pDescIllumDeviceGPRGBWV10->pwmSourceWhite))
        {
            goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10_exit;
        }
    }

illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10_exit:
    return status;
}

/*!
 * @copydoc IllumDeviceDataSet
 */
FLCN_STATUS
illumDeviceDataSet_GPIO_PWM_RGBW_V10
(
    ILLUM_DEVICE *pIllumDevice,
    ILLUM_ZONE   *pIllumZone
)
{
    ILLUM_DEVICE_GPIO_PWM_RGBW_V10 *pIllumDeviceRGBWV10 =
                    (ILLUM_DEVICE_GPIO_PWM_RGBW_V10 *)pIllumDevice;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
        };
    FLCN_STATUS     status = FLCN_OK;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        switch (pIllumZone->ctrlMode)
        {
            case LW2080_CTRL_ILLUM_CTRL_MODE_MANUAL_RGB:
            {
                ILLUM_ZONE_RGBW     *pIllumZoneRGBW = (ILLUM_ZONE_RGBW *)pIllumZone;
                LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_MANUAL_RGBW_PARAMS
                     *pManualRGBWParams = &pIllumZoneRGBW->ctrlModeParams.manualRGBW.rgbwParams;
                LwU32 dutyCycleR;
                LwU32 dutyCycleG;
                LwU32 dutyCycleB;
                LwU32 dutyCycleW;

                dutyCycleR = ((pManualRGBWParams->colorR * pManualRGBWParams->brightnessPct) / 255) * (pIllumDeviceRGBWV10->rawPeriod / 100);
                dutyCycleG = ((pManualRGBWParams->colorG * pManualRGBWParams->brightnessPct) / 255) * (pIllumDeviceRGBWV10->rawPeriod / 100);
                dutyCycleB = ((pManualRGBWParams->colorB * pManualRGBWParams->brightnessPct) / 255) * (pIllumDeviceRGBWV10->rawPeriod / 100);
                dutyCycleW = ((pManualRGBWParams->colorW * pManualRGBWParams->brightnessPct) / 255) * (pIllumDeviceRGBWV10->rawPeriod / 100);

                //
                // Check if the PWM sources belong to DISP HW. Just checking one source
                // is sufficient for applying the DISP patch required for bug 200629496.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_ILLUM_DISP_SOR_CLK_WAR_BUG_200629496) &&
                    RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pIllumDeviceRGBWV10->pwmSourceRed, MASK_DISP))
                {
                    dispForceSorClk_HAL(&Disp, LW_TRUE);
                }

                //
                // Note - This critical section takes ~20.2 us (on an average) to execute.
                // Profiling was done on a GA102 PG133 SKU 30 with PMU RISCV environment and
                // averaged out on over 330 runs of this function.
                //
                appTaskCriticalEnter();

                // Apply red color component value to HW.
                status = pwmParamsSet(pIllumDeviceRGBWV10->pPwmSrcDescRed, dutyCycleR, pIllumDeviceRGBWV10->rawPeriod, LW_TRUE, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto illumDeviceDataSet_SINGLE_COLOR_V10_cs_exit;
                }

                // Apply green color component value to HW.
                status = pwmParamsSet(pIllumDeviceRGBWV10->pPwmSrcDescGreen, dutyCycleG, pIllumDeviceRGBWV10->rawPeriod, LW_TRUE, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto illumDeviceDataSet_SINGLE_COLOR_V10_cs_exit;
                }

                // Apply blue color component value to HW.
                status = pwmParamsSet(pIllumDeviceRGBWV10->pPwmSrcDescBlue, dutyCycleB, pIllumDeviceRGBWV10->rawPeriod, LW_TRUE, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto illumDeviceDataSet_SINGLE_COLOR_V10_cs_exit;
                }

                // Apply white color component value to HW.
                status = pwmParamsSet(pIllumDeviceRGBWV10->pPwmSrcDescWhite, dutyCycleW, pIllumDeviceRGBWV10->rawPeriod, LW_TRUE, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto illumDeviceDataSet_SINGLE_COLOR_V10_cs_exit;
                }

illumDeviceDataSet_SINGLE_COLOR_V10_cs_exit:
                appTaskCriticalExit();

                // Restore SOR CLK to AUTO.
                if (PMUCFG_FEATURE_ENABLED(PMU_ILLUM_DISP_SOR_CLK_WAR_BUG_200629496) &&
                    RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pIllumDeviceRGBWV10->pwmSourceRed, MASK_DISP))
                {
                    dispForceSorClk_HAL(&Disp, LW_FALSE);
                }

                if (status != FLCN_OK)
                {
                    goto illumDeviceDataSet_SINGLE_COLOR_V10_exit;
                }
                break;
            }
            case LW2080_CTRL_ILLUM_CTRL_MODE_PIECEWISE_LINEAR_RGB:
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                goto illumDeviceDataSet_SINGLE_COLOR_V10_exit;
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto illumDeviceDataSet_SINGLE_COLOR_V10_exit;
            }
        }
     
illumDeviceDataSet_SINGLE_COLOR_V10_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc IllumDeviceSync
 */
FLCN_STATUS
illumDeviceSync_GPIO_PWM_RGBW_V10
(
    ILLUM_DEVICE             *pIllumDevice,
    RM_PMU_ILLUM_DEVICE_SYNC *pSyncData
)
{
    // This device does not support sync command.
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
