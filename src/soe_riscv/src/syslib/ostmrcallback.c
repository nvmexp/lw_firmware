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
 * @file    ostmrcallback.c
 * @brief   OS_TMR_CALLBACK hooks.
 */

/* ------------------------ System Includes --------------------------------- */
//#include "soesw.h"
#include "unit_dispatch.h"
#include "soesw.h"
/* ------------------------ Application Includes ---------------------------- */
#include "lwostmrcallback.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Private Functions ------------------------------- */
/* ------------------------ Implementation ---------------------------------- */
/*!
 * @brief   Notifies respective task that the callback requires servicing.
 *
 * @param[in]   pCallback   Handle of the callback that requires servicing.
 *
 * @return  Boolean if task notification has succeeded.
 *
 * @pre Caller is responsible to assure that pCallback is not NULL.
 */
sysSYSLIB_CODE LwBool
osTmrCallbackNotifyTaskISR
(
    OS_TMR_CALLBACK* pCallback
)
{
    DISP2UNIT_OS_TMR_CALLBACK callback;
    FLCN_STATUS               status;
    LwBool                    bSuccess = LW_FALSE;

    callback.eventType = DISP2UNIT_EVT_OS_TMR_CALLBACK;
    callback.pCallback = pCallback;

    status = lwrtosQueueSendFromISRWithStatus(pCallback->queueHandle, &callback,
                                              sizeof(callback));
    if (FLCN_OK == status)
    {
        bSuccess = LW_TRUE;
    }
    else if (FLCN_ERR_TIMEOUT != status)
    {
        SOE_BREAKPOINT();
    }

    return bSuccess;
}

/*!
 * @brief  Returns the max number of callbacks that can be registered
 */
LwU32
osTmrCallbackMaxCountGet()
{
    LwU32 cbCount = 0;

    if (SOECFG_FEATURE_ENABLED(SOETASK_SMBPBI))
        cbCount++;

    if (SOECFG_FEATURE_ENABLED(SOETASK_THERM))
        cbCount++;

    if (SOECFG_FEATURE_ENABLED(SOETASK_CORE))
        cbCount++;

    return cbCount;
}
