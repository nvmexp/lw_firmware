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
 * @file   lw_app_dispatch.c
 * @brief  Functions to interpret application message and handle appropriately.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "spdm_responder_lib_internal.h"
#include "rmspdmselwredif.h"
#include "lw_apm_spdm_common.h"

/* ------------------------- Prototypes ------------------------------------- */
static return_status _application_message_apm_update_iv(IN uintn request_size,
    IN void *request, IN OUT uintn *response_size, OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "_application_message_apm_update_iv");

/* ------------------------- Static Functions ------------------------------- */
/**
    Process the LWPU Command Update IV. This command takes a list of IVs and
    stores them in the corresponding slot of the APM encryption IV table.

    @param request_size   Size in bytes of the request data.
    @param request        A pointer to the request data.
    @param response_size  Size in bytes of the response data.
                          On input, it means the size in bytes of response data buffer.
                          On output, it means the size in bytes of copied response data
                          buffer if RETURN_SUCCESS is returned.
    @param response       A pointer to the response data.

    @return Returns return_status indicating success, or relevant error code.
**/
static return_status
_application_message_apm_update_iv
(
    IN     uintn   request_size,
    IN     void   *request,
    IN OUT uintn  *response_size,
    OUT    void   *response
)
{
    RM_SPDM_LW_CMD_REQ_UPDATE_IV *pUpdateIvCmd = NULL;
    RM_SPDM_UPDATE_IV_IV_SLOT    *pIvSlots     = NULL;
    RM_SPDM_LW_CMD_RSP_SUCCESS   *pSuccessCmd  = NULL;
    LwU32                         ivSlotsSize  = 0;
    LwU32                         i            = 0;                 
    FLCN_STATUS                   flcnStatus   = FLCN_OK;
    return_status                 spdmStatus   = RETURN_SUCCESS;


    // Basic parameter validation.
    if (request_size == 0 || request == NULL || response_size == NULL ||
        *response_size == 0 || response == NULL)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Ensure we have enough size for request and (expected) response.
    if (request_size < sizeof(RM_SPDM_LW_CMD_REQ_UPDATE_IV) ||
        *response_size < sizeof(RM_SPDM_LW_CMD_RSP_SUCCESS))
    {
        return RETURN_ILWALID_PARAMETER;
    }

    pUpdateIvCmd = (RM_SPDM_LW_CMD_REQ_UPDATE_IV *)request;

    //
    // Ensure the number of IV slots matches the message size. Subtract
    // the header (struct doesn't include IV slots), then compare sizes.
    //
    ivSlotsSize = request_size - sizeof(RM_SPDM_LW_CMD_REQ_UPDATE_IV);
    if (pUpdateIvCmd->numSlots * sizeof(RM_SPDM_UPDATE_IV_IV_SLOT) != ivSlotsSize)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    pIvSlots = (RM_SPDM_UPDATE_IV_IV_SLOT *)(request + sizeof(RM_SPDM_LW_CMD_REQ_UPDATE_IV));

    //
    // Unload data overlay to create space for shared APM-SPDM overlay that
    // is used during IV write. Ensure we reload overlay before exiting.
    //
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(spdm));
    for (i = 0; i < pUpdateIvCmd->numSlots; i++)
    {
        flcnStatus = apm_spdm_shared_write_iv_table(
            pIvSlots[i].iv, pIvSlots[i].slotId, APM_SPDM_COMMON_MUTEX_TIMEOUT);
        if (flcnStatus != FLCN_OK)
        {
            spdmStatus = RETURN_DEVICE_ERROR;
            goto UpdateIvError;
        }
    }

UpdateIvError:
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(spdm));

    if (spdmStatus == RETURN_SUCCESS)
    {
        // Update to success response.
        pSuccessCmd              = (RM_SPDM_LW_CMD_RSP_SUCCESS *)response;
        pSuccessCmd->hdr.cmdType = RM_SPDM_LW_CMD_TYPE_RSP_SUCCESS;
        *response_size           = sizeof(RM_SPDM_LW_CMD_RSP_SUCCESS);
    }

    return spdmStatus;
}

/* ------------------------- Public Functions ------------------------------- */
/**
    Process the APP request and return the response. The APP message format is
    LWPU-defined, and can be found in rmspdmselwredif.h.

    @param spdm_context               A pointer to the SPDM context.
    @param session_id                 Indicates the session ID, as the APP message is sent as a part
                                      of a SPDM Session. Must not be NULL.
    @param request_size               Size in bytes of the request data.
    @param request                    A pointer to the request data.
    @param response_size              Size in bytes of the response data.
                                      On input, it means the size in bytes of response data buffer.
                                      On output, it means the size in bytes of copied response data
                                      buffer if RETURN_SUCCESS is returned.
    @param response                   A pointer to the response data.

    @return Returns return_status indicating success, or relevant error code.
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
    RM_SPDM_LW_CMD_HDR *pCmdHdr;
    void               *pSessionId;
    return_status       status;

    if (spdm_context == NULL || session_id == NULL || request_size == 0 ||
        request == NULL || response_size == NULL || *response_size == 0 ||
        response == NULL)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // Check if the session id is valid.
    pSessionId = spdm_get_session_info_via_session_id(spdm_context,
                                                      *session_id);
    if (pSessionId == NULL)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // We expect this to be an LW_CMD.
    if (request_size < sizeof(RM_SPDM_LW_CMD_HDR))
    {
        return RETURN_ILWALID_PARAMETER;
    }

    pCmdHdr = (RM_SPDM_LW_CMD_HDR *)request;
    switch (pCmdHdr->cmdType)
    {
        case RM_SPDM_LW_CMD_TYPE_REQ_UPDATE_IV:
        {
            status = _application_message_apm_update_iv(
                request_size, request, response_size, response);
            break;
        }
        default:
        {
            status = RETURN_NOT_FOUND;
            break;
        }
    }

    if (status != RETURN_SUCCESS)
    {
        RM_SPDM_LW_CMD_RSP_FAILURE *pFailureCmd;

        //
        // No other messages supported for now.
        // Send default error message.
        //
        if (*response_size < sizeof(RM_SPDM_LW_CMD_RSP_FAILURE))
        {
            return RETURN_ILWALID_PARAMETER;
        }

        pFailureCmd              = (RM_SPDM_LW_CMD_RSP_FAILURE *)response;
        pFailureCmd->hdr.cmdType = RM_SPDM_LW_CMD_TYPE_RSP_FAILURE;
        pFailureCmd->extError    = status;
        *response_size           = sizeof(RM_SPDM_LW_CMD_RSP_FAILURE);

        // Set status as success, as we have valid (failure) payload.
        status                   = RETURN_SUCCESS;
    }

    return status;
}
