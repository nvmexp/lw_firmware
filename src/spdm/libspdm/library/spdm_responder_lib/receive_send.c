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

//
// LWE (tandrewprinz) LW_REMOVE_FP:
// Structures and functions below use function pointers to call response
// message handlers. Remove and replace with direct function calls.
//
//  typedef struct {
//      uint8 request_response_code;
//      spdm_get_spdm_response_func get_response_func;
//  } spdm_get_response_struct_t;
//
//  spdm_get_response_struct_t mSpdmGetResponseStruct[] = {
//      { SPDM_GET_VERSION, spdm_get_response_version },
//      { SPDM_GET_CAPABILITIES, spdm_get_response_capabilities },
//      { SPDM_NEGOTIATE_ALGORITHMS, spdm_get_response_algorithms },
//      { SPDM_GET_DIGESTS, spdm_get_response_digests },
//      { SPDM_GET_CERTIFICATE, spdm_get_response_certificate },
//      { SPDM_CHALLENGE, spdm_get_response_challenge_auth },
//      { SPDM_GET_MEASUREMENTS, spdm_get_response_measurements },
//      { SPDM_KEY_EXCHANGE, spdm_get_response_key_exchange },
//      { SPDM_PSK_EXCHANGE, spdm_get_response_psk_exchange },
//      { SPDM_GET_ENCAPSULATED_REQUEST,
//        spdm_get_response_encapsulated_request },
//      { SPDM_DELIVER_ENCAPSULATED_RESPONSE,
//        spdm_get_response_encapsulated_response_ack },
//      { SPDM_RESPOND_IF_READY, spdm_get_response_respond_if_ready },
//
//      { SPDM_FINISH, spdm_get_response_finish },
//      { SPDM_PSK_FINISH, spdm_get_response_psk_finish },
//      { SPDM_END_SESSION, spdm_get_response_end_session },
//      { SPDM_HEARTBEAT, spdm_get_response_heartbeat },
//      { SPDM_KEY_UPDATE, spdm_get_response_key_update },
//  };
//  
//  /**
//    Return the GET_SPDM_RESPONSE function via request code.
//
//    @param  request_code                  The SPDM request code.
//
//    @return GET_SPDM_RESPONSE function according to the request code.
//  **/
//  spdm_get_spdm_response_func
//  spdm_get_response_func_via_request_code(IN uint8 request_code)
//  {
//      uintn index;
//
//      ASSERT(request_code != SPDM_RESPOND_IF_READY);
//      for (index = 0; index < sizeof(mSpdmGetResponseStruct) /
//                      sizeof(mSpdmGetResponseStruct[0]);
//           index++) {
//          if (request_code ==
//              mSpdmGetResponseStruct[index].request_response_code) {
//              return mSpdmGetResponseStruct[index].get_response_func;
//          }
//      }
//      return NULL;
//  }
//
//  /**
//    Return the GET_SPDM_RESPONSE function via last request.
//
//    @param  spdm_context                  The SPDM context for the device.
//
//    @return GET_SPDM_RESPONSE function according to the last request.
//  **/
//  spdm_get_spdm_response_func
//  spdm_get_response_func_via_last_request(IN spdm_context_t *spdm_context)
//  {
//      spdm_message_header_t *spdm_request;
//  
//      spdm_request = (void *)spdm_context->last_spdm_request;
//      return spdm_get_response_func_via_request_code(
//          spdm_request->request_response_code);
//  }


//
// LWE (tandrewprinz) LW_REMOVE_FP:
// Provide custom function spdm_call_response_func to call response message
// handlers directly, rather than using FPs.
//
/**
  Call SPDM response message handler, based on request message present
  in last_spdm_request.

  @param  context                      A pointer to the SPDM context.
  @param  request_size                 Size in bytes of the request data.
  @param  request                      A pointer to the request data.
  @param  response_size                Size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  response                     A pointer to the response data.
  @retval RETURN_SUCCESS               The request is processed and the response is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_call_response_func(IN void *context,
	IN uintn request_size, IN void *request,
	IN OUT uintn *response_size, OUT void *response)
{
  spdm_context_t *spdm_context;
  spdm_message_header_t *spdm_request;
  return_status status;
  spdm_context = context;
  spdm_request = (void *)spdm_context->last_spdm_request;
  status = RETURN_SUCCESS;

  switch (spdm_request->request_response_code) {
    case SPDM_GET_VERSION:
      status = spdm_get_response_version(context,
            request_size, request, response_size, response);
      break;
    case SPDM_GET_CAPABILITIES:
      status = spdm_get_response_capabilities(context,
            request_size, request, response_size, response);
      break;
    case SPDM_NEGOTIATE_ALGORITHMS:
      status = spdm_get_response_algorithms(context,
            request_size, request, response_size, response);
      break;
    case SPDM_GET_DIGESTS:
      status = spdm_get_response_digests(context,
            request_size, request, response_size, response);
      break;
    case SPDM_GET_CERTIFICATE:
      status = spdm_get_response_certificate(context,
            request_size, request, response_size, response);
      break;
    case SPDM_KEY_EXCHANGE:
      status = spdm_get_response_key_exchange(context,
            request_size, request, response_size, response);
      break;
    case SPDM_FINISH:
      status = spdm_get_response_finish(context,
            request_size, request, response_size, response);
      break;
    case SPDM_GET_MEASUREMENTS:
      status = spdm_get_response_measurements(context,
            request_size, request, response_size, response);
      break;
    case SPDM_KEY_UPDATE:
      status = spdm_get_response_key_update(context,
            request_size, request, response_size, response);
      break;
	case SPDM_END_SESSION:
      status = spdm_get_response_end_session(context,
            request_size, request, response_size, response);
	  break;

// LWE(eddichang): Adding vendor defined request handler
#ifdef LIBSPDM_VENDOR_DEFINED_MESSAGES_SUPPORT
    case SPDM_VENDOR_DEFINED_REQUEST:
      status = spdm_get_response_vendor_defined(context,
            request_size, request, response_size, response);
      break;
#endif
	//
	// NOTE: Be careful adding more response handlers to this function, as
	// code size of library will increase for all Falcons. Consider moving this
	// function to Falcon-specific implementations if more handlers necessary.
	//
  default:
      //
      // LWE (tandrewprinz) APM-TODO CONFCOMP-397: Determine if we want any
      // special handling for unsupported messages.
      //
      status = RETURN_NOT_FOUND;
      break;
  }

  return status;
}

/**
  Process a SPDM request from a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the request is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
  @param  request_size                  size in bytes of the request data buffer.
  @param  request                      A pointer to a destination buffer to store the request.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM request is received successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM request is received from the device.
**/
return_status spdm_process_request(IN void *context, OUT uint32 **session_id,
				   OUT boolean *is_app_message,
				   IN uintn request_size, IN void *request)
{
	spdm_context_t *spdm_context;
	return_status status;
    spdm_session_info_t *session_info;
	uint32 *message_session_id;

	spdm_context = context;

	if (request == NULL) {
		return RETURN_ILWALID_PARAMETER;
	}
	if (request_size == 0) {
		return RETURN_ILWALID_PARAMETER;
	}

	DEBUG((DEBUG_INFO, "SpdmReceiveRequest[.] ...\n"));

	message_session_id = NULL;
	spdm_context->last_spdm_request_session_id_valid = FALSE;
	spdm_context->last_spdm_request_size =
		sizeof(spdm_context->last_spdm_request);
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct function call.
    status = spdm_transport_decode_message(
        spdm_context, &message_session_id, is_app_message, TRUE,
        request_size, request, &spdm_context->last_spdm_request_size,
        spdm_context->last_spdm_request);
//  status = spdm_context->transport_decode_message(
//      spdm_context, &message_session_id, is_app_message, TRUE,
//      request_size, request, &spdm_context->last_spdm_request_size,
//      spdm_context->last_spdm_request);
	if (RETURN_ERROR(status)) {
		DEBUG((DEBUG_INFO, "transport_decode_message : %p\n", status));
		if (spdm_context->last_spdm_error.error_code != 0) {
			//
			// If the SPDM error code is Non-Zero, that means we need send the error message back to requester.
			// In this case, we need return SUCCESS and let caller ilwoke spdm_build_response() to send an ERROR message.
			//
			*session_id = &spdm_context->last_spdm_error.session_id;
			*is_app_message = FALSE;
			return RETURN_SUCCESS;
		}
		return status;
	}
	if (spdm_context->last_spdm_request_size <
	    sizeof(spdm_message_header_t)) {
		return RETURN_UNSUPPORTED;
	}

	*session_id = message_session_id;

	if (message_session_id != NULL) {
		session_info = spdm_get_session_info_via_session_id(
			spdm_context, *message_session_id);
		if (session_info == NULL) {
			return RETURN_UNSUPPORTED;
		}
		spdm_context->last_spdm_request_session_id =
			*message_session_id;
		spdm_context->last_spdm_request_session_id_valid = TRUE;
	}

	DEBUG((DEBUG_INFO, "SpdmReceiveRequest[%x] (0x%x): \n",
	       (message_session_id != NULL) ? *message_session_id : 0,
	       spdm_context->last_spdm_request_size));
	internal_dump_hex((uint8 *)spdm_context->last_spdm_request,
			  spdm_context->last_spdm_request_size);

	return RETURN_SUCCESS;
}

/**
  Notify the session state to a session APP.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session_id of a session.
  @param  session_state                 The state of a session.
**/
void spdm_trigger_session_state_callback(IN spdm_context_t *spdm_context,
					 IN uint32 session_id,
					 IN spdm_session_state_t session_state)
{
// LWE (tandrewprinz) LW_REMOVE_FP: Directly call callbacks, rather than use FP.
	spdm_dispatch_session_state_callback(spdm_context, session_id,
		session_state);
//
//  uintn index;
//
//  for (index = 0; index < MAX_SPDM_SESSION_STATE_CALLBACK_NUM; index++) {
//      if (spdm_context->spdm_session_state_callback[index] != 0) {
//          ((spdm_session_state_callback_func)spdm_context
//               ->spdm_session_state_callback[index])(
//              spdm_context, session_id, session_state);
//      }
//  }
}

/**
  Set session_state to an SPDM selwred message context and trigger callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicate the SPDM session ID.
  @param  session_state                 Indicate the SPDM session state.
*/
void spdm_set_session_state(IN spdm_context_t *spdm_context,
			    IN uint32 session_id,
			    IN spdm_session_state_t session_state)
{
	spdm_session_info_t *session_info;
	spdm_session_state_t old_session_state;

	session_info =
		spdm_get_session_info_via_session_id(spdm_context, session_id);
	if (session_info == NULL) {
		ASSERT(FALSE);
		return;
	}

	old_session_state = spdm_selwred_message_get_session_state(
		session_info->selwred_message_context);
	if (old_session_state != session_state) {
		spdm_selwred_message_set_session_state(
			session_info->selwred_message_context, session_state);
		spdm_trigger_session_state_callback(
			spdm_context, session_info->session_id, session_state);
	}
}

/**
  Notify the connection state to an SPDM context register.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  connection_state              Indicate the SPDM connection state.
**/
void spdm_trigger_connection_state_callback(IN spdm_context_t *spdm_context,
					    IN spdm_connection_state_t
						    connection_state)
{
// LWE (tandrewprinz) LW_REMOVE_FP: Directly call callbacks, rather than use FP.
	spdm_dispatch_connection_state_callback(spdm_context, connection_state);

//  uintn index;
//
//  for (index = 0; index < MAX_SPDM_CONNECTION_STATE_CALLBACK_NUM;
//       index++) {
//      if (spdm_context->spdm_connection_state_callback[index] != 0) {
//          ((spdm_connection_state_callback_func)spdm_context
//               ->spdm_connection_state_callback[index])(
//              spdm_context, connection_state);
//      }
//  }
}

/**
  Set connection_state to an SPDM context and trigger callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  connection_state              Indicate the SPDM connection state.
*/
void spdm_set_connection_state(IN spdm_context_t *spdm_context,
			       IN spdm_connection_state_t connection_state)
{
	if (spdm_context->connection_info.connection_state !=
	    connection_state) {
		spdm_context->connection_info.connection_state =
			connection_state;
		spdm_trigger_connection_state_callback(spdm_context,
						       connection_state);
	}
}

/**
  Build a SPDM response to a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the response is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
  @param  response_size                 size in bytes of the response data buffer.
  @param  response                     A pointer to a destination buffer to store the response.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM response is sent successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM response is sent to the device.
**/
return_status spdm_build_response(IN void *context, IN uint32 *session_id,
				  IN boolean is_app_message,
				  IN OUT uintn *response_size,
				  OUT void *response)
{
	spdm_context_t *spdm_context;
	uint8 my_response[MAX_SPDM_MESSAGE_BUFFER_SIZE];
	uintn my_response_size;
	return_status status;
// LWE (tandrewprinz) LW_REMOVE_FP: Remove FP struct.
//  spdm_get_spdm_response_func get_response_func;
    spdm_session_info_t *session_info;
	spdm_message_header_t *spdm_request;
	spdm_message_header_t *spdm_response;

	spdm_context = context;

    // LWE (tandrewprinz): Default value for status to appease compiler.
    status = RETURN_SUCCESS;

	if (spdm_context->last_spdm_error.error_code != 0) {
		//
		// Error in spdm_process_request(), and we need send error message directly.
		//
		my_response_size = sizeof(my_response);
		zero_mem(my_response, sizeof(my_response));
		switch (spdm_context->last_spdm_error.error_code) {
		case SPDM_ERROR_CODE_DECRYPT_ERROR:
			// session ID is valid. Use it to encrypt the error message.
			spdm_generate_error_response(
				spdm_context, SPDM_ERROR_CODE_DECRYPT_ERROR, 0,
				&my_response_size, my_response);
			break;
		case SPDM_ERROR_CODE_ILWALID_SESSION:
			// don't use session ID, because we dont know which right session ID should be used.
			spdm_generate_extended_error_response(
				spdm_context, SPDM_ERROR_CODE_ILWALID_SESSION,
				0, sizeof(uint32), (void *)session_id,
				&my_response_size, my_response);
			session_id = NULL;
			break;
		default:
			ASSERT(FALSE);
			return RETURN_UNSUPPORTED;
		}

		DEBUG((DEBUG_INFO, "SpdmSendResponse[%x] (0x%x): \n",
		       (session_id != NULL) ? *session_id : 0,
		       my_response_size));
		internal_dump_hex(my_response, my_response_size);

// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct function call.
        status = spdm_transport_encode_message(
            spdm_context, session_id, FALSE, FALSE,
            my_response_size, my_response, response_size, response);
//      status = spdm_context->transport_encode_message(
//          spdm_context, session_id, FALSE, FALSE,
//          my_response_size, my_response, response_size, response);
		if (RETURN_ERROR(status)) {
			DEBUG((DEBUG_INFO, "transport_encode_message : %p\n",
			       status));
			return status;
		}

		zero_mem(&spdm_context->last_spdm_error,
			 sizeof(spdm_context->last_spdm_error));
		return RETURN_SUCCESS;
	}

	if (session_id != NULL) {
		session_info = spdm_get_session_info_via_session_id(
			spdm_context, *session_id);
		if (session_info == NULL) {
			ASSERT(FALSE);
			return RETURN_UNSUPPORTED;
		}
	}

	if (response == NULL) {
		return RETURN_ILWALID_PARAMETER;
	}
	if (response_size == NULL) {
		return RETURN_ILWALID_PARAMETER;
	}
	if (*response_size == 0) {
		return RETURN_ILWALID_PARAMETER;
	}

	DEBUG((DEBUG_INFO, "SpdmSendResponse[%x] ...\n",
	       (session_id != NULL) ? *session_id : 0));

	spdm_request = (void *)spdm_context->last_spdm_request;
	if (spdm_context->last_spdm_request_size == 0) {
		return RETURN_NOT_READY;
	}

	my_response_size = sizeof(my_response);
	zero_mem(my_response, sizeof(my_response));
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct function call.
//  get_response_func = NULL;
	if (!is_app_message) {
        status = spdm_call_response_func(spdm_context,
            spdm_context->last_spdm_request_size,
            spdm_context->last_spdm_request,
            &my_response_size, my_response);
//      get_response_func =
//          spdm_get_response_func_via_last_request(spdm_context);
//      if (get_response_func != NULL) {
//          status = get_response_func(
//              spdm_context,
//              spdm_context->last_spdm_request_size,
//              spdm_context->last_spdm_request,
//              &my_response_size, my_response);
//      }
	}
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct function call.
    if (is_app_message) {
		status = spdm_dispatch_application_message(
			spdm_context, session_id,
			spdm_context->last_spdm_request_size,
			spdm_context->last_spdm_request,
			&my_response_size, my_response);
	}
//  if (is_app_message || (get_response_func == NULL)) {
//      if (spdm_context->get_response_func != 0) {
//         status = ((spdm_get_response_func)
//                   spdm_context->get_response_func)(
//              spdm_context, session_id, is_app_message,
//              spdm_context->last_spdm_request_size,
//              spdm_context->last_spdm_request,
//              &my_response_size, my_response);
//      } else {
//         status = RETURN_NOT_FOUND;
//      }
//  }
	if (status != RETURN_SUCCESS) {
		spdm_generate_error_response(
			spdm_context, SPDM_ERROR_CODE_UNSUPPORTED_REQUEST,
			spdm_request->request_response_code, &my_response_size,
			my_response);
	}

	DEBUG((DEBUG_INFO, "SpdmSendResponse[%x] (0x%x): \n",
	       (session_id != NULL) ? *session_id : 0, my_response_size));
	internal_dump_hex(my_response, my_response_size);

// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct function call.
    status = spdm_transport_encode_message(
        spdm_context, session_id, is_app_message, FALSE,
        my_response_size, my_response, response_size, response);
//  status = spdm_context->transport_encode_message(
//      spdm_context, session_id, is_app_message, FALSE,
//      my_response_size, my_response, response_size, response);
	if (RETURN_ERROR(status)) {
		DEBUG((DEBUG_INFO, "transport_encode_message : %p\n", status));
		return status;
	}

	spdm_response = (void *)my_response;
	if (session_id != NULL) {
		switch (spdm_response->request_response_code) {
		case SPDM_FINISH_RSP:
			if (!spdm_is_capabilities_flag_supported(
				    spdm_context, FALSE,
				    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP,
				    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP)) {
				spdm_set_session_state(
					spdm_context, *session_id,
					SPDM_SESSION_STATE_ESTABLISHED);
			}
			break;
		case SPDM_PSK_FINISH_RSP:
			spdm_set_session_state(spdm_context, *session_id,
					       SPDM_SESSION_STATE_ESTABLISHED);
			break;
		case SPDM_END_SESSION_ACK:
			spdm_set_session_state(spdm_context, *session_id,
					       SPDM_SESSION_STATE_NOT_STARTED);
			spdm_free_session_id(spdm_context, *session_id);
			break;
		}
	} else {
		switch (spdm_response->request_response_code) {
		case SPDM_FINISH_RSP:
			if (spdm_is_capabilities_flag_supported(
				    spdm_context, FALSE,
				    SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP,
				    SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP)) {
				spdm_set_session_state(
					spdm_context,
					spdm_context->latest_session_id,
					SPDM_SESSION_STATE_ESTABLISHED);
			}
			break;
		}
	}

	return RETURN_SUCCESS;
}

//
// LWE (tandrewprinz) LW_REMOVE_FP: Remove FP registration functions.
// We have no use for them, removing to avoid accidental use.
//
//  /**
//    Register an SPDM or APP message process function.
//  
//    If the default message process function cannot handle the message,
//    this function will be ilwoked.
//  
//    @param  spdm_context                  A pointer to the SPDM context.
//    @param  get_response_func              The function to process the encapsuled message.
//  **/
//  void spdm_register_get_response_func(
//      IN void *context, IN spdm_get_response_func get_response_func)
//  {
//      spdm_context_t *spdm_context;
//  
//      spdm_context = context;
//      spdm_context->get_response_func = (uintn)get_response_func;
//  
//      return;
//  }
//  
//  /**
//    Register an SPDM session state callback function.
//  
//    This function can be called multiple times to let different session APPs register its own callback.
//  
//    @param  spdm_context                  A pointer to the SPDM context.
//    @param  spdm_session_state_callback     The function to be called in SPDM session state change.
//  
//    @retval RETURN_SUCCESS          The callback is registered.
//    @retval RETURN_ALREADY_STARTED  No enough memory to register the callback.
//  **/
//  return_status spdm_register_session_state_callback_func(
//      IN void *context,
//      IN spdm_session_state_callback_func spdm_session_state_callback)
//  {
//      spdm_context_t *spdm_context;
//      uintn index;
//  
//      spdm_context = context;
//      for (index = 0; index < MAX_SPDM_SESSION_STATE_CALLBACK_NUM; index++) {
//          if (spdm_context->spdm_session_state_callback[index] == 0) {
//              spdm_context->spdm_session_state_callback[index] =
//                  (uintn)spdm_session_state_callback;
//              return RETURN_SUCCESS;
//          }
//      }
//      ASSERT(FALSE);
//  
//      return RETURN_ALREADY_STARTED;
//  }
//  
//  /**
//    Register an SPDM connection state callback function.
//  
//    This function can be called multiple times to let different register its own callback.
//  
//    @param  spdm_context                  A pointer to the SPDM context.
//    @param  spdm_connection_state_callback  The function to be called in SPDM connection state change.
//  
//    @retval RETURN_SUCCESS          The callback is registered.
//    @retval RETURN_ALREADY_STARTED  No enough memory to register the callback.
//  **/
//  return_status spdm_register_connection_state_callback_func(
//      IN void *context,
//      IN spdm_connection_state_callback_func spdm_connection_state_callback)
//  {
//      spdm_context_t *spdm_context;
//      uintn index;
//  
//      spdm_context = context;
//      for (index = 0; index < MAX_SPDM_CONNECTION_STATE_CALLBACK_NUM;
//           index++) {
//          if (spdm_context->spdm_connection_state_callback[index] == 0) {
//              spdm_context->spdm_connection_state_callback[index] =
//                  (uintn)spdm_connection_state_callback;
//              return RETURN_SUCCESS;
//          }
//      }
//      ASSERT(FALSE);
//  
//      return RETURN_ALREADY_STARTED;
//  }
