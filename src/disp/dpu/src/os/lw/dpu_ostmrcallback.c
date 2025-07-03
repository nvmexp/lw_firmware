/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dpu_ostmrcallback.c
 * @brief   DPU falcon specific OS_TMR_CALLBACK hooks.
 */

/* ------------------------ System Includes --------------------------------- */
#include "dpusw.h"
#include "unit_dispatch.h"
#include "dpu_objic.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwostmrcallback.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Private Functions ------------------------------- */
/* ------------------------ Implementation ---------------------------------- */
/*!
 * @brief   Retrieves current "callback mode" and only support normal mode now.
 *
 * @return  Lwrent "callback mode" as OS_TMR_CALLBACK_MODE_<xyz>
 */
LwU32
osTmrCallbackModeGetISR(void)
{
    return OS_TMR_CALLBACK_MODE_NORMAL;
}

/*!
 * @brief   Notifies respective task that the callback requires servicing.
 *
 * @param[in]   pCallback   Handle of the callback that requires servicing.
 *
 * @return  Boolean if task notification has succeeded.
 *
 * @pre Caller is responsible to assure that pCallback is not NULL.
 */
LwBool
osTmrCallbackNotifyTaskISR
(
    OS_TMR_CALLBACK *pCallback
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
        DPU_HALT();
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

#if DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED)
    cbCount++;
#endif

    return cbCount;
}
