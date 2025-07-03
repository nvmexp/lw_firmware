/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_transport_layer.c
 * @brief  File that provides transport layer functionality to libspdm, using
 *         RM<->SEC2 CMD/MSGQ to encode and decode SPDM messages to be sent.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2/sec2ifspdm.h"
#include "rmspdmselwredif.h"
#include "spdm_common_lib_internal.h"
#include "lw_transport_layer.h"

/* ------------------------ Macros and Defines ----------------------------- */
// Struct to keep track of payload information.
typedef struct
{
  LwU8  *bufferAddr;
  LwU32  bufferSize;
} PAYLOAD_BUFFER_INFO;

/* ------------------------- Global Variables ------------------------------- */
static PAYLOAD_BUFFER_INFO payloadBufferInfo
    GCC_ATTRIB_SECTION("dmem_spdm", "payloadBufferInfo");

static LwU32 decodedSessionId
    GCC_ATTRIB_SECTION("dmem_spdm", "decodedSessionId");

/* ---------------------- Spdm_common_lib.h Functions ----------------------- */
/**
  Initialize libspdm payload buffer information with location and size
  of EMEM payload buffer, shared between RM and SEC2.

  @note Must be called before libspdm can process messages, the behavior
  is undefined otherwise.

  @param buffer_addr  Address of payload buffer.
  @param buffer_size  Size of payload buffer, in bytes.

  @retval boolean signifying success of operation.
**/
boolean
spdm_transport_layer_initialize
(
    IN uint8  *buffer_addr,
    IN uint32  buffer_size
)
{
    if (buffer_addr == NULL || buffer_size == 0)
    {
        // Best we can do to ensure invalid.
        payloadBufferInfo.bufferAddr = NULL;
        payloadBufferInfo.bufferSize = 0;
        return LW_FALSE;
    }

    payloadBufferInfo.bufferAddr = buffer_addr;
    payloadBufferInfo.bufferSize = buffer_size;

    return LW_TRUE;
}

/**
  Encode an SPDM or APP message to a RM<->SEC2 MSG. For all messages, it will
  fill out the relevant fields in the message, and then copy the payload into
  a shared buffer.

  For normal SPDM message, it copies the payload directly.
  For selwred SPDM message, it encrypts the payload before copying.
  For selwred APP message, it encrypts the payload before copying.

  The APP message is encoded to a selwred message directly in SPDM session.
  The APP message format is defined by the transport layer.
  Take MCTP as example: APP message == MCTP header (MCTP_MESSAGE_TYPE_SPDM) + SPDM message

  @note Expects that the shared payload buffer has already been initialized via
  a call to spdm_initialize_transport_layer.

  @param spdm_context            A pointer to the SPDM context.
  @param session_id              Indicates if it is a selwred message protected via SPDM session.
                                 If session_id is NULL, it is a normal message.
                                 If session_id is NOT NULL, it is a selwred message.
  @param is_app_message          Indicates if it is an APP message or SPDM message.
  @param is_requester            Indicates if it is a requester message.
  @param message_size            Size in bytes of the message data buffer.
  @param message                 A pointer to a source buffer to store the message.
  @param transport_message_size  Size in bytes of the transport message data buffer.
  @param transport_message       A pointer to a destination buffer to store the transport message.

  @retval RETURN_SUCCESS            The message is encoded successfully.
  @retval RETURN_ILWALID_PARAMETER  Necessary parameters are NULL or the message_size is invalid.
  @retval RETURN_UNSUPPORTED        The parameters indicate an unsupported operation.
**/
return_status
spdm_transport_encode_message
(
    IN     void    *spdm_context,
    IN     uint32  *session_id,
    IN     boolean  is_app_message,
    IN     boolean  is_requester,
    IN     uintn    spdm_message_size,
    IN     void    *spdm_message,
    IN OUT uintn   *transport_message_size,
    OUT    void    *transport_message
)
{
    LwU8                             appData[MAX_SPDM_MESSAGE_BUFFER_SIZE];
    LwU32                            appDataSize;
    RM_SPDM_APP_DATA_HDR            *pAppDataHdr;
    LwU32                            selwredMessageSize;
    void                            *pSelwredMessageContext;
    RM_SEC2_SPDM_MSG                *pSec2Msg;
    return_status                    status;
    LwU8                             selwredMessage[MAX_SPDM_MESSAGE_BUFFER_SIZE];
    spdm_selwred_message_callbacks_t selwredMessageInfo;

    // Ensure valid input
    if (spdm_context == NULL || spdm_message == NULL || spdm_message_size == 0 ||
        transport_message == NULL || transport_message_size == NULL ||
        *transport_message_size != sizeof(RM_SEC2_SPDM_MSG))
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Filter out unsupported requests. We only implement responder messages.
    if (is_requester)
    {
        return RETURN_UNSUPPORTED;
    }

    // Best effort to ensure payload buffer information is valid before use.
    if (payloadBufferInfo.bufferAddr == NULL || payloadBufferInfo.bufferSize == 0)
    {
        return RETURN_NO_MAPPING;
    }

    pSec2Msg = (RM_SEC2_SPDM_MSG *)transport_message;

    zero_mem(selwredMessage, sizeof(selwredMessage));
    zero_mem(&selwredMessageInfo, sizeof(selwredMessageInfo));
    selwredMessageSize                         = sizeof(selwredMessage);
    selwredMessageInfo.version                 = SPDM_SELWRED_MESSAGE_CALLBACK_VERSION_APM;
    selwredMessageInfo.sequence_number_size    = SPDM_SELWRED_MESSAGE_SEQUENCE_NUMBER_BYTES_APM;
    selwredMessageInfo.max_random_number_count = SPDM_SELWRED_MESSAGE_MAX_RANDOM_NUMBER_BYTES_APM;

    //
    // Check to see if error message. Since we consider all errors fatal,
    // we just return error, rather than prepare response payload. Error
    // message structs differ for application messages and SPDM messages.
    //
    if (is_app_message)
    {
        RM_SPDM_LW_CMD_HDR *pCmdHdr = spdm_message;
        if (spdm_message_size < sizeof(RM_SPDM_LW_CMD_HDR))
        {
            return RETURN_ILWALID_PARAMETER;
        }

        if (pCmdHdr->cmdType == RM_SPDM_LW_CMD_TYPE_RSP_FAILURE)
        {
            return RETURN_PROTOCOL_ERROR;
        }
    }
    else
    {
        spdm_message_header_t *pSpdmMessageHeader = spdm_message;
        if (spdm_message_size < sizeof(spdm_message_header_t))
        {
            return RETURN_ILWALID_PARAMETER;
        }

        //
        // We also treat END_SESSION_ACK as an error, as our behavior
        // is the same as when we hit error. Use different response code
        // to differentiate successful END_SESSION_ACK from error code.
        //
        switch (pSpdmMessageHeader->request_response_code)
        {
            case SPDM_ERROR:
                return RETURN_PROTOCOL_ERROR;
            case SPDM_END_SESSION_ACK:
                return RETURN_ABORTED;
            default:
                break;
        }
    }

    // Default to normal message type. We can override later if it is not.
    pSec2Msg->rspMsg.rspPayloadType = SpdmPayloadTypeNormalMessage;

    // If session message, encode as selwred message.
    if (session_id != NULL)
    {
        // Retrieve the context for the selwred message, ensuring it is valid.
        pSelwredMessageContext =
            spdm_get_selwred_message_context_via_session_id(spdm_context, *session_id);

        if (pSelwredMessageContext == NULL)
        {
            return RETURN_UNSUPPORTED;
        }

        // We need to add RM_SPDM_APP_DATA_HDR to message before encrypting.
        zero_mem(appData, sizeof(appData));
   
        // First, ensure we have enough size in app data buffer for message.
        if ((sizeof(RM_SPDM_APP_DATA_HDR) + spdm_message_size) > sizeof(appData))
        {
            return RETURN_UNSUPPORTED;
        }

        // Fill header, then copy message payload directly after.
        pAppDataHdr          = (RM_SPDM_APP_DATA_HDR *)appData;
        pAppDataHdr->msgType = is_app_message ?
            RM_SPDM_APP_DATA_MSG_TYPE_LW : RM_SPDM_APP_DATA_MSG_TYPE_SPDM;
        copy_mem(pAppDataHdr + 1, spdm_message, spdm_message_size);
        appDataSize = sizeof(RM_SPDM_APP_DATA_HDR) + spdm_message_size;

        // Encode app data into selwred message.
        status = spdm_encode_selwred_message(
                pSelwredMessageContext, *session_id, is_requester,
                appDataSize, appData, &selwredMessageSize,
                selwredMessage, &selwredMessageInfo);
        if (RETURN_ERROR(status))
        {
            return status;
        }

        // Payload type is now encoded selwred message.
        spdm_message_size               = selwredMessageSize;
        spdm_message                    = selwredMessage;
        pSec2Msg->rspMsg.rspPayloadType = SpdmPayloadTypeSelwredMessage;
    }

    // Ensure message will fit in buffer before copying payload
    if (spdm_message_size > payloadBufferInfo.bufferSize)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    //
    // Prepare RM<->SEC2 MSG. If we got here, assume the payload is valid.
    // SEC2 will handle setting MSG status before sending.
    //
    copy_mem(payloadBufferInfo.bufferAddr, spdm_message, spdm_message_size);
    pSec2Msg->rspMsg.rspPayloadEmemAddr = (uint32)payloadBufferInfo.bufferAddr;
    pSec2Msg->rspMsg.rspPayloadSize     = spdm_message_size;

    // Should already be set.
    *transport_message_size = sizeof(RM_SEC2_SPDM_MSG);

    return RETURN_SUCCESS;
}

/**
  Decode an SPDM or APP message from a RM<->SEC2 CMD. For all messages, it will
  interpret the relevant fields in the command to determine message type, then
  copy the relevant payload from the shared buffer.

  For normal SPDM message, it copies the payload directly.
  For selwred SPDM message, it copies the payload, then decrypts and verifies the selwred message.
  For selwred APP message, it copies the payload, then decrypts and verifies the selwred message.

  The APP message is decoded from a selwred message directly in SPDM session.
  The APP message format is defined by the transport layer.
  Take MCTP as example: APP message == MCTP header (MCTP_MESSAGE_TYPE_SPDM) + SPDM message

  @note Expects that the shared payload buffer has already been initialized via
  a call to spdm_initialize_transport_layer.

  @param spdm_context            A pointer to the SPDM context.
  @param session_id              Indicates if it is a selwred message protected via SPDM session.
                                 If *session_id is NULL, it is a normal message.
                                 If *session_id is NOT NULL, it is a selwred message.
  @param is_app_message          Indicates if it is an APP message or SPDM message.
  @param is_requester            Indicates if it is a requester message.
  @param transport_message_size  Size in bytes of the transport message data buffer.
  @param transport_message       A pointer to a source buffer to store the transport message.
  @param message_size            Size in bytes of the message data buffer.
  @param message                 A pointer to a destination buffer to store the message.

  @retval RETURN_SUCCESS            The message is decoded successfully.
  @retval RETURN_ILWALID_PARAMETER  The message is NULL or the message_size is zero.
  @retval RETURN_UNSUPPORTED        The transport_message is unsupported.
**/
return_status
spdm_transport_decode_message
(
    IN     void     *spdm_context,
    OUT    uint32  **session_id,
    OUT    boolean  *is_app_message,
    IN     boolean   is_requester,
    IN     uintn     transport_message_size,
    IN     void     *transport_message,
    IN OUT uintn    *message_size,
    OUT    void     *message
)
{
    RM_SEC2_SPDM_CMD                 *pSec2Cmd;
    LwU32                             payloadBufferStartAddr;
    LwU32                             payloadBufferEndAddr;
    LwU32                             requestAddr;
    LwU32                             requestSize;
    return_status                     status;
    void                             *pSelwredMessageContext;
    spdm_selwred_message_callbacks_t  selwredMessageInfo;
    spdm_error_struct_t               spdmError;
    RM_SPDM_APP_DATA_HDR             *pAppDataHdr;

    // Ensure valid input
    if (spdm_context == NULL || session_id == NULL || is_app_message == NULL ||
        transport_message == NULL || transport_message_size != sizeof(RM_SEC2_SPDM_CMD) ||
        message == NULL || message_size == NULL || *message_size == 0)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // We only support decoding requester messsages.
    if (!is_requester)
    {
        return RETURN_UNSUPPORTED;
    }

    // Best effort to ensure payload buffer information is valid before use.
    if (payloadBufferInfo.bufferAddr == NULL || payloadBufferInfo.bufferSize == 0)
    {
        return RETURN_NO_MAPPING;
    }

    pSec2Cmd = (RM_SEC2_SPDM_CMD *)transport_message;

    spdmError.error_code = 0;
    spdmError.session_id = 0;
    spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
    
    zero_mem(&selwredMessageInfo, sizeof(selwredMessageInfo));
    selwredMessageInfo.version                 = SPDM_SELWRED_MESSAGE_CALLBACK_VERSION_APM;
    selwredMessageInfo.sequence_number_size    = SPDM_SELWRED_MESSAGE_SEQUENCE_NUMBER_BYTES_APM;
    selwredMessageInfo.max_random_number_count = SPDM_SELWRED_MESSAGE_MAX_RANDOM_NUMBER_BYTES_APM;

    payloadBufferStartAddr = (uint32)payloadBufferInfo.bufferAddr;
    payloadBufferEndAddr   = payloadBufferStartAddr + payloadBufferInfo.bufferSize;

    requestAddr = pSec2Cmd->reqCmd.reqPayloadEmemAddr;
    requestSize = pSec2Cmd->reqCmd.reqPayloadSize;

    // Validate request fits constraints of payload buffer.
    if ((requestAddr + requestSize > payloadBufferEndAddr) ||
        (requestAddr < payloadBufferStartAddr)             ||
        (requestSize > *message_size))
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Copy payload from EMEM
    copy_mem(message, (uint8 *)requestAddr, requestSize);  
    *message_size = requestSize;

    //
    // Now that we have stored payload from EMEM, handle based on message type.
    // We do not support app messages.
    //
    *is_app_message = LW_FALSE;
    *session_id     = NULL;

    switch (pSec2Cmd->reqCmd.reqPayloadType)
    {
        case SpdmPayloadTypeNormalMessage:
        {
            // No additional processing needed.
            break;
        }
        case SpdmPayloadTypeSelwredMessage:
        {
            // Session ID should be first four bytes of message.
            if (*message_size < sizeof(LwU32))
            {
                return RETURN_UNSUPPORTED;
            }

            decodedSessionId = *(LwU32 *)(message);
            *session_id = &decodedSessionId;

            // Get session context from ID, ensuring it is valid.
            pSelwredMessageContext =
                spdm_get_selwred_message_context_via_session_id(spdm_context,
                                                                decodedSessionId);
            if (pSelwredMessageContext == NULL)
            {
                spdmError.error_code = SPDM_ERROR_CODE_ILWALID_SESSION;
                spdmError.session_id = decodedSessionId;
                spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
                return RETURN_UNSUPPORTED;
            }

            //
            // Decode message. We can decode into same buffer, as decoded
            // message must be smaller than selwred message payload.
            //
            status = spdm_decode_selwred_message(pSelwredMessageContext,
                                                 decodedSessionId, is_requester,
                                                 *message_size, message,
                                                 message_size, message,
                                                 &selwredMessageInfo);
            if (RETURN_ERROR(status))
            {
                spdm_selwred_message_get_last_spdm_error_struct(pSelwredMessageContext,
                                                                &spdmError);
                spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
                return RETURN_UNSUPPORTED;
            }

            //
            // Now that we have decoded application data into message buffer,
            // we need to determine whether SPDM message or app message.
            //
            pAppDataHdr = (RM_SPDM_APP_DATA_HDR *)(message);

            // If the message isn't large enough, we know it's invalid.
            if (*message_size < sizeof(RM_SPDM_APP_DATA_HDR))
            {
                spdmError.error_code = SPDM_ERROR_CODE_ILWALID_REQUEST;
                spdmError.session_id = decodedSessionId;
                spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
                return RETURN_UNSUPPORTED;
            }

            switch (pAppDataHdr->msgType)
            {
                case RM_SPDM_APP_DATA_MSG_TYPE_SPDM:
                case RM_SPDM_APP_DATA_MSG_TYPE_LW:
                {
                    //
                    // If LW message type, flag as an APP message. In both cases,
                    // skip the application data header,  as the message_payload
                    // afterwards is the message payload.
                    //
                    // APM-TODO: This copy_mem will work, but its not safe if
                    // underlying implementation changes.
                    //
                    *is_app_message  = (pAppDataHdr->msgType == RM_SPDM_APP_DATA_MSG_TYPE_LW);
                    *message_size   -= sizeof(RM_SPDM_APP_DATA_HDR);
                    copy_mem(message, message + sizeof(RM_SPDM_APP_DATA_HDR), *message_size);
                    break;
                }
                default:
                    spdmError.error_code = SPDM_ERROR_CODE_ILWALID_REQUEST;
                    spdmError.session_id = decodedSessionId;
                    spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
                    return RETURN_UNSUPPORTED;
            }
            break;
        }

        // We don't support any other message types.
        default:
            return RETURN_UNSUPPORTED;
    }

    return RETURN_SUCCESS;
}
