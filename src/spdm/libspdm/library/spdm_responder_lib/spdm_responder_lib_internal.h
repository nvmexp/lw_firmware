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

#ifndef __SPDM_RESPONDER_LIB_INTERNAL_H__
#define __SPDM_RESPONDER_LIB_INTERNAL_H__

#include <library/spdm_responder_lib.h>
#include <library/spdm_selwred_message_lib.h>
#include "spdm_common_lib_internal.h"

/**
  Process the SPDM request and return the response.

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
typedef return_status (*spdm_get_spdm_response_func)(
	IN void *spdm_context, IN uintn request_size, IN void *request,
	IN OUT uintn *response_size, OUT void *response);

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
	GCC_ATTRIB_SECTION("imem_libspdm", "spdm_call_response_func");

/**
  Build the response when the response state is incorrect.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  request_code                  The SPDM request code.
  @param  response_size                 size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  response                     A pointer to the response data.

  @retval RETURN_SUCCESS               The response is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_responder_handle_response_state(IN void *spdm_context,
						   IN uint8 request_code,
						   IN OUT uintn *response_size,
						   OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_responder_handle_response_state");

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
return_status spdm_get_response_respond_if_ready(IN void *spdm_context,
						 IN uintn request_size,
						 IN void *request,
						 IN OUT uintn *response_size,
						 OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_respond_if_ready");

/**
  Process the SPDM GET_VERSION request and return the response.

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
return_status spdm_get_response_version(IN void *spdm_context,
					IN uintn request_size, IN void *request,
					IN OUT uintn *response_size,
					OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_version");

/**
  Process the SPDM GET_CAPABILITIES request and return the response.

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
return_status spdm_get_response_capabilities(IN void *spdm_context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_capabilities");

/**
  Process the SPDM NEGOTIATE_ALGORITHMS request and return the response.

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
return_status spdm_get_response_algorithms(IN void *spdm_context,
					   IN uintn request_size,
					   IN void *request,
					   IN OUT uintn *response_size,
					   OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_algorithms");

/**
  Process the SPDM GET_DIGESTS request and return the response.

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
return_status spdm_get_response_digests(IN void *spdm_context,
					IN uintn request_size, IN void *request,
					IN OUT uintn *response_size,
					OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_digests");

/**
  Process the SPDM GET_CERTIFICATE request and return the response.

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
return_status spdm_get_response_certificate(IN void *spdm_context,
					    IN uintn request_size,
					    IN void *request,
					    IN OUT uintn *response_size,
					    OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_certificate");

/**
  Process the SPDM CHALLENGE request and return the response.

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
return_status spdm_get_response_challenge_auth(IN void *spdm_context,
					       IN uintn request_size,
					       IN void *request,
					       IN OUT uintn *response_size,
					       OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_challenge_auth");

/**
  Process the SPDM GET_MEASUREMENT request and return the response.

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
return_status spdm_get_response_measurements(IN void *spdm_context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_measurements");

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
return_status spdm_get_response_key_exchange(IN void *spdm_context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_key_exchange");

/**
  Process the SPDM FINISH request and return the response.

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
return_status spdm_get_response_finish(IN void *spdm_context,
				       IN uintn request_size, IN void *request,
				       IN OUT uintn *response_size,
				       OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_finish");

/**
  Process the SPDM PSK_EXCHANGE request and return the response.

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
return_status spdm_get_response_psk_exchange(IN void *spdm_context,
					     IN uintn request_size,
					     IN void *request,
					     IN OUT uintn *response_size,
					     OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_psk_exchange");

/**
  Process the SPDM PSK_FINISH request and return the response.

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
return_status spdm_get_response_psk_finish(IN void *spdm_context,
					   IN uintn request_size,
					   IN void *request,
					   IN OUT uintn *response_size,
					   OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_psk_finish");

/**
  Process the SPDM END_SESSION request and return the response.

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
return_status spdm_get_response_end_session(IN void *spdm_context,
					    IN uintn request_size,
					    IN void *request,
					    IN OUT uintn *response_size,
					    OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_end_session");

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
return_status spdm_get_response_heartbeat(IN void *spdm_context,
					  IN uintn request_size,
					  IN void *request,
					  IN OUT uintn *response_size,
					  OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_heartbeat");

/**
  Process the SPDM KEY_UPDATE request and return the response.

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
return_status spdm_get_response_key_update(IN void *spdm_context,
					   IN uintn request_size,
					   IN void *request,
					   IN OUT uintn *response_size,
					   OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_key_update");

/**
  Process the SPDM ENCAPSULATED_REQUEST request and return the response.

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
return_status spdm_get_response_encapsulated_request(
	IN void *spdm_context, IN uintn request_size, IN void *request,
	IN OUT uintn *response_size, OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_encapsulated_request");

/**
  Process the SPDM ENCAPSULATED_RESPONSE_ACK request and return the response.

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
return_status spdm_get_response_encapsulated_response_ack(
	IN void *spdm_context, IN uintn request_size, IN void *request,
	IN OUT uintn *response_size, OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_encapsulated_response_ack");

/**
  Get the SPDM encapsulated GET_DIGESTS request.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_request_size             size in bytes of the encapsulated request data.
                                       On input, it means the size in bytes of encapsulated request data buffer.
                                       On output, it means the size in bytes of copied encapsulated request data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired encapsulated request data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  encap_request                 A pointer to the encapsulated request data.

  @retval RETURN_SUCCESS               The encapsulated request is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status
spdm_get_encap_request_get_digest(IN spdm_context_t *spdm_context,
				  IN OUT uintn *encap_request_size,
				  OUT void *encap_request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_request_get_digest");

/**
  Process the SPDM encapsulated DIGESTS response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_response_size            size in bytes of the encapsulated response data.
  @param  encap_response                A pointer to the encapsulated response data.
  @param  need_continue                     Indicate if encapsulated communication need continue.

  @retval RETURN_SUCCESS               The encapsulated response is processed.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_process_encap_response_digest(
	IN spdm_context_t *spdm_context, IN uintn encap_response_size,
	IN void *encap_response, OUT boolean *need_continue)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_encap_response_digest");

/**
  Get the SPDM encapsulated GET_CERTIFICATE request.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_request_size             size in bytes of the encapsulated request data.
                                       On input, it means the size in bytes of encapsulated request data buffer.
                                       On output, it means the size in bytes of copied encapsulated request data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired encapsulated request data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  encap_request                 A pointer to the encapsulated request data.

  @retval RETURN_SUCCESS               The encapsulated request is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status
spdm_get_encap_request_get_certificate(IN spdm_context_t *spdm_context,
				       IN OUT uintn *encap_request_size,
				       OUT void *encap_request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_request_get_certificate");

/**
  Process the SPDM encapsulated CERTIFICATE response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_response_size            size in bytes of the encapsulated response data.
  @param  encap_response                A pointer to the encapsulated response data.
  @param  need_continue                     Indicate if encapsulated communication need continue.

  @retval RETURN_SUCCESS               The encapsulated response is processed.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_process_encap_response_certificate(
	IN spdm_context_t *spdm_context, IN uintn encap_response_size,
	IN void *encap_response, OUT boolean *need_continue)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_encap_response_certificate");

/**
  Get the SPDM encapsulated CHALLENGE request.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_request_size             size in bytes of the encapsulated request data.
                                       On input, it means the size in bytes of encapsulated request data buffer.
                                       On output, it means the size in bytes of copied encapsulated request data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired encapsulated request data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  encap_request                 A pointer to the encapsulated request data.

  @retval RETURN_SUCCESS               The encapsulated request is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status spdm_get_encap_request_challenge(IN spdm_context_t *spdm_context,
					       IN OUT uintn *encap_request_size,
					       OUT void *encap_request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_request_challenge");

/**
  Process the SPDM encapsulated CHALLENGE_AUTH response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_response_size            size in bytes of the encapsulated response data.
  @param  encap_response                A pointer to the encapsulated response data.
  @param  need_continue                     Indicate if encapsulated communication need continue.

  @retval RETURN_SUCCESS               The encapsulated response is processed.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_process_encap_response_challenge_auth(
	IN spdm_context_t *spdm_context, IN uintn encap_response_size,
	IN void *encap_response, OUT boolean *need_continue)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_encap_response_challenge_auth");

/**
  Get the SPDM encapsulated KEY_UPDATE request.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_request_size             size in bytes of the encapsulated request data.
                                       On input, it means the size in bytes of encapsulated request data buffer.
                                       On output, it means the size in bytes of copied encapsulated request data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired encapsulated request data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  encap_request                 A pointer to the encapsulated request data.

  @retval RETURN_SUCCESS               The encapsulated request is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status
spdm_get_encap_request_key_update(IN spdm_context_t *spdm_context,
				  IN OUT uintn *encap_request_size,
				  OUT void *encap_request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_encap_request_key_update");

/**
  Process the SPDM encapsulated KEY_UPDATE response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  encap_response_size            size in bytes of the encapsulated response data.
  @param  encap_response                A pointer to the encapsulated response data.
  @param  need_continue                     Indicate if encapsulated communication need continue.

  @retval RETURN_SUCCESS               The encapsulated response is processed.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_process_encap_response_key_update(
	IN spdm_context_t *spdm_context, IN uintn encap_response_size,
	IN void *encap_response, OUT boolean *need_continue)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_process_encap_response_key_update");

/**
  Return the GET_SPDM_RESPONSE function via request code.

  @param  request_code                  The SPDM request code.

  @return GET_SPDM_RESPONSE function according to the request code.
**/
spdm_get_spdm_response_func
spdm_get_response_func_via_request_code(IN uint8 request_code)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_func_via_request_code");

/**
  This function initializes the mut_auth encapsulated state.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  mut_auth_requested             Indicate of the mut_auth_requested through KEY_EXCHANGE response.
**/
void spdm_init_mut_auth_encap_state(IN spdm_context_t *spdm_context,
				    IN uint8 mut_auth_requested)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_init_mut_auth_encap_state");

/**
  This function initializes the basic_mut_auth encapsulated state.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  basic_mut_auth_requested        Indicate of the mut_auth_requested through CHALLENG response.
**/
void spdm_init_basic_mut_auth_encap_state(IN spdm_context_t *spdm_context,
					  IN uint8 basic_mut_auth_requested)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_init_basic_mut_auth_encap_state");

/**
  This function handles the encap error response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  managed_buffer_t                The managed buffer to be shrinked.
  @param  shrink_buffer_size             The size in bytes of the size of the buffer to be shrinked.
  @param  error_code                    Indicate the error code.

  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_handle_encap_error_response_main(
	IN spdm_context_t *spdm_context, IN OUT void *m_buffer,
	IN uintn shrink_buffer_size, IN uint8 error_code)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_handle_encap_error_response_main");

/**
  Set session_state to an SPDM selwred message context and trigger callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicate the SPDM session ID.
  @param  session_state                 Indicate the SPDM session state.
*/
void spdm_set_session_state(IN spdm_context_t *spdm_context,
			    IN uint32 session_id,
			    IN spdm_session_state_t session_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_set_session_state");

/**
  Set connection_state to an SPDM context and trigger callback.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  connection_state              Indicate the SPDM connection state.
*/
void spdm_set_connection_state(IN spdm_context_t *spdm_context,
			       IN spdm_connection_state_t connection_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_set_connection_state");

//
// LWE (tandrewprinz): The below functions did not have function declarations.
// Add declarations here to allow us to specify their sections.
//
/**
  Notify the session state to a session APP.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session_id of a session.
  @param  session_state                 The state of a session.
**/
void spdm_trigger_session_state_callback(IN spdm_context_t *spdm_context,
					 IN uint32 session_id,
					 IN spdm_session_state_t session_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_trigger_session_state_callback");

/**
  Notify the connection state to an SPDM context register.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  connection_state              Indicate the SPDM connection state.
**/
void spdm_trigger_connection_state_callback(IN spdm_context_t *spdm_context,
					    IN spdm_connection_state_t connection_state)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_trigger_connection_state_callback");

/**
  Select the preferred supproted algorithm according to the priority_table.

  @param  priority_table                The priority table.
  @param  priority_table_count           The count of the priroty table entry.
  @param  local_algo                    Local supported algorithm.
  @param  peer_algo                     Peer supported algorithm.

  @return final preferred supported algorithm
**/
uint32 spdm_prioritize_algorithm(IN uint32 *priority_table,
                 IN uintn priority_table_count,
                 IN uint32 local_algo, IN uint32 peer_algo)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_prioritize_algorithm");

/**
  This function checks the compability of the received CAPABILITES flag.
  Some flags are mutually inclusive/exclusive.

  @param  capabilities_flag             The received CAPABILITIES Flag.
  @param  version                      The SPMD message version.


  @retval True                         The received Capabilities flag is valid.
  @retval False                        The received Capabilities flag is invalid.
**/
boolean spdm_check_request_flag_compability(IN uint32 capabilities_flag,
                        IN uint8 version)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_check_request_flag_compability");

/**
  This function creates the measurement signature to response message based upon l1l2.
  @param  spdm_context                  A pointer to the SPDM context.
  @param  response_message              The measurement response message with empty signature to be filled.
  @param  response_message_size          Total size in bytes of the response message including signature.

  @retval TRUE  measurement signature is created.
  @retval FALSE measurement signature is not created.
**/
boolean spdm_create_measurement_signature(IN spdm_context_t *spdm_context,
                      IN OUT void *response_message,
                      IN uintn response_message_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_create_measurement_signature");

/**
  This function creates the opaque data to response message.
  @param  spdm_context                  A pointer to the SPDM context.
  @param  response_message              The measurement response message with empty signature to be filled.
  @param  response_message_size          Total size in bytes of the response message including signature.
**/
void spdm_create_measurement_opaque(IN spdm_context_t *spdm_context,
                    IN OUT void *response_message,
                    IN uintn response_message_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_create_measurement_opaque");

// LWE(eddichang) : Declare vendor defined request function type.
/**
  Process the SPDM VENDOR_DEFINED_REQUEST request and return the response.

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
#ifdef LIBSPDM_VENDOR_DEFINED_MESSAGES_SUPPORT
    return_status
    spdm_get_response_vendor_defined (IN void *spdm_context,
                          IN uintn request_size, IN void *request,
                          IN OUT uintn *response_size,
                          OUT void *response)
        GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_response_vendor_defined");
#endif
#endif

