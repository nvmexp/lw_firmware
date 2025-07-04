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

// LWE (tandrewprinz): Including LWPU copyright, as file contains LWPU modifications.

#include "spdm_responder_lib_internal.h"

/**
  Process the SPDM KEY_EXCHANGE request and return the response.

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
return_status spdm_get_response_key_exchange(IN void *context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
{
	spdm_key_exchange_request_t *spdm_request;
	spdm_key_exchange_response_t *spdm_response;
	uintn dhe_key_size;
	uint32 measurement_summary_hash_size;
	uint32 signature_size;
	uint32 hmac_size;
	uint8 *ptr;
	uint16 opaque_data_length;
	boolean result;
	uint8 slot_id;
	uint32 session_id;
	void *dhe_context;
	spdm_session_info_t *session_info;
	uintn total_size;
	spdm_context_t *spdm_context;
	uint16 req_session_id;
	uint16 rsp_session_id;
	return_status status;
	uintn opaque_key_exchange_rsp_size;
	uint8 th1_hash_data[64];

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
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_EX_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP)) {
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_UNSUPPORTED_REQUEST,
			SPDM_KEY_EXCHANGE, response_size, response);
		return RETURN_SUCCESS;
	}
	if (spdm_context->connection_info.connection_state <
	    SPDM_CONNECTION_STATE_NEGOTIATED) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_UNEXPECTED_REQUEST,
					     0, response_size, response);
		return RETURN_SUCCESS;
	}

	if (spdm_is_capabilities_flag_supported(
		    spdm_context, FALSE,
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MUT_AUTH_CAP)) {
		if (spdm_context->encap_context.error_state !=
		    SPDM_STATUS_SUCCESS) {
			DEBUG((DEBUG_INFO,
			       "spdm_get_response_key_exchange fail due to Mutual Auth fail\n"));
			spdm_generate_error_response(
				spdm_context, SPDM_ERROR_CODE_ILWALID_REQUEST,
				0, response_size, response);
			return RETURN_SUCCESS;
		}
	}

	slot_id = spdm_request->header.param2;
	if ((slot_id != 0xFF) &&
	    (slot_id >= spdm_context->local_context.slot_count)) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	if (slot_id == 0xFF) {
		slot_id = spdm_context->local_context.provisioned_slot_id;
	}

	signature_size = spdm_get_asym_signature_size(
		spdm_context->connection_info.algorithm.base_asym_algo);
	hmac_size = spdm_get_hash_size(
		spdm_context->connection_info.algorithm.bash_hash_algo);
	dhe_key_size = spdm_get_dhe_pub_key_size(
		spdm_context->connection_info.algorithm.dhe_named_group);
	measurement_summary_hash_size = spdm_get_measurement_summary_hash_size(
		spdm_context, FALSE, spdm_request->header.param1);

	if (request_size < sizeof(spdm_key_exchange_request_t) + dhe_key_size +
				   sizeof(uint16)) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	opaque_data_length =
		*(uint16 *)((uint8 *)request +
			    sizeof(spdm_key_exchange_request_t) + dhe_key_size);
	if (request_size < sizeof(spdm_key_exchange_request_t) + dhe_key_size +
				   sizeof(uint16) + opaque_data_length) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	request_size = sizeof(spdm_key_exchange_request_t) + dhe_key_size +
		       sizeof(uint16) + opaque_data_length;

	ptr = (uint8 *)request + sizeof(spdm_key_exchange_request_t) +
	      dhe_key_size + sizeof(uint16);
	status = spdm_process_opaque_data_supported_version_data(
		spdm_context, opaque_data_length, ptr);
	if (RETURN_ERROR(status)) {
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	opaque_key_exchange_rsp_size =
		spdm_get_opaque_data_version_selection_data_size(spdm_context);

	if (spdm_is_capabilities_flag_supported(
		    spdm_context, FALSE,
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP)) {
		hmac_size = 0;
	}

	total_size = sizeof(spdm_key_exchange_response_t) + dhe_key_size +
		     measurement_summary_hash_size + sizeof(uint16) +
		     opaque_key_exchange_rsp_size + signature_size + hmac_size;

	ASSERT(*response_size >= total_size);
	*response_size = total_size;
	zero_mem(response, *response_size);
	spdm_response = response;

	spdm_response->header.spdm_version = SPDM_MESSAGE_VERSION_11;
	spdm_response->header.request_response_code = SPDM_KEY_EXCHANGE_RSP;
	spdm_response->header.param1 = 0;

	req_session_id = spdm_request->req_session_id;
	rsp_session_id = spdm_allocate_rsp_session_id(spdm_context);
	if (rsp_session_id == (ILWALID_SESSION_ID & 0xFFFF)) {
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_SESSION_LIMIT_EXCEEDED, 0,
			response_size, response);
		return RETURN_SUCCESS;
	}
	session_id = (req_session_id << 16) | rsp_session_id;
	session_info = spdm_assign_session_id(spdm_context, session_id, FALSE);
	if (session_info == NULL) {
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_SESSION_LIMIT_EXCEEDED, 0,
			response_size, response);
		return RETURN_SUCCESS;
	}

	status = spdm_append_message_k(session_info, request, request_size);
	if (RETURN_ERROR(status)) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	spdm_response->rsp_session_id = rsp_session_id;

	spdm_response->mut_auth_requested = 0;
	if (spdm_is_capabilities_flag_supported(
		    spdm_context, FALSE,
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MUT_AUTH_CAP) &&
	    (spdm_is_capabilities_flag_supported(
		     spdm_context, FALSE,
		     SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP, 0) ||
	     spdm_is_capabilities_flag_supported(
		     spdm_context, FALSE,
		     SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PUB_KEY_ID_CAP, 0))) {
		spdm_response->mut_auth_requested =
			spdm_context->local_context.mut_auth_requested;
	}
	if (spdm_response->mut_auth_requested != 0) {
//
// LWE (tandrewprinz): We will not support mutual authentication.
// Rather than stub out function, just return failure.
//
        return RETURN_UNSUPPORTED;
//		spdm_init_mut_auth_encap_state(
//			context, spdm_response->mut_auth_requested);
//		spdm_response->req_slot_id_param =
//			(spdm_context->encap_context.req_slot_id & 0xF);
	} else {
		spdm_response->req_slot_id_param = 0;
	}

	spdm_get_random_number(SPDM_RANDOM_DATA_SIZE,
			       spdm_response->random_data);

	ptr = (void *)(spdm_response + 1);
	dhe_context = spdm_selwred_message_dhe_new(
		spdm_context->connection_info.algorithm.dhe_named_group);
	spdm_selwred_message_dhe_generate_key(
		spdm_context->connection_info.algorithm.dhe_named_group,
		dhe_context, ptr, &dhe_key_size);
	DEBUG((DEBUG_INFO, "Calc SelfKey (0x%x):\n", dhe_key_size));
	internal_dump_hex(ptr, dhe_key_size);

	DEBUG((DEBUG_INFO, "Calc peer_key (0x%x):\n", dhe_key_size));
	internal_dump_hex((uint8 *)request +
				  sizeof(spdm_key_exchange_request_t),
			  dhe_key_size);

	result = spdm_selwred_message_dhe_compute_key(
		spdm_context->connection_info.algorithm.dhe_named_group,
		dhe_context,
		(uint8 *)request + sizeof(spdm_key_exchange_request_t),
		dhe_key_size, session_info->selwred_message_context);
	spdm_selwred_message_dhe_free(
		spdm_context->connection_info.algorithm.dhe_named_group,
		dhe_context);
	if (!result) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	ptr += dhe_key_size;

	result = spdm_generate_measurement_summary_hash(
		spdm_context, FALSE, spdm_request->header.param1, ptr);
	if (!result) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	ptr += measurement_summary_hash_size;

	*(uint16 *)ptr = (uint16)opaque_key_exchange_rsp_size;
	ptr += sizeof(uint16);
	status = spdm_build_opaque_data_version_selection_data(
		spdm_context, &opaque_key_exchange_rsp_size, ptr);
	ASSERT_RETURN_ERROR(status);
	ptr += opaque_key_exchange_rsp_size;

	spdm_context->connection_info.local_used_cert_chain_buffer =
		spdm_context->local_context.local_cert_chain_provision[slot_id];
	spdm_context->connection_info.local_used_cert_chain_buffer_size =
		spdm_context->local_context
			.local_cert_chain_provision_size[slot_id];

	status = spdm_append_message_k(session_info, spdm_response,
				       (uintn)ptr - (uintn)spdm_response);
	if (RETURN_ERROR(status)) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	result = spdm_generate_key_exchange_rsp_signature(spdm_context,
							  session_info, ptr);
	if (!result) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_UNSUPPORTED_REQUEST,
			SPDM_KEY_EXCHANGE_RSP, response_size, response);
		return RETURN_SUCCESS;
	}

	status = spdm_append_message_k(session_info, ptr, signature_size);
	if (RETURN_ERROR(status)) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	DEBUG((DEBUG_INFO, "spdm_generate_session_handshake_key[%x]\n",
	       session_id));
	status = spdm_callwlate_th1_hash(spdm_context, session_info, FALSE,
					 th1_hash_data);
	if (RETURN_ERROR(status)) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}
	status = spdm_generate_session_handshake_key(
		session_info->selwred_message_context, th1_hash_data);
	if (RETURN_ERROR(status)) {
		spdm_free_session_id(spdm_context, session_id);
		spdm_generate_error_response(spdm_context,
					     SPDM_ERROR_CODE_ILWALID_REQUEST, 0,
					     response_size, response);
		return RETURN_SUCCESS;
	}

	ptr += signature_size;

	if (!spdm_is_capabilities_flag_supported(
		    spdm_context, FALSE,
		    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP,
		    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP)) {
		result = spdm_generate_key_exchange_rsp_hmac(spdm_context,
							     session_info, ptr);
		if (!result) {
			spdm_free_session_id(spdm_context, session_id);
			spdm_generate_error_response(
				spdm_context,
				SPDM_ERROR_CODE_UNSUPPORTED_REQUEST,
				SPDM_KEY_EXCHANGE_RSP, response_size, response);
			return RETURN_SUCCESS;
		}
		status = spdm_append_message_k(session_info, ptr, hmac_size);
		if (RETURN_ERROR(status)) {
			spdm_free_session_id(spdm_context, session_id);
			spdm_generate_error_response(
				spdm_context, SPDM_ERROR_CODE_ILWALID_REQUEST,
				0, response_size, response);
			return RETURN_SUCCESS;
		}

// LWE (tandrewprinz) LW_COVERITY: Result is unused, keep commented out.
//		ptr += hmac_size;
	}

	session_info->mut_auth_requested = spdm_response->mut_auth_requested;
	spdm_set_session_state(spdm_context, session_id,
			       SPDM_SESSION_STATE_HANDSHAKING);

	return RETURN_SUCCESS;
}
