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
// We don't expect to use any of these, but specifying section for safety.
//

#ifndef __SPDM_REQUESTER_LIB_H__
#define __SPDM_REQUESTER_LIB_H__

#include <library/spdm_common_lib.h>

/**
  Send an SPDM or an APP request to a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the request is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
  @param  request_size                  size in bytes of the request data buffer.
  @param  request                      A pointer to a destination buffer to store the request.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM request is sent successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM request is sent to the device.
**/
return_status spdm_send_request(IN void *spdm_context, IN uint32 *session_id,
				IN boolean is_app_message,
				IN uintn request_size, IN void *request)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_request");

/**
  Receive an SPDM or an APP response from a device.

  @param  spdm_context                  The SPDM context for the device.
  @param  session_id                    Indicate if the response is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  is_app_message                 Indicates if it is an APP message or SPDM message.
  @param  response_size                 size in bytes of the response data buffer.
  @param  response                     A pointer to a destination buffer to store the response.
                                       The caller is responsible for having
                                       either implicit or explicit ownership of the buffer.

  @retval RETURN_SUCCESS               The SPDM response is received successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when the SPDM response is received from the device.
**/
return_status spdm_receive_response(IN void *spdm_context,
				    IN uint32 *session_id,
				    IN boolean is_app_message,
				    IN OUT uintn *response_size,
				    OUT void *response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_receive_response");

/**
  This function sends GET_VERSION, GET_CAPABILITIES, NEGOTIATE_ALGORITHMS
  to initialize the connection with SPDM responder.

  Before this function, the requester configuration data can be set via spdm_set_data.
  After this function, the negotiated configuration data can be got via spdm_get_data.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  get_version_only               If the requester sends GET_VERSION only or not.

  @retval RETURN_SUCCESS               The connection is initialized successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_init_connection(IN void *spdm_context,
				   IN boolean get_version_only)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_init_connection");

/**
  This function sends GET_DIGEST
  to get all digest of the certificate chains from device.

  If the peer certificate chain is deployed,
  this function also verifies the digest with the certificate chain.

  TotalDigestSize = sizeof(digest) * count in slot_mask

  @param  spdm_context                  A pointer to the SPDM context.
  @param  slot_mask                     The slots which deploy the CertificateChain.
  @param  total_digest_buffer            A pointer to a destination buffer to store the digest buffer.

  @retval RETURN_SUCCESS               The digests are got successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_get_digest(IN void *spdm_context, OUT uint8 *slot_mask,
			      OUT void *total_digest_buffer)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_digest");

/**
  This function sends GET_CERTIFICATE
  to get certificate chain in one slot from device.

  This function verify the integrity of the certificate chain.
  root_hash -> Root certificate -> Intermediate certificate -> Leaf certificate.

  If the peer root certificate hash is deployed,
  this function also verifies the digest with the root hash in the certificate chain.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  slot_id                      The number of slot for the certificate chain.
  @param  cert_chain_size                On input, indicate the size in bytes of the destination buffer to store the digest buffer.
                                       On output, indicate the size in bytes of the certificate chain.
  @param  cert_chain                    A pointer to a destination buffer to store the certificate chain.

  @retval RETURN_SUCCESS               The certificate chain is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_get_certificate(IN void *spdm_context, IN uint8 slot_id,
				   IN OUT uintn *cert_chain_size,
				   OUT void *cert_chain)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_certificate");

/**
  This function sends GET_CERTIFICATE
  to get certificate chain in one slot from device.

  This function verify the integrity of the certificate chain.
  root_hash -> Root certificate -> Intermediate certificate -> Leaf certificate.

  If the peer root certificate hash is deployed,
  this function also verifies the digest with the root hash in the certificate chain.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  slot_id                      The number of slot for the certificate chain.
  @param  length                       MAX_SPDM_CERT_CHAIN_BLOCK_LEN.
  @param  cert_chain_size                On input, indicate the size in bytes of the destination buffer to store the digest buffer.
                                       On output, indicate the size in bytes of the certificate chain.
  @param  cert_chain                    A pointer to a destination buffer to store the certificate chain.

  @retval RETURN_SUCCESS               The certificate chain is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_get_certificate_choose_length(IN void *spdm_context,
						 IN uint8 slot_id,
						 IN uint16 length,
						 IN OUT uintn *cert_chain_size,
						 OUT void *cert_chain)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_certificate_choose_length");

/**
  This function sends CHALLENGE
  to authenticate the device based upon the key in one slot.

  This function verifies the signature in the challenge auth.

  If basic mutual authentication is requested from the responder,
  this function also perform the basic mutual authentication.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  slot_id                      The number of slot for the challenge.
  @param  measurement_hash_type          The type of the measurement hash.
  @param  measurement_hash              A pointer to a destination buffer to store the measurement hash.

  @retval RETURN_SUCCESS               The challenge auth is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_challenge(IN void *spdm_context, IN uint8 slot_id,
			     IN uint8 measurement_hash_type,
			     OUT void *measurement_hash)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_challenge");

/**
  This function sends GET_MEASUREMENT
  to get measurement from the device.

  If the signature is requested, this function verifies the signature of the measurement.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicates if it is a selwred message protected via SPDM session.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.
  @param  request_attribute             The request attribute of the request message.
  @param  measurement_operation         The measurement operation of the request message.
  @param  slot_id                      The number of slot for the certificate chain.
  @param  number_of_blocks               The number of blocks of the measurement record.
  @param  measurement_record_length      On input, indicate the size in bytes of the destination buffer to store the measurement record.
                                       On output, indicate the size in bytes of the measurement record.
  @param  measurement_record            A pointer to a destination buffer to store the measurement record.

  @retval RETURN_SUCCESS               The measurement is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_get_measurement(IN void *spdm_context, IN uint32 *session_id,
				   IN uint8 request_attribute,
				   IN uint8 measurement_operation,
				   IN uint8 slot_id,
				   OUT uint8 *number_of_blocks,
				   IN OUT uint32 *measurement_record_length,
				   OUT void *measurement_record)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_get_measurement");

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
return_status spdm_start_session(IN void *spdm_context, IN boolean use_psk,
				 IN uint8 measurement_hash_type,
				 IN uint8 slot_id, OUT uint32 *session_id,
				 OUT uint8 *heartbeat_period,
				 OUT void *measurement_hash)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_start_session");

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
return_status spdm_stop_session(IN void *spdm_context, IN uint32 session_id,
				IN uint8 end_session_attributes)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_stop_session");

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
return_status spdm_send_receive_data(IN void *spdm_context,
				     IN uint32 *session_id,
				     IN boolean is_app_message,
				     IN void *request, IN uintn request_size,
				     OUT void *response,
				     IN OUT uintn *response_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_data");

/**
  This function sends HEARTBEAT
  to an SPDM Session.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session ID of the session.

  @retval RETURN_SUCCESS               The heartbeat is sent and received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_heartbeat(IN void *spdm_context, IN uint32 session_id)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_heartbeat");

/**
  This function sends KEY_UPDATE
  to update keys for an SPDM Session.

  After keys are updated, this function also uses VERIFY_NEW_KEY to verify the key.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The session ID of the session.
  @param  single_direction              TRUE means the operation is UPDATE_KEY.
                                       FALSE means the operation is UPDATE_ALL_KEYS.

  @retval RETURN_SUCCESS               The keys of the session are updated.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
return_status spdm_key_update(IN void *spdm_context, IN uint32 session_id,
			      IN boolean single_direction)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_key_update");

/**
  This function exelwtes a series of SPDM encapsulated requests and receives SPDM encapsulated responses.

  This function starts with the first encapsulated request (such as GET_ENCAPSULATED_REQUEST)
  and ends with last encapsulated response (such as RESPONSE_PAYLOAD_TYPE_ABSENT or RESPONSE_PAYLOAD_TYPE_SLOT_NUMBER).

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    Indicate if the encapsulated request is a selwred message.
                                       If session_id is NULL, it is a normal message.
                                       If session_id is NOT NULL, it is a selwred message.

  @retval RETURN_SUCCESS               The SPDM Encapsulated requests are sent and the responses are received.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
**/
return_status spdm_send_receive_encap_request(IN void *spdm_context,
					      IN uint32 *session_id)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_send_receive_encap_request");

/**
  Process the encapsulated request and return the encapsulated response.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  spdm_request_size              size in bytes of the request data.
  @param  spdm_request                  A pointer to the request data.
  @param  spdm_response_size             size in bytes of the response data.
                                       On input, it means the size in bytes of response data buffer.
                                       On output, it means the size in bytes of copied response data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired response data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  spdm_response                 A pointer to the response data.

  @retval RETURN_SUCCESS               The request is processed and the response is returned.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
  @retval RETURN_DEVICE_ERROR          A device error oclwrs when communicates with the device.
  @retval RETURN_SELWRITY_VIOLATION    Any verification fails.
**/
typedef return_status (*spdm_get_encap_response_func)(
	IN void *spdm_context, IN uintn spdm_request_size,
	IN void *spdm_request, IN OUT uintn *spdm_response_size,
	OUT void *spdm_response);

/**
  Register an SPDM encapsulated message process function.

  If the default encapsulated message process function cannot handle the encapsulated message,
  this function will be ilwoked.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  get_encap_response_func         The function to process the encapsuled message.
**/
void spdm_register_get_encap_response_func(IN void *spdm_context,
					   IN spdm_get_encap_response_func
						   get_encap_response_func)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_register_get_encap_response_func");

/**
  Generate encapsulated ERROR message.

  This function can be called in spdm_get_encap_response_func.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  error_code                    The error code of the message.
  @param  error_data                    The error data of the message.
  @param  spdm_response_size             size in bytes of the response data.
                                       On input, it means the size in bytes of data buffer.
                                       On output, it means the size in bytes of copied data buffer if RETURN_SUCCESS is returned,
                                       and means the size in bytes of desired data buffer if RETURN_BUFFER_TOO_SMALL is returned.
  @param  spdm_response                 A pointer to the response data.

  @retval RETURN_SUCCESS               The error message is generated.
  @retval RETURN_BUFFER_TOO_SMALL      The buffer is too small to hold the data.
**/
return_status spdm_generate_encap_error_response(
	IN void *spdm_context, IN uint8 error_code, IN uint8 error_data,
	IN OUT uintn *spdm_response_size, OUT void *spdm_response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_encap_error_response");

/**
  Generate encapsulated ERROR message with extended error data.

  This function can be called in spdm_get_encap_response_func.

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
return_status spdm_generate_encap_extended_error_response(
	IN void *spdm_context, IN uint8 error_code, IN uint8 error_data,
	IN uintn extended_error_data_size, IN uint8 *extended_error_data,
	IN OUT uintn *spdm_response_size, OUT void *spdm_response)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_encap_extended_error_response");

#endif