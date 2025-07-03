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
 * @file   lw_vendor_app.c
 * @brief  Dispatch functions to call any GSP-application specific vendor defined routine
 *         upon certain connection or session state.
 */

/* ------------------------- Application Includes --------------------------- */
#include "spdm_responder_lib_internal.h"
#include "rmspdmvendordef.h"

#ifdef LIBSPDM_VENDOR_DEFINED_MESSAGES_SUPPORT
return_status
spdm_get_response_vendor_defined
(
   void           *context,
   uintn           request_size,
   void           *request,
   uintn          *response_size,
   void           *response
)
{
    uintn          spdm_rsponse_size, i;
    uint16         len;
    uint16         standardId;
    uint16         reqPayloadSize;
    uint16         rspPayloadSize = 0;
    uint16        *pBuffer16;
    uint8         *pBuffer8;
    uint8          vendorId[SPDM_VENDOR_DEFINED_VENDOR_ID_SIZE_MAX];

    spdm_context_t *spdm_context;
    PSPDM_MESSAGE_VENDOR_DEFINED_REQUEST   pVendorDefinedReq;
    PSPDM_MESSAGE_VENDOR_DEFINED_RESPONSE  pVendorDefinedRsp;

    if (context == NULL || request == NULL ||
        response_size  == NULL || response == NULL)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    spdm_context = context;
    pVendorDefinedReq = (PSPDM_MESSAGE_VENDOR_DEFINED_REQUEST)request;

    if (spdm_context->response_state != SPDM_RESPONSE_STATE_NORMAL)
    {
        return spdm_responder_handle_response_state(spdm_context,
                                                    pVendorDefinedReq->requestResponseCode,
                                                    response_size, response);
    }

    if (request_size < sizeof(SPDM_MESSAGE_VENDOR_DEFINED_REQUEST) ||
        *response_size < sizeof(SPDM_MESSAGE_VENDOR_DEFINED_RESPONSE))
    {
        return spdm_generate_error_response(spdm_context,
                                     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
                                     response_size, response);
    }

    if (pVendorDefinedReq->reserved1  != 0  ||
        pVendorDefinedReq->reserved2  != 0)
    {
        return spdm_generate_error_response(spdm_context,
                                     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
                                     response_size, response);
    }

    pBuffer16 = (uint16 *)((uint8 *)request + sizeof(SPDM_MESSAGE_VENDOR_DEFINED_REQUEST));
    standardId = *pBuffer16;

    pBuffer8 = (uint8 *)((uint8 *)pBuffer16 + SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE);
    len = *pBuffer8;
    pBuffer8 += SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE;

    //
    // TODO: Pending by https://confluence.lwpu.com/display/SC/Vendor+defined+message+use-cases
    // Lwrrently, we just set SPDM spec defined fields and return.
    // To get vendor defined request payload start address and payload size
    //
    if (len != 0)
    {
        for (i = 0; i < len; i++)
        {
            vendorId[i] = pBuffer8[i];
        }
        pBuffer8 += len;
    }

    reqPayloadSize = *(uint16 *)pBuffer8;
    // Get paylaod start address
    pBuffer8 += SPDM_VENDOR_DEFINED_REQ_LEN_FIELD_SIZE_IN_BYTE;

    // TODO: Adding request message handler here

    // TODO: Remove below code once we finalize vendor defined request message confluence.
    pVendorDefinedRsp = (PSPDM_MESSAGE_VENDOR_DEFINED_RESPONSE)response;
    spdm_rsponse_size = sizeof(SPDM_MESSAGE_VENDOR_DEFINED_RESPONSE);

    if (spdm_is_version_supported(spdm_context, SPDM_MESSAGE_VERSION_11))
    {
        pVendorDefinedRsp->spdmVersionId = SPDM_MESSAGE_VERSION_11;
    }
    else
    {
        pVendorDefinedRsp->spdmVersionId = SPDM_MESSAGE_VERSION_10;
    }

    pVendorDefinedRsp->requestResponseCode = SPDM_VENDOR_DEFINED_RESPONSE;
    pVendorDefinedRsp->reserved1  = 0;
    pVendorDefinedRsp->reserved2  = 0;
    pBuffer16 = (uint16 *)((uint8 *)pVendorDefinedRsp + sizeof(SPDM_MESSAGE_VENDOR_DEFINED_RESPONSE));

    *pBuffer16 = standardId;
    pBuffer8 = (uint8 *)((uint8 *)pBuffer16 + SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE);
    spdm_rsponse_size += SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE;

    *pBuffer8 = len;
    spdm_rsponse_size += SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE;
    pBuffer8 += SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE;

    if (len != 0)
    {
        for(i = 0 ;i < len; i++)
        {
            pBuffer8[i] = vendorId[i];
        }
        pBuffer8 += len;
        spdm_rsponse_size += len;
    }

    //
    // The response payload address is a misaligned address.
    // So we need to utilize copy function to fill this field,
    // otherwise RISCV will get trap error.
    //
    copy_mem((void *)pBuffer8, (void *)&rspPayloadSize, SPDM_VENDOR_DEFINED_RSP_LEN_FIELD_SIZE_IN_BYTE);
    spdm_rsponse_size += SPDM_VENDOR_DEFINED_RSP_LEN_FIELD_SIZE_IN_BYTE;

    *response_size = spdm_rsponse_size;
    return RETURN_SUCCESS;
}
#endif
