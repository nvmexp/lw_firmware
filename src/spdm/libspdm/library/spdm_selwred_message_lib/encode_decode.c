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

#include "spdm_selwred_message_lib_internal.h"

/**
  Encode an application message to a selwred message.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  session_id                    The session ID of the SPDM session.
  @param  is_requester                  Indicates if it is a requester message.
  @param  app_message_size               size in bytes of the application message data buffer.
  @param  app_message                   A pointer to a source buffer to store the application message.
  @param  selwred_message_size           size in bytes of the selwred message data buffer.
  @param  selwred_message               A pointer to a destination buffer to store the selwred message.
  @param  spdm_selwred_message_callbacks_t  A pointer to a selwred message callback functions structure.

  @retval RETURN_SUCCESS               The application message is encoded successfully.
  @retval RETURN_ILWALID_PARAMETER     The message is NULL or the message_size is zero.
**/
return_status spdm_encode_selwred_message(
	IN void *spdm_selwred_message_context, IN uint32 session_id,
	IN boolean is_requester, IN uintn app_message_size,
	IN void *app_message, IN OUT uintn *selwred_message_size,
	OUT void *selwred_message,
	IN spdm_selwred_message_callbacks_t *spdm_selwred_message_callbacks_t)
{
	spdm_selwred_message_context_t *selwred_message_context;
	uintn total_selwred_message_size;
	uintn plain_text_size;
	uintn cipher_text_size;
	uintn aead_pad_size;
	uintn aead_tag_size;
	uintn aead_key_size;
	uintn aead_iv_size;
	uint8 *a_data;
	uint8 *enc_msg;
	uint8 *dec_msg;
	uint8 *tag;
	spdm_selwred_message_a_data_header1_t *record_header1;
	spdm_selwred_message_a_data_header2_t *record_header2;
	uintn record_header_size;
	spdm_selwred_message_cipher_header_t *enc_msg_header;
	boolean result;
	uint8 key[MAX_AEAD_KEY_SIZE];
	uint8 salt[MAX_AEAD_IV_SIZE];
	uint64 sequence_number;
	uint64 sequence_num_in_header;
	uint8 sequence_num_in_header_size = 0;
	spdm_session_type_t session_type;
	uint32 rand_count;
	uint32 max_rand_count;
	spdm_session_state_t session_state;

	selwred_message_context = spdm_selwred_message_context;

	session_type = selwred_message_context->session_type;
	ASSERT((session_type == SPDM_SESSION_TYPE_MAC_ONLY) ||
	       (session_type == SPDM_SESSION_TYPE_ENC_MAC));
	session_state = selwred_message_context->session_state;
	ASSERT((session_state == SPDM_SESSION_STATE_HANDSHAKING) ||
	       (session_state == SPDM_SESSION_STATE_ESTABLISHED));

	aead_tag_size = selwred_message_context->aead_tag_size;
	aead_key_size = selwred_message_context->aead_key_size;
	aead_iv_size = selwred_message_context->aead_iv_size;

	switch (session_state) {
	case SPDM_SESSION_STATE_HANDSHAKING:
		if (is_requester) {
			copy_mem(key,
				 selwred_message_context->handshake_secret
					 .request_handshake_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->handshake_secret
					 .request_handshake_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->handshake_secret
					.request_handshake_sequence_number;
		} else {
			copy_mem(key,
				 selwred_message_context->handshake_secret
					 .response_handshake_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->handshake_secret
					 .response_handshake_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->handshake_secret
					.response_handshake_sequence_number;
		}
		break;
	case SPDM_SESSION_STATE_ESTABLISHED:
		if (is_requester) {
			copy_mem(key,
				 selwred_message_context->application_secret
					 .request_data_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->application_secret
					 .request_data_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->application_secret
					.request_data_sequence_number;
		} else {
			copy_mem(key,
				 selwred_message_context->application_secret
					 .response_data_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->application_secret
					 .response_data_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->application_secret
					.response_data_sequence_number;
		}
		break;
	default:
		ASSERT(FALSE);
		return RETURN_UNSUPPORTED;
		break;
	}

	if (sequence_number == (uint64)-1) {
		return RETURN_OUT_OF_RESOURCES;
	}

	*(uint64 *)salt = *(uint64 *)salt ^ sequence_number;

	sequence_num_in_header = 0;
//
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct value.
// Let transport layer decide whether to include sequence number,
// and the size of the sequence number.
//
	sequence_num_in_header_size =
		spdm_selwred_message_callbacks_t->sequence_number_size;
	ASSERT(sizeof(sequence_num_in_header) == sizeof(sequence_number));
	ASSERT(sequence_num_in_header_size <= sizeof(sequence_num_in_header));
	copy_mem((uint8 *)&sequence_num_in_header, (uint8 *)&sequence_number,
		sequence_num_in_header_size);
//  sequence_num_in_header_size =
//      spdm_selwred_message_callbacks_t->get_sequence_number(
//          sequence_number, (uint8 *)&sequence_num_in_header);
//  ASSERT(sequence_num_in_header_size <= sizeof(sequence_num_in_header));

	sequence_number++;
	switch (session_state) {
	case SPDM_SESSION_STATE_HANDSHAKING:
		if (is_requester) {
			selwred_message_context->handshake_secret
				.request_handshake_sequence_number =
				sequence_number;
		} else {
			selwred_message_context->handshake_secret
				.response_handshake_sequence_number =
				sequence_number;
		}
		break;
	case SPDM_SESSION_STATE_ESTABLISHED:
		if (is_requester) {
			selwred_message_context->application_secret
				.request_data_sequence_number = sequence_number;
		} else {
			selwred_message_context->application_secret
				.response_data_sequence_number =
				sequence_number;
		}
		break;
    // LWE (tandrewprinz): Compiler requires default case for enums.
    default:
        ASSERT(FALSE);
        return RETURN_UNSUPPORTED;
	}

	record_header_size = sizeof(spdm_selwred_message_a_data_header1_t) +
			     sequence_num_in_header_size +
			     sizeof(spdm_selwred_message_a_data_header2_t);

	switch (session_type) {
	case SPDM_SESSION_TYPE_ENC_MAC:
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct value.
		max_rand_count =
			spdm_selwred_message_callbacks_t->max_random_number_count;
//      max_rand_count = spdm_selwred_message_callbacks_t
//                   ->get_max_random_number_count();
		if (max_rand_count != 0) {
			rand_count = 0;
			random_bytes((uint8 *)&rand_count, sizeof(rand_count));
			rand_count = (uint8)((rand_count % max_rand_count) + 1);
		} else {
			rand_count = 0;
		}

		plain_text_size = sizeof(spdm_selwred_message_cipher_header_t) +
				  app_message_size + rand_count;
		cipher_text_size = plain_text_size;
		aead_pad_size = cipher_text_size - plain_text_size;
		total_selwred_message_size =
			record_header_size + cipher_text_size + aead_tag_size;

		ASSERT(*selwred_message_size >= total_selwred_message_size);
		if (*selwred_message_size < total_selwred_message_size) {
			*selwred_message_size = total_selwred_message_size;
			return RETURN_BUFFER_TOO_SMALL;
		}
		*selwred_message_size = total_selwred_message_size;
		record_header1 = (void *)selwred_message;
		record_header2 =
			(void *)((uint8 *)record_header1 +
				 sizeof(spdm_selwred_message_a_data_header1_t) +
				 sequence_num_in_header_size);
		record_header1->session_id = session_id;
		copy_mem(record_header1 + 1, &sequence_num_in_header,
			 sequence_num_in_header_size);
		record_header2->length =
			(uint16)(cipher_text_size + aead_tag_size);
		enc_msg_header = (void *)(record_header2 + 1);
		enc_msg_header->application_data_length =
			(uint16)app_message_size;
		copy_mem(enc_msg_header + 1, app_message, app_message_size);
		random_bytes(
			(uint8 *)enc_msg_header +
				sizeof(spdm_selwred_message_cipher_header_t) +
				app_message_size,
			rand_count);
		zero_mem((uint8 *)enc_msg_header + plain_text_size,
			 aead_pad_size);

		a_data = (uint8 *)record_header1;
		enc_msg = (uint8 *)enc_msg_header;
		dec_msg = (uint8 *)enc_msg_header;
		tag = (uint8 *)record_header1 + record_header_size +
		      cipher_text_size;

		result = spdm_aead_encryption(
			selwred_message_context->aead_cipher_suite, key,
			aead_key_size, salt, aead_iv_size, (uint8 *)a_data,
			record_header_size, dec_msg, cipher_text_size, tag,
			aead_tag_size, enc_msg, &cipher_text_size);
		break;

	case SPDM_SESSION_TYPE_MAC_ONLY:
		total_selwred_message_size =
			record_header_size + app_message_size + aead_tag_size;

		ASSERT(*selwred_message_size >= total_selwred_message_size);
		if (*selwred_message_size < total_selwred_message_size) {
			*selwred_message_size = total_selwred_message_size;
			return RETURN_BUFFER_TOO_SMALL;
		}
		*selwred_message_size = total_selwred_message_size;
		record_header1 = (void *)selwred_message;
		record_header2 =
			(void *)((uint8 *)record_header1 +
				 sizeof(spdm_selwred_message_a_data_header1_t) +
				 sequence_num_in_header_size);
		record_header1->session_id = session_id;
		copy_mem(record_header1 + 1, &sequence_num_in_header,
			 sequence_num_in_header_size);
		record_header2->length =
			(uint16)(app_message_size + aead_tag_size);
		copy_mem(record_header2 + 1, app_message, app_message_size);
		a_data = (uint8 *)record_header1;
		tag = (uint8 *)record_header1 + record_header_size +
		      app_message_size;

		result = spdm_aead_encryption(
			selwred_message_context->aead_cipher_suite, key,
			aead_key_size, salt, aead_iv_size, (uint8 *)a_data,
			record_header_size + app_message_size, NULL, 0, tag,
			aead_tag_size, NULL, NULL);
		break;

	default:
		ASSERT(FALSE);
		return RETURN_UNSUPPORTED;
	}
	if (!result) {
		return RETURN_OUT_OF_RESOURCES;
	}
	return RETURN_SUCCESS;
}

/**
  Decode an application message from a selwred message.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  session_id                    The session ID of the SPDM session.
  @param  is_requester                  Indicates if it is a requester message.
  @param  selwred_message_size           size in bytes of the selwred message data buffer.
  @param  selwred_message               A pointer to a source buffer to store the selwred message.
  @param  app_message_size               size in bytes of the application message data buffer.
  @param  app_message                   A pointer to a destination buffer to store the application message.
  @param  spdm_selwred_message_callbacks_t  A pointer to a selwred message callback functions structure.

  @retval RETURN_SUCCESS               The application message is decoded successfully.
  @retval RETURN_ILWALID_PARAMETER     The message is NULL or the message_size is zero.
  @retval RETURN_UNSUPPORTED           The selwred_message is unsupported.
**/
return_status spdm_decode_selwred_message(
	IN void *spdm_selwred_message_context, IN uint32 session_id,
	IN boolean is_requester, IN uintn selwred_message_size,
	IN void *selwred_message, IN OUT uintn *app_message_size,
	OUT void *app_message,
	IN spdm_selwred_message_callbacks_t *spdm_selwred_message_callbacks_t)
{
	spdm_selwred_message_context_t *selwred_message_context;
	uintn plain_text_size;
	uintn cipher_text_size;
	uintn aead_tag_size;
	uintn aead_key_size;
	uintn aead_iv_size;
	uint8 *a_data;
	uint8 *enc_msg;
	uint8 *dec_msg;
	uint8 *tag;
	spdm_selwred_message_a_data_header1_t *record_header1;
	spdm_selwred_message_a_data_header2_t *record_header2;
	uintn record_header_size;
	spdm_selwred_message_cipher_header_t *enc_msg_header;
	boolean result;
	uint8 key[MAX_AEAD_KEY_SIZE];
	uint8 salt[MAX_AEAD_IV_SIZE];
	uint64 sequence_number;
	uint64 sequence_num_in_header;
	uint8 sequence_num_in_header_size;
	spdm_session_type_t session_type;
	spdm_session_state_t session_state;
	spdm_error_struct_t spdm_error;
	uint8 dec_message[MAX_SPDM_MESSAGE_BUFFER_SIZE];

	spdm_error.error_code = 0;
	spdm_error.session_id = 0;
	spdm_selwred_message_set_last_spdm_error_struct(
		spdm_selwred_message_context, &spdm_error);

	spdm_error.error_code = SPDM_ERROR_CODE_DECRYPT_ERROR;
	spdm_error.session_id = session_id;

	selwred_message_context = spdm_selwred_message_context;

	session_type = selwred_message_context->session_type;
	ASSERT((session_type == SPDM_SESSION_TYPE_MAC_ONLY) ||
	       (session_type == SPDM_SESSION_TYPE_ENC_MAC));
	session_state = selwred_message_context->session_state;
	ASSERT((session_state == SPDM_SESSION_STATE_HANDSHAKING) ||
	       (session_state == SPDM_SESSION_STATE_ESTABLISHED));

	aead_tag_size = selwred_message_context->aead_tag_size;
	aead_key_size = selwred_message_context->aead_key_size;
	aead_iv_size = selwred_message_context->aead_iv_size;

	switch (session_state) {
	case SPDM_SESSION_STATE_HANDSHAKING:
		if (is_requester) {
			copy_mem(key,
				 selwred_message_context->handshake_secret
					 .request_handshake_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->handshake_secret
					 .request_handshake_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->handshake_secret
					.request_handshake_sequence_number;
		} else {
			copy_mem(key,
				 selwred_message_context->handshake_secret
					 .response_handshake_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->handshake_secret
					 .response_handshake_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->handshake_secret
					.response_handshake_sequence_number;
		}
		break;
	case SPDM_SESSION_STATE_ESTABLISHED:
		if (is_requester) {
			copy_mem(key,
				 selwred_message_context->application_secret
					 .request_data_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->application_secret
					 .request_data_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->application_secret
					.request_data_sequence_number;
		} else {
			copy_mem(key,
				 selwred_message_context->application_secret
					 .response_data_encryption_key,
				 selwred_message_context->aead_key_size);
			copy_mem(salt,
				 selwred_message_context->application_secret
					 .response_data_salt,
				 selwred_message_context->aead_iv_size);
			sequence_number =
				selwred_message_context->application_secret
					.response_data_sequence_number;
		}
		break;
	default:
		ASSERT(FALSE);
		return RETURN_UNSUPPORTED;
	}

	if (sequence_number == (uint64)-1) {
		spdm_selwred_message_set_last_spdm_error_struct(
			spdm_selwred_message_context, &spdm_error);
		return RETURN_SELWRITY_VIOLATION;
	}

	*(uint64 *)salt = *(uint64 *)salt ^ sequence_number;

	sequence_num_in_header = 0;
//
// LWE (tandrewprinz) LW_REMOVE_FP: Replace FP with direct value.
// Let transport layer decide whether to include sequence number,
// and the size of the sequence number.
//
	sequence_num_in_header_size =
		spdm_selwred_message_callbacks_t->sequence_number_size;
	ASSERT(sizeof(sequence_num_in_header) == sizeof(sequence_number));
	ASSERT(sequence_num_in_header_size <= sizeof(sequence_num_in_header));
	copy_mem((uint8 *)&sequence_num_in_header, (uint8 *)&sequence_number,
		sequence_num_in_header_size);
//  sequence_num_in_header_size =
//      spdm_selwred_message_callbacks_t->get_sequence_number(
//          sequence_number, (uint8 *)&sequence_num_in_header);
//  ASSERT(sequence_num_in_header_size <= sizeof(sequence_num_in_header));

	sequence_number++;
	switch (session_state) {
	case SPDM_SESSION_STATE_HANDSHAKING:
		if (is_requester) {
			selwred_message_context->handshake_secret
				.request_handshake_sequence_number =
				sequence_number;
		} else {
			selwred_message_context->handshake_secret
				.response_handshake_sequence_number =
				sequence_number;
		}
		break;
	case SPDM_SESSION_STATE_ESTABLISHED:
		if (is_requester) {
			selwred_message_context->application_secret
				.request_data_sequence_number = sequence_number;
		} else {
			selwred_message_context->application_secret
				.response_data_sequence_number =
				sequence_number;
		}
		break;
    // LWE (tandrewprinz): Compiler requires default case for enums.
    default:
        ASSERT(FALSE);
        return RETURN_UNSUPPORTED;
	}

	record_header_size = sizeof(spdm_selwred_message_a_data_header1_t) +
			     sequence_num_in_header_size +
			     sizeof(spdm_selwred_message_a_data_header2_t);

	switch (session_type) {
	case SPDM_SESSION_TYPE_ENC_MAC:
		if (selwred_message_size < record_header_size + aead_tag_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		record_header1 = (void *)selwred_message;
		record_header2 =
			(void *)((uint8 *)record_header1 +
				 sizeof(spdm_selwred_message_a_data_header1_t) +
				 sequence_num_in_header_size);
		if (record_header1->session_id != session_id) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (compare_mem(record_header1 + 1, &sequence_num_in_header,
				sequence_num_in_header_size) != 0) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (record_header2->length >
		    selwred_message_size - record_header_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (record_header2->length < aead_tag_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		cipher_text_size = (record_header2->length - aead_tag_size);
		if (cipher_text_size > sizeof(dec_message)) {
			return RETURN_OUT_OF_RESOURCES;
		}
		zero_mem(dec_message, sizeof(dec_message));
		enc_msg_header = (void *)(record_header2 + 1);
		a_data = (uint8 *)record_header1;
		enc_msg = (uint8 *)enc_msg_header;
		dec_msg = (uint8 *)dec_message;
		enc_msg_header = (void *)dec_msg;
		tag = (uint8 *)record_header1 + record_header_size +
		      cipher_text_size;
		result = spdm_aead_decryption(
			selwred_message_context->aead_cipher_suite, key,
			aead_key_size, salt, aead_iv_size, (uint8 *)a_data,
			record_header_size, enc_msg, cipher_text_size, tag,
			aead_tag_size, dec_msg, &cipher_text_size);
		if (!result) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		plain_text_size = enc_msg_header->application_data_length;
		if (plain_text_size > cipher_text_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}

		ASSERT(*app_message_size >= plain_text_size);
		if (*app_message_size < plain_text_size) {
			*app_message_size = plain_text_size;
			return RETURN_BUFFER_TOO_SMALL;
		}
		*app_message_size = plain_text_size;
		copy_mem(app_message, enc_msg_header + 1, plain_text_size);
		break;

	case SPDM_SESSION_TYPE_MAC_ONLY:
		if (selwred_message_size < record_header_size + aead_tag_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		record_header1 = (void *)selwred_message;
		record_header2 =
			(void *)((uint8 *)record_header1 +
				 sizeof(spdm_selwred_message_a_data_header1_t) +
				 sequence_num_in_header_size);
		if (record_header1->session_id != session_id) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (compare_mem(record_header1 + 1, &sequence_num_in_header,
				sequence_num_in_header_size) != 0) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (record_header2->length >
		    selwred_message_size - record_header_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		if (record_header2->length < aead_tag_size) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}
		a_data = (uint8 *)record_header1;
		tag = (uint8 *)record_header1 + record_header_size +
		      record_header2->length - aead_tag_size;
		result = spdm_aead_decryption(
			selwred_message_context->aead_cipher_suite, key,
			aead_key_size, salt, aead_iv_size, (uint8 *)a_data,
			record_header_size + record_header2->length -
				aead_tag_size,
			NULL, 0, tag, aead_tag_size, NULL, NULL);
		if (!result) {
			spdm_selwred_message_set_last_spdm_error_struct(
				spdm_selwred_message_context, &spdm_error);
			return RETURN_SELWRITY_VIOLATION;
		}

		plain_text_size = record_header2->length - aead_tag_size;
		ASSERT(*app_message_size >= plain_text_size);
		if (*app_message_size < plain_text_size) {
			*app_message_size = plain_text_size;
			return RETURN_BUFFER_TOO_SMALL;
		}
		*app_message_size = plain_text_size;
		copy_mem(app_message, record_header2 + 1, plain_text_size);
		break;

	default:
		ASSERT(FALSE);
		return RETURN_UNSUPPORTED;
	}

	return RETURN_SUCCESS;
}
