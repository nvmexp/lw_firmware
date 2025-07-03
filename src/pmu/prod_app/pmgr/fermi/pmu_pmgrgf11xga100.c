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
 * @file   pmu_pmgrgf11xga100.c
 * @brief  Contains all PCB Management (PGMR) routines specific to GF11X till GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lib/lib_gpio.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
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
pmgrGpioGetState_GMXXX
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
        reg32   = REG_RD32(FECS, LW_PMGR_GPIO_OUTPUT_CNTL(gpioPin));
        bOnData = FLD_TEST_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_INPUT, _1, reg32);
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
pmgrGpioSetStateList_GMXXX
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

        reg32 = REG_RD32(FECS, LW_PMGR_GPIO_OUTPUT_CNTL(gpioPin));

        bSelNormal = (GPIO_OUTPUT_HW_ENUM_GET(gpioPin) ==
                        LW_PMGR_GPIO_OUTPUT_CNTL_SEL_NORMAL);

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
            reg32 = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _1, reg32);
        }
        else
        {
            reg32 = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _0, reg32);
        }

        // Determines whether the GPIO will be enabled for output or not.
        if (bSelNormal)
        {
            if (bIoOutEn)
            {
                reg32 = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _NO, reg32);
            }
            else
            {
                reg32 = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, reg32);
            }
        }

        REG_WR32(FECS, LW_PMGR_GPIO_OUTPUT_CNTL(gpioPin), reg32);
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
