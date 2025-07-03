/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file     lwostmrcallback-mock.c
 * @details  This file contains all the mocking functions needed for LWOS timer
 *           callback.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwostmrcallback.h"
#include "lwostmrcallback-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
OS_TMR_CALLBACK_CREATE_MOCK_CONFIG osTmrCallbackCreate_MOCK_CONFIG =
{
    .status = FLCN_OK,
    .bOverride = LW_FALSE
};

OS_TMR_CALLBACK_SCHEDULE_MOCK_CONFIG osTmrCallbackSchedule_MOCK_CONFIG =
{
    .status = FLCN_OK,
    .bOverride = LW_FALSE
};

OS_TMR_CALLBACK_UPDATE_MOCK_CONFIG osTmrCallbackUpdate_MOCK_CONFIG =
{
    .status = FLCN_OK,
    .bOverride = LW_FALSE
};

OS_TMR_CALLBACK_EXELWTE_MOCK_CONFIG osTmrCallbackExelwte_MOCK_CONFIG =
{
    .status = FLCN_OK,
    .bOverride = LW_FALSE
};

OS_TMR_CALLBACK_CANCEL_MOCK_CONFIG osTmrCallbackCancel_MOCK_CONFIG =
{
    .returlwalue = LW_TRUE,
    .bOverride = LW_FALSE
};

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Local Variables --------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Mocked Functions -------------------------------- */
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
)
{
    if (osTmrCallbackCreate_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return osTmrCallbackCreate_MOCK_CONFIG.status;
    }
    else
    {
        return osTmrCallbackCreate_IMPL(pCallback, type, ovlImem,
            pTmrCallbackFunc, queueHandle, periodNormalUs, periodSleepUs, bRelaxedUseSleep,
            taskId);
    }
}

FLCN_STATUS osTmrCallbackSchedule_MOCK
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (osTmrCallbackSchedule_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return osTmrCallbackSchedule_MOCK_CONFIG.status;
    }
    else
    {
        return osTmrCallbackSchedule_IMPL(pCallback);
    }
}

FLCN_STATUS osTmrCallbackUpdate_MOCK
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            periodNewNormalUs,
    LwU32            periodNewSleepUs,
    LwBool           bRelaxedUseSleep,
    LwBool           bUpdateUseLwrrent 
)
{
    if (osTmrCallbackUpdate_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return osTmrCallbackUpdate_MOCK_CONFIG.status;
    }
    else
    {
        return osTmrCallbackUpdate_IMPL(pCallback, periodNewNormalUs,
            periodNewSleepUs, bRelaxedUseSleep, bUpdateUseLwrrent);
    }
}

FLCN_STATUS osTmrCallbackExelwte_MOCK
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (osTmrCallbackExelwte_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return osTmrCallbackExelwte_MOCK_CONFIG.status;
    }
    else
    {
        return osTmrCallbackExelwte_IMPL(pCallback);
    }
}

LwBool osTmrCallbackCancel_MOCK
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (osTmrCallbackCancel_MOCK_CONFIG.bOverride == LW_TRUE)
    {
        return osTmrCallbackCancel_MOCK_CONFIG.returlwalue;
    }
    else
    {
        return osTmrCallbackCancel_IMPL(pCallback);
    }
}
