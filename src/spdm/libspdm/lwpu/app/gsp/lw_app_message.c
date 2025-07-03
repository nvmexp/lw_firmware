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

// Included libspdm copyright, as file is LW-authored uses on libspdm code.

/*!
 * @file   lw_app_message.c
 * @brief  Functions to interpret application message and handle appropriately.
 */

/* ------------------------- Application Includes --------------------------- */
#include "spdm_responder_lib_internal.h"

/* ------------------------- Public Functions ------------------------------- */
/**
    Process the APP request and return the response. The APP message format is
    LWPU-defined, and can be found in rmspdmselwredif.h.

    @param spdm_context               A pointer to the SPDM context.
    @param session_id                 Indicates the session ID, as the APP message is sent as a part of a SPDM Session.
                                      Must not be NULL.
    @param request_size               Size in bytes of the request data.
    @param request                    A pointer to the request data.
    @param response_size              Size in bytes of the response data.
                                      On input, it means the size in bytes of response data buffer.
                                      On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                      and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
    @param response                   A pointer to the response data.

    @retval RETURN_SUCCESS             The request is processed and the response is returned.
    @retval RETURN_BUFFER_TOO_SMALL    The buffer is too small to hold the data.
    @retval RETURN_DEVICE_ERROR        A device error oclwrs when communicates with the device.
    @retval RETURN_SELWRITY_VIOLATION  Any verification fails.
**/
return_status
spdm_dispatch_application_message
(
    IN     void   *spdm_context,
    IN     uint32 *session_id,
    IN     uintn   request_size,
    IN     void   *request,
    IN OUT uintn  *response_size,
    OUT    void   *response
)
{
    // No implementation yet.
    return RETURN_NOT_FOUND;
}
