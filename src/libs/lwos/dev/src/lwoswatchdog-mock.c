/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwoswatch-mock.c
 *
 * @brief  Mock implementations of the @ref lwoswatchdog.c functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwoswatchdog.h"
#include "lwoswatchdog-mock.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
LWOS_WATCHDOG_ADD_ITEM_FROM_ISR_MOCK_CONFIG
lwosWatchdogAddItemFromISR_MOCK_CONFIG =
{
    .status = FLCN_OK,
    .bOverride = LW_FALSE
};

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS lwosWatchdogAddItemFromISR_MOCK(const LwU8 taskId)
{
    if (lwosWatchdogAddItemFromISR_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return lwosWatchdogAddItemFromISR_MOCK_CONFIG.status;
    }
    else
    {
        return lwosWatchdogAddItemFromISR_IMPL(taskId);
    }
}

void
lwosWatchdogMockInit(void)
{
    RESET_FAKE(lwosWatchdogCallbackExpired_MOCK);
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VOID_FUNC(lwosWatchdogCallbackExpired_MOCK);

/* ------------------------- Static Functions  ------------------------------ */
