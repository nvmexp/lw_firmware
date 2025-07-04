/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_responder_lib_internal.h"

/**
  Process the SPDM RESPONSE_IF_READY request and return the response.

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
return_status spdm_get_response_respond_if_ready(IN void *context,
						 IN uintn request_size,
						 IN void *request,
						 IN OUT uintn *response_size,
						 OUT void *response)
{
	spdm_message_header_t *spdm_request;
	spdm_context_t *spdm_context;
	spdm_get_spdm_response_func get_response_func;
	return_status status;

	spdm_context = context;
	spdm_request = request;

	if (spdm_context->response_state == SPDM_RESPONSE_STATE_NEED_RESYNC) {
		return spdm_responder_handle_response_state(
			spdm_context, spdm_request->request_response_code,
			response_size, response);
	}

	if (request_size != sizeof(spdm_message_header_t)) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	ASSERT(spdm_request->request_response_code == SPDM_RESPOND_IF_READY);
	if (spdm_request->param1 != spdm_context->error_data.request_code) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	if (spdm_request->param1 == SPDM_RESPOND_IF_READY) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	if (spdm_request->param2 != spdm_context->error_data.token) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	get_response_func = NULL;
	get_response_func =
		spdm_get_response_func_via_request_code(spdm_request->param1);
	if (get_response_func == NULL) {
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_UNSUPPORTED_REQUEST,
			spdm_request->param1, response_size, response);
		return RETURN_SUCCESS;
	}
	status = get_response_func(spdm_context,
				   spdm_context->cache_spdm_request_size,
				   spdm_context->cache_spdm_request,
				   response_size, response);

	return status;
}
