/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_selwred_message_lib_internal.h"

/**
  Return the size in bytes of the SPDM selwred message context.

  @return the size in bytes of the SPDM selwred message context.
**/
uintn spdm_selwred_message_get_context_size(void)
{
	return sizeof(spdm_selwred_message_context_t);
}

/**
  Initialize an SPDM selwred message context.

  The size in bytes of the spdm_selwred_message_context can be returned by spdm_selwred_message_get_context_size.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
*/
void spdm_selwred_message_init_context(IN void *spdm_selwred_message_context)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	zero_mem(selwred_message_context,
		 sizeof(spdm_selwred_message_context_t));

	random_seed(NULL, 0);
}

/**
  Set use_psk to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  use_psk                       Indicate if the SPDM session use PSK.
*/
void spdm_selwred_message_set_use_psk(IN void *spdm_selwred_message_context,
				      IN boolean use_psk)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	selwred_message_context->use_psk = use_psk;
}

/**
  Set session_state to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  session_state                 Indicate the SPDM session state.
*/
void spdm_selwred_message_set_session_state(
	IN void *spdm_selwred_message_context,
	IN spdm_session_state_t session_state)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	selwred_message_context->session_state = session_state;
}

/**
  Return session_state of an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.

  @return the SPDM session state.
*/
spdm_session_state_t
spdm_selwred_message_get_session_state(IN void *spdm_selwred_message_context)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	return selwred_message_context->session_state;
}

/**
  Set session_type to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  session_type                  Indicate the SPDM session type.
*/
void spdm_selwred_message_set_session_type(IN void *spdm_selwred_message_context,
					   IN spdm_session_type_t session_type)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	selwred_message_context->session_type = session_type;
}

/**
  Set algorithm to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  bash_hash_algo                 Indicate the negotiated bash_hash_algo for the SPDM session.
  @param  dhe_named_group                Indicate the negotiated dhe_named_group for the SPDM session.
  @param  aead_cipher_suite              Indicate the negotiated aead_cipher_suite for the SPDM session.
  @param  key_schedule                  Indicate the negotiated key_schedule for the SPDM session.
*/
void spdm_selwred_message_set_algorithms(IN void *spdm_selwred_message_context,
					 IN uint32 bash_hash_algo,
					 IN uint16 dhe_named_group,
					 IN uint16 aead_cipher_suite,
					 IN uint16 key_schedule)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	selwred_message_context->bash_hash_algo = bash_hash_algo;
	selwred_message_context->dhe_named_group = dhe_named_group;
	selwred_message_context->aead_cipher_suite = aead_cipher_suite;
	selwred_message_context->key_schedule = key_schedule;

	selwred_message_context->hash_size =
		spdm_get_hash_size(selwred_message_context->bash_hash_algo);
	selwred_message_context->dhe_key_size = spdm_get_dhe_pub_key_size(
		selwred_message_context->dhe_named_group);
	selwred_message_context->aead_key_size = spdm_get_aead_key_size(
		selwred_message_context->aead_cipher_suite);
	selwred_message_context->aead_iv_size = spdm_get_aead_iv_size(
		selwred_message_context->aead_cipher_suite);
	selwred_message_context->aead_block_size = spdm_get_aead_block_size(
		selwred_message_context->aead_cipher_suite);
	selwred_message_context->aead_tag_size = spdm_get_aead_tag_size(
		selwred_message_context->aead_cipher_suite);
}

/**
  Set the psk_hint to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  psk_hint                      Indicate the PSK hint.
  @param  psk_hint_size                  The size in bytes of the PSK hint.
*/
void spdm_selwred_message_set_psk_hint(IN void *spdm_selwred_message_context,
				       IN void *psk_hint,
				       IN uintn psk_hint_size)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	selwred_message_context->psk_hint = psk_hint;
	selwred_message_context->psk_hint_size = psk_hint_size;
}

/**
  Import the DHE Secret to an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  dhe_secret                    Indicate the DHE secret.
  @param  dhe_secret_size                The size in bytes of the DHE secret.

  @retval RETURN_SUCCESS  DHE Secret is imported.
*/
return_status
spdm_selwred_message_import_dhe_secret(IN void *spdm_selwred_message_context,
				       IN void *dhe_secret,
				       IN uintn dhe_secret_size)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	if (dhe_secret_size > selwred_message_context->dhe_key_size) {
		return RETURN_OUT_OF_RESOURCES;
	}
	selwred_message_context->dhe_key_size = dhe_secret_size;
	copy_mem(selwred_message_context->master_secret.dhe_secret, dhe_secret,
		 dhe_secret_size);
	return RETURN_SUCCESS;
}

/**
  Export the export_master_secret from an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  export_master_secret           Indicate the buffer to store the export_master_secret.
  @param  export_master_secret_size       The size in bytes of the export_master_secret.

  @retval RETURN_SUCCESS  export_master_secret is exported.
*/
return_status spdm_selwred_message_export_master_secret(
	IN void *spdm_selwred_message_context, OUT void *export_master_secret,
	IN OUT uintn *export_master_secret_size)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	if (*export_master_secret_size < selwred_message_context->hash_size) {
		*export_master_secret_size = selwred_message_context->hash_size;
		return RETURN_BUFFER_TOO_SMALL;
	}
	*export_master_secret_size = selwred_message_context->hash_size;
	copy_mem(export_master_secret,
		 selwred_message_context->handshake_secret.export_master_secret,
		 selwred_message_context->hash_size);
	return RETURN_SUCCESS;
}

/**
  Export the SessionKeys from an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  SessionKeys                  Indicate the buffer to store the SessionKeys in spdm_selwre_session_keys_struct_t.
  @param  SessionKeysSize              The size in bytes of the SessionKeys in spdm_selwre_session_keys_struct_t.

  @retval RETURN_SUCCESS  SessionKeys are exported.
*/
return_status
spdm_selwred_message_export_session_keys(IN void *spdm_selwred_message_context,
					 OUT void *SessionKeys,
					 IN OUT uintn *SessionKeysSize)
{
	spdm_selwred_message_context_t *selwred_message_context;
	uintn struct_size;
	spdm_selwre_session_keys_struct_t *session_keys_struct;
	uint8 *ptr;

	selwred_message_context = spdm_selwred_message_context;
	struct_size = sizeof(spdm_selwre_session_keys_struct_t) +
		      (selwred_message_context->aead_key_size +
		       selwred_message_context->aead_iv_size + sizeof(uint64)) *
			      2;

	if (*SessionKeysSize < struct_size) {
		*SessionKeysSize = struct_size;
		return RETURN_BUFFER_TOO_SMALL;
	}

	session_keys_struct = SessionKeys;
	session_keys_struct->version = SPDM_SELWRE_SESSION_KEYS_STRUCT_VERSION;
	session_keys_struct->aead_key_size =
		(uint32)selwred_message_context->aead_key_size;
	session_keys_struct->aead_iv_size =
		(uint32)selwred_message_context->aead_iv_size;

	ptr = (void *)(session_keys_struct + 1);
	copy_mem(ptr,
		 selwred_message_context->application_secret
			 .request_data_encryption_key,
		 selwred_message_context->aead_key_size);
	ptr += selwred_message_context->aead_key_size;
	copy_mem(ptr,
		 selwred_message_context->application_secret.request_data_salt,
		 selwred_message_context->aead_iv_size);
	ptr += selwred_message_context->aead_iv_size;
	copy_mem(ptr,
		 &selwred_message_context->application_secret
			  .request_data_sequence_number,
		 sizeof(uint64));
	ptr += sizeof(uint64);
	copy_mem(ptr,
		 selwred_message_context->application_secret
			 .response_data_encryption_key,
		 selwred_message_context->aead_key_size);
	ptr += selwred_message_context->aead_key_size;
	copy_mem(ptr,
		 selwred_message_context->application_secret.response_data_salt,
		 selwred_message_context->aead_iv_size);
	ptr += selwred_message_context->aead_iv_size;
	copy_mem(ptr,
		 &selwred_message_context->application_secret
			  .response_data_sequence_number,
		 sizeof(uint64));
	ptr += sizeof(uint64);
	return RETURN_SUCCESS;
}

/**
  Import the SessionKeys from an SPDM selwred message context.

  @param  spdm_selwred_message_context    A pointer to the SPDM selwred message context.
  @param  SessionKeys                  Indicate the buffer to store the SessionKeys in spdm_selwre_session_keys_struct_t.
  @param  SessionKeysSize              The size in bytes of the SessionKeys in spdm_selwre_session_keys_struct_t.

  @retval RETURN_SUCCESS  SessionKeys are imported.
*/
return_status
spdm_selwred_message_import_session_keys(IN void *spdm_selwred_message_context,
					 IN void *SessionKeys,
					 IN uintn SessionKeysSize)
{
	spdm_selwred_message_context_t *selwred_message_context;
	uintn struct_size;
	spdm_selwre_session_keys_struct_t *session_keys_struct;
	uint8 *ptr;

	selwred_message_context = spdm_selwred_message_context;
	struct_size = sizeof(spdm_selwre_session_keys_struct_t) +
		      (selwred_message_context->aead_key_size +
		       selwred_message_context->aead_iv_size + sizeof(uint64)) *
			      2;

	if (SessionKeysSize != struct_size) {
		return RETURN_ILWALID_PARAMETER;
	}

	session_keys_struct = SessionKeys;
	if ((session_keys_struct->version !=
	     SPDM_SELWRE_SESSION_KEYS_STRUCT_VERSION) ||
	    (session_keys_struct->aead_key_size !=
	     selwred_message_context->aead_key_size) ||
	    (session_keys_struct->aead_iv_size !=
	     selwred_message_context->aead_iv_size)) {
		return RETURN_ILWALID_PARAMETER;
	}

	ptr = (void *)(session_keys_struct + 1);
	copy_mem(selwred_message_context->application_secret
			 .request_data_encryption_key,
		 ptr, selwred_message_context->aead_key_size);
	ptr += selwred_message_context->aead_key_size;
	copy_mem(selwred_message_context->application_secret.request_data_salt,
		 ptr, selwred_message_context->aead_iv_size);
	ptr += selwred_message_context->aead_iv_size;
	copy_mem(&selwred_message_context->application_secret
			  .request_data_sequence_number,
		 ptr, sizeof(uint64));
	ptr += sizeof(uint64);
	copy_mem(selwred_message_context->application_secret
			 .response_data_encryption_key,
		 ptr, selwred_message_context->aead_key_size);
	ptr += selwred_message_context->aead_key_size;
	copy_mem(selwred_message_context->application_secret.response_data_salt,
		 ptr, selwred_message_context->aead_iv_size);
	ptr += selwred_message_context->aead_iv_size;
	copy_mem(&selwred_message_context->application_secret
			  .response_data_sequence_number,
		 ptr, sizeof(uint64));
	ptr += sizeof(uint64);
	return RETURN_SUCCESS;
}

/**
  Get the last SPDM error struct of an SPDM context.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  last_spdm_error                Last SPDM error struct of an SPDM context.
*/
void spdm_selwred_message_get_last_spdm_error_struct(
	IN void *spdm_selwred_message_context,
	OUT spdm_error_struct_t *last_spdm_error)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	copy_mem(last_spdm_error, &selwred_message_context->last_spdm_error,
		 sizeof(spdm_error_struct_t));
}

/**
  Set the last SPDM error struct of an SPDM context.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  last_spdm_error                Last SPDM error struct of an SPDM context.
*/
void spdm_selwred_message_set_last_spdm_error_struct(
	IN void *spdm_selwred_message_context,
	IN spdm_error_struct_t *last_spdm_error)
{
	spdm_selwred_message_context_t *selwred_message_context;

	selwred_message_context = spdm_selwred_message_context;
	copy_mem(&selwred_message_context->last_spdm_error, last_spdm_error,
		 sizeof(spdm_error_struct_t));
}
