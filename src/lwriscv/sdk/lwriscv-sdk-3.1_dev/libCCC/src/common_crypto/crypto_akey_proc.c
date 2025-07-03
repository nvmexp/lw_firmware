/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* Support for asymmetric ciphers with SE engine (RSA cipher)
 */
#include <crypto_system_config.h>

#if CCC_WITH_RSA || CCC_WITH_ECC

#include <crypto_asig_proc.h>

#if CCC_WITH_ECC
/* EC context setup functions */
#include <crypto_ec.h>
#endif

#if CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT
#if CCC_SOC_WITH_PKA1
#include <tegra_pka1_ks_rw.h>
#endif

#if CCC_SOC_WITH_LWPKA
/* LWPKA support not yet implemented */
#endif
#endif /* CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT */

#ifndef HAVE_RSA_PKEY_HANDLER
/* Set this in config file if you need RSA private key copy functions
 * when RSA signing and RSA ciphering are both disabled.
 */
#define HAVE_RSA_PKEY_HANDLER 0
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if CCC_WITH_RSA

static bool rsa_uses_key_slot(const uint32_t k_flags,
			      const bool engine_requires_hw_keyslot,
			      const bool request_dynamic_slot)
{
	return ((BOOL_IS_TRUE(engine_requires_hw_keyslot) &&
		!BOOL_IS_TRUE(request_dynamic_slot)) ||
		((k_flags & KEY_FLAG_FORCE_KEYSLOT) != 0U) ||
		((k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) ||
		((k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U));
}

static status_t rsa_copy_public_key(struct se_rsa_context *rsa_context,
				    te_crypto_key_t *key,
				    const te_args_key_data_t *kvargs,
				    bool key_big_endian, bool *is_valid_p)
{
	status_t ret = NO_ERROR;
	bool is_valid = false;

	LTRACEF("entry\n");

	if (NULL == is_valid_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	is_valid = *is_valid_p;
	*is_valid_p = false;

	// XXX Does this make sense to verify here?
	if ((rsa_context->ec.rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Operation requires a private key"));
	}

	/* Copy the public key object to key space */
	CCC_OBJECT_ASSIGN(key->k_rsa_pub, kvargs->k_rsa_pub);

	/* Allow context to access the key material
	 *
	 * Public exponent size is one word.
	 * Public modulus size is rsa_size.
	 */
	rsa_context->ec.rsa_pub_expsize = sizeof_u32(key->k_rsa_pub.pub_exponent);

	rsa_context->ec.rsa_pub_exponent = key->k_rsa_pub.pub_exponent;
	rsa_context->ec.rsa_modulus  = key->k_rsa_pub.modulus;
	rsa_context->ec.rsa_priv_exponent = NULL;

	is_valid = (se_util_vec_is_odd(rsa_context->ec.rsa_pub_exponent,
				       rsa_context->ec.rsa_pub_expsize,
				       key_big_endian) && is_valid);

#if SE_RD_DEBUG
	LTRACEF("Setting RSA pubkey key (exponent %u bytes) to slot: %u\n",
		sizeof_u32(key->k_rsa_pub.pub_exponent), key->k_keyslot);
	LOG_HEXBUF("RSA pub exp", key->k_rsa_pub.pub_exponent,
		   sizeof_u32(key->k_rsa_pub.pub_exponent));
	LOG_HEXBUF("RSA modulus", key->k_rsa_pub.modulus, key->k_byte_size);
#endif

	*is_valid_p = is_valid;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_RSA_SIGN || HAVE_RSA_CIPHER || HAVE_RSA_PKEY_HANDLER
static status_t rsa_copy_private_key(struct se_rsa_context *rsa_context,
				    te_crypto_key_t *key,
				    const te_args_key_data_t *kvargs,
				    bool key_big_endian, bool *is_valid_p)
{
	status_t ret = NO_ERROR;
	bool is_valid = false;

	LTRACEF("entry\n");

	if (NULL == is_valid_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	is_valid = *is_valid_p;
	*is_valid_p = false;

	/* Copy the private key object to key space */
	CCC_OBJECT_ASSIGN(key->k_rsa_priv, kvargs->k_rsa_priv);

	/* Allow context to access the key material */

	/* Public exponent size (in private key object) is one word.
	 * (If value is zero => set size to 0 as well. pubkey exponent not known
	 *  in that case).
	 *
	 * Private exponent size is rsa_size.
	 * Private modulus size is rsa_size.
	 */
	rsa_context->ec.rsa_pub_expsize  = sizeof_u32(key->k_rsa_priv.pub_exponent);

	rsa_context->ec.rsa_pub_exponent  = &key->k_rsa_priv.pub_exponent[0];
	rsa_context->ec.rsa_modulus	  = &key->k_rsa_priv.modulus[0];
	rsa_context->ec.rsa_priv_exponent = &key->k_rsa_priv.priv_exponent[0];

	/* Private key exponent must be odd */
	is_valid = (se_util_vec_is_odd(rsa_context->ec.rsa_priv_exponent,
				       key->k_byte_size,
				       key_big_endian) && is_valid);

	{
		status_t rbad = ERR_BAD_STATE;

		if (se_util_vec_is_zero(rsa_context->ec.rsa_pub_exponent,
					sizeof_u32(key->k_rsa_priv.pub_exponent),
					&rbad)) {
			/* PUB EXPONENT value zero =>
			 * it is not defined in private key object.
			 */
			LTRACEF("RSA pub exponent undefined\n");
			rsa_context->ec.rsa_pub_expsize  = 0U;
		} else {
			is_valid = (se_util_vec_is_odd(rsa_context->ec.rsa_pub_exponent,
						       rsa_context->ec.rsa_pub_expsize,
							       key_big_endian) && is_valid);
		}

		if (NO_ERROR != rbad) {
			ret = rbad;
		}
	}

	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LTRACEF("Setting RSA private key (pub exp: %u bytes), "
		"(priv exponent %u bytes), to slot: %u\n",
		sizeof_u32(key->k_rsa_priv.pub_exponent),
		key->k_byte_size, key->k_keyslot);
	LOG_HEXBUF("RSA pub exp", key->k_rsa_priv.pub_exponent,
		   sizeof_u32(key->k_rsa_priv.pub_exponent));
	LOG_HEXBUF("RSA modulus", key->k_rsa_priv.modulus, key->k_byte_size);
	LOG_HEXBUF("RSA priv expon", key->k_rsa_priv.priv_exponent,
		   key->k_byte_size);
#endif

	*is_valid_p = is_valid;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_SIGN || HAVE_RSA_CIPHER || HAVE_RSA_PKEY_HANDLER */

static status_t rsa_copy_key(struct se_rsa_context *rsa_context,
			     te_crypto_key_t *key,
			     const te_args_key_data_t *kvargs,
			     bool key_big_endian)
{
	status_t ret = NO_ERROR;
	bool is_valid = true;

	LTRACEF("entry\n");

	/* If not using an existing keyslot key => copy key material over */
	switch (key->k_key_type) {
	case KEY_TYPE_RSA_PUBLIC:
		ret = rsa_copy_public_key(rsa_context, key, kvargs,
					  key_big_endian, &is_valid);
		break;

	case KEY_TYPE_RSA_PRIVATE:
#if HAVE_RSA_SIGN || HAVE_RSA_CIPHER || HAVE_RSA_PKEY_HANDLER
		ret = rsa_copy_private_key(rsa_context, key, kvargs,
					   key_big_endian, &is_valid);
#else
		LTRACEF("RSA private key support disabled\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		LTRACEF("rsa key type invalid: %u\n", key->k_key_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* Modulus and pub exponent must be odd */
	is_valid = (se_util_vec_is_odd(rsa_context->ec.rsa_modulus,
				       key->k_byte_size, key_big_endian) && is_valid);

	/* Modulus high order bit must be set */
	is_valid = (se_util_vec_is_msb_set(rsa_context->ec.rsa_modulus,
					   key->k_byte_size, key_big_endian) && is_valid);

	if (! BOOL_IS_TRUE(is_valid)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA modulus or exponent invalid\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION
/* Special case for RSA2048 bit RSA keys in automotive
 * environment: Allow RSA2048 bit signature verifications with
 * the algorithms listed in the switch case.
 *
 * Shorter keys are blocked by other means before this call.
 */
static status_t lw_automotive_rsa_use_check(struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((2048U / 8U) == rsa_context->ec.rsa_size) {
		switch (rsa_context->ec.algorithm) {
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:

			if (TE_ALG_MODE_VERIFY == rsa_context->mode) {
				/* Only allows sig verifications with RSA2048
				 * bit keys with the specified algos.
				 */
				ret = NO_ERROR;
			} else {
				ret = SE_ERROR(ERR_NOT_SUPPORTED);
			}
			break;

		default:
			LTRACEF("Algorithm 0x%x not supported for this RSA keysize %u\n",
				rsa_context->ec.algorithm, rsa_context->ec.rsa_size);
			ret = SE_ERROR(ERR_NOT_SUPPORTED);
			break;
		}
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION */

static status_t se_rsa_select_engine(struct se_rsa_context *rsa_context,
				     uint32_t k_byte_size)
{
	status_t ret = NO_ERROR;
	uint32_t k_bit_size = 0U;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_SAFE_MULT_U32(k_bit_size, k_byte_size, 8U);

	switch (k_bit_size) {
#if CCC_WITH_RSA4096
	case 4096U:
	case 3072U:
		/* Just in case earlier selection was up to 2048 bit rsa key => switch engine
		 * if the key is longer than that. Use the previous selection as a hint.
		 *
		 * The only engine supporting these key lengths is (lwrrently) the PKA1.
		 */
		ret = ccc_select_engine(&rsa_context->ec.engine,
					CCC_ENGINE_CLASS_RSA_4096,
					rsa_context->ec.engine->e_id);
		break;
#endif

	case 2048U:
	case 1536U:
	case 1024U:
	case  512U:
		break;
	default:
		CCC_ERROR_MESSAGE("Unsupported SE RSA key length %u\n", k_bit_size);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_get_dynamic_keyslot(const struct se_rsa_context *rsa_context,
					   te_crypto_key_t *key,
					   bool engine_requires_hw_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)rsa_context;

	if (((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) ||
	    ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Dynamic key does not use HW keyslots required for provided flags\n"));
	}

	/* RSA operation dynamic key uses either (PKA1) bank registers only OR
	 * (SE RSA0) will use this k_keyslot value.
	 */
	if (BOOL_IS_TRUE(engine_requires_hw_keyslot)) {
#if HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT)
#if HAVE_SE_RSA
		/* Here CCC needs a dynamic keyslot for PKA0 (RSA) engine.
		 *
		 * If not enabled, the operation fails.
		 */
		ret = pka0_engine_get_dynamic_keyslot(&rsa_context->ec,
						      &key->k_keyslot);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		CCC_ERROR_CHECK(ret,
				LTRACEF("RSA dynamic keyslot not set up\n"));
#else /* HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT) */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("PKA0 dynamic keyslot not supported"));
#endif /* HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT) */
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_process_keyslot(struct se_rsa_context *rsa_context,
				       te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;
	bool engine_requires_hw_keyslot = false;
	bool request_dynamic_slot = false;

	LTRACEF("entry\n");

	/* SE PKA0 RSA unit reguires HW keyslot
	 */
	engine_requires_hw_keyslot = (rsa_context->ec.engine->e_id == CCC_ENGINE_SE0_PKA0);

	/* Caller requests use of dynamic keyslot? */
	request_dynamic_slot = ((key->k_flags & KEY_FLAG_DYNAMIC_SLOT) != 0U);

	/* SE0/SE1 PKA0 engines do not need to set
	 * KEY_FLAG_FORCE_KEYSLOT to use a keyslot
	 * since engine requires it. Only use dyn keyslot
	 * on explicit request for such with PKA0.
	 */
	if (rsa_uses_key_slot(key->k_flags, engine_requires_hw_keyslot,
				request_dynamic_slot)) {
		rsa_context->ec.rsa_flags |= RSA_FLAGS_USE_KEYSLOT;

		key->k_flags &= ~KEY_FLAG_DYNAMIC_SLOT;
	} else {
		/* The RSA op is done using PKA1 => can set
		 * this unless other flags above prevent it.
		 */
		key->k_flags |= KEY_FLAG_DYNAMIC_SLOT;
	}

	/* Allows FORCE_KEYSLOT to be set also when "dynamic" with PKA1.
	 * Not much sense, bank regs are better in that case.
	 */
	if ((key->k_flags & KEY_FLAG_DYNAMIC_SLOT) != 0U) {
		ret = se_rsa_get_dynamic_keyslot(rsa_context, key,
						 engine_requires_hw_keyslot);
		CCC_ERROR_CHECK(ret);
	}

	if (key->k_keyslot >= SE_RSA_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("RSA key slot index too large (%u)\n",
						   key->k_keyslot));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_set_rsa_pub_expsize(struct se_rsa_context *rsa_context,
				       const te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == rsa_context) || (NULL == key)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (key->k_key_type) {
	case KEY_TYPE_RSA_PUBLIC:
		rsa_context->ec.rsa_pub_expsize = sizeof_u32(key->k_rsa_pub.pub_exponent);
		break;
	case KEY_TYPE_RSA_PRIVATE:
		rsa_context->ec.rsa_pub_expsize  = sizeof_u32(key->k_rsa_priv.pub_exponent);
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret, LTRACEF("rsa key type invalid: %u\n", key->k_key_type));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_set_key_to_context(struct se_rsa_context *rsa_context,
				       const te_args_key_data_t *kvargs,
				       te_crypto_key_t *key, bool copy_key)
{
	status_t ret = NO_ERROR;
	bool key_big_endian = false;

	LTRACEF("entry\n");

	/* No individual user control for exponent / modulus endianess for now.
	 *
	 * A key in a keyslot is always LITTLE ENDIAN, no matter what the
	 * flags say. This defines the endianess of keys passed in
	 * from the client.
	 */
	if ((key->k_flags & KEY_FLAG_LITTLE_ENDIAN) == 0U) {
		rsa_context->ec.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_MODULUS;
		rsa_context->ec.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_EXPONENT;

		key_big_endian = true;
	}

	if (BOOL_IS_TRUE(copy_key)) {
		ret = rsa_copy_key(rsa_context, key, kvargs, key_big_endian);
		CCC_ERROR_CHECK(ret);
	}

	rsa_context->ec.rsa_size    = key->k_byte_size;
	rsa_context->ec.rsa_keytype = key->k_key_type;
	rsa_context->ec.rsa_keyslot = key->k_keyslot;

	if ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) {
		rsa_context->ec.rsa_flags |= RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT;
	}

#if HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION
	ret = lw_automotive_rsa_use_check(rsa_context);
	CCC_ERROR_CHECK(ret);
#endif
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RSA key setup function */
status_t se_setup_rsa_key(struct se_rsa_context *rsa_context, te_crypto_key_t *key,
			  const te_args_key_data_t *kvargs)
{
	status_t ret = NO_ERROR;
	bool copy_key = true;

	LTRACEF("entry\n");

	key->k_byte_size = kvargs->k_byte_size;

	if (!BOOL_IS_TRUE(IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(key->k_byte_size))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("Unsupported RSA key length\n"));
	}

	ret = se_rsa_select_engine(rsa_context, key->k_byte_size);
	CCC_ERROR_CHECK(ret);

	key->k_key_type  = kvargs->k_key_type;
	key->k_flags     = kvargs->k_flags;
	key->k_keyslot   = kvargs->k_keyslot;

	ret = se_rsa_process_keyslot(rsa_context, key);
	CCC_ERROR_CHECK(ret);

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		LTRACEF("Using existing asymmetric key lwrrently in slot: %u\n", key->k_keyslot);
		rsa_context->ec.rsa_flags |= RSA_FLAGS_USE_PRESET_KEY;
		ret = se_set_rsa_pub_expsize(rsa_context, key);
		CCC_ERROR_CHECK(ret, LTRACEF("rsa key type invalid: %u\n", key->k_key_type));

		/* No matter if using pre-loaded key: if client states the key
		 * is valid in key object => copy key material over.
		 *
		 * Do this even when using pre-loaded key values from key slot.
		 * It is then mandatory that the key value is present!
		 */
		if ((key->k_flags & KEY_FLAG_KMAT_VALID) != 0U) {
			rsa_context->ec.rsa_flags |= RSA_FLAGS_KMAT_VALID;
			copy_key = true;
		} else {
			copy_key = false;
		}
	}

	ret = rsa_set_key_to_context(rsa_context, kvargs, key, copy_key);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECC

#if CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT
static status_t read_ec_pubkey_from_keyslot(struct se_engine_ec_context *econtext,
					    te_ec_point_t *pk,
					    uint32_t kslot)
{
	status_t ret = NO_ERROR;
	uint32_t pflags = pk->point_flags;

	LTRACEF("entry\n");

	if ((kslot >= PKA1_MAX_KEYSLOTS) || (NULL == pk) || (NULL == econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* X, Y co-ordinates need to be UNDEFINED in the object.
	 */
	if ((pflags & CCC_EC_POINT_FLAG_UNDEFINED) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* X,Y values need to be read from keyslot.
	 */
	if ((pflags & CCC_EC_POINT_FLAG_IN_KEYSLOT) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Here the flags that are passed to the EC pubkey reader
	 * are set also in the returned pub key object.
	 *
	 * Supported flags here are (lwrrently):
	 *   CCC_EC_POINT_FLAG_LITTLE_ENDIAN
	 *   CCC_EC_POINT_FLAG_COMPRESSED_ED25519
	 */
	pflags &= ~(CCC_EC_POINT_FLAG_UNDEFINED | CCC_EC_POINT_FLAG_IN_KEYSLOT);

#if CCC_SOC_WITH_PKA1
	ret = pka1_read_ec_point_keyslot(econtext->engine->e_id, kslot,
					 econtext->ec_lwrve->id,
					 pflags, pk);
	CCC_ERROR_CHECK(ret);
#elif CCC_SOC_WITH_LWPKA
	ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
#endif /* CCC_SOC_WITH_LWPKA */

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT */

#if CCC_WITH_X25519
/* Reformat the private key (scalar) for c25519 lwrve operations to match the
 * requirements.
 */
static status_t x25519_scalar_expand(struct se_engine_ec_context *econtext, ec_private_t *ec_key)
{
	status_t ret = NO_ERROR;
	bool reversed = false;

	LTRACEF("entry\n");

	/* Easier to handle little endian values => reverse byte order if
	 * it is big endian.
	 */
	if ((econtext->ec_flags & CCC_EC_FLAGS_BIG_ENDIAN_KEY) != 0U) {
		ret = se_util_reverse_list(ec_key->key, ec_key->key, ec_key->key_length);
		CCC_ERROR_CHECK(ret);

		reversed = true;
	}

	if (ec_key->key_length < 32U) {
		/* Zeropad a short little endian key */
		se_util_mem_set(&ec_key->key[ec_key->key_length], 0U, 32U - ec_key->key_length);
	}

	/* The key length is fixed to 255 bits [0..254]; the msb(255) of MSB(32) is set 0,
	 * and 3 lowest bits of LSB(0) are cleared.
	 */
	ec_key->key_length = 32U;

	ec_key->key[0]  = BYTE_VALUE(ec_key->key[0]  & 0xF8U);	/* Clear least significant 3 bits */
	ec_key->key[31] = BYTE_VALUE(ec_key->key[31] & 0x7FU);	/* Clear bit 255 */
	ec_key->key[31] = BYTE_VALUE(ec_key->key[31] | 0x40U);	/* Set   bit 254 */

	if (BOOL_IS_TRUE(reversed)) {
		/* Reverse the (now 32 byte) key back to big endian byte order.
		 * (TODO (consider): Maybe could leave LE and swap the flag)
		 */
		ret = se_util_reverse_list(ec_key->key, ec_key->key, ec_key->key_length);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_X25519 */

#if CCC_WITH_ED25519 && HAVE_ED25519_SIGN
/* Reformat the private key (scalar) for ed25519 lwrve operations to match the
 * requirements.
 */
static status_t ed25519_scalar_expand(const struct se_engine_ec_context *econtext,
				      ec_private_t *ec_key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("Encoding ED25519 scalar\n");
	// Only supports 32 byte scalar keys
	// (OTOH, could be arbitrary, using SHA-512 for this scalar value)
	//
	if (32U != ec_key->key_length) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Overwrites the bytes [0..31] of the in-kernel copy
	 * ec_key->key field with the encoded value.
	 *
	 * Value "h" used for signature creation (last 32 bytes of
	 * SHA512 digest are stored to the bytes key[32..63].
	 *
	 * XXX Maybe copy H only if this is a SIGN operation =>
	 * TODO/FIXME XXX and not save it on verify ops (verify
	 * version which does not provide pubkey)
	 */
	ret = ed25519_encode_scalar(econtext, ec_key->key, &ec_key->key[32U],
				    ec_key->key, ec_key->key_length);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to encode scalar\n"));

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ED25519 && HAVE_ED25519_SIGN */

static status_t setup_ec_key_arg_check(const struct se_ec_context *ec_context,
				       const te_crypto_key_t *key,
				       const te_args_key_data_t *kvargs)
{
	status_t ret = NO_ERROR;

	if ((NULL == ec_context) || (NULL == key) || (NULL == kvargs)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ec_context->ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve undefined\n"));
	}

fail:
	return ret;
}

static status_t ec_kflags(struct se_ec_context *ec_context,
			  te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	/* E.g. PKA1 unit EC point multiplication uses bank registers
	 * by default => these options makes it use keyslots instead.
	 *
	 * For PKA1: If KEY_FLAG_FORCE_KEYSLOT is set PKA1 will use (for point mult only)
	 *  key value lwrrently set in key->k_keyslot. If this flag is not set,
	 *  PKA1 will do EC point multiplication with BANK registers only.
	 *
	 * Note: Out of the EC operations => only EC point
	 * multiplication used PKA1 HW keyslots, so there is no real
	 * reason to use these flags for anything else.
	 */
	if (((key->k_flags & KEY_FLAG_FORCE_KEYSLOT) != 0U) ||
	    ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) ||
	    ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U)) {
		ec_context->ec.ec_flags |= CCC_EC_FLAGS_USE_KEYSLOT;

		key->k_flags &= ~KEY_FLAG_DYNAMIC_SLOT;
	} else {
		/* Because EC ops are always using PKA1 (which supports
		 * bank regs) => if keyslot is not forced above =>
		 * using dynamic keyslot by default!
		 */
		key->k_flags |= KEY_FLAG_DYNAMIC_SLOT;
	}

	if ((key->k_flags & KEY_FLAG_DYNAMIC_SLOT) != 0U) {
		/* EC operation dynamic key uses bank registers only;
		 * k_keyslot value ignored
		 *
		 * Setting invalid PKA1 keyslot value; PKA1 uses bank
		 * registers in this case
		 */
		key->k_keyslot = PKA1_MAX_KEYSLOTS;
	} else {
		/* Do not let this be arbitrary when keyslot is used */
		if (key->k_keyslot >= PKA1_MAX_KEYSLOTS) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("EC key slot number too large (%u)\n",
							   key->k_keyslot));
		}
	}

	/* BIG ENDIAN private key by default.
	 */
	if ((key->k_flags & KEY_FLAG_LITTLE_ENDIAN) == 0U) {
		ec_context->ec.ec_flags |= CCC_EC_FLAGS_BIG_ENDIAN_KEY;
	}

	if ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) {
		ec_context->ec.ec_flags |= CCC_EC_FLAGS_LEAVE_KEY_TO_KEYSLOT;
	}

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}

static status_t ec_kvalue_pkey(struct se_ec_context *ec_context, te_crypto_key_t *key,
			       const te_args_key_data_t *kvargs)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Copy the private key object to key space in kernel */
	CCC_OBJECT_ASSIGN(key->k_ec_private, kvargs->k_ec_private);

	/* Verify that private key byte length matches lwrve byte length
	 *
	 * XXX What about the "odd size lwrves" like P521?
	 * TODO => Verify (deal with these when P521 implemented)
	 */
	if (key->k_ec_private.key_length != key->k_byte_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC private key length must be same as lwrve size in bytes: %u != %u\n",
					     key->k_ec_private.key_length, key->k_byte_size));
	}

	/* These lwrves require "pre-formatting" the private key. The key value
	 * is just modified to match the requirements.
	 *
	 * A local copy of the key object is mangled, original is left unmodified.
	 */
	if (TE_LWRVE_C25519 == ec_context->ec.ec_lwrve->id) {
#if CCC_WITH_X25519
		ret = x25519_scalar_expand(&ec_context->ec, &key->k_ec_private);
		CCC_ERROR_CHECK(ret, LTRACEF("c25519 scalar value decode failed"));
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#endif
	}

	if (TE_LWRVE_ED25519 == ec_context->ec.ec_lwrve->id) {
#if CCC_WITH_ED25519 && HAVE_ED25519_SIGN
		ret = ed25519_scalar_expand(&ec_context->ec, &key->k_ec_private);
		CCC_ERROR_CHECK(ret, LTRACEF("ed25519 scalar value decode failed"));
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#endif
	}

	/* Allow context to access the priv key material */

	/* Depends on ec_private.pubkey.point_flags if this
	 * contains a valid pubkey.
	 */
	ec_context->ec.ec_pubkey      = &key->k_ec_private.pubkey;
	ec_context->ec.ec_pkey        = key->k_ec_private.key;
	ec_context->ec.ec_keytype     = key->k_key_type;

#if SE_RD_DEBUG
	LOG_HEXBUF("EC pkey", ec_context->ec.ec_pkey, key->k_byte_size);
#endif

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}

static status_t ec_kvalue(struct se_ec_context *ec_context, te_crypto_key_t *key,
			  const te_args_key_data_t *kvargs, uint32_t pubkey_pass_kslot)
{
	status_t ret = NO_ERROR;

	(void)pubkey_pass_kslot;

	LTRACEF("entry\n");

	/* If not using an existing keyslot key => copy key material over */
	switch (key->k_key_type) {
	case KEY_TYPE_EC_PUBLIC:
		/* Copy the public key object (effectively a
		 * point) to key space in kernel
		 */
		CCC_OBJECT_ASSIGN(key->k_ec_public, kvargs->k_ec_public);

		/* Allow engine context to access the key material
		 */
		ec_context->ec.ec_pubkey = &key->k_ec_public.pubkey;
		break;

	case KEY_TYPE_EC_PRIVATE:
		ret = ec_kvalue_pkey(ec_context, key, kvargs);
		break;
	default:
		LTRACEF("EC key type invalid: %u\n", key->k_key_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if ((ec_context->ec.ec_pubkey->point_flags & CCC_EC_POINT_FLAG_IN_KEYSLOT) != 0U) {
#if CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT
		ret = read_ec_pubkey_from_keyslot(&ec_context->ec,
						  ec_context->ec.ec_pubkey,
						  pubkey_pass_kslot);
		CCC_ERROR_CHECK(ret);
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("EC public key in keyslot is not supported\n"));
#endif /* CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT */
	}

#if SE_RD_DEBUG
	LOG_HEXBUF("EC pub Px", ec_context->ec.ec_pubkey->x, key->k_byte_size);
	LOG_HEXBUF("EC pub Py", ec_context->ec.ec_pubkey->y, key->k_byte_size);
	if ((ec_context->ec.ec_pubkey->point_flags &
	     CCC_EC_POINT_FLAG_UNDEFINED) != 0) {
		LTRACEF("Public key is undefined\n");
	}
#if CCC_WITH_ED25519
	if (TE_LWRVE_ED25519 == ec_context->ec.ec_lwrve->id) {
		if ((ec_context->ec.ec_pubkey->point_flags &
		     CCC_EC_POINT_FLAG_COMPRESSED_ED25519) != 0) {
			LTRACEF("ED25519 compressed public key\n");
		}
	}
#endif
#endif /* SE_RD_DEBUG */

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}

status_t se_setup_ec_key(struct se_ec_context *ec_context, te_crypto_key_t *key,
			 const te_args_key_data_t *kvargs)
{
	status_t ret = NO_ERROR;
	uint32_t keyslot = PKA1_MAX_KEYSLOTS;

	LTRACEF("entry\n");

	ret = setup_ec_key_arg_check(ec_context, key, kvargs);
	CCC_ERROR_CHECK(ret);

	key->k_key_type = kvargs->k_key_type;

	switch (key->k_key_type) {
	case KEY_TYPE_EC_PUBLIC:
	case KEY_TYPE_EC_PRIVATE:
		break;

	default:
		LTRACEF("EC setup key: invalid key type %u\n",
			key->k_key_type);
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	CCC_ERROR_CHECK(ret);

	/* This defines all byte lengths used in the EC operations.
	 *
	 * The k_byte_size is not used from EC key material args; public key
	 * is a point object (which contains coordinates which are of lwrve size);
	 * the private key is a ec_private_t object which contains a key object
	 * and the byte length of the key is in that object => the k_byte_size
	 * is not used for structured objects.
	 */
	key->k_byte_size = ec_context->ec.ec_lwrve->nbytes;
	key->k_flags     = kvargs->k_flags;
	keyslot		 = kvargs->k_keyslot;
	key->k_keyslot   = keyslot;

	ret = ec_kflags(ec_context, key);
	CCC_ERROR_CHECK(ret);

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		LTRACEF("Using existing asymmetric key lwrrently in slot: %u\n", key->k_keyslot);
		ec_context->ec.ec_flags |= CCC_EC_FLAGS_USE_PRESET_KEY;
	} else {
		ret = ec_kvalue(ec_context, key, kvargs, keyslot);
		CCC_ERROR_CHECK(ret);
	}

	ec_context->ec.ec_keyslot = key->k_keyslot;

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECC */

#endif /* CCC_WITH_RSA || CCC_WITH_ECC */
