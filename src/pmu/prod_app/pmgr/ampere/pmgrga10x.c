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
 * @file   pmgrga10x.c
 * @brief  Contains all PCB Management (PGMR) routines specific to GA10X+.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lib/lib_gpio.h"

#include "dev_pmgr.h"
#include "dev_pwr_csb.h"
#include "dev_gc6_island.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Maximum latency for latching the GPIOs.
 */
#define GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US                              (50)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Obtains the current GPIO state i.e. whether GPIO is ON/OFF.
 *
 * @note    This HAL should be called only from @ref gpioGetState.
 */
FLCN_STATUS
pmgrGpioGetState_GA10X
(
    LwU8    gpioPin,
    LwBool *pBState
)
{
    LwU8    inputHwEnum = GPIO_INPUT_HW_ENUM_GET(gpioPin);
    LwBool  bOnData;
    LwU32   reg32;

    //
    // If input function is assigned to a GPIO pin,
    // check LW_PMGR_GPIO_INPUT_CNTL_<inputHwEnum>_READ.
    // Else, check LW_PMGR_GPIO_<gpioPin>_OUTPUT_CNTL_IO_INPUT.
    // This distinguishes between input and output GPIO pins.
    //
    if (inputHwEnum != LW_PMGR_GPIO_INPUT_CNTL_0__UNASSIGNED)
    {
        reg32   = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL(inputHwEnum));
        bOnData = FLD_TEST_DRF(_PMGR_GPIO, _INPUT_CNTL, _READ, _1, reg32);
    }
    else
    {
        reg32   = REG_RD32(FECS, LW_GPIO_OUTPUT_CNTL(gpioPin));
        bOnData = FLD_TEST_DRF(_GPIO, _OUTPUT_CNTL, _IO_INPUT, _1, reg32);
    }

    *pBState =
        (bOnData == FLD_TEST_DRF(_RM_PMU_PMGR, _GPIO_INIT_CFG_FLAGS,
                                    _ON_DATA, _1, GPIO_FLAGS_GET(gpioPin)));

    return FLCN_OK;
}

/*!
 * Set GPIO state of each GPIO in the list to ON/OFF.
 *
 * @note    This HAL should be called only from @ref gpioSetStateList.
 */
FLCN_STATUS
pmgrGpioSetStateList_GA10X
(
    LwU8            count,
    GPIO_LIST_ITEM *pList,
    LwBool          bTrigger
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwBool      bIoOutEn    = LW_FALSE;
    LwBool      bIoOutput;
    LwBool      bState;
    LwBool      bSelNormal;
    LwU32       reg32;
    LwU8        gpioPin;
    LwU8        i;

    // Set the target state for each GPIO in the list.
    for (i = 0; i < count; i++)
    {
        gpioPin = pList[i].gpioPin;
        bState  = pList[i].bState;

        reg32 = REG_RD32(FECS, LW_GPIO_OUTPUT_CNTL(gpioPin));

        bSelNormal = (GPIO_OUTPUT_HW_ENUM_GET(gpioPin) ==
                        LW_GPIO_OUTPUT_CNTL_SEL_NORMAL);

        // Switch the GPIO ON.
        if (bState)
        {
            bIoOutput = FLD_TEST_DRF(_RM_PMU_PMGR, _GPIO_INIT_CFG_FLAGS,
                            _ON_DATA, _1, GPIO_FLAGS_GET(gpioPin));

            if (bSelNormal)
            {
                bIoOutEn = FLD_TEST_DRF(_RM_PMU_PMGR, _GPIO_INIT_CFG_FLAGS,
                            _ON_ENABLE, _1, GPIO_FLAGS_GET(gpioPin));
            }
        }
        // Switch the GPIO OFF.
        else
        {
            bIoOutput = FLD_TEST_DRF(_RM_PMU_PMGR, _GPIO_INIT_CFG_FLAGS,
                            _OFF_DATA, _1, GPIO_FLAGS_GET(gpioPin));

            if (bSelNormal)
            {
                bIoOutEn = FLD_TEST_DRF(_RM_PMU_PMGR, _GPIO_INIT_CFG_FLAGS,
                            _OFF_ENABLE, _1, GPIO_FLAGS_GET(gpioPin));
            }
        }

        // Determine whether the GPIO will output a 1 or a 0.
        if (bIoOutput)
        {
            reg32 = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _1, reg32);
        }
        else
        {
            reg32 = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _0, reg32);
        }

        // Determines whether the GPIO will be enabled for output or not.
        if (bSelNormal)
        {
            if (bIoOutEn)
            {
                reg32 = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _NO, reg32);
            }
            else
            {
                reg32 = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, reg32);
            }
        }

        REG_WR32(FECS, LW_GPIO_OUTPUT_CNTL(gpioPin), reg32);
    }

    if (bTrigger)
    {
        // Update trigger register to flush most recent GPIO settings to HW.
        REG_FLD_WR_DRF_DEF(FECS, _PMGR, _GPIO_OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER);

        // Poll for LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER to change to _DONE.
        if (!PMU_REG32_POLL_US(
             USE_FECS(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER),
             DRF_SHIFTMASK(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE),
             LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_DONE,
             GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US,
             PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_TIMEOUT;
        }
    }

    return status;
}

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
pmgrPwmSourceDescriptorConstruct_GA10X
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
        case RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1:
        {
            ct_assert(DRF_MASK(LW_PGC6_SCI_FAN_CFG0_PERIOD) ==
                        DRF_MASK(LW_PGC6_SCI_FAN_CFG1_HI));
            ct_assert(DRF_BASE(LW_PGC6_SCI_FAN_CFG0_PERIOD) == 0);
            ct_assert(LW_PGC6_SCI_FAN_CFG1_UPDATE_DONE == 0);
            ct_assert(LW_PGC6_SCI_FAN_CFG1_UPDATE_TRIGGER == 1);
            LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0);

            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_FAN_CFG0(idx);
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_FAN_CFG1(idx);
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
            goto pmgrPwmSourceDescriptorConstruct_GA10X_exit;
        }
    }
    pwmSrcDesc.type = PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT;

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &pwmSrcDesc);

pmgrPwmSourceDescriptorConstruct_GA10X_exit:
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
pmgrSciPwmParamsActGet_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if ((pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0) ||
        (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1))
    {
        LwU32   act0A;
        LwU32   act0B;
        LwU32   act1;
        LwU8    idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0);

        // Ensure that PWM settings are retrieved atomically.
        act0B = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT0(idx));
        do
        {
            act0A = act0B;
            act1  = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT1(idx));
            act0B = REG_RD32(FECS, LW_PGC6_SCI_FAN_ACT0(idx));
        } while (act0A != act0B);

        *pPwmPeriod    = DRF_VAL(_PGC6, _SCI_FAN_ACT0, _PERIOD, act0B);
        *pPwmDutyCycle = DRF_VAL(_PGC6, _SCI_FAN_ACT1, _HI,     act1);

        // Clear an overflow state if detected.
        if (FLD_TEST_DRF(_PGC6, _SCI_FAN_ACT0, _OVERFLOW, _DETECTED, act0B))
        {
            REG_WR32(FECS, LW_PGC6_SCI_FAN_ACT0(idx),
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
pmgrHwMaxFanInit_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                   maxFanDutyCycle
)
{
    LwU32 tempAlertVal;
    LwU32 configVal;

    if ((pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0) ||
        (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1))
    {
        LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0);

        tempAlertVal = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT(idx));
        configVal    = REG_RD32(FECS, LW_PGC6_SCI_FAN_CFG1(idx));

        // Cancel any pending updates.
        if (FLD_TEST_DRF(_PGC6, _SCI_FAN_CFG1, _UPDATE,
                         _PENDING, configVal))
        {
            REG_WR32(FECS, LW_PGC6_SCI_FAN_CFG1(idx),
                FLD_SET_DRF(_PGC6, _SCI_FAN_CFG1, _UPDATE, _DONE, configVal));
        }

        // Setup the config to SCI_FAN_TEMP_ALERT required for HW MAXFAN.
        tempAlertVal = FLD_SET_DRF_NUM(_PGC6, _SCI_FAN_TEMP_ALERT,
                                       _HI, maxFanDutyCycle, tempAlertVal);
        tempAlertVal = FLD_SET_DRF(_PGC6, _SCI_FAN_TEMP_ALERT,
                                   _OVERRIDE, _ENABLE, tempAlertVal);

        // Write the config to SCI_FAN_TEMP_ALERT.
        REG_WR32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT(idx), tempAlertVal);

        // Trigger an update for the above configuration to take effect.
        REG_WR32(FECS, LW_PGC6_SCI_FAN_CFG1(idx),
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
    else
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * @brief Set the force temperature alert condition.
 *
 * @param[in]  bForce  Indicating whether to set/clear forced temp alert.
 */
void
pmgrHwMaxFanTempAlertForceSet_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwBool                  bForce
)
{
    LwU32 reg;

    if ((pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0) ||
        (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1))
    {
        LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0);

        reg = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT(idx));

        reg = bForce ?
                    FLD_SET_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _FORCE, _YES, reg) :
                    FLD_SET_DRF(_PGC6_SCI, _FAN_TEMP_ALERT, _FORCE, _NO, reg);

        REG_WR32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT(idx), reg);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        reg = REG_RD32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT);

        reg = bForce ?
                    FLD_SET_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _FAKE, _YES, reg) :
                    FLD_SET_DRF(_CPWR_THERM, _PWM_TEMP_ALERT, _FAKE, _NO, reg);

        REG_WR32(CSB, LW_CPWR_THERM_PWM_TEMP_ALERT, reg);
    }
    else
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * @brief Check if there is a forced temp alert condition.
 *
 * @return LW_TRUE      if there is a forced temp alert condition.
 *         LW_FALSE     Otherwise.
 */
LwBool
pmgrHwMaxFanTempAlertForceGet_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource
)
{
    LwBool bMaxFanForce = LW_FALSE;
    LwU32  reg;

    if ((pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0) ||
        (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1))
    {
        LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0);

        reg = REG_RD32(FECS, LW_PGC6_SCI_FAN_TEMP_ALERT(idx));
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
    else
    {
        PMU_BREAKPOINT();
    }

    return bMaxFanForce;
}

/*!
 * Initialize interrupts to be serviced by PMGR
 */
void
pmgrInterruptInit_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
    {
        LwU32 reg32;

        // Route the ISENSE_BEACONX interrupts to PMU
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_ROUTE);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _ISENSE_BEACON1_INTR, _PMU,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _ISENSE_BEACON2_INTR, _PMU,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_ROUTE, reg32);

        // Enable the ISENSE_BEACONX interrupts
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON1_INTR, _ENABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON2_INTR, _ENABLED,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);

        //
        // Initialize the device index for beacon interrupt to invalid.
        // The power device which will service beacon interrupts will populate
        // this index to a valid value during construct.
        //
        PMGR_PWR_DEV_IDX_BEACON_INIT(&Pmgr);
    }
}

/*!
 * Specific handler-routine for dealing with beacon interrupts.
 */
void
pmgrPwrDevBeaconInterruptService_GA10X(void)
{
    DISPATCH_PMGR   disp2Pmgr = {{ 0 }};
    LwU32           reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);

    if ((FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON1_INTR,
                     _PENDING, reg32)) ||
        (FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON2_INTR,
                     _PENDING, reg32)))
    {
        //
        // Disable beacon interrupts while it is being serviced
        // Event handler will ensure to enable these interrupts after servicing
        //
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON1_INTR, _DISABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON2_INTR, _DISABLED,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);

        disp2Pmgr.hdr.eventType = PMGR_EVENT_ID_PWR_DEV_BEACON_INTERRUPT;
        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, PMGR),
                &disp2Pmgr, sizeof(disp2Pmgr));
    }
}

/*!
 * Specific handler-routine for clearing and re-enabling beacon interrupts.
 */
void
pmgrPwrDevBeaconInterruptClearAndReenable_GA10X(void)
{
    LwU32 reg32;

    // Clear the pending beacon interrupts
    REG_WR32(CSB, LW_CPWR_THERM_INTR_0,
        (DRF_DEF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON1_INTR, _CLEAR) |
         DRF_DEF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON2_INTR, _CLEAR)));

    appTaskCriticalEnter();
    {
        // Re-enable beacon interrupts
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0,
                _ISENSE_BEACON1_INTR, _ENABLED, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0,
                _ISENSE_BEACON2_INTR, _ENABLED, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);
    }
    appTaskCriticalExit();
}
