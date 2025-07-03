/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ei_callback.c
 * @brief  Engine Idle Framework Callback Infrastructure code
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objei.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "main.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

// Timer Callback Structure
OS_TMR_CALLBACK OsCBEi;

/* ------------------------- Prototypes --------------------------------------*/
static void              s_eiCallbackSchedule(void)
                         GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiCallbackSchedule");
static void              s_eiCallbackDeschedule(void)
                         GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiCallbackDeschedule");
static OsTmrCallbackFunc (s_eiHandleOsCallback)
                         GCC_ATTRIB_USED()
                         GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiHandleOsCallback");

/* ------------------------- Private Variables ------------------------------ */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize the centralized EI callback parameters
 *
 * @return  FLCN_OK                 On success
 * @return  FLCN_ERR_NO_FREE_MEM    If malloc() fails
 */
FLCN_STATUS
eiCallbackParamsInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    Ei.pEiCallback = lwosCallocType(OVL_INDEX_DMEM(lpwrLp), 1, EI_CALLBACK);
    if (Ei.pEiCallback == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;

        goto eiCallbackInit_exit;
    }

    // Initialization the central (base) callback period from VBIOS
    Ei.pEiCallback->basePeriodMs = Lpwr.pVbiosData->ei.clientCallbackBaseMs;

eiCallbackInit_exit:
    return status;
}

/*!
 * @brief Schedule EI Central callback
 *
 * @param[in] ctientMask  EI Client ID which needs this callback
 */
void
eiCallbackSchedule(void)
{
    // Schedule the callback
    s_eiCallbackSchedule();
}

/*!
 * @brief Deschedule EI Central callback
 *
 * @param[in] ctrlId    LPWR callback controller ID
 */
void
eiCallbackDeschedule(void)
{
    EI_CALLBACK *pEiCallback = EI_GET_CALLBACK();

    // Deschedule centralized callback
    s_eiCallbackDeschedule();

    // Clear all client scheduled bit mask
    Ei.clientCallbackScheduleMask = 0;

    // Rest the base count of callback
    pEiCallback->baseCount = 0;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @ref OsTmrCallback
 *
 * @brief   Handles centralised EI callback
 *
 * @param[in] pEiCallback    Pointer to OS_TMR_CALLBACK
 */
static FLCN_STATUS
s_eiHandleOsCallback
(
    OS_TMR_CALLBACK *pOsCallback
)
{
    EI_CALLBACK    *pEiCallback = EI_GET_CALLBACK();
    EI_CLIENT      *pEiClient   = NULL;
    FLCN_STATUS     status      = FLCN_OK;
    LwU32           clientId;

    //
    // Increment the base count to record how many times the callback
    // has exectuted as per base period.
    //
    pEiCallback->baseCount++;

    //
    // Sanity check to make sure EI FSM is still engaged.
    // If its not engaged, just bail out. There is no need
    // to send any notification.
    //
    if (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_EI))
    {
        goto s_eiHandleOsCallback_exit;
    }

    // Loop over all the client which has requested the delayed notification
    FOR_EACH_INDEX_IN_MASK(32, clientId, Ei.clientCallbackScheduleMask)
    {
        pEiClient = EI_GET_CLIENT(clientId);

        //
        // Send the Entry notification to client if base count is overflowing for
        // the client.
        //
        if (pEiCallback->baseCount >= pEiClient->baseMultiplier)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                eiNotificationSend(clientId, LPWR_EI_NOTIFICATION_ENTRY_DONE),
                s_eiHandleOsCallback_exit);

            //
            // Remove this client from the clientCallbackScheduleMask since we have
            // send the notification to this client. Next time again callback expries
            // we do not need to send entry notification to this client again.
            //
            Ei.clientCallbackScheduleMask &= (~BIT(clientId));
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Once we have served all the clients, there is no need of this central
    // callback to keep running. Therefore if clientScheduleMask is zero,
    // cancel/deschedule the callback.
    //
    // Callback will get scheduled once again we we get next ENTRY EVENT from
    // EI FSM.
    //
    if (Ei.clientCallbackScheduleMask == 0)
    {
        eiCallbackDeschedule();
    }

s_eiHandleOsCallback_exit:
    return status;
}

/*!
 * @brief Schedule EI_CALLBACK
 */
static void
s_eiCallbackSchedule(void)
{
    EI_CALLBACK *pEiCallback = EI_GET_CALLBACK();

    if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBEi))
    {
        osTmrCallbackCreate(&OsCBEi,                                    // EI TIMER callback structure
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                OVL_INDEX_ILWALID,                                      // ovlImem
                s_eiHandleOsCallback,                                   // pTmrCallbackFunc
                LWOS_QUEUE(PMU, LPWR_LP),                               // queueHandle
                (pEiCallback->basePeriodMs * 1000),                     // periodNormalus
                (pEiCallback->basePeriodMs * 1000),                     // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_PMU_TASK_ID_LPWR_LP);                                // taskId
    }
    osTmrCallbackSchedule(&OsCBEi);
}

/*!
 * @brief Deschedule EI_CALLBACK
 */
static void
s_eiCallbackDeschedule(void)
{
    osTmrCallbackCancel(&OsCBEi);
}
