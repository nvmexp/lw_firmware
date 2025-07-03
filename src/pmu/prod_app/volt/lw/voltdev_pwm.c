/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  voltdev_pwm.c
 * @brief Volt Device Model Specific to PWM
 *
 * This module is a collection of functions managing and manipulating state
 * related to the PWM Serial VID voltage devices which controls/regulates
 * the voltage on an output power rail (e.g. LWVDD).
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "volt/voltdev.h"
#include "lib/lib_pwm.h"
#include "lib/lib_gpio.h"
#include "pmu_objpg.h"

#include "config/g_volt_hal.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_voltDeviceDynamicCast_PWM)
    GCC_ATTRIB_SECTION("imem_libVolt", "s_voltDeviceDynamicCast_PWM")
    GCC_ATTRIB_USED();
static FLCN_STATUS s_voltDeviceSetVoltage_PWM_VF_SWITCH(VOLT_DEVICE *pDevice, LwU32 voltageuV, LwBool bTrigger, LwBool bWait);
static FLCN_STATUS s_voltDeviceComputeAndProgramDutyCycle_PWM(VOLT_DEVICE_PWM *pDevicePWM, LwU32 voltageuV, LwBool bTrigger);
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
static FLCN_STATUS s_voltDeviceSetVoltage_PWM_GATE(VOLT_DEVICE *pDevice, LwU32 voltageuV, LwBool bTrigger, LwBool bWait);
static FLCN_STATUS s_voltDeviceSetVoltage_PWM_UNGATE(VOLT_DEVICE *pDevice, LwU32 voltageuV, LwBool bTrigger, LwBool bWait);
#endif

static FLCN_STATUS s_voltDeviceSanityCheck_PWM_VF_SWITCH(VOLT_DEVICE *pDevice);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Virtual table for VOLT_DEVICE_PWM object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VoltDevicePwmVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_voltDeviceDynamicCast_PWM)
};

/* ------------------------- Macros and Defines ----------------------------- */
#define VOLT_DEVICE_PWM_GPIO_EN_MAX_ENTRIES                                    1

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Construct a @ref VOLT_DEVICE_PWM object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
voltDeviceGrpIfaceModel10ObjSetImpl_PWM
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VOLT_VOLT_DEVICE_PWM_BOARDOBJ_SET   *pSet            =
        (RM_PMU_VOLT_VOLT_DEVICE_PWM_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool                                      bFirstConstruct = (*ppBoardObj == NULL);
    VOLT_DEVICE_PWM                            *pDev;
    FLCN_STATUS                                 status;

    status = voltDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto voltDeviceGrpIfaceModel10ObjSetImpl_PWM_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDev, *ppBoardObj, VOLT, VOLT_DEVICE, PWM,
                                  &status, voltDeviceGrpIfaceModel10ObjSetImpl_PWM_exit);

    pDev->rawPeriod             = pSet->rawPeriod;
    pDev->voltageBaseuV         = pSet->voltageBaseuV;
    pDev->voltageOffsetScaleuV  = pSet->voltageOffsetScaleuV;
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    pDev->voltEnGpioPin         = pSet->voltEnGpioPin;
    pDev->gateVoltageuV         = pSet->gateVoltageuV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION)

    if (bFirstConstruct)
    {
        // Construct PWM source descriptor data.
        pDev->pPwmSrcDesc =
            pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pSet->pwmSource);
        if (pDev->pPwmSrcDesc == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto voltDeviceGrpIfaceModel10ObjSetImpl_PWM_exit;
        }

        pDev->pwmSource = pSet->pwmSource;
    }
    else
    {
        if (pDev->pwmSource != pSet->pwmSource)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto voltDeviceGrpIfaceModel10ObjSetImpl_PWM_exit;
        }
    }

voltDeviceGrpIfaceModel10ObjSetImpl_PWM_exit:
    return status;
}

/*!
 * @brief Class implementation.
 *
 * @copydoc VoltDeviceGetVoltage()
 */
FLCN_STATUS
voltDeviceGetVoltage_PWM
(
    VOLT_DEVICE    *pDevice,
    LwU32          *pVoltageuV
)
{
    LwU32               dutyCycle    = 0;
    LwU32               period       = 0;
    VOLT_DEVICE_PWM   *pDevicePWM;
    LwBool              bPwmIlwerted;
    LwS32               tmpVoltageuV;
    LwBool              bDone;
    FLCN_STATUS         status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
    };

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDevicePWM, &pDevice->super, VOLT, VOLT_DEVICE, PWM,
                                  &status, voltDeviceGetVoltage_PWM_no_ovl_exit);

    bPwmIlwerted = (pDevicePWM->voltageOffsetScaleuV < 0);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Obtain the current duty cycle of the PWM generator.
        status = pwmParamsGet(pDevicePWM->pPwmSrcDesc, &dutyCycle, &period,
                              &bDone, bPwmIlwerted);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltDeviceGetVoltage_PWM_exit;
        }

        if (period != 0UL)
        {
            // TargetVoltage = BaseVoltage + DutyCycle * OffsetScale / RawPeriod.
            tmpVoltageuV = (
                            (pDevicePWM->voltageBaseuV) +
                            ((((LwS32)dutyCycle) * pDevicePWM->voltageOffsetScaleuV) /
                            ((LwS32)period))
                            );
            // Avoided cast from composite expression to fix MISRA Rule 10.8.
            *pVoltageuV = (LwU32)tmpVoltageuV;
        }
        else
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto voltDeviceGetVoltage_PWM_exit;
        }

voltDeviceGetVoltage_PWM_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

voltDeviceGetVoltage_PWM_no_ovl_exit:
    return status;
}

/*!
 * @copydoc VoltDeviceSetVoltage()
 */
FLCN_STATUS
voltDeviceSetVoltage_PWM
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait,
    LwU8            railAction
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGpio)
#endif
    };
    FLCN_STATUS     status;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
        switch (railAction)
        {
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH:
            {
                status = s_voltDeviceSetVoltage_PWM_VF_SWITCH(pDevice, voltageuV, bTrigger, bWait);
                break;
            }
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE:
            {
                status = s_voltDeviceSetVoltage_PWM_GATE(pDevice, voltageuV, bTrigger, bWait);
                break;
            }
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE:
            {
                status = s_voltDeviceSetVoltage_PWM_UNGATE(pDevice, voltageuV, bTrigger, bWait);
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto voltDeviceSetVoltage_PWM_exit;
            }
        }
#else
        status = s_voltDeviceSetVoltage_PWM_VF_SWITCH(pDevice, voltageuV, bTrigger, bWait);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltDeviceSetVoltage_PWM_exit;
        }
#endif

voltDeviceSetVoltage_PWM_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_IPC_VMIN))
/*!
 * @copydoc VoltDeviceConfigure()
 */
FLCN_STATUS
voltDeviceConfigure_PWM
(
    VOLT_DEVICE *pDevice,
    LwBool       bEnable
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
    };
    VOLT_DEVICE_PWM   *pDevicePWM;
    FLCN_STATUS         status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDevicePWM, &pDevice->super, VOLT, VOLT_DEVICE, PWM,
                                  &status, voltDeviceConfigure_PWM_exit);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = pwmSourceConfigure(pDevicePWM->pPwmSrcDesc, bEnable);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceConfigure_PWM_exit;
    }

voltDeviceConfigure_PWM_exit:
    return status;
}
#endif

/*!
 * @brief   VOLT_DEVICE_PWM implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_voltDeviceDynamicCast_PWM
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void              *pObject        = NULL;
    VOLT_DEVICE_PWM  *pVoltDevicePwm = (VOLT_DEVICE_PWM *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, PWM))
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceDynamicCast_PWM_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, BASE):
        {
            VOLT_DEVICE *pVoltDev = &pVoltDevicePwm->super;
            pObject = (void *)pVoltDev;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, PWM):
        {
            pObject = (void *)pVoltDevicePwm;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_voltDeviceDynamicCast_PWM_exit:
    return pObject;
}

/*!
 * @copydoc VoltDeviceSanityCheck()
 */
FLCN_STATUS
voltDeviceSanityCheck_PWM
(
    VOLT_DEVICE    *pDevice,
    LwU8            railAction
)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    switch (railAction)
    {
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH:
        {
            status = s_voltDeviceSanityCheck_PWM_VF_SWITCH(pDevice);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE:
        {
            // No sanity checks for GATE as of now.
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE:
        {
            // No sanity checks for UNGATE as of now.
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto voltDeviceSanityCheck_PWM_exit;
        }
    }
#else
    status = s_voltDeviceSanityCheck_PWM_VF_SWITCH(pDevice);
#endif

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceSanityCheck_PWM_exit;
    }

voltDeviceSanityCheck_PWM_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
/*!
 * @copydoc VoltDeviceLoadSanityCheck()
 */
FLCN_STATUS
voltDeviceLoadSanityCheck_PWM
(
    VOLT_DEVICE    *pDevice
)
{
    VOLT_DEVICE_PWM   *pVoltDevPWM = (VOLT_DEVICE_PWM *)pDevice;
    FLCN_STATUS         status      = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        // Ensure that the GPIO pin for the requested device is valid.
        if (pVoltDevPWM->voltEnGpioPin >= GPIO_PIN_COUNT_GET())
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto voltDeviceLoadSanityCheck_PWM_exit;
        }
    }

voltDeviceLoadSanityCheck_PWM_exit:
    return status;
}
#endif

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Sets voltage on the appropriate VOLT_DEVICE in response to VF_SWITCH rail action.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_OK
 *      Requested target voltage was set successfully.
 */
static FLCN_STATUS
s_voltDeviceSetVoltage_PWM_VF_SWITCH
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait
)
{
    VOLT_DEVICE_PWM   *pDevicePWM   = (VOLT_DEVICE_PWM *)pDevice;
    FLCN_STATUS         status       = FLCN_OK;

    // Compute and program the duty cycle in the PWM generator.
    status = s_voltDeviceComputeAndProgramDutyCycle_PWM(pDevicePWM, voltageuV, bTrigger);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_VF_SWITCH_exit;
    }

    // Call super-class implementation.
    status = voltDeviceSetVoltage_SUPER(pDevice, voltageuV, bTrigger, bWait, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_VF_SWITCH_exit;
    }

s_voltDeviceSetVoltage_PWM_VF_SWITCH_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
/*!
 * @brief Gates the appropriate VOLT_DEVICE in response to GATE rail action.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 *
 * @return  FLCN_OK
 *      Requested gate operation completed successfully.
 * @return  Errors propagated by other functions.
 */
static FLCN_STATUS
s_voltDeviceSetVoltage_PWM_GATE
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait
)
{
    VOLT_DEVICE_PWM *pVoltDevPWM = (VOLT_DEVICE_PWM *)pDevice;
    GPIO_LIST_ITEM   gpioData[VOLT_DEVICE_PWM_GPIO_EN_MAX_ENTRIES];
    FLCN_STATUS      status;

    // Program the target voltage.
    if (pVoltDevPWM->gateVoltageuV != LW2080_CTRL_VOLT_VOLT_DEVICE_PSV_GATE_VOLTAGE_UV_ILWALID)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM_WAR_BUG_200628767) &&
                    pVoltDevPWM->gateVoltageuV == 0)
        {
            status = s_voltDeviceComputeAndProgramDutyCycle_PWM(pVoltDevPWM,
                            400000, LW_TRUE);
        }
        else
        {
            status = s_voltDeviceComputeAndProgramDutyCycle_PWM(pVoltDevPWM,
                            pVoltDevPWM->gateVoltageuV, LW_TRUE);
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_voltDeviceSetVoltage_PWM_GATE_exit;
        }
        OS_TIMER_WAIT_US(pDevice->switchDelayus);
    }

    // De-assert the Voltage En GPIO pin to gate the device.
    gpioData[0].bState  = LW_FALSE;
    gpioData[0].gpioPin = pVoltDevPWM->voltEnGpioPin;

    status = gpioSetStateList(VOLT_DEVICE_PWM_GPIO_EN_MAX_ENTRIES, gpioData, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_GATE_exit;
    }

    // Call super-class implementation.
    status = voltDeviceSetVoltage_SUPER(pDevice, voltageuV, bTrigger, bWait,
                LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_GATE_exit;
    }

s_voltDeviceSetVoltage_PWM_GATE_exit:
    return status;
}

/*!
 * @brief Ungates the appropriate VOLT_DEVICE in response to GATE rail action.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 *
 * @return  FLCN_OK
 *      Requested ungate operation completed successfully.
 * @return  Errors propagated by other functions.
 */
static FLCN_STATUS
s_voltDeviceSetVoltage_PWM_UNGATE
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait
)
{
    VOLT_DEVICE_PWM *pVoltDevPWM = (VOLT_DEVICE_PWM *)pDevice;
    GPIO_LIST_ITEM   gpioData[VOLT_DEVICE_PWM_GPIO_EN_MAX_ENTRIES];
    FLCN_STATUS      status;

    // Program the target voltage.
    status = s_voltDeviceComputeAndProgramDutyCycle_PWM(pVoltDevPWM, voltageuV,
                bTrigger);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_UNGATE_exit;
    }

    // Assert the GPIO pin to ungate the device.
    gpioData[0].bState  = LW_TRUE;
    gpioData[0].gpioPin = pVoltDevPWM->voltEnGpioPin;

    status = gpioSetStateList(VOLT_DEVICE_PWM_GPIO_EN_MAX_ENTRIES, gpioData, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_UNGATE_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM_PGOOD))
    // Confirm that PGOOD_STATUS is ON.
    status = voltPollPgoodStatus_HAL(&Volt, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_UNGATE_exit;
    }
#endif

    // Call super-class implementation.
    status = voltDeviceSetVoltage_SUPER(pDevice, voltageuV, bTrigger, bWait,
                LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceSetVoltage_PWM_UNGATE_exit;
    }

s_voltDeviceSetVoltage_PWM_UNGATE_exit:
    return status;
}
#endif

/*!
 * @brief Computes the duty cycle corresponding to the target voltage and programs
 *        the PWM generator accordingly.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 *
 * @return  FLCN_OK
 *      Requested operation completed successfully.
 * @return  Errors propagated from other functions.
 */
static FLCN_STATUS
s_voltDeviceComputeAndProgramDutyCycle_PWM
(
    VOLT_DEVICE_PWM   *pDevicePWM,
    LwU32               voltageuV,
    LwBool              bTrigger
)
{
    FLCN_STATUS status       = FLCN_OK;
    LwBool      bPwmIlwerted = (pDevicePWM->voltageOffsetScaleuV < 0);
    LwS32       tmpDutyCycle;
    LwU32       dutyCycle;

    //
    // TargetVoltage = BaseVoltage + DutyCycle * OffsetScale / RawPeriod.
    // DutyCycle = ((TargetVoltage - BaseVoltage) * RawPeriod) / OffsetScale.
    //
    if (voltageuV != 0)
    {
        tmpDutyCycle = (
                        ((((LwS32)(voltageuV)-(pDevicePWM->voltageBaseuV))) *
                            ((LwS32)pDevicePWM->rawPeriod)) /
                            (pDevicePWM->voltageOffsetScaleuV)
                        );
    }
    else
    {
       tmpDutyCycle = 0; 
    }

    // Avoided cast from composite expression to fix MISRA Rule 10.8.
    dutyCycle = (LwU32)tmpDutyCycle;


    // Set the duty cycle of the PWM generator.
    status = pwmParamsSet(pDevicePWM->pPwmSrcDesc, dutyCycle,
                          pDevicePWM->rawPeriod, bTrigger, bPwmIlwerted);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltDeviceComputeAndProgramDutyCycle_PWM_exit;
    }

s_voltDeviceComputeAndProgramDutyCycle_PWM_exit:
    return status;
}

/*!
 * @brief Sanity checks the voltage device for the VF_SWITCH rail action.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 *
 *
 * @return  FLCN_OK
 *      Requested sanity check operation completed successfully.
 * @return  Errors propagated by other functions.
 */
static FLCN_STATUS
s_voltDeviceSanityCheck_PWM_VF_SWITCH
(
    VOLT_DEVICE *pDevice
)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGpio)
    };
    VOLT_DEVICE_PWM *pDevicePWM  = (VOLT_DEVICE_PWM *)pDevice;
    LwBool           bState;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // PMU_VOLT_VOLT_RAIL_ACTION is an optional feature,
        // so first check if it is enabled. If its enabled, ensure that the GPIO
        // is ON. Fail in case it is not since we cannot VF switch when GPIO is OFF.
        //
        if (pDevicePWM->voltEnGpioPin < GPIO_PIN_COUNT_GET())
        {
            status = gpioGetState(pDevicePWM->voltEnGpioPin, &bState);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_voltDeviceSanityCheck_PWM_VF_SWITCH_exit;
            }

            if (!bState)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_voltDeviceSanityCheck_PWM_VF_SWITCH_exit;
            }
        }

s_voltDeviceSanityCheck_PWM_VF_SWITCH_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif
    return status;
}
