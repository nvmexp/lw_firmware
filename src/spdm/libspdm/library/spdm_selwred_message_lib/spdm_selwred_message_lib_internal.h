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

#ifndef __SPDM_SELWRED_MESSAGE_LIB_INTERNAL_H__
#define __SPDM_SELWRED_MESSAGE_LIB_INTERNAL_H__

#include <library/spdm_selwred_message_lib.h>

typedef struct {
	uint8 dhe_secret[MAX_DHE_KEY_SIZE];
	uint8 handshake_secret[MAX_HASH_SIZE];
	uint8 master_secret[MAX_HASH_SIZE];
} spdm_session_info_struct_master_secret_t;

typedef struct {
	uint8 request_handshake_secret[MAX_HASH_SIZE];
	uint8 response_handshake_secret[MAX_HASH_SIZE];
	uint8 export_master_secret[MAX_HASH_SIZE];
	uint8 request_finished_key[MAX_HASH_SIZE];
	uint8 response_finished_key[MAX_HASH_SIZE];
	uint8 request_handshake_encryption_key[MAX_AEAD_KEY_SIZE];
	uint8 request_handshake_salt[MAX_AEAD_IV_SIZE];
	uint64 request_handshake_sequence_number;
	uint8 response_handshake_encryption_key[MAX_AEAD_KEY_SIZE];
	uint8 response_handshake_salt[MAX_AEAD_IV_SIZE];
	uint64 response_handshake_sequence_number;
} spdm_session_info_struct_handshake_secret_t;

typedef struct {
	uint8 request_data_secret[MAX_HASH_SIZE];
	uint8 response_data_secret[MAX_HASH_SIZE];
	uint8 request_data_encryption_key[MAX_AEAD_KEY_SIZE];
	uint8 request_data_salt[MAX_AEAD_IV_SIZE];
	uint64 request_data_sequence_number;
	uint8 response_data_encryption_key[MAX_AEAD_KEY_SIZE];
	uint8 response_data_salt[MAX_AEAD_IV_SIZE];
	uint64 response_data_sequence_number;
} spdm_session_info_struct_application_secret_t;

typedef struct {
	spdm_session_type_t session_type;
	uint32 bash_hash_algo;
	uint16 dhe_named_group;
	uint16 aead_cipher_suite;
	uint16 key_schedule;
	uintn hash_size;
	uintn dhe_key_size;
	uintn aead_key_size;
	uintn aead_iv_size;
	uintn aead_block_size;
	uintn aead_tag_size;
	boolean use_psk;
	spdm_session_state_t session_state;
	spdm_session_info_struct_master_secret_t master_secret;
	spdm_session_info_struct_handshake_secret_t handshake_secret;
	spdm_session_info_struct_application_secret_t application_secret;
	spdm_session_info_struct_application_secret_t application_secret_backup;
	uintn psk_hint_size;
	void *psk_hint;
	//
	// Cache the error in spdm_decode_selwred_message. It is handled in spdm_build_response.
	//
	spdm_error_struct_t last_spdm_error;
} spdm_selwred_message_context_t;

//
// LWE (tandrewprinz): The below functions did not have function declarations.
// Add declarations here to allow us to specify their sections.
//
/**
  This function generates SPDM AEAD key and IV for a session.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  major_secret                  The major secret.
  @param  key                          The buffer to store the AEAD key.
  @param  iv                           The buffer to store the AEAD IV.

  @retval RETURN_SUCCESS  SPDM AEAD key and IV for a session is generated.
**/
return_status spdm_generate_aead_key_and_iv(
    IN spdm_selwred_message_context_t *selwred_message_context,
    IN uint8 *major_secret, OUT uint8 *key, OUT uint8 *iv)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_aead_key_and_iv");

/**
  This function generates SPDM FinishedKey for a session.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  handshake_secret              The handshake secret.
  @param  FinishedKey                  The buffer to store the finished key.

  @retval RETURN_SUCCESS  SPDM FinishedKey for a session is generated.
**/
return_status spdm_generate_finished_key(
    IN spdm_selwred_message_context_t *selwred_message_context,
    IN uint8 *handshake_secret, OUT uint8 *FinishedKey)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_generate_finished_key");

#endif