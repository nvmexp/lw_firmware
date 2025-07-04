/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_requester_lib_internal.h"

/**
  This function sends GET_VERSION, GET_CAPABILITIES, NEGOTIATE_ALGORITHM
  to initialize the connection with SPDM responder.

  Before this function, the requester configuration data can be set via spdm_set_data.
  After this function, the negotiated configuration data can be got via spdm_get_data.

  @param  spdm_context                  A pointer to the SPDM context.

  @retval RETURN_SUCCESS               The connection is initialized successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_init_connection(IN void *context,
				   IN boolean get_version_only)
{
	return_status status;
	spdm_context_t *spdm_context;

	spdm_context = context;

	status = spdm_get_version(spdm_context);
	if (RETURN_ERROR(status)) {
		return status;
	}

	if (!get_version_only) {
		status = spdm_get_capabilities(spdm_context);
		if (RETURN_ERROR(status)) {
			return status;
		}
		status = spdm_negotiate_algorithms(spdm_context);
		if (RETURN_ERROR(status)) {
			return status;
		}
	}
	return RETURN_SUCCESS;
}

/**
  This function sends KEY_EXCHANGE/FINISH or PSK_EXCHANGE/PSK_FINISH
  to start an SPDM Session.

  If encapsulated mutual authentication is requested from the responder,
  this function also perform the encapsulated mutual authentication.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  use_psk                       FALSE means to use KEY_EXCHANGE/FINISH to start a session.
                                       TRUE means to use PSK_EXCHANGE/PSK_FINISH to start a session.
  @param  measurement_hash_type          The type of the measurement hash.
  @param  slot_id                      The number of slot for the certificate chain.
  @param  session_id                    The session ID of the session.
  @param  heartbeat_period              The heartbeat period for the session.
  @param  measurement_hash              A pointer to a destination buffer to store the measurement hash.

  @retval RETURN_SUCCESS               The SPDM session is started.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_start_session(IN void *context, IN boolean use_psk,
				 IN uint8 measurement_hash_type,
				 IN uint8 slot_id, OUT uint32 *session_id,
				 OUT uint8 *heartbeat_period,
				 OUT void *measurement_hash)
{
	return_status status;
	spdm_context_t *spdm_context;
	spdm_session_info_t *session_info;
	uint8 req_slot_id_param;

	spdm_context = context;

	if (!use_psk) {
		status = spdm_send_receive_key_exchange(
			spdm_context, measurement_hash_type, slot_id,
			session_id, heartbeat_period, &req_slot_id_param,
			measurement_hash);
		if (RETURN_ERROR(status)) {
			DEBUG((DEBUG_INFO,
			       "spdm_start_session - spdm_send_receive_key_exchange - %p\n",
			       status));
			return status;
		}

		session_info = spdm_get_session_info_via_session_id(
			spdm_context, *session_id);
		if (session_info == NULL) {
			ASSERT(FALSE);
			return RETURN_UNSUPPORTED;
		}

		switch (session_info->mut_auth_requested) {
		case 0:
			break;
		case SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED:
			break;
		case SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_ENCAP_REQUEST:
		case SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_GET_DIGESTS:
			status = spdm_encapsulated_request(
				spdm_context, session_id,
				session_info->mut_auth_requested,
				&req_slot_id_param);
			DEBUG((DEBUG_INFO,
			       "spdm_start_session - spdm_encapsulated_request - %p\n",
			       status));
			if (RETURN_ERROR(status)) {
				return status;
			}
			break;
		default:
			DEBUG((DEBUG_INFO,
			       "spdm_start_session - unknown mut_auth_requested - 0x%x\n",
			       session_info->mut_auth_requested));
			return RETURN_UNSUPPORTED;
		}

		if (req_slot_id_param == 0xF) {
			req_slot_id_param = 0xFF;
		}
		status = spdm_send_receive_finish(spdm_context, *session_id,
						  req_slot_id_param);
		DEBUG((DEBUG_INFO,
		       "spdm_start_session - spdm_send_receive_finish - %p\n",
		       status));
	} else {
		status = spdm_send_receive_psk_exchange(
			spdm_context, measurement_hash_type, session_id,
			heartbeat_period, measurement_hash);
		if (RETURN_ERROR(status)) {
			DEBUG((DEBUG_INFO,
			       "spdm_start_session - spdm_send_receive_psk_exchange - %p\n",
			       status));
			return status;
		}

		// send PSK_FINISH only if Responder supports context.
		if (spdm_is_capabilities_flag_supported(
			    spdm_context, TRUE, 0,
			    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP_RESPONDER_WITH_CONTEXT)) {
			status = spdm_send_receive_psk_finish(spdm_context,
							      *session_id);
			DEBUG((DEBUG_INFO,
			       "spdm_start_session - spdm_send_receive_psk_finish - %p\n",
			       status));
		}
	}
	return status;
}

/**
  This function sends END_SESSION
  to stop an SPDM Session.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session ID of the session.
  @param  end_session_attributes         The end session attribute for the session.

  @retval RETURN_SUCCESS               The SPDM session is stopped.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_stop_session(IN void *context, IN uint32 session_id,
				IN uint8 end_session_attributes)
{
	return_status status;
	spdm_context_t *spdm_context;

	spdm_context = context;

	status = spdm_send_receive_end_session(spdm_context, session_id,
					       end_session_attributes);
	DEBUG((DEBUG_INFO, "spdm_stop_session - %p\n", status));

	return status;
}

/**
  Send and receive an SPDM or APP message.

  The SPDM message can be a normal message or a selwred message in SPDM session.

  The APP message is encoded to a selwred message directly in SPDM session.
  The APP message format is defined by the transport layer.
  Take MCTP as example: APP message == MCTP header (MCTP_MESSAGE_TYPE_SPDM) + SPDM message

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicates if it is a selwred message protected via SPDM session.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
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
return_status spdm_send_receive_data(IN void *context, IN uint32 *session_id,
				     IN boolean is_app_message,
				     IN void *request, IN uintn request_size,
				     IN OUT void *response,
				     IN OUT uintn *response_size)
{
	return_status status;
	spdm_context_t *spdm_context;

	spdm_context = context;

	status = spdm_send_request(spdm_context, session_id, is_app_message,
				   request_size, request);
	if (RETURN_ERROR(status)) {
		return RETURN_DEVICE_ERROR;
	}

	status = spdm_receive_response(spdm_context, session_id, is_app_message,
				       response_size, response);
	if (RETURN_ERROR(status)) {
		return RETURN_DEVICE_ERROR;
	}

	return RETURN_SUCCESS;
}
