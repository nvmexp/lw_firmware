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
  Build the response when the response state is incorrect.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  request_code                  The SPDM request code.
  @param  response_size                 size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  response                     A pointer to the response data.

  @retval RETURN_SUCCESS               The response is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_responder_handle_response_state(IN void *context,
               IN uint8 request_code,
               IN OUT uintn *response_size,
               OUT void *response)
{
  spdm_context_t *spdm_context;

  spdm_context = context;
  switch (spdm_context->response_state) {
//
// LWE (tandrewprinz) LW_REFACTOR_CONTEXT: The below handlers rely on structures
// that were removed from context. However, we should never reach any of these
// states. Therefore, always return device error, as we cannot recover.
//
  default:
    return RETURN_DEVICE_ERROR;
//  case SPDM_RESPONSE_STATE_BUSY:
//    spdm_generate_error_response(spdm_context, SPDM_ERROR_CODE_BUSY,
//               0, response_size, response);
//    // NOTE: Need to reset status to Normal in up level
//    return RETURN_SUCCESS;
//  case SPDM_RESPONSE_STATE_NEED_RESYNC:
//    spdm_generate_error_response(spdm_context,
//               SPDM_ERROR_CODE_REQUEST_RESYNCH, 0,
//               response_size, response);
//    // NOTE: Need to let SPDM_VERSION reset the State
//    spdm_set_connection_state(spdm_context,
//               SPDM_CONNECTION_STATE_NOT_STARTED);
//    return RETURN_SUCCESS;
//  case SPDM_RESPONSE_STATE_NOT_READY:
//    spdm_context->cache_spdm_request_size =
//      spdm_context->last_spdm_request_size;
//    copy_mem(spdm_context->cache_spdm_request,
//       spdm_context->last_spdm_request,
//       spdm_context->last_spdm_request_size);
//    spdm_context->error_data.rd_exponent = 1;
//    spdm_context->error_data.rd_tm = 1;
//    spdm_context->error_data.request_code = request_code;
//    spdm_context->error_data.token = spdm_context->lwrrent_token++;
//    spdm_generate_extended_error_response(
//      spdm_context, SPDM_ERROR_CODE_RESPONSE_NOT_READY, 0,
//      sizeof(spdm_error_data_response_not_ready_t),
//      (uint8 *)(void *)&spdm_context->error_data,
//      response_size, response);
//    // NOTE: Need to reset status to Normal in up level
//    return RETURN_SUCCESS;
//  case SPDM_RESPONSE_STATE_PROCESSING_ENCAP:
//    spdm_generate_error_response(spdm_context,
//               SPDM_ERROR_CODE_REQUEST_IN_FLIGHT,
//               0, response_size, response);
//    // NOTE: Need let SPDM_ENCAPSULATED_RESPONSE_ACK reset the State
//    return RETURN_SUCCESS;
//  default:
//    return RETURN_SUCCESS;
  }
}
