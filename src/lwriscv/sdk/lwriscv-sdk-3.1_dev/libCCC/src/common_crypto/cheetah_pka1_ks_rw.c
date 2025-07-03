/*
 * Copyright (c) 2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* PKA1 engine keyslot public key read / write.
 *
 * Only compile this code if your subsystem requires reading PKA1
 * keyslot values passed in from external entities in keyslots.
 *
 * CCC core does not need this code for any purpose it exists for
 * clients that pass public values in ks.
 */
#include <crypto_system_config.h>

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC

#include <crypto_api.h>
#include <tegra_cdev.h>
#include <tegra_pka1_gen.h>
#include <tegra_pka1_ks_rw.h>

#if (HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY || HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY) && HAVE_PKA1_ECC
#include <crypto_ec.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY || HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY

static status_t get_keyslot_field_value(const struct engine_s *engine,
					uint32_t key_slot,
					uint8_t *param,
					uint32_t key_words,
					bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t val32 = 0U;
	uint32_t offset = 0U;
	uint32_t len = word_count_to_bytes(key_words);

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(data_big_endian)) {
		/* PKA unit operates on little endian data, must swap word and byte order */
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = tegra_engine_get_reg(engine, SE_PKA1_KEYSLOT_DATA_OFFSET(key_slot));
			se_util_u32_to_buf(val32, &param[(offset - BYTES_IN_WORD)], data_big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = tegra_engine_get_reg(engine, SE_PKA1_KEYSLOT_DATA_OFFSET(key_slot));
			se_util_u32_to_buf(val32, &param[offset], data_big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset != len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Read the key material from the pre-defined field in the specified SE engine keyslot.
 */
static status_t pka1_get_keyslot_field(const struct engine_s *engine,
				       pka1_keymat_field_t field,
				       uint32_t key_slot,
				       uint8_t *param,
				       uint32_t key_words,
				       bool data_big_endian)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == param) || (0U == key_words) ||
	    (key_slot >= PKA1_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Read <kslot,field> in auto-increment mode starting from beginning of field */
	tegra_engine_set_reg(engine,
			     SE_PKA1_KEYSLOT_ADDR_OFFSET(key_slot),
			     SE_PKA1_KEYSLOT_ADDR_FIELD(field) |
			     SE_PKA1_KEYSLOT_ADDR_WORD(0U) |
			     SE_PKA1_KEYSLOT_ADDR_AUTO_INC(SE_ELP_ENABLE));

	LTRACEF("Setting %u words in field %u in keyslot %u (BE=%u)\n",
		key_words, field, key_slot, data_big_endian);

	ret = get_keyslot_field_value(engine, key_slot, param,
				      key_words, data_big_endian);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY && HAVE_PKA1_RSA

/* reads RSA public key <exp,modulus,M_prime,R2> from given keyslot.
 */
status_t pka1_read_rsa_keyslot(
	engine_id_t eid,
	uint32_t rsa_keyslot, uint32_t rsa_bit_len, bool data_big_endian,
	struct ks_rsa_data *ks_data)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;
	const uint32_t pub_exponent_words = RSA_PUBLIC_EXPONENT_BYTE_SIZE / BYTES_IN_WORD;
	const engine_t *engine = NULL;
	uint32_t len = rsa_bit_len / 8U;

	LTRACEF("entry\n");

	if (NULL == ks_data) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (! IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(len)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	ret = ccc_select_engine(&engine, CCC_ENGINE_CLASS_RSA_4096, eid);
	CCC_ERROR_CHECK(ret);

	rsa_size_words = len / BYTES_IN_WORD;

	LTRACEF("Extracting RSA-%u parameters from PKA1 keyslot %u\n",
		rsa_bit_len, rsa_keyslot);

	LTRACEF("Reading %u word public exponent\n", pub_exponent_words);
	ret = pka1_get_keyslot_field(engine, KMAT_RSA_EXPONENT,
				     rsa_keyslot,
				     &ks_data->rsa_pub_exponent[0],
				     pub_exponent_words, data_big_endian);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA kslot %u pub exponent (field %u)\n",
					  rsa_keyslot, KMAT_RSA_EXPONENT));

	LTRACEF("Extracting %u word modulus\n", rsa_size_words);
	ret = pka1_get_keyslot_field(engine, KMAT_RSA_MODULUS,
				     rsa_keyslot,
				     &ks_data->rsa_modulus[0],
				     rsa_size_words, data_big_endian);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA kslot %u modulus (field %u)\n",
					  rsa_keyslot, KMAT_RSA_MODULUS));

	LTRACEF("Extracting %u word m_prime\n", rsa_size_words);
	ret = pka1_get_keyslot_field(engine, KMAT_RSA_M_PRIME,
				     rsa_keyslot,
				     &ks_data->rsa_m_prime[0],
				     rsa_size_words, data_big_endian);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA kslot %u M' (field %u)\n",
					  rsa_keyslot, KMAT_RSA_M_PRIME));
	LTRACEF("Extracting %u word r2_sqr\n", rsa_size_words);
	ret = pka1_get_keyslot_field(engine, KMAT_RSA_R2_SQR,
				     rsa_keyslot,
				     &ks_data->rsa_r2_sqr[0],
				     rsa_size_words, data_big_endian);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA kslot %u R2 (field %u)\n",
					  rsa_keyslot, KMAT_RSA_R2_SQR));

#if SE_RD_DEBUG
	LOG_HEXBUF("read RSA exponent", ks_data->rsa_pub_exponent, RSA_PUBLIC_EXPONENT_BYTE_SIZE);
	LOG_HEXBUF("read RSA modulus", ks_data->rsa_modulus, len);
	LOG_HEXBUF("read RSA M_PRIME", ks_data->rsa_m_prime, len);
	LOG_HEXBUF("read RSA R2_SQR", ks_data->rsa_r2_sqr, len);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY && HAVE_PKA1_RSA */

#if HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY && HAVE_PKA1_ECC
/* Reads EC public key point <X,Y> from given keyslot.
 *
 * point_flags CCC_EC_POINT_FLAG_LITTLE_ENDIAN can be set if point
 * should be stored in pubkey parameter in little endian byte order.
 *
 * For ED25519 lwrve the public key in the keyslot could be compressed
 * and in such case the CCC_EC_POINT_FLAG_COMPRESSED_ED25519 must be
 * set to point_flags.  (for a compressed point there is no Y
 * co-ordinate. The Y co-ordinate is encoded into the 32 byte X
 * co-ordinate and CCC will perform the decompression on signature
 * handling). The compressed point ofr this lwrve is little endian value.
 */
status_t pka1_read_ec_point_keyslot(
	engine_id_t eid,
	uint32_t ec_keyslot,
	te_ec_lwrve_id_t lwrve_id,
	uint32_t point_flags,
	te_ec_point_t *pubkey)
{
	status_t ret    = NO_ERROR;
	uint32_t nwords = 0U;
	const pka1_ec_lwrve_t *lwrve = ccc_ec_get_lwrve(lwrve_id);
	const engine_t *engine = NULL;
	uint32_t valid_point_flags = CCC_EC_POINT_FLAG_LITTLE_ENDIAN;

	LTRACEF("entry\n");

	if (NULL == pubkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve %u not supported\n", lwrve_id));
	}

	if (TE_LWRVE_ED25519 == lwrve_id) {
		valid_point_flags |= CCC_EC_POINT_FLAG_COMPRESSED_ED25519;
	}

	if ((point_flags & ~valid_point_flags) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Unsupported point flags in 0x%x\n",
					     point_flags));
	}

	ret = ccc_select_engine(&engine, CCC_ENGINE_CLASS_EC, eid);
	CCC_ERROR_CHECK(ret);

	nwords = lwrve->nbytes / BYTES_IN_WORD;

	if ((point_flags & CCC_EC_POINT_FLAG_COMPRESSED_ED25519) == 0U) {
		bool data_big_endian = (point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U;

		LTRACEF("%s point <x,y> (%u bytes, flags 0x%x) from PKA keyslot %u\n",
			lwrve->name, lwrve->nbytes, point_flags, ec_keyslot);

		pubkey->point_flags = point_flags;

		LTRACEF("Reading %u word X coordinate\n", nwords);
		ret = pka1_get_keyslot_field(engine, KMAT_EC_PX,
					     ec_keyslot,
					     &pubkey->x[0],
					     nwords, data_big_endian);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to read EC kslot %u X coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PX));

		LTRACEF("Reading %u word Y coordinate\n", nwords);
		ret = pka1_get_keyslot_field(engine, KMAT_EC_PY,
					     ec_keyslot,
					     &pubkey->y[0],
					     nwords, data_big_endian);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to read EC kslot %u Y coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PY));
#if SE_RD_DEBUG
		LOG_HEXBUF("EC point X", pubkey->x, lwrve->nbytes);
		LOG_HEXBUF("EC point Y", pubkey->y, lwrve->nbytes);
#endif
	} else {
		LTRACEF("%s compressed point (%u bytes) from PKA keyslot %u\n",
			lwrve->name, lwrve->nbytes, ec_keyslot);

		pubkey->point_flags = CCC_EC_POINT_FLAG_COMPRESSED_ED25519 | CCC_EC_POINT_FLAG_LITTLE_ENDIAN;

		/* Compressed point is the same size, in little endian */
		LTRACEF("Reading %u word ED25519 compressed point\n", nwords);
		ret = pka1_get_keyslot_field(engine, KMAT_EC_PX,
					     ec_keyslot,
					     &pubkey->ed25519_compressed_point[0],
					     nwords, false);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to read EC kslot %u X coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PX));

		se_util_mem_set(&pubkey->y[0], 0U, lwrve->nbytes);
#if SE_RD_DEBUG
		LOG_HEXBUF("ED25519 compressed point", pubkey->ed25519_compressed_point, lwrve->nbytes);
#endif
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY && HAVE_PKA1_ECC */
#endif /* HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY || HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY */

#if HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY && HAVE_PKA1_ECC
/* Write EC public key point to ec_keyslot.
 *
 * Uses CCC core function pka1_set_keyslot_field()
 */
status_t pka1_write_ec_point_keyslot(
	engine_id_t eid,
	uint32_t ec_keyslot,
	te_ec_lwrve_id_t lwrve_id,
	const te_ec_point_t *pubkey)
{
	status_t ret    = NO_ERROR;
	uint32_t nwords = 0U;
	const pka1_ec_lwrve_t *lwrve = ccc_ec_get_lwrve(lwrve_id);
	const engine_t *engine = NULL;
	uint32_t valid_point_flags = CCC_EC_POINT_FLAG_LITTLE_ENDIAN;

	LTRACEF("entry\n");

	if (NULL == pubkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve %u not supported\n", lwrve_id));
	}

	if (TE_LWRVE_ED25519 == lwrve_id) {
		valid_point_flags |= CCC_EC_POINT_FLAG_COMPRESSED_ED25519;
	}

	ret = ccc_select_engine(&engine, CCC_ENGINE_CLASS_EC, eid);
	CCC_ERROR_CHECK(ret);

	if ((pubkey->point_flags & CCC_EC_POINT_FLAG_UNDEFINED) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Pubkey is undefined 0x%x\n",
					     pubkey->point_flags));
	}

	/* Safeguard: If this traps, add support for
	 * new/unsupported flags or explicitly ignore them!
	 */
	if ((pubkey->point_flags & ~valid_point_flags) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Unsupported point flags in 0x%x\n",
					     pubkey->point_flags));
	}

	nwords = lwrve->nbytes / BYTES_IN_WORD;

	if ((pubkey->point_flags & CCC_EC_POINT_FLAG_COMPRESSED_ED25519) == 0U) {
		/* Bytes order of co-ordinates in the pubkey object.
		 * PKA1 keyslots values are in little endian byte order.
		 */
		bool data_big_endian = (pubkey->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U;

		LTRACEF("%s point <x,y> (%u bytes, flags 0x%x) to PKA keyslot %u\n",
			lwrve->name, lwrve->nbytes, pubkey->point_flags, ec_keyslot);

#if SE_RD_DEBUG
		/* Endian depends of pubkey flags */
		LOG_HEXBUF("EC point X", pubkey->x, lwrve->nbytes);
		LOG_HEXBUF("EC point Y", pubkey->y, lwrve->nbytes);
#endif

		LTRACEF("Writing %u word X coordinate\n", nwords);
		ret = pka1_set_keyslot_field(engine, KMAT_EC_PX,
					     ec_keyslot,
					     &pubkey->x[0],
					     nwords, data_big_endian, nwords);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to write EC kslot %u X coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PX));

		LTRACEF("Writing %u word Y coordinate\n", nwords);
		ret = pka1_set_keyslot_field(engine, KMAT_EC_PY,
					     ec_keyslot,
					     &pubkey->y[0],
					     nwords, data_big_endian, nwords);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to write EC kslot %u Y coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PY));
	} else {
		/* Compressed ED25519 points are EXPECTED to be in
		 * LITTLE ENDIAN always.
		 *
		 * The CCC_EC_POINT_FLAG_LITTLE_ENDIAN is not checked
		 * here for this reason!!! Simple to add the check
		 * here if needed. But check also ed2519 point
		 * compression functions if you need to do this here.
		 */
#if SE_RD_DEBUG
		LOG_HEXBUF("ED25519 compressed point (LE)",
			   pubkey->ed25519_compressed_point, lwrve->nbytes);
#endif

		LTRACEF("%s compressed point (%u bytes) to keyslot %u\n",
			lwrve->name, lwrve->nbytes, ec_keyslot);

		/* Compressed point is the same size as X co-ordinate,
		 * in little endian.
		 */
		LTRACEF("Writing %u word ED25519 compressed point\n", nwords);
		ret = pka1_set_keyslot_field(engine, KMAT_EC_PX,
					     ec_keyslot,
					     &pubkey->ed25519_compressed_point[0],
					     nwords, false, nwords);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to write EC kslot %u X coordinate (field %u)\n",
						  ec_keyslot, KMAT_EC_PX));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY && HAVE_PKA1_ECC */

#endif /* HAVE_PKA1_RSA || HAVE_PKA1_ECC */
