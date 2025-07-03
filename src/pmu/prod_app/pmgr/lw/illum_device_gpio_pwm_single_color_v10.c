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
 * @file    illum_device_gpio_pwm_single_color_v10.c
 * @copydoc illum_device_gpio_pwm_single_color_v10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device_gpio_pwm_single_color_v10.h"
#include "pmu_objpmgr.h"

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
illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10 *pDescIllumDeviceGPSCV10 =
        (RM_PMU_PMGR_ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10 *)pBoardObjDesc;
    LwBool                                     bFirstConstruct = (*ppBoardObj == NULL);
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwmConstruct)
        };
    ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10    *pIllumDeviceGPSCV10;
    FLCN_STATUS             status;

    status = illumDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10_exit;
    }
    pIllumDeviceGPSCV10 = (ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10 *)*ppBoardObj;

    // Set member variables.
    pIllumDeviceGPSCV10->gpioFuncSingleColor = pDescIllumDeviceGPSCV10->gpioFuncSingleColor;
    pIllumDeviceGPSCV10->gpioPinSingleColor  = pDescIllumDeviceGPSCV10->gpioPinSingleColor;
    pIllumDeviceGPSCV10->pwmSource           = pDescIllumDeviceGPSCV10->pwmSource;
    pIllumDeviceGPSCV10->rawPeriod           = pDescIllumDeviceGPSCV10->rawPeriod;

    if (bFirstConstruct)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Construct PWM source descriptor data.
            pIllumDeviceGPSCV10->pPwmSrcDesc =
                pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pDescIllumDeviceGPSCV10->pwmSource);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (pIllumDeviceGPSCV10->pPwmSrcDesc == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10_exit;
        }

        pIllumDeviceGPSCV10->pwmSource = pDescIllumDeviceGPSCV10->pwmSource;
    }
    else
    {
        if (pIllumDeviceGPSCV10->pwmSource != pDescIllumDeviceGPSCV10->pwmSource)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10_exit;
        }
    }

illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10_exit:
    return status;
}

/*!
 * @copydoc IllumDeviceDataSet
 */
FLCN_STATUS
illumDeviceDataSet_GPIO_PWM_SINGLE_COLOR_V10
(
    ILLUM_DEVICE *pIllumDevice,
    ILLUM_ZONE   *pIllumZone
)
{
    ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10 *pIllumDeviceSingleColorV10 =
        (ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10 *)pIllumDevice;
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
                ILLUM_ZONE_SINGLE_COLOR *pIllumZoneSingleColor = (ILLUM_ZONE_SINGLE_COLOR *)pIllumZone;
                LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_MANUAL_SINGLE_COLOR_PARAMS
                     *pManualSingleColorParams = &pIllumZoneSingleColor->ctrlModeParams.manualSingleColor.singleColorParams;
                LwU32 dutyCycle = (pManualSingleColorParams->brightnessPct * (pIllumDeviceSingleColorV10->rawPeriod )/ 100);

                status = pwmParamsSet(pIllumDeviceSingleColorV10->pPwmSrcDesc, dutyCycle, pIllumDeviceSingleColorV10->rawPeriod, LW_TRUE, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
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
illumDeviceSync_GPIO_PWM_SINGLE_COLOR_V10
(
    ILLUM_DEVICE             *pIllumDevice,
    RM_PMU_ILLUM_DEVICE_SYNC *pSyncData
)
{
    // This device does not support sync command.
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
