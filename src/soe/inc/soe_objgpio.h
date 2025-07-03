/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJGPIO_H
#define SOE_OBJGPIO_H

/*!
 * @file    soe_objgpio.h 
 * @copydoc soe_objgpio.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_gpio_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
typedef struct
{
    LwU32   pin;
    LwU32   function;
    LwU32   hw_select;
    LwU32   misc;
} LWSWITCH_GPIO_INFO;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern GPIO_HAL_IFACES GpioHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */

//
// TODO: Move these macros to a shared folder between the driver and SOE.
//

#define LWSWITCH_GPIO_ENTRY_FUNCTION                 7:0
#define LWSWITCH_GPIO_ENTRY_FUNCTION_THERMAL_EVENT    17
#define LWSWITCH_GPIO_ENTRY_FUNCTION_OVERTEMP         35
#define LWSWITCH_GPIO_ENTRY_FUNCTION_THERMAL_ALERT    52
#define LWSWITCH_GPIO_ENTRY_FUNCTION_THERMAL_CRITICAL 53
#define LWSWITCH_GPIO_ENTRY_FUNCTION_POWER_ALERT      76
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID0    209
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID1    210
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID2    211
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID3    212
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID4    213
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID5    214
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID6    215
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID7    216
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID8    217
#define LWSWITCH_GPIO_ENTRY_FUNCTION_INSTANCE_ID9    218
#define LWSWITCH_GPIO_ENTRY_FUNCTION_SKIP_ENTRY      255

#define LWSWITCH_GPIO_ENTRY_OUTPUT                   7:0

#define LWSWITCH_GPIO_ENTRY_INPUT_HW_SELECT                 4:0
#define LWSWITCH_GPIO_ENTRY_INPUT_HW_SELECT_NONE              0
#define LWSWITCH_GPIO_ENTRY_INPUT_HW_SELECT_THERMAL_ALERT    22
#define LWSWITCH_GPIO_ENTRY_INPUT_HW_SELECT_POWER_ALERT      23
#define LWSWITCH_GPIO_ENTRY_INPUT_GSYNC                     5:5
#define LWSWITCH_GPIO_ENTRY_INPUT_OPEN_DRAIN                6:6
#define LWSWITCH_GPIO_ENTRY_INPUT_PWM                       7:7
//#define LWSWITCH_GPIO_ENTRY_INPUT_3V3                ?:?

#define LWSWITCH_GPIO_ENTRY_MISC_LOCK                   3:0
#define LWSWITCH_GPIO_ENTRY_MISC_IO                     7:4
#define LWSWITCH_GPIO_ENTRY_MISC_IO_UNUSED              0x0
#define LWSWITCH_GPIO_ENTRY_MISC_IO_ILW_OUT             0x1
#define LWSWITCH_GPIO_ENTRY_MISC_IO_ILW_OUT_TRISTATE    0x3
#define LWSWITCH_GPIO_ENTRY_MISC_IO_OUT                 0x4
#define LWSWITCH_GPIO_ENTRY_MISC_IO_IN_STEREO_TRISTATE  0x6
#define LWSWITCH_GPIO_ENTRY_MISC_IO_ILW_OUT_TRISTATE_LO 0x9
#define LWSWITCH_GPIO_ENTRY_MISC_IO_ILW_IN              0xB
#define LWSWITCH_GPIO_ENTRY_MISC_IO_OUT_TRISTATE        0xC
#define LWSWITCH_GPIO_ENTRY_MISC_IO_IN                  0xE

#define LWSWITCH_I2C_VERSION        0x40
#define LWSWITCH_MAX_GPIO_PINS        25

//
// PMGR board configuration information
//

#define LWSWITCH_DESCRIBE_GPIO_PIN(_pin, _func, _hw_select, _misc_io)    \
    {_pin, LWSWITCH_GPIO_ENTRY_FUNCTION ## _func, _hw_select,            \
        LWSWITCH_GPIO_ENTRY_MISC_IO_ ## _misc_io}

/* ------------------------ Public Functions ------------------------------- */
void constructGpio(void) GCC_ATTRIB_SECTION("imem_init", "constructGpio");
void gpioPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "gpioPreInit");

#endif // SOE_OBJGPIO_H
