/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
* @file    soe_positionlr10.c
* @brief   SOE HAL functions for LR10 to identify device positional id.
*/

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "soe_objgpio.h"
#include "soe_bar0.h"
#include "dev_pmgr.h"
#include "config/g_soe_private.h"

/* ------------------------ Static Variables ------------------------------- */

//
// The defaults need to be updated/removed when EPROM reading support is added.
// Tracked in bug 2659754.
//

static const LWSWITCH_GPIO_INFO lwswitch_gpio_pin_Defaults[] =
{
    LWSWITCH_DESCRIBE_GPIO_PIN( 0, _INSTANCE_ID0,   0, IN),          // Instance ID bit 0
    LWSWITCH_DESCRIBE_GPIO_PIN( 1, _INSTANCE_ID1,   0, IN),          // Instance ID bit 1
    LWSWITCH_DESCRIBE_GPIO_PIN( 2, _INSTANCE_ID2,   0, IN),          // Instance ID bit 2
    LWSWITCH_DESCRIBE_GPIO_PIN( 3, _INSTANCE_ID3,   0, IN),          // Instance ID bit 3
    LWSWITCH_DESCRIBE_GPIO_PIN( 4, _INSTANCE_ID4,   0, IN),          // Instance ID bit 4
    LWSWITCH_DESCRIBE_GPIO_PIN( 5, _INSTANCE_ID5,   0, IN),          // Instance ID bit 5
    LWSWITCH_DESCRIBE_GPIO_PIN( 6, _INSTANCE_ID6,   0, IN),          // Instance ID bit 6
};

static const LwU32 lwswitch_gpio_pin_Default_size = LW_ARRAY_ELEMENTS(lwswitch_gpio_pin_Defaults);

//
// TODO: The GPIO settings must be read from DCB/Firmware.
// If these GPIO's are not present in firmware, fall back to defaults.
// Tracked in bug 2659754.
//

LwU32
soeReadGpuPositionId_LR10()
{
    LwU32 physical_id = -1;
    LwU32 data;
    LwU32 idx_gpio;
    LwU32 input_ilw;
    LwU32 function_offset;
    LWSWITCH_GPIO_INFO *gpio_pin;

    gpio_pin = (LWSWITCH_GPIO_INFO*)lwswitch_gpio_pin_Defaults;
    for (idx_gpio = 0; idx_gpio < lwswitch_gpio_pin_Default_size; idx_gpio++)
    {
        if ((gpio_pin[idx_gpio].function >= LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID0) &&
            (gpio_pin[idx_gpio].function <= LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID6))
        {
            if (gpio_pin[idx_gpio].misc == LWSWITCH_GPIO_ENTRY_MISC_IO_ILW_IN)
            {
                input_ilw = LW_PMGR_GPIO_INPUT_CNTL_1_ILW_YES;
            }
            else
            {
                input_ilw = LW_PMGR_GPIO_INPUT_CNTL_1_ILW_NO;
            }

            BAR0_REG_WR32(LW_PMGR_GPIO_INPUT_CNTL_1,
                DRF_NUM(_PMGR, _GPIO_INPUT_CNTL_1, _PINNUM, gpio_pin[idx_gpio].pin) |
                DRF_NUM(_PMGR, _GPIO_INPUT_CNTL_1, _ILW, input_ilw) |
                DRF_DEF(_PMGR, _GPIO_INPUT_CNTL_1, _BYPASS_FILTER, _NO));

            data = BAR0_REG_RD32(LW_PMGR_GPIO_INPUT_CNTL_1);
            function_offset = gpio_pin[idx_gpio].function -
                          LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID0;
            physical_id |=
                (DRF_VAL(_PMGR, _GPIO_INPUT_CNTL_1, _READ, data) << function_offset);
        }
    }

    return physical_id;
}

