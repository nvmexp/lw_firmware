/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_responder_lib_internal.h"

/**
  Process the SPDM HEARTBEAT request and return the response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  request_size                  size in bytes of the request data.
  @param  request                      A pointer to the request data.
  @param  response_size                 size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  response                     A pointer to the response data.

  @retval RETURN_SUCCESS               The request is processed and the response is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_get_response_heartbeat(IN void *context,
					  IN uintn request_size,
					  IN void *request,
					  IN OUT uintn *response_size,
					  OUT void *response)
{
	spdm_heartbeat_response_t *spdm_response;
	spdm_heartbeat_request_t *spdm_request;
	spdm_context_t *spdm_context;
	spdm_session_info_t *session_info;
	spdm_session_state_t session_state;

	spdm_context = context;
	spdm_request = request;

	if (spdm_context->response_state != SPDM_RESPONSE_STATE_NORMAL) {
		return spdm_responder_handle_response_state(
			spdm_context,
			spdm_request->header.request_response_code,
			response_size, response);
	}
	if (!spdm_is_capabilities_flag_supported(
		    spdm_context, FALSE,
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HBEAT_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HBEAT_CAP)) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_UNEXPECTED_REQUEST,
					     0, response_size, response);
		return RETURN_SUCCESS;
	}
	if (spdm_context->connection_info.connection_state <
	    SPDM_CONNECTION_STATE_NEGOTIATED) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_UNEXPECTED_REQUEST,
					     0, response_size, response);
		return RETURN_SUCCESS;
	}

	if (!spdm_context->last_spdm_request_session_id_valid) {
		spdm_generate_error_response(context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	session_info = spdm_get_session_info_via_session_id(
		spdm_context, spdm_context->last_spdm_request_session_id);
	if (session_info == NULL) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	session_state = spdm_selwred_message_get_session_state(
		session_info->selwred_message_context);
	if (session_state != SPDM_SESSION_STATE_ESTABLISHED) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	if (request_size != sizeof(spdm_heartbeat_request_t)) {
		spdm_generate_error_response(context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	ASSERT(*response_size >= sizeof(spdm_heartbeat_response_t));
	*response_size = sizeof(spdm_heartbeat_response_t);
	zero_mem(response, *response_size);
	spdm_response = response;

	spdm_response->header.spdm_version = SPDM_MESSAGE_VERSION_11;
	spdm_response->header.request_response_code = SPDM_HEARTBEAT_ACK;
	spdm_response->header.param1 = 0;
	spdm_response->header.param2 = 0;

	return RETURN_SUCCESS;
}
