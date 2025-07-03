/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwostmrcallback-mock.h
 * @brief   LWOS timer callback mocking library.
 */

#ifndef LWOSTMRCALLBACK_MOCK_H
#define LWOSTMRCALLBACK_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "lwostmrcallback.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief    @ref osTmrCallbackCreate mocking configuration.
 * @details  Share same mechanism with:
 *           - @ref OS_TMR_CALLBACK_SCHEDULE_MOCK_CONFIG
 *           - @ref OS_TMR_CALLBACK_UPDATE_MOCK_CONFIG
 *           - @ref OS_TMR_CALLBACK_EXELWTE_MOCK_CONFIG
 *           - @ref OS_TMR_CALLBACK_CANCEL_MOCK_CONFIG
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
} OS_TMR_CALLBACK_CREATE_MOCK_CONFIG;

/*!
 * @brief  Similar to @ref OS_TMR_CALLBACK_CREATE_MOCK_CONFIG.
 */
typedef struct
{
    FLCN_STATUS status;
    LwBool bOverride;
} OS_TMR_CALLBACK_SCHEDULE_MOCK_CONFIG;

/*!
 * @brief  Similar to @ref OS_TMR_CALLBACK_CREATE_MOCK_CONFIG.
 */
typedef struct
{
    FLCN_STATUS status;
    LwBool bOverride;
} OS_TMR_CALLBACK_UPDATE_MOCK_CONFIG;

/*!
 * @brief  Similar to @ref OS_TMR_CALLBACK_CREATE_MOCK_CONFIG.
 */
typedef struct
{
    FLCN_STATUS status;
    LwBool bOverride;
} OS_TMR_CALLBACK_EXELWTE_MOCK_CONFIG;

/*!
 * @brief  Similar to @ref OS_TMR_CALLBACK_CREATE_MOCK_CONFIG.
 */
typedef struct
{
    LwBool returlwalue;
    LwBool bOverride;
} OS_TMR_CALLBACK_CANCEL_MOCK_CONFIG;

/* ------------------------ External Definitions ---------------------------- */
extern OS_TMR_CALLBACK_CREATE_MOCK_CONFIG osTmrCallbackCreate_MOCK_CONFIG;
extern OS_TMR_CALLBACK_SCHEDULE_MOCK_CONFIG osTmrCallbackSchedule_MOCK_CONFIG;
extern OS_TMR_CALLBACK_UPDATE_MOCK_CONFIG osTmrCallbackUpdate_MOCK_CONFIG;
extern OS_TMR_CALLBACK_EXELWTE_MOCK_CONFIG osTmrCallbackExelwte_MOCK_CONFIG;
extern OS_TMR_CALLBACK_CANCEL_MOCK_CONFIG osTmrCallbackCancel_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc  osTmrCallbackCreate.
 * @details  Mock implementation of @ref osTmrCallbackCreate .
 */
FLCN_STATUS osTmrCallbackCreate_MOCK
(
    OS_TMR_CALLBACK     *pCallback,
    OS_TMR_CALLBACK_TYPE type,
    LwU8                 ovlImem,
    OsTmrCallbackFunc  (*pTmrCallbackFunc),
    LwrtosQueueHandle    queueHandle,
    LwU32                periodNormalUs,
    LwU32                periodSleepUs,
    LwBool               bRelaxedUseSleep,
    LwU8                 taskId
);

/*!
 * @copydoc  osTmrCallbackSchedule.
 * @details  Mock implementation of @ref osTmrCallbackSchedule .
 */
FLCN_STATUS osTmrCallbackSchedule_MOCK
(
    OS_TMR_CALLBACK *pCallback
);

/*!
 * @copydoc  osTmrCallbackUpdate.
 * @details  Mock implementation of @ref osTmrCallbackUpdate .
 */
FLCN_STATUS osTmrCallbackUpdate_MOCK
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            periodNewNormalUs,
    LwU32            periodNewSleepUs,
    LwBool           bRelaxedUseSleep,
    LwBool           bUpdateUseLwrrent
);

/*!
 * @copydoc  osTmrCallbackExelwte.
 * @details  Mock implementation of @ref osTmrCallbackExelwte .
 */
FLCN_STATUS osTmrCallbackExelwte_MOCK
(
    OS_TMR_CALLBACK *pCallback
);

/*!
 * @copydoc  osTmrCallbackCancel.
 * @details  Mock implementation of @ref osTmrCallbackCancel .
 */
LwBool osTmrCallbackCancel_MOCK
(
    OS_TMR_CALLBACK *pCallback
);

/* ------------------------ Include Derived Types --------------------------- */

#endif // LWOSTMRCALLBACK_MOCK_H
