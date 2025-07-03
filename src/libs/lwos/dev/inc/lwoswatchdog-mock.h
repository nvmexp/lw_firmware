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
 * @file       lwoswatchdog-mock.h
 *
 * @brief      Mock declarations for @ref lwoswatchdog.h declarations
 *             tasks.
 */

#ifndef LWOSWATCHDOG_MOCK_H
#define LWOSWATCHDOG_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "fff.h"
#include "lwoswatchdog.h"

/* ------------------------- Enum Definitions ------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief    @ref lwosWatchdogAddItemFromISR mocking configuration.
 */
typedef struct
{
    /*!
     * @brief  Mocking status to return when `bOverride` is set.
     */
    FLCN_STATUS status;
    /*!
     * @brief    If need to override with the mocking value.
     * @details  Based on `bOverride` value, the mocking function will:
     *           - LW_TRUE: directly return mocking value.
     *           - LW_FALSE(default): continue with `_IMPL`.
     */
    LwBool bOverride;
} LWOS_WATCHDOG_ADD_ITEM_FROM_ISR_MOCK_CONFIG;

/* ------------------------ External Definitions ---------------------------- */
extern LWOS_WATCHDOG_ADD_ITEM_FROM_ISR_MOCK_CONFIG
    lwosWatchdogAddItemFromISR_MOCK_CONFIG;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Initializes the LWOS Watchdog mocking state.
 */
void lwosWatchdogMockInit(void);

/* ------------------------- Mocked Functions -------------------------------- */
/*!
 * @copydoc lwosWatchdogAddItemFromISR
 *
 * @note    Mock implementation of @ref lwosWatchdogAddItemFromISR
 */
FLCN_STATUS lwosWatchdogAddItemFromISR_MOCK(const LwU8 taskId);

/*!
 * @copydoc lwosWatchdogCallbackExpired
 *
 * @note    Mock implementation of @ref lwosWatchdogCallbackExpired
 */
DECLARE_FAKE_VOID_FUNC(lwosWatchdogCallbackExpired_MOCK);

/* ------------------------- Defines ---------------------------------------- */

#endif // LWOSWATCHDOG_MOCK_H
