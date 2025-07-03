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
// All functions declarations have had their sections specified via GCC_ATTRIB_SECTION.
//

#ifndef __SPDM_RESPONDER_LIB_H__
#define __SPDM_RESPONDER_LIB_H__

#include <library/spdm_common_lib.h>

/**
  Process the SPDM or APP request and return the response.

  The APP message is encoded to a selwred message directly in SPDM session.
  The APP message format is defined by the transport layer.
  Take MCTP as example: APP message == MCTP header (MCTP_MESSAGE_TYPE_SPDM) + SPDM message

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicates if it is a selwred message protected via SPDM session.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
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
typedef return_status (*spdm_get_response_func)(
	IN void *spdm_context, IN uint32 *session_id, IN boolean is_app_message,
	IN uintn request_size, IN void *request, IN OUT uintn *response_size,
	OUT void *response);

/**
  Register an SPDM or APP message process function.

  If the default message process function cannot handle the message,
  this function will be ilwoked.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  get_response_func              The function to process the encapsuled message.
**/
void spdm_register_get_response_func(
	IN void *spdm_context, IN spdm_get_response_func get_response_func)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_register_get_response_func");

//
// LWE (tandrewprinz) LW_REMOVE_FP: Use direct function calls for handling APP messages.
// Consumer is responsible for implementing "spdm_dispatch_application_message".
//
/**
    Process the APP request and return the response.

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
return_status spdm_dispatch_application_message(IN void *spdm_context,
    IN uint32 *session_id, IN uintn request_size, IN void *request,
    IN OUT uintn *response_size, OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_dispatch_application_message");

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
return_status spdm_process_request(IN void *spdm_context,
				   OUT uint32 **session_id,
				   OUT boolean *is_app_message,
				   IN uintn request_size, IN void *request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_request");

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
return_status spdm_build_response(IN void *spdm_context, IN uint32 *session_id,
				  IN boolean is_app_message,
				  IN OUT uintn *response_size,
				  OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_build_response");

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
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_message");

/**
  This is the main dispatch function in SPDM responder.

  It receives one request message, processes it and sends the response message.

  It should be called in a while loop or an timer/interrupt handler.

  @param  spdm_context                  A pointer to the SPDM context.

  @retval RETURN_SUCCESS               One SPDM request message is processed.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_UNSUPPORTED           One request message is not supported.
**/
return_status spdm_responder_dispatch_message(IN void *spdm_context)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_responder_dispatch_message");

/**
  Generate ERROR message.

  This function can be called in spdm_get_response_func.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  error_code                    The error code of the message.
  @param  error_data                    The error data of the message.
  @param  spdm_response_size             size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  spdm_response                 A pointer to the response data.

  @retval RETURN_SUCCESS               The error message is generated.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status spdm_generate_error_response(IN void *spdm_context,
					   IN uint8 error_code,
					   IN uint8 error_data,
					   IN OUT uintn *spdm_response_size,
					   OUT void *spdm_response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_error_response");

/**
  Generate ERROR message with extended error data.

  This function can be called in spdm_get_response_func.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  error_code                    The error code of the message.
  @param  error_data                    The error data of the message.
  @param  extended_error_data_size        The size in bytes of the extended error data.
  @param  extended_error_data            A pointer to the extended error data.
  @param  spdm_response_size             size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  spdm_response                 A pointer to the response data.

  @retval RETURN_SUCCESS               The error message is generated.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status spdm_generate_extended_error_response(
	IN void *context, IN uint8 error_code, IN uint8 error_data,
	IN uintn extended_error_data_size, IN uint8 *extended_error_data,
	IN OUT uintn *spdm_response_size, OUT void *spdm_response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_extended_error_response");

/**
  Notify the session state to a session APP.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session_id of a session.
  @param  session_state                 The state of a session.
**/
typedef void (*spdm_session_state_callback_func)(
	IN void *spdm_context, IN uint32 session_id,
	IN spdm_session_state_t session_state);

/**
  Register an SPDM state callback function.

  This function can be called multiple times to let different session APPs register its own callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  spdm_session_state_callback     The function to be called in SPDM session state change.

  @retval RETURN_SUCCESS          The callback is registered.
  @retval RETURN_ALREADY_STARTED  No enough memory to register the callback.
**/
return_status spdm_register_session_state_callback_func(
	IN void *spdm_context,
	IN spdm_session_state_callback_func spdm_session_state_callback)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_register_session_state_callback_func");

/**
  Notify the connection state to an SPDM context register.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  connection_state              Indicate the SPDM connection state.
**/
typedef void (*spdm_connection_state_callback_func)(
	IN void *spdm_context, IN spdm_connection_state_t connection_state);

/**
  Register an SPDM connection state callback function.

  This function can be called multiple times to let different register its own callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  spdm_connection_state_callback  The function to be called in SPDM connection state change.

  @retval RETURN_SUCCESS          The callback is registered.
  @retval RETURN_ALREADY_STARTED  No enough memory to register the callback.
**/
return_status spdm_register_connection_state_callback_func(
	IN void *spdm_context,
	IN spdm_connection_state_callback_func spdm_connection_state_callback)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_register_connection_state_callback_func");

//
// LWE (tandrewprinz) LW_REMOVE_FP: Use direct function calls for connection
// and session state callbacks. Consumer is responsible for implementing
// "spdm_dispatch_connection_state_callback" and "spdm_dispatch_session_state_callback".
//
/**
  Notify a given callback function upon a change in connection state.

  @note This callback has no way to directly return failure, keep
  this in mind when implementing.

  @param  spdm_context      A pointer to the SPDM context.
  @param  connection_state  Indicate the SPDM connection state.
**/
void spdm_dispatch_connection_state_callback(
    IN void *spdm_context, IN spdm_connection_state_t connection_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_dispatch_connection_state_callback");

/**
  Notify a given callback function upon a change in session state.

  @note This callback has no way to directly return failure, keep
  this in mind when implementing.

  @param  spdm_context   A pointer to the SPDM context.
  @param  session_id     The session_id of a session.
  @param  session_state  The state of a session.
**/
void spdm_dispatch_session_state_callback(
    IN void *spdm_context, IN uint32 session_id,
    IN spdm_session_state_t session_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_dispatch_session_state_callback");

/**
  This function initializes the key_update encapsulated state.

  @param  spdm_context                  A pointer to the SPDM context.
**/
void spdm_init_key_update_encap_state(IN void *spdm_context)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_init_key_update_encap_state");

#endif