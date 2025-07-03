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

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_transport_layer.c
 * @brief  File that provides transport layer functionality to libSPDM, using
 *         RM<->GSP CMD/MSGQ to encode and decode SPDM messages to be sent.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include <lwpu-sdk/lwmisc.h>
#include "rmspdmtransport.h"
#include "rmspdmselwredif.h"

/* ------------------------- Application Includes --------------------------- */
#include "spdm_common_lib_internal.h"

/* ------------------------- Global Variables ------------------------------- */
static LwU32 decodedSessionId GCC_ATTRIB_SECTION("dmem_libspdm", "decodedSessionId");

/* ------------------------- Public Functions ------------------------------- */
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

  @param  spdm_context              A pointer to the SPDM context.
  @param  session_id                Indicates if it is a selwred message protected via SPDM session.
                                    If session_id is NULL, it is a normal message.
                                    If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message            Indicates if it is an APP message or SPDM message.
  @param  is_requester              Indicates if it is a requester message.
  @param  message_size              Size in bytes of the message data buffer.
  @param  message                   A pointer to a source buffer to store the message.
  @param  transport_message_size    Size in bytes of the transport message data buffer.
  @param  transport_message         A pointer to a destination buffer to store the transport message.

  @retval RETURN_SUCCESS            The message is encoded successfully.
  @retval RETURN_ILWALID_PARAMETER  Necessary parameters are NULL or the message_size is invalid.
  @retval RETURN_UNSUPPORTED        The parameters indicate an unsupported operation.
**/
return_status spdm_transport_encode_message
(
    IN void *spdm_context,
    IN uint32 *session_id,
    IN boolean is_app_message,
    IN boolean is_requester,
    IN uintn spdm_message_size,
    IN void *spdm_message,
    IN OUT uintn *transport_message_size,
    OUT void *transport_message
)
{
    uintn                            selwredMessageSize;
    void                            *pSelwredMessageContext;
    spdm_selwred_message_callbacks_t selwredMessageInfo;
    PLW_SPDM_MESSAGE_HEADER          pLwSpdmMsgHdr;
    PSPDM_SELWRED_MESSAGE_HEADER     pSpdmSelwredMsgHdr;
    LwU8                             selwredMessage[MAX_SPDM_MESSAGE_BUFFER_SIZE];
    LwU8                            *pMsgStartAddr;
    spdm_message_header_t           *pSpdmMessageHeader = spdm_message;

    // Ensure valid input
    if (spdm_context == NULL || spdm_message == NULL || spdm_message_size == 0 ||
        transport_message == NULL || transport_message_size == NULL)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Filter out unsupported requests. We only implement responder messages.
    if (is_requester)
    {
        return RETURN_UNSUPPORTED;
    }

    zero_mem(selwredMessage, sizeof(selwredMessage));
    zero_mem(&selwredMessageInfo, sizeof(selwredMessageInfo));
    selwredMessageSize                         = sizeof(selwredMessage);
    selwredMessageInfo.version                 = SPDM_SELWRED_MESSAGE_CALLBACK_VERSION_CC;
    selwredMessageInfo.sequence_number_size    = SPDM_SELWRED_MESSAGE_SEQUENCE_NUMBER_BYTES_CC;
    selwredMessageInfo.max_random_number_count = SPDM_SELWRED_MESSAGE_MAX_RANDOM_NUMBER_BYTES_CC;

    if (spdm_message_size < sizeof(spdm_message_header_t))
    {
        return RETURN_ILWALID_PARAMETER;
    }

    switch (pSpdmMessageHeader->request_response_code)
    {
        case SPDM_ERROR:
            // TODO : CONFCOMP-575 need to scrub secert components once we get error. 
            return RETURN_PROTOCOL_ERROR;
        default:
            break;
    }

    pLwSpdmMsgHdr = (PLW_SPDM_MESSAGE_HEADER)transport_message;
    pLwSpdmMsgHdr->version = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;

    pMsgStartAddr = (LwU8 *)pLwSpdmMsgHdr + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE;

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

        pLwSpdmMsgHdr->msgType = LW_SPDM_MESSAGE_TYPE_SELWRED;

        //
        // TODO: eddichang CONFCOMP-575. For selwred message we should encrypt message before we return to RM.
        // But RmTest or RM side doesn't support decrypt process yet, so we just return plain text now.
        //
        pLwSpdmMsgHdr->flags = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;

        // Encode app data into selwred message.
        if (pLwSpdmMsgHdr->flags & LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_TRUE)
        {
            return_status                    status;

            status = spdm_encode_selwred_message(
                      pSelwredMessageContext, *session_id, is_requester,
                      spdm_message_size, spdm_message, &selwredMessageSize,
                      (uintn *)selwredMessage, &selwredMessageInfo);
            if (RETURN_ERROR(status))
            {
                return status;
            }

            copy_mem((uint8 *)pMsgStartAddr, selwredMessage, selwredMessageSize);
            *transport_message_size = selwredMessageSize + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE;
            pLwSpdmMsgHdr->size     = (LwU32)selwredMessageSize;
        }
        else
        {
            // Return non-encrypted message but still also need to append SPDM_SELWRED_MESSAGE_HEADER
            pSpdmSelwredMsgHdr = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHdr + 1);
            zero_mem(pSpdmSelwredMsgHdr, SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
            pSpdmSelwredMsgHdr->sessionId = *session_id;

            pMsgStartAddr = (LwU8 *)(pSpdmSelwredMsgHdr + 1);

            copy_mem((uint8 *)pMsgStartAddr, spdm_message, spdm_message_size);
           *transport_message_size = spdm_message_size + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
            pLwSpdmMsgHdr->size    = SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE + (LwU32)spdm_message_size;
        }
    }
    else
    {
        pLwSpdmMsgHdr->msgType = LW_SPDM_MESSAGE_TYPE_NORMAL;
        pLwSpdmMsgHdr->flags = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;

        copy_mem((uint8 *)pMsgStartAddr, spdm_message, spdm_message_size);
        *transport_message_size = spdm_message_size + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE;
        pLwSpdmMsgHdr->size     = (LwU32)spdm_message_size;
    }

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

  @param  spdm_context              A pointer to the SPDM context.
  @param  session_id                Indicates if it is a selwred message protected via SPDM session.
                                    If *session_id is NULL, it is a normal message.
                                    If *session_id is NOT NULL, it is a selwred message.
  @param  is_app_message            Indicates if it is an APP message or SPDM message.
  @param  is_requester              Indicates if it is a requester message.
  @param  transport_message_size    Size in bytes of the transport message data buffer.
  @param  transport_message         A pointer to a source buffer to store the transport message.
  @param  message_size              Size in bytes of the message data buffer.
  @param  message                   A pointer to a destination buffer to store the message.

  @retval RETURN_SUCCESS            The message is decoded successfully.
  @retval RETURN_ILWALID_PARAMETER  The message is NULL or the message_size is zero.
  @retval RETURN_UNSUPPORTED        The transport_message is unsupported.
**/
return_status spdm_transport_decode_message
(
    IN void *spdm_context,
    OUT uint32 **session_id,
    OUT boolean *is_app_message,
    IN boolean is_requester,
    IN uintn transport_message_size,
    IN void *transport_message,
    IN OUT uintn *message_size,
    OUT void *message
)
{
    void                             *pSelwredMessageContext;
    spdm_selwred_message_callbacks_t  selwredMessageInfo;
    spdm_error_struct_t               spdmError;
    PLW_SPDM_MESSAGE_HEADER           pLwSpdmMsgHdr;
    PSPDM_SELWRED_MESSAGE_HEADER      pSpdmSelwredMsgHdr;
    LwU8                             *pMsgStartAddr;
    LwU32                             msgBodySize;
    LwBool                            bEncryptedMsg = FALSE;
    return_status                     status;

    // Ensure valid input
    if (spdm_context == NULL || session_id == NULL || is_app_message == NULL ||
        transport_message == NULL || message == NULL || message_size == NULL ||
        *message_size == 0)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Filter out unsupported requests.
    if (!is_requester)
    {
        return RETURN_UNSUPPORTED;
    }

    spdmError.error_code = 0;
    spdmError.session_id = 0;
    spdm_set_last_spdm_error_struct(spdm_context, &spdmError);

    zero_mem(&selwredMessageInfo, sizeof(selwredMessageInfo));
    selwredMessageInfo.version                 = SPDM_SELWRED_MESSAGE_CALLBACK_VERSION_CC;
    selwredMessageInfo.sequence_number_size    = SPDM_SELWRED_MESSAGE_SEQUENCE_NUMBER_BYTES_CC;
    selwredMessageInfo.max_random_number_count = SPDM_SELWRED_MESSAGE_MAX_RANDOM_NUMBER_BYTES_CC;

    // Retrieve LW_SPDM_MESSAGE_HEADER0
    pLwSpdmMsgHdr = (PLW_SPDM_MESSAGE_HEADER)transport_message;

    if (pLwSpdmMsgHdr->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT)
    {
        return  RETURN_ILWALID_PARAMETER;
    }

    if (pLwSpdmMsgHdr->msgType == LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        if (transport_message_size < LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE)
        {
            return RETURN_UNSUPPORTED;
        }

        msgBodySize = (LwU32) (transport_message_size - LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
        pMsgStartAddr = (LwU8 *)((LwU8 *)pLwSpdmMsgHdr + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    }
    else if (pLwSpdmMsgHdr->msgType == LW_SPDM_MESSAGE_TYPE_SELWRED)
    {
        if (transport_message_size < (LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE))
        {
            return RETURN_UNSUPPORTED;
        }

        pSpdmSelwredMsgHdr = (SPDM_SELWRED_MESSAGE_HEADER *)((LwU8 *)pLwSpdmMsgHdr + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

        decodedSessionId = pSpdmSelwredMsgHdr->sessionId;
        *session_id = &decodedSessionId;

        // Get session context from ID, ensuring it is valid.
        pSelwredMessageContext = spdm_get_selwred_message_context_via_session_id(spdm_context,
                                                                  decodedSessionId);
		
        if (pSelwredMessageContext == NULL)
        {
            spdmError.error_code = SPDM_ERROR_CODE_ILWALID_SESSION;
            spdmError.session_id = decodedSessionId;
            spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
			
            return RETURN_UNSUPPORTED;
        }

        //
        // Because RmTest doesn't have crypto function support, so we still pass plain text instead of selwred message.
        // This is why we add bEncryptedMsg check.
        //

        bEncryptedMsg = (pLwSpdmMsgHdr->flags & LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_TRUE ? TRUE : FALSE);

        if (bEncryptedMsg)
        {
            msgBodySize  = (LwU32)(transport_message_size - LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
            pMsgStartAddr = (LwU8 *)((LwU8 *)pLwSpdmMsgHdr + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

            //
            // Decode message. We can decode into same buffer, as decoded
            // message must be smaller than selwred message payload.
            //
            status = spdm_decode_selwred_message(pSelwredMessageContext,
                                                 decodedSessionId,   is_requester,
                                                 (uintn)msgBodySize, pMsgStartAddr,
                                                 message_size,       pMsgStartAddr,
                                                 &selwredMessageInfo);
            if (RETURN_ERROR(status))
            {
                spdm_selwred_message_get_last_spdm_error_struct(pSelwredMessageContext,
                                                                 &spdmError);
                spdm_set_last_spdm_error_struct(spdm_context, &spdmError);
                return RETURN_UNSUPPORTED;
            }
       
            msgBodySize   = (LwU32)(*message_size);
        }
        else
        {
            msgBodySize  = transport_message_size - LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
            pMsgStartAddr = (LwU8 *)((LwU8 *)pLwSpdmMsgHdr + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
        }
    }
    else
    {
        return RETURN_UNSUPPORTED;
    }

    // Copy payload to device context
    copy_mem(message, (uint8 *)pMsgStartAddr, msgBodySize);
    *message_size = (uintn)msgBodySize;

    //
    // APM-TODO CONFCOMP-397: Enable support for session and app messages.
    // Leave unsupported for now.
    //
    *is_app_message = FALSE;

    return RETURN_SUCCESS;
}
