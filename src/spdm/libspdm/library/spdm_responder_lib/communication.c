/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file contains LWPU modifications.
// Tabs have been replaced with spaces.
//

#include "spdm_responder_lib_internal.h"

/**
  Process a transport layer message.

  The message can be a normal message or a selwred message in SPDM session.
  The message can be an SPDM message or an APP message.

  This function is called in spdm_responder_dispatch_message to process the message.
  The alternative is: an SPDM responder may receive the request message directly
  and call this function to process it, then send the response message.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicates if it is a selwred message protected via SPDM session.
                                       If *session_id is NULL, it is a normal message.
                                       If *session_id is NOT NULL, it is a selwred message.
  @param  request                      A pointer to the request data.
  @param  request_size                  size in bytes of the request data.
  @param  response                     A pointer to the response data.
  @param  response_size                 size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.

  @retval RETURN_SUCCESS               The SPDM request is set successfully.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_process_message(IN void *context, IN OUT uint32 **session_id,
           IN void *request, IN uintn request_size,
           OUT void *response,
           IN OUT uintn *response_size)
{
  return_status status;
  spdm_context_t *spdm_context;
  boolean is_app_message;

  spdm_context = context;

  status = spdm_process_request(spdm_context, session_id, &is_app_message,
             request_size, request);
  if (RETURN_ERROR(status)) {
      return status;
  }

  status = spdm_build_response(spdm_context, *session_id, is_app_message,
             response_size, response);
  if (RETURN_ERROR(status)) {
      return status;
  }
  return RETURN_SUCCESS;
}

//
// LWE (tandrewprinz) LW_REMOVE_FP: Remove function, as it is easier for us
// to use spdm_process_message as entry hook from Falcon. Must be removed,
// rather than left untouched, due to usage of FP.
//
//  /**
//    This is the main dispatch function in SPDM responder.
//  
//    It receives one request message, processes it and sends the response message.
//  
//    It should be called in a while loop or an timer/interrupt handler.
//  
//    @param  spdm_context                  A pointer to the SPDM context.
//  
//    @retval RETURN_SUCCESS               One SPDM request message is processed.
//    @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
//    @retval RETURN_UNSUPPORTED           One request message is not supported.
//  **/
//  return_status spdm_responder_dispatch_message(IN void *context)
//  {
//    return_status status;
//    spdm_context_t *spdm_context;
//    uint8 request[MAX_SPDM_MESSAGE_BUFFER_SIZE];
//    uintn request_size;
//    uint8 response[MAX_SPDM_MESSAGE_BUFFER_SIZE];
//    uintn response_size;
//    uint32 *session_id;
//
//    spdm_context = context;
//
//    request_size = sizeof(request);
//    status = spdm_context->receive_message(spdm_context, &request_size,
//                request, 0);
//    if (RETURN_ERROR(status)) {
//        return status;
//    }
//
//    response_size = sizeof(response);
//    status = spdm_process_message(spdm_context, &session_id, request,
//                request_size, response, &response_size);
//    if (RETURN_ERROR(status)) {
//        return status;
//    }
//    
//    status = spdm_context->send_message(spdm_context, response_size,
//                response, 0);
//    
//    return status;
//  }
