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
// All functions have had their sections specified via GCC_ATTRIB_SECTION.
// We do not expect to use these, but adding sections for safety.
//

#ifndef __SPDM_REQUESTER_LIB_INTERNAL_H__
#define __SPDM_REQUESTER_LIB_INTERNAL_H__

#include <library/spdm_requester_lib.h>
#include <library/spdm_selwred_message_lib.h>
#include "spdm_common_lib_internal.h"

/**
  This function handles simple error code.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  error_code                    Indicate the error code.

  @retval RETURN_NO_RESPONSE           If the error code is BUSY.
  @retval RETURN_DEVICE_ERROR          If the error code is REQUEST_RESYNCH or others.
**/
return_status spdm_handle_simple_error_response(IN void *context,
						IN uint8 error_code)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_handle_simple_error_response");

/**
  This function handles the error response.

  The SPDM response code must be SPDM_ERROR.
  For error code RESPONSE_NOT_READY, this function sends RESPOND_IF_READY and receives an expected SPDM response.
  For error code BUSY, this function shrinks the managed buffer, and return RETURN_NO_RESPONSE.
  For error code REQUEST_RESYNCH, this function shrinks the managed buffer, clears connection_state, and return RETURN_DEVICE_ERROR.
  For any other error code, this function shrinks the managed buffer, and return RETURN_DEVICE_ERROR.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  managed_buffer_t                The managed buffer to be shrinked.
  @param  shrink_buffer_size             The size in bytes of the size of the buffer to be shrinked.
  @param  response_size                 The size of the response.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned.
  @param  response                     The SPDM response message.
  @param  original_request_code          Indicate the original request code.
  @param  expected_response_code         Indicate the expected response code.
  @param  expected_response_size         Indicate the expected response size.

  @retval RETURN_SUCCESS               The error code is RESPONSE_NOT_READY. The RESPOND_IF_READY is sent and an expected SPDM response is received.
  @retval RETURN_NO_RESPONSE           The error code is BUSY.
  @retval RETURN_DEVICE_ERROR          The error code is REQUEST_RESYNCH or others.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_handle_error_response_main(
	IN spdm_context_t *spdm_context, IN uint32 *session_id,
	IN OUT void *managed_buffer_t, IN uintn shrink_buffer_size,
	IN OUT uintn *response_size, IN OUT void *response,
	IN uint8 original_request_code, IN uint8 expected_response_code,
	IN uintn expected_response_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_handle_error_response_main");

/**
  This function sends GET_VERSION and receives VERSION.

  @param  spdm_context                  A pointer to the SPDM context.

  @retval RETURN_SUCCESS               The GET_VERSION is sent and the VERSION is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_get_version(IN spdm_context_t *spdm_context)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_version");

/**
  This function sends GET_CAPABILITIES and receives CAPABILITIES.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  RequesterCTExponent          RequesterCTExponent to the GET_CAPABILITIES request.
  @param  RequesterFlags               RequesterFlags to the GET_CAPABILITIES request.
  @param  ResponderCTExponent          ResponderCTExponent from the CAPABILITIES response.
  @param  ResponderFlags               ResponderFlags from the CAPABILITIES response.

  @retval RETURN_SUCCESS               The GET_CAPABILITIES is sent and the CAPABILITIES is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_get_capabilities(IN spdm_context_t *spdm_context)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_capabilities");

/**
  This function sends NEGOTIATE_ALGORITHMS and receives ALGORITHMS.

  @param  spdm_context                  A pointer to the SPDM context.

  @retval RETURN_SUCCESS               The NEGOTIATE_ALGORITHMS is sent and the ALGORITHMS is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_negotiate_algorithms(IN spdm_context_t *spdm_context)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_negotiate_algorithms");

/**
  This function sends KEY_EXCHANGE and receives KEY_EXCHANGE_RSP for SPDM key exchange.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  measurement_hash_type          measurement_hash_type to the KEY_EXCHANGE request.
  @param  slot_id                      slot_id to the KEY_EXCHANGE request.
  @param  heartbeat_period              heartbeat_period from the KEY_EXCHANGE_RSP response.
  @param  session_id                    session_id from the KEY_EXCHANGE_RSP response.
  @param  req_slot_id_param               req_slot_id_param from the KEY_EXCHANGE_RSP response.
  @param  measurement_hash              measurement_hash from the KEY_EXCHANGE_RSP response.

  @retval RETURN_SUCCESS               The KEY_EXCHANGE is sent and the KEY_EXCHANGE_RSP is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_key_exchange(
	IN spdm_context_t *spdm_context, IN uint8 measurement_hash_type,
	IN uint8 slot_id, OUT uint32 *session_id, OUT uint8 *heartbeat_period,
	OUT uint8 *req_slot_id_param, OUT void *measurement_hash)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_key_exchange");

/**
  This function sends FINISH and receives FINISH_RSP for SPDM finish.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    session_id to the FINISH request.
  @param  req_slot_id_param               req_slot_id_param to the FINISH request.

  @retval RETURN_SUCCESS               The FINISH is sent and the FINISH_RSP is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_finish(IN spdm_context_t *spdm_context,
				       IN uint32 session_id,
				       IN uint8 req_slot_id_param)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_finish");

/**
  This function sends PSK_EXCHANGE and receives PSK_EXCHANGE_RSP for SPDM PSK exchange.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  measurement_hash_type          measurement_hash_type to the PSK_EXCHANGE request.
  @param  heartbeat_period              heartbeat_period from the PSK_EXCHANGE_RSP response.
  @param  session_id                    session_id from the PSK_EXCHANGE_RSP response.
  @param  measurement_hash              measurement_hash from the PSK_EXCHANGE_RSP response.

  @retval RETURN_SUCCESS               The PSK_EXCHANGE is sent and the PSK_EXCHANGE_RSP is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_psk_exchange(IN spdm_context_t *spdm_context,
					     IN uint8 measurement_hash_type,
					     OUT uint32 *session_id,
					     OUT uint8 *heartbeat_period,
					     OUT void *measurement_hash)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_psk_exchange");

/**
  This function sends PSK_FINISH and receives PSK_FINISH_RSP for SPDM PSK finish.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    session_id to the PSK_FINISH request.

  @retval RETURN_SUCCESS               The PSK_FINISH is sent and the PSK_FINISH_RSP is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_psk_finish(IN spdm_context_t *spdm_context,
					   IN uint32 session_id)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_psk_finish");

/**
  This function sends END_SESSION and receives END_SESSION_ACK for SPDM session end.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    session_id to the END_SESSION request.
  @param  end_session_attributes         end_session_attributes to the END_SESSION_ACK request.

  @retval RETURN_SUCCESS               The END_SESSION is sent and the END_SESSION_ACK is received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_end_session(IN spdm_context_t *spdm_context,
					    IN uint32 session_id,
					    IN uint8 end_session_attributes)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_end_session");

/**
  This function exelwtes a series of SPDM encapsulated requests and receives SPDM encapsulated responses.

  This function starts with the first encapsulated request (such as GET_ENCAPSULATED_REQUEST)
  and ends with last encapsulated response (such as RESPONSE_PAYLOAD_TYPE_ABSENT or RESPONSE_PAYLOAD_TYPE_SLOT_NUMBER).

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicate if the encapsulated request is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  mut_auth_requested             Indicate of the mut_auth_requested through KEY_EXCHANGE or CHALLENG response.
  @param  req_slot_id_param               req_slot_id_param from the RESPONSE_PAYLOAD_TYPE_REQ_SLOT_NUMBER.

  @retval RETURN_SUCCESS               The SPDM Encapsulated requests are sent and the responses are received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_encapsulated_request(IN spdm_context_t *spdm_context,
					IN uint32 *session_id,
					IN uint8 mut_auth_requested,
					OUT uint8 *req_slot_id_param)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_encapsulated_request");

/**
  Process the SPDM encapsulated GET_DIGESTS request and return the response.

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
return_status spdm_get_encap_response_digest(IN void *context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_response_digest");

/**
  Process the SPDM encapsulated GET_CERTIFICATE request and return the response.

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
return_status spdm_get_encap_response_certificate(IN void *context,
						  IN uintn request_size,
						  IN void *request,
						  IN OUT uintn *response_size,
						  OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_response_certificate");

/**
  Process the SPDM encapsulated CHALLENGE request and return the response.

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
return_status spdm_get_encap_response_challenge_auth(
	IN void *context, IN uintn request_size, IN void *request,
	IN OUT uintn *response_size, OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_response_challenge_auth");

/**
  Process the SPDM encapsulated KEY_UPDATE request and return the response.

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
return_status spdm_get_encap_response_key_update(IN void *context,
						 IN uintn request_size,
						 IN void *request,
						 IN OUT uintn *response_size,
						 OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_response_key_update");

/**
  Send an SPDM request to a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the request is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  request_size                  size in bytes of the request data buffer.
  @param  request                      A pointer to a destination buffer to store the request.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM request is sent successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM request is sent to the device.
**/
return_status spdm_send_spdm_request(IN spdm_context_t *spdm_context,
				     IN uint32 *session_id,
				     IN uintn request_size, IN void *request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_spdm_request");

/**
  Receive an SPDM response from a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the response is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  response_size                 size in bytes of the response data buffer.
  @param  response                     A pointer to a destination buffer to store the response.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM response is received successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM response is received from the device.
**/
return_status spdm_receive_spdm_response(IN spdm_context_t *spdm_context,
					 IN uint32 *session_id,
					 IN OUT uintn *response_size,
					 OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_receive_spdm_response");

#endif
