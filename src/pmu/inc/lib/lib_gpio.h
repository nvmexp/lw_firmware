/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-20201 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_GPIO_H
#define LIB_GPIO_H

/*!
 * @file    lib_gpio.h
 * @copydoc lib_gpio.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "lib_mutex.h"

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure containing GPIO config data for GPIO_HALINDEX_INTERNAL type.
 * The config data would be used to set GPIO pin state i.e. ON/OFF in PMU.
 */
typedef struct
{
    LwU8                            gpioPinCnt; //<! GPIO pin count
    RM_PMU_PMGR_GPIO_ENTRY_STRUCT  *pGpioTable; //<! GPIO config data
} OBJGPIO;

/*!
 * Structure containing the target state (ON/OFF) of the corresponding GPIO
 * pin. Clients need to pack several items in the list for simultaneously
 * changing state of multiple GPIO pins.
 */
typedef struct
{
    LwU8    gpioPin;    //<! GPIO pin number
    LwBool  bState;     //<! LW_TRUE for ON and LW_FALSE for OFF state
} GPIO_LIST_ITEM;

/* ------------------------ External Definitions --------------------------- */
extern OBJGPIO Gpio;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Timeout value (in ns) that PMU will use when attempting to acquire the GPIO
 * PMU mutex. Timeout value (in ms) is 200.
 */
#define GPIO_MUTEX_ACQUIRE_TIMEOUT_NS                              (200000000)

#define gpioMutexAcquire(tk)    mutexAcquire(LW_MUTEX_ID_GPIO,                 \
                                       GPIO_MUTEX_ACQUIRE_TIMEOUT_NS, tk)

#define gpioMutexRelease(tk)    mutexRelease(LW_MUTEX_ID_GPIO, tk)

/*!
 * Macro to retrieve GPIO pin count.
 */
#define GPIO_PIN_COUNT_GET()    (Gpio.gpioPinCnt)

/*!
 * Retrieve input HW enum from RM_PMU_PMGR_GPIO_ENTRY_STRUCT using the
 * GPIO pin number.
 *
 * @param[in]  gpioPin    GPIO pin number
 */
#define GPIO_INPUT_HW_ENUM_GET(gpioPin)                 \
    (Gpio.pGpioTable[(gpioPin)].inputHwEnum)

/*!
 * Retrieve output HW enum from RM_PMU_PMGR_GPIO_ENTRY_STRUCT using the
 * GPIO pin number.
 *
 * @param[in]  gpioPin    GPIO pin number
 */
#define GPIO_OUTPUT_HW_ENUM_GET(gpioPin)                \
    (Gpio.pGpioTable[(gpioPin)].outputHwEnum)

/*!
 * Retrieve flags from RM_PMU_PMGR_GPIO_ENTRY_STRUCT using the GPIO pin number.
 *
 * @param[in]  gpioPin    GPIO pin number
 */
#define GPIO_FLAGS_GET(gpioPin)                         \
    (Gpio.pGpioTable[(gpioPin)].flags)

/* ------------------------ Function Prototypes ---------------------------- */
void    gpioPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "gpioPreInit");

FLCN_STATUS gpioGetState(LwU8 gpioPin, LwBool *pBState)
    GCC_ATTRIB_SECTION("imem_libGpio", "gpioGetState");

FLCN_STATUS gpioSetStateList(LwU8 gpioPinCnt, GPIO_LIST_ITEM *pList, LwBool bTrigger)
    GCC_ATTRIB_SECTION("imem_libGpio", "gpioSetStateList");

#endif // LIB_GPIO_H
