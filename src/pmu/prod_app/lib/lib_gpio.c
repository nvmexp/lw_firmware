/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_gpio.c
 * @brief  GPIO-Related Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objpmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_gpio.h"

#include "g_pmurpc.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJGPIO Gpio;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initialize the GPIO object from INIT overlay.
 */
void
gpioPreInit(void)
{
    memset(&Gpio, 0, sizeof(OBJGPIO));

    /*
     * WAR for bug 3153224, VBIOS patching for GPIO_INPUT_CNTL registers.
     * Refer for details: https://confluence.lwpu.com/display/BS/Bug+3153224:+LW_PMGR_GPIO_INPUT_CNTL+BYPASS_FILTER+related+WAR
     */
    pmgrGpioPatchBug3153224_HAL(&Pmgr);
}

/*!
 * @brief   Handler for GPIO_INIT_CFG RPC.
 */
FLCN_STATUS
pmuRpcPmgrGpioInitCfg
(
    RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG *pParams
)
{
    if (Gpio.pGpioTable == NULL)
    {
        Gpio.pGpioTable = lwosCallocResidentType(pParams->gpioPinCnt,
                                                 RM_PMU_PMGR_GPIO_ENTRY_STRUCT);
        if (Gpio.pGpioTable == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    // Copy in the set of GPIO entries.
    Gpio.gpioPinCnt = pParams->gpioPinCnt;
    memcpy(Gpio.pGpioTable, pParams->gpio,
           pParams->gpioPinCnt * sizeof(RM_PMU_PMGR_GPIO_ENTRY_STRUCT));

    return FLCN_OK;
}

/*!
 * Obtains the current GPIO state i.e. whether GPIO is ON/OFF.
 */
FLCN_STATUS
gpioGetState
(
    LwU8    gpioPin,
    LwBool *pBState
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    // Sanity check.
    if (gpioPin >= GPIO_PIN_COUNT_GET())
    {
        PMU_BREAKPOINT();
        goto gpioGetState_exit;
    }

    // Query PMGR for the state of the GPIO.
    status = pmgrGpioGetState_HAL(&Pmgr, gpioPin, pBState);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto gpioGetState_exit;
    }

gpioGetState_exit:
    return status;
}

/*!
 * Set GPIO state of each GPIO in the list to ON/OFF.
 */
FLCN_STATUS
gpioSetStateList
(
    LwU8            gpioPinCnt,
    GPIO_LIST_ITEM *pList,
    LwBool          bTrigger
)
{
    FLCN_STATUS status       = FLCN_ERR_ILWALID_ARGUMENT;
    LwBool      bAcquire     = LW_FALSE;
    LwU8        mutexTokenId = LW_U8_MAX;
    LwU8        i;

    // Sanity check GPIO gpioPinCnt.
    if ((gpioPinCnt == 0) ||
        (gpioPinCnt > GPIO_PIN_COUNT_GET()))
    {
        PMU_BREAKPOINT();
        goto gpioSetStateList_exit;
    }

    // Sanity check GPIO list.
    for (i = 0; i < gpioPinCnt; i++)
    {
        // If any list item contains invalid GPIO, exit.
        if (pList[i].gpioPin >= GPIO_PIN_COUNT_GET())
        {
            PMU_BREAKPOINT();
            goto gpioSetStateList_exit;
        }
    }

    // Acquire GPIO mutex.
    status = gpioMutexAcquire(&mutexTokenId);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto gpioSetStateList_exit;
    }
    bAcquire = LW_TRUE;

    // Actually change the state of GPIOs.
    status = pmgrGpioSetStateList_HAL(&Pmgr, gpioPinCnt, pList, bTrigger);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto gpioSetStateList_exit;
    }

gpioSetStateList_exit:

    // Release GPIO mutex, if acquired.
    if (bAcquire)
    {
        (void)gpioMutexRelease(mutexTokenId);
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
