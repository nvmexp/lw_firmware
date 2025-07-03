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
 * @file   ei_client.c
 * @brief  EI Client Specific Code
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objei.h"
#include "main.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes --------------------------------------*/
static FLCN_STATUS s_eiPmuClientNotificationSend(LwU8 clientId, LwU8 notificationType)
                   GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiPmuClientNotificationSend");
static FLCN_STATUS s_eiRmClientNotificationSend(LwU8 clientId, LwU8 notificationType)
                   GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiRmClientNotificationSend");

/* ------------------------- Private Variables ------------------------------ */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize the EI(Engine Idle Framework) Client
 *
 * @param[in]   clientId        EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   baseMultiplier  Client's base multiplier for notification callback
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 */
FLCN_STATUS
eiClientInit
(
    LwU8  clientId
)
{
    EI_CLIENT   *pEiClient = EI_GET_CLIENT(clientId);
    FLCN_STATUS  status    = FLCN_OK;

    //
    // Skip the initialization if EI Client is already initialized. This
    // might be a duplicate call.
    //
    if ((pEiClient != NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto eiClientInit_exit;
    }

    // Allocate the client structure in LPWR_LP DMEM overlay
    pEiClient = lwosCallocType(OVL_INDEX_DMEM(lpwrLp), 1, EI_CLIENT);
    if (pEiClient == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;

        goto eiClientInit_exit;
    }

    // Save the client pointer in EI data structure
    Ei.pEiClient[clientId] = pEiClient;

    // Based on client id update the client information
    switch (clientId)
    {
        case RM_PMU_LPWR_EI_CLIENT_ID_PMU_LPWR:
        {
            pEiClient->taskQueueId   = LWOS_QUEUE_ID(PMU, LPWR);
            pEiClient->taskEventType = LPWR_EVENT_ID_EI_NOTIFICATION;
            pEiClient->bIsPmuClient  = LW_TRUE;

            break;
        }

        case RM_PMU_LPWR_EI_CLIENT_ID_RM:
        {
            pEiClient->bIsPmuClient = LW_FALSE;

            break;
        }

        default:
        {

            // All the new client added should have a valid switch case here.

            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;

            goto eiClientInit_exit;
        }
    }

    // Set the client's base multiplier from the VBIOS
    pEiClient->baseMultiplier =
      (LwU32)Lpwr.pVbiosData->ei.clientEntry[clientId].clientCallbackMultiplier;

    // Set the client supportMask
    Ei.clientSupportMask |= BIT(clientId);

eiClientInit_exit:

    return status;
}

/*!
 * @brief Enable/Disable the EI Client's Notification
 *
 * @param[in]   clientId      EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   bEnable       LW_TRUE ->  Enable the Notification
 *                            LW_FALSE -> Disable the notification
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Invalid request
 */
FLCN_STATUS
eiClientNotificationEnable
(
    LwU8   clientId,
    LwBool bEnable
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // If EI Client is not supported, then return
    // early and send the error
    //
    if (!EI_IS_CLIENT_SUPPORTED(clientId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto eiClientNotificationEnable_exit;
    }

    if (bEnable)
    {
        //
        // If Notification is already enabled, return early
        // with error as this is not expected.
        //
        if (EI_IS_CLIENT_NOTIFICATION_ENABLED(clientId))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;

            goto eiClientNotificationEnable_exit;
        }

        // set the client's notification activation mask
        Ei.clientNotificationEnableMask |= BIT(clientId);
    }
    else
    {
        //
        // If Notification is already disabled, return early
        // with error as this is not expected.
        //
        if (!EI_IS_CLIENT_NOTIFICATION_ENABLED(clientId))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;

            goto eiClientNotificationEnable_exit;
        }

        // Clear the client's activation mask
        Ei.clientNotificationEnableMask &= ~BIT(clientId);
    }

eiClientNotificationEnable_exit:

    return status;
}

/*!
 * @brief Update the Notification base multiplier for EI Client Callback
 *
 * @param[in]   clientId       EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   callbackTimeMs New deferred callback time
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Invalid request
 */
FLCN_STATUS
eiClientDeferredCallbackUpdate
(
    LwU8   clientId,
    LwU32  callbackTimeMs
)
{
    EI_CLIENT   *pEiClient   = EI_GET_CLIENT(clientId);
    EI_CALLBACK *pEiCallback = EI_GET_CALLBACK();
    FLCN_STATUS  status      = FLCN_OK;

    //
    // If EI Client is not supported, then return
    // early and send the error
    //
    if (!EI_IS_CLIENT_SUPPORTED(clientId))
    {
        status = FLCN_ERR_ILWALID_STATE;

        goto eiClientDeferredCallbackUpdate_exit;
    }

    // Update the new base multiplier for callback
    if (callbackTimeMs == 0U)
    {
        pEiClient->baseMultiplier = 0;
    }
    else
    {
        pEiClient->baseMultiplier = LW_CEIL(callbackTimeMs,
                                            pEiCallback->basePeriodMs);
    }

eiClientDeferredCallbackUpdate_exit:

    return status;
}

/*!
 * @brief Send ENTRY notitification to EI Clients
 *  or schedule the callback for deferred notification
 *
 * @return      FLCN_OK        On success
 *              FLCN_ERR_xxx   On Failure
 */
FLCN_STATUS
eiClientEntryNotificationSend(void)
{
    FLCN_STATUS status = FLCN_OK;
    EI_CLIENT  *pEiClient;
    LwU32       clientId;

    // Start the centralized callback
    eiCallbackSchedule();

    //
    // Ensure that previous callback work is finished and there is no pending client
    // callback. If there is any, then it is a bug in the code.
    //
    if (Ei.clientCallbackScheduleMask != 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto eiClientEntryNotificationSend_exit;
    }

    //
    // Sanity check to make sure EI FSM is still engaged.
    // If its not engaged, just bail out. There is no need
    // to send any notification.
    //
    if (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_EI))
    {
        goto eiClientEntryNotificationSend_exit;
    }

    // Loop over all the clients which has notification enabled
    FOR_EACH_INDEX_IN_MASK(32, clientId, Ei.clientNotificationEnableMask)
    {
        pEiClient = EI_GET_CLIENT(clientId);

        // If client does not need deferred notificaiton, send the notificaiton now
        if (pEiClient->baseMultiplier == 0)
        {
           //send the entry notification
           status = eiNotificationSend(clientId, LPWR_EI_NOTIFICATION_ENTRY_DONE);
           if (status != FLCN_OK)
           {
               PMU_BREAKPOINT();

               goto eiClientEntryNotificationSend_exit;
           }

           //
           // Sanity check to make sure EI FSM is still engaged.
           // If its not engaged, just bail out. There is no need
           // to send any further notification.
           //
           if (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_EI))
           {
               goto eiClientEntryNotificationSend_exit;
           }
        }
        else
        {
            //
            // Steps to schedule the callback:
            // 1. Create the bit mask of all the client which needs deferred notification
            // 2. Once we are done with all the clients, schedule the callback if mask is
            // non zero
            //
            Ei.clientCallbackScheduleMask |= BIT(clientId);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

eiClientEntryNotificationSend_exit:

    //
    // Deschedule the callback if
    // 1. scheduleMask is zero that means none of the client needs
    // defferred callback,  OR
    // 2. There is some error
    //
    if ((Ei.clientCallbackScheduleMask == 0) ||
        (status != FLCN_OK))
    {
        eiCallbackDeschedule();
    }

    return status;
}

/*!
 * @brief Send EXIT notitification to EI Clients
 *
 * @return      FLCN_OK        On success
 */
FLCN_STATUS
eiClientExitNotificationSend(void)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU32        clientId;

    //
    // If there are any clients which needed deferred callback, then
    // first deschedule the callback before sending any exit notification.
    // So callback does not get trigger to send entry notification which
    // is not needed now.
    //
    if (Ei.clientCallbackScheduleMask != 0)
    {
        eiCallbackDeschedule();
    }

    //
    // Loop over all the clients for which we have sent the entry notification.
    // We need to send the exit notification only to those clients.
    //
    FOR_EACH_INDEX_IN_MASK(32, clientId, Ei.clientEntryNotificationSentMask)
    {
        //send the exit notification
        status = eiNotificationSend(clientId, LPWR_EI_NOTIFICATION_EXIT_DONE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();

            goto eiClientExitNotificationSend_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

eiClientExitNotificationSend_exit:

    return status;
}

/*!
 * @brief Function to send Notification to Clients - PMU and RM based clients
 *
 * @param[in]   clientId       EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   notification   Entry/Exit Notificatio
 *
 * Note: Based on the clientId, this function will send the notification either
 * to PMU based client using task queues - s_eiPmuClientNotificationSend
 * or
 * It will send an RPC to RM for RM based clients.
 *
 * @return      FLCN_OK        On success
 * @return      FLCN_ERROR     On failure to send notification
 */
FLCN_STATUS
eiNotificationSend
(
    LwU8 clientId,
    LwU8 notificationType
)
{
    EI_CLIENT  *pEiClient = EI_GET_CLIENT(clientId);
    FLCN_STATUS status    = FLCN_OK;

    // If Client is managed by PMU, send the notification Event to corresponding PMU task
    if (pEiClient->bIsPmuClient)
    {
        status = s_eiPmuClientNotificationSend(clientId, notificationType);

    }
    // Send the Notification to RM
    else
    {
        status = s_eiRmClientNotificationSend(clientId, notificationType);
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;

        goto eiNotificationSend_exit;

    }

    //
    // Update the entry notification sent mask based on the type of
    // notification.
    // If we are sending the Entry notification, set the bit for given client
    // If we are sending the Exit notification, clear the bit for given client
    //
    if (notificationType == LPWR_EI_NOTIFICATION_ENTRY_DONE)
    {
        Ei.clientEntryNotificationSentMask |= BIT(clientId);
    }
    else if (notificationType == LPWR_EI_NOTIFICATION_EXIT_DONE)
    {
        Ei.clientEntryNotificationSentMask &= (~BIT(clientId));
    }

eiNotificationSend_exit:

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Function to send Notification to PMU based Clients
 *
 * @param[in]   clientId       EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   notification   Entry/Exit Notificatio
 *
 * @return      FLCN_OK        On success
 * @return      FLCN_ERROR     On failure to send notification
 */
FLCN_STATUS
s_eiPmuClientNotificationSend
(
    LwU8 clientId,
    LwU8 notificationType
)
{
    EI_CLIENT   *pEiClient = EI_GET_CLIENT(clientId);
    FLCN_STATUS  status    = FLCN_OK;

    //
    // Ideally we are not aware of the destination PMU task here.
    // Destination task is identified by taskQueueId.
    // Therfore, we should send the DISPATCH_LPWR_LP_EI_NOTIFICATION
    // structure to destination task, as task is supposed to
    // handle the request based on this structure. We can not
    // send the DISPATCH_LPWR_LP structure here since we are
    // sending the request from LPWR_LP task to other task not the
    // other way around.
    //
    // However, lwrrently we do not have a good way to handle this
    // scenario and lwrrently we only have one PMU clients which i.e
    // GPC-RG and which is being managed by LPWR Task.
    //
    // Thefore, for now, we are directly sending the  DISPATCH_LPWR
    // structure here. However, as we add more clients which are not
    // in LPWR Task, we need to come back here and handle this
    // scenario may be on per client basis.
    //
    DISPATCH_LPWR disp2Lpwr = {{0}};

    // Fill the notification data for the client
    disp2Lpwr.eiNotification.hdr.eventType         = pEiClient->taskEventType;
    disp2Lpwr.eiNotification.data.clientId         = clientId;
    disp2Lpwr.eiNotification.data.notificationType = notificationType;

    // Send notification signal to client task in PMU.
    status = lwrtosQueueIdSendBlocking(pEiClient->taskQueueId,
                                       &disp2Lpwr,
                                       sizeof(disp2Lpwr));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @brief Function to send Notification to RM based Clients
 *
 * @param[in]   clientId       EI Client ID based on RM_PMU_LPWR_EI_CLIENT_xvy
 * @param[in]   notification   Entry/Exit Notificatio
 *
 * @return      FLCN_OK        On success
 * @return      FLCN_ERROR     On failure to send notification
 */
FLCN_STATUS
s_eiRmClientNotificationSend
(
    LwU8 clientId,
    LwU8 notificationType
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_RM_RPC_STRUCT_LPWR_EI_NOTIFICATION rpc;

    rpc.eiData.clientId         = clientId;
    rpc.eiData.notificationType = notificationType;

    PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, EI_NOTIFICATION, &rpc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}
