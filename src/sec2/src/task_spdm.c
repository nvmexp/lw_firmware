/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_spdm.c
 * @brief  Task that implements Security Protocol and Data Model (SPDM).
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objspdm.h"
#include "sec2_chnmgmt.h"
#include "sec2_objapm.h"
#include "lw_sec2_rtos.h"
#include "lw_utils.h"
#include "spdm_responder_lib_internal.h"
#include "spdm_selwred_message_lib_internal.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define LIBSPDM_CONTEXT_SIZE_BYTES (sizeof(spdm_context_t) +                \
                                    sizeof(spdm_selwred_message_context_t))

/* ------------------------- Prototypes ------------------------------------- */
static void _spdmProcessCommand(DISP2UNIT_CMD *pDispatchedCmd)
    GCC_ATTRIB_SECTION("imem_spdm", "_spdmProcessCommand");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be SPDM commands.
 */
LwrtosQueueHandle SpdmQueue;

/* ------------------------- Static Variables ------------------------------- */
// EMEM buffer for RM and SEC2 to place SPDM message payloads.
static LwU8 g_ememPayload[MAX_SPDM_MESSAGE_BUFFER_SIZE]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rmrtos", "g_ememPayload");

//
// libspdm keeps state in large context structure. Session state is kept in
// separate structure, but libspdm expects it placed immediately after
// context structure in memory. Therefore, store both in large buffer.
//
static LwU8 g_deviceContext[LIBSPDM_CONTEXT_SIZE_BYTES]
    GCC_ATTRIB_SECTION("dmem_spdm", "g_deviceContext");

//
// Static bool to alert SPDM task whether or not init was successful.
// Required so we can fail gracefully if task init fails.
//
static LwBool g_spdmInitialized
    GCC_ATTRIB_SECTION("dmem_spdm", "g_spdmInitialized");

//
// Static bool to alert SPDM task whether libspdm has reached
// fatal error. This will prevent further usage of library.
//
static LwBool g_spdmFatalError
    GCC_ATTRIB_SECTION("dmem_spdm", "g_spdmFatalError");

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Dispatches commands receieved to private handler functions.
 *        Posts response to message queue upon completion.
 */
static void
_spdmProcessCommand(DISP2UNIT_CMD *pDispatchedCmd)
{
    return_status      spdmStatus      = RETURN_SUCCESS;
    uintn              responseSize    = 0;
    uintn             *pSessionId      = NULL;
    spdm_context_t    *pContext        = (spdm_context_t *)&(g_deviceContext);
    RM_SEC2_SPDM_CMD  *pCmd            = &(pDispatchedCmd->pCmd->cmd.spdm);
    RM_FLCN_QUEUE_HDR  hdr             = {0};
    RM_SEC2_SPDM_MSG   msg             = {0};
    LwU64              rtsOffset       = 0;

    // Initialize response message header
    hdr.unitId    = pDispatchedCmd->pCmd->hdr.unitId;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pDispatchedCmd->pCmd->hdr.seqNumId;

    // If we have hit fatal error, immediately return failure.
    if (g_spdmFatalError)
    {
        hdr.size                      = RM_SEC2_MSG_SIZE(SPDM, RESPONSE);
        msg.msgType                   = RM_SEC2_MSG_TYPE(SPDM, RESPONSE);
        msg.rspMsg.spdmStatus         = RETURN_DEVICE_ERROR;
        msg.rspMsg.rspPayloadEmemAddr = 0;
        msg.rspMsg.rspPayloadSize     = 0;
        goto ErrorExit;
    }

    // Handle incoming command
    switch(pCmd->cmdType)
    {
        case RM_SEC2_SPDM_CMD_ID_INIT:
        {
            hdr.size                       = RM_SEC2_MSG_SIZE(SPDM, INIT);
            msg.msgType                    = RM_SEC2_MSG_TYPE(SPDM, INIT);

            // Initialize to error, overwrite if successful.
            msg.initMsg.spdmStatus         = RETURN_DEVICE_ERROR;
            msg.initMsg.reqPayloadEmemAddr = 0;
            msg.initMsg.reqPayloadSize     = 0;

            if (!g_spdmInitialized)
            {
                // Consider failure during init fatal, unset if successful.
                g_spdmFatalError = LW_TRUE;

                // Check if SPDM is allowed to run on the current board
                if (spdmCheckIfSpdmIsSupported_HAL() != FLCN_OK)
                {
                    goto ErrorExit;
                }

                // Perform any required SEC2 initialization.
                if (apmGetRtsOffset(&rtsOffset) != FLCN_OK)
                {
                    goto ErrorExit;
                }
                msg.initMsg.spdmStatus = spdm_sec2_initialize(pContext,
                                                              g_ememPayload,
                                                              sizeof(g_ememPayload),
                                                              rtsOffset);
                CHECK_SPDM_STATUS(msg.initMsg.spdmStatus);

                g_spdmInitialized = LW_TRUE;
                g_spdmFatalError  = LW_FALSE;
            }

            // Inform RM of transport layer information.
            msg.initMsg.reqPayloadEmemAddr = (LwU32)&g_ememPayload;
            msg.initMsg.reqPayloadSize     = sizeof(g_ememPayload);
            msg.initMsg.spdmStatus         = RETURN_SUCCESS;

            break;
        }
        case RM_SEC2_SPDM_CMD_ID_REQUEST:
        {
            // If not initialized, return error signifying issue.
            if (!g_spdmInitialized)
            {
                hdr.size                      = RM_SEC2_MSG_SIZE(SPDM, RESPONSE);
                msg.msgType                   = RM_SEC2_MSG_TYPE(SPDM, RESPONSE);
                msg.rspMsg.spdmStatus         = RETURN_NOT_STARTED;
                msg.rspMsg.rspPayloadEmemAddr = 0;
                msg.rspMsg.rspPayloadSize     = 0;
                goto ErrorExit;
            }

            hdr.size     = RM_SEC2_MSG_SIZE(SPDM, RESPONSE);
            msg.msgType  = RM_SEC2_MSG_TYPE(SPDM, RESPONSE);
            responseSize = sizeof(msg);

            //
            // Entry point into libspdm code. It will read message from RM,
            // parse it, and generate the correct response, along with
            // filling the RM<->SEC2 MSG struct.
            //
            spdmStatus = spdm_process_message(pContext, &pSessionId, pCmd,
                                              sizeof(RM_SEC2_SPDM_CMD),
            	                              &msg, &responseSize);

            msg.rspMsg.spdmStatus = spdmStatus;

            // Check for valid response from SPDM handler.
            if (spdmStatus != RETURN_SUCCESS || responseSize != sizeof(msg))
            {
                //
                // Return SPDM error to RM.
                // APM-TODO: When do we want to consider fatal error?
                //
                msg.rspMsg.rspPayloadEmemAddr = 0;
                msg.rspMsg.rspPayloadSize     = 0;
                break;
            }

            //
            // Otherwise, SPDM transport layer has already handled filling in
            // message struct, and message is ready to send.
            //
            break;
        }
        default:
        {
            // Invalid command. Send a SPDM response to alert RM of error.
            hdr.size                      = RM_SEC2_MSG_SIZE(SPDM, RESPONSE);
            msg.msgType                   = RM_SEC2_MSG_TYPE(SPDM, RESPONSE);
            msg.rspMsg.spdmStatus         = RETURN_PROTOCOL_ERROR;
            msg.rspMsg.rspPayloadEmemAddr = 0;
            msg.rspMsg.rspPayloadSize     = 0;
            break;
        }
    }

ErrorExit:

    osCmdmgmtCmdQSweep(&pDispatchedCmd->pCmd->hdr, pDispatchedCmd->cmdQueueId);
    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_spdm, pvParameters)
{
    DISPATCH_SPDM dispatch;

    // Set SPDM as uninitialized. Only initialize if task is called.
    g_spdmInitialized = LW_FALSE;
    g_spdmFatalError  = LW_FALSE;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(SpdmQueue, &dispatch))
        {
            (void)_spdmProcessCommand(&dispatch.cmd);
        }
    }
}
