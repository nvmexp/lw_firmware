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

#include <crypto_system_config.h>

#if CCC_WITH_RSA

#include <crypto_asig_proc.h>
#include <crypto_md_proc.h>

#if HAVE_CRYPTO_KCONFIG
#include <crypto_kconfig.h>
#endif /* HAVE_CRYPTO_KCONFIG */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* RSA-PSS signature (from RFC-8017) =>
 *
 *                                   +-----------+
 *                                   |     M     |
 *                                   +-----------+
 *                                         |
 *                                         V
 *                                       Hash
 *                                         |
 *                                         V
 *                           +--------+----------+----------+
 *                      M' = |Padding1|  mHash   |   salt   |
 *                           +--------+----------+----------+
 *                                          |
 *                +--------+--+-------+     V
 *          DB =  |Padding2|01| salt  |   Hash
 *                +--------+--+-------+     |
 *                          |               |
 *                          V               |
 *                         xor <--- MGF <---|
 *                          |               |
 *                          |               |
 *                          V               V
 *                +-------------------+----------+--+
 *          EM =  |    maskedDB       |     H    |bc|
 *                +-------------------+----------+--+
 */

#define RSA_PSS_M_PRIME_PADDING_LENGTH 8U

static status_t se_mgf_mask_generation_check_args(
	const uint8_t *in_seed, const uint32_t in_seed_len,
	const uint8_t *out_mask_buffer, const uint32_t hlen,
	const uint8_t *cbuffer, const uint32_t cbuf_len,
	const uint8_t *mask_to_xor)
{
	status_t ret = NO_ERROR;
	uint32_t in_seed_len_expand = 0U;

	LTRACEF("entry\n");

	if ((NULL == out_mask_buffer) || (NULL == in_seed) ||
	    (NULL == mask_to_xor)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_SAFE_ADD_U32(in_seed_len_expand, in_seed_len, 4U);

	if ((NULL == cbuffer) || (cbuf_len < in_seed_len_expand)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((0U == hlen) || (hlen > MAX_DIGEST_LEN)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mgf_mask_one_round(struct se_data_params *input,
				   struct se_engine_sha_context *sha_ectx,
				   uint8_t *dst,
				   uint32_t out_mask_len,
				   uint32_t hlen,
				   uint32_t *copied_p)
{
	status_t ret = NO_ERROR;
	uint32_t bytes = hlen;
	uint32_t processed_len = 0U;
	uint32_t copied = *copied_p;

	LTRACEF("entry\n");

	ret = CCC_ENGINE_SWITCH_SHA(sha_ectx->engine, input, sha_ectx);
	CCC_ERROR_CHECK(ret);

	CCC_SAFE_ADD_U32(processed_len, hlen, copied);

	if (processed_len > out_mask_len) {
		bytes = (out_mask_len - copied);
	}

	se_util_mem_move(dst, &sha_ectx->tmphash[0], bytes);
	CCC_SAFE_ADD_U32(copied, copied, bytes);

	*copied_p = copied;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_mgf_mask_generation_do_hash(
	struct se_engine_sha_context *sha_ectx, uint8_t *out_mask_buffer,
	uint32_t out_mask_len, uint32_t in_seed_len, uint8_t *cbuffer,
	uint32_t hlen)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t counter = 0U;
	uint32_t copied = 0U;
	uint32_t loop_limit = 0U;
	uint32_t in_seed_len_expand = 0U;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_SAFE_ICEIL_U32(loop_limit, out_mask_len, hlen);
	if (loop_limit > 256U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	CCC_SAFE_ADD_U32(in_seed_len_expand, in_seed_len, 4U);

	CCC_LOOP_BEGIN {
		uint32_t counter_inx = in_seed_len + 3U;

		if (counter >= loop_limit) {
			break;
		}

		/* Construct SHA digest input params */
		input.src = cbuffer;
		input.input_size = in_seed_len_expand;

		/* Use sha_ectx buf for SHA result (no need to be aligned).
		 * Then copy only out_mask_len bytes to out_mask_buffer to avoid
		 * overshoots on last copy.
		 */
		input.dst = sha_ectx->tmphash;
		input.output_size = sizeof_u32(sha_ectx->tmphash);

		sha_ectx->is_last = true;
		sha_ectx->is_first = true;

		cbuffer[counter_inx] = (uint8_t)counter;

		ret = mgf_mask_one_round(&input,
					 sha_ectx,
					 &out_mask_buffer[counter * hlen],
					 out_mask_len,
					 hlen,
					 &copied);
		CCC_ERROR_END_LOOP(ret);

		counter++;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(u32, counter);

	if (counter != loop_limit) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RSA mask generation function using SHA
 */
status_t se_mgf_mask_generation(
	struct context_mem_s *cmem,
	const uint8_t *in_seed, uint32_t in_seed_len,
	uint8_t *out_mask_buffer, uint32_t out_mask_len,
	te_crypto_algo_t hash_algorithm, uint32_t hlen,
	uint8_t *cbuffer, uint32_t cbuf_len,
	const uint8_t *mask_to_xor)
{
	status_t ret = NO_ERROR;
	bool locked = false;
	struct se_engine_sha_context *sha_ectx = NULL;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	ret = se_mgf_mask_generation_check_args(in_seed, in_seed_len,
						out_mask_buffer, hlen, cbuffer,
						cbuf_len, mask_to_xor);
	CCC_ERROR_CHECK(ret);

	/* in_seed_len checked in se_mgf_mask_generation_check_args */
	DEBUG_ASSERT_PHYS_DMA_OK(cbuffer, (in_seed_len + 4U));

	/* no DMA access needed for engine contexts */
	sha_ectx = CMTAG_MEM_GET_OBJECT(cmem,
					CMTAG_ECONTEXT,
					0U,
					struct se_engine_sha_context,
					struct se_engine_sha_context *);
	if (NULL == sha_ectx) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = sha_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY, hash_algorithm,
						sha_ectx, cmem);
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(sha_ectx->engine);
	locked = true;

	se_util_mem_move(cbuffer, in_seed, in_seed_len);

	/* do Cert-C boundary check */
	if (in_seed_len > MAX_RSA_BYTE_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	cbuffer[in_seed_len + 0U] = 0U;
	cbuffer[in_seed_len + 1U] = 0U;
	cbuffer[in_seed_len + 2U] = 0U;
	cbuffer[in_seed_len + 3U] = 0U;

	ret = se_mgf_mask_generation_do_hash(sha_ectx, out_mask_buffer,
					     out_mask_len, in_seed_len, cbuffer,
					     hlen);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("DB_MASK buffer", out_mask_buffer, out_mask_len);

	/* Step 8. Let DB = dbMask XOR mask_to_xor
	 */
	for (inx = 0U; inx < out_mask_len; inx++) {
		out_mask_buffer[inx] = BYTE_VALUE((uint32_t)out_mask_buffer[inx] ^ mask_to_xor[inx]);
	}

	FI_BARRIER(u32, inx);

	if (inx != out_mask_len) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	if (BOOL_IS_TRUE(locked)) {
		HW_MUTEX_RELEASE_ENGINE(sha_ectx->engine);
	}

	if (NULL != sha_ectx) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ECONTEXT,
				  sha_ectx);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RSA signature helpers */
static status_t rsa_pss_verify_em_len(const struct se_rsa_context *rsa_context,
				      uint32_t em_len,
				      uint32_t hlen,
				      bool dynamic_salt)
{
	status_t ret = NO_ERROR;
	uint32_t min_em_len = 0U;

	LTRACEF("entry\n");

	if (hlen > MAX_DIGEST_LEN) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	if (BOOL_IS_TRUE(dynamic_salt)) {
		/* must hold hash + 0xBC + 0x01 meta bytes */
		min_em_len = 2U + hlen;
		if (em_len < min_em_len) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
		}
	} else {
		if (rsa_context->sig_slen > MAX_RSA_BYTE_SIZE) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
		min_em_len = hlen + rsa_context->sig_slen + 2U;
		if (em_len < min_em_len) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_verify_slen(const struct se_rsa_context *rsa_context,
				    bool dynamic_salt,
				    uint32_t masked_dblen)
{
	status_t ret = NO_ERROR;

	if (!BOOL_IS_TRUE(dynamic_salt)) {
		if (rsa_context->sig_slen >= masked_dblen) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
		}
	}
fail:
	return ret;
}

static status_t rsa_pss_verify_check_db_up_to_salt(const uint8_t *db,
						   uint32_t masked_dblen,
						   uint32_t *db_salt_offset_p)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t max_index = 0U;

	/* skip optional leading zero bytes 0..(masked_dblen - 1) */
	CCC_SAFE_SUB_U32(max_index, masked_dblen, 1U);
	for (inx = 0U; (db[inx] == 0U) && (inx < max_index); inx++) {
		/* skip zero bytes */
	}

	/* Byte following PS must be 0x01 */
	if (0x01U != db[inx]) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	/* salt follows the 0x01 byte */
	*db_salt_offset_p = inx+1U;

fail:
	return ret;
}

static status_t rsa_pss_setup_m_prime(const struct se_data_params *input_params,
				      uint8_t *m_prime, uint32_t hlen,
				      status_t *ret2_p)
{
	status_t ret  = NO_ERROR;
	status_t ret2 = ERR_BAD_STATE;
	uint32_t inx = 0U;

	*ret2_p = ret2;

	for (inx = 0U; inx < RSA_PSS_M_PRIME_PADDING_LENGTH; inx++) {
		m_prime[inx] = 0U;
	}

	FI_BARRIER(u32, inx);

	if (RSA_PSS_M_PRIME_PADDING_LENGTH != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ret2 = NO_ERROR;
	FI_BARRIER(status, ret2);

	for (inx = 0U; inx < hlen; inx++) {
		m_prime[RSA_PSS_M_PRIME_PADDING_LENGTH + inx] = input_params->src_digest[inx];
	}

	FI_BARRIER(u32, inx);

	if (hlen != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	*ret2_p = ret2;

fail:
	return ret;
}

/**
 * @def M_PRIME_SIZE_EXTENSION
 *
 * @brief Max salt length may increase M_PRIME (M') length up to
 * six bytes longer than RSA_SIZE by the specification.
 *
 * Value 6 is not directly stated in the specs, but it can be
 * derived (ref: ascii picture above in this file):
 *
 * length(maskedDB) == (RSA_SIZE - length(H) - 1)
 *
 * DB_length == length(maskedDB) == (RSA_SIZE - length(H) - 1)
 *  when length(Padding2) == 0 salt is longest.
 *
 * SALT_max_length == (DB_length - 1) == (RSA_SIZE - length(H) - 2)
 *
 * and length(padding1) is 8, so =>
 *
 * Max length(M') = 8 + length(H) + SALT_max_length
 *
 * thus =>
 *
 * Max length(M') = (8 + length(H)) + (RSA_SIZE - length(H) - 2) => (RSA_SIZE + 6)
 *
 * Max length(M') == (RSA_SIZE + 6)
 */
#define M_PRIME_SIZE_EXTENSION 6U

/**
 * @def M_PRIME_MAX_LENGTH
 *
 * @brief Max length of M' with the longest possible salt length
 * is RSA_SIZE + M_PRIMT_SIZE_EXTENSION
 */
#define M_PRIME_MAX_LENGTH(rsa_size) ((rsa_size) + M_PRIME_SIZE_EXTENSION)

/**
 * @def RSA_EM_BUFFER_POW2_ALIGN
 *
 * @brief This no longer needs huge alignment since ALIGNED_BUFFERS are now
 * from contiguous memory pool or CMEM. So, just use CCC_DMA_MEM_ALIGN (CACHE_LINE)
 * alignment for the RSA verify buffer.
 *
 * This was earlier:
 * 1 KB align => 2*4096 RSA key byte size for M' buffer, length
 * of which may overshoot RSA_SIZE by 6 bytes (M_PRIME_SIZE_EXTENSION).
 *
 */
#define RSA_EM_BUFFER_POW2_ALIGN CCC_DMA_MEM_ALIGN

static status_t rsa_pss_verify_em(const struct se_rsa_context *rsa_context,
				  const uint8_t *em, uint32_t *em_len_p,
				  uint32_t hlen, bool dynamic_salt)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_bitsize = 0U;
	uint32_t em_len = 0U;
	uint32_t rightmost_byte = 0U;

	LTRACEF("entry\n");

	if (NULL == em_len_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((rsa_context->ec.rsa_size > MAX_RSA_BYTE_SIZE) ||
	    (rsa_context->ec.rsa_size == 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	rsa_bitsize = rsa_context->ec.rsa_size * 8U;
	CCC_SAFE_ICEIL_U32(em_len, (rsa_bitsize - 1U), 8U);

	ret = rsa_pss_verify_em_len(rsa_context, em_len, hlen, dynamic_salt);
	CCC_ERROR_CHECK(ret);

	/* EM = maskedDB || H || 0xbc */
	CCC_SAFE_SUB_U32(rightmost_byte, em_len, 1U);
	if (0xblw != em[rightmost_byte]) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	*em_len_p = em_len;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_set_masked_dblen(uint32_t *masked_dblen_p,
					   uint32_t em_len,
					   uint32_t hlen)
{
	status_t ret = NO_ERROR;
	uint32_t masked_dblen = 0U;

	LTRACEF("entry\n");

	if (NULL == masked_dblen_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_SAFE_SUB_U32(masked_dblen, em_len, hlen);
	CCC_SAFE_SUB_U32(masked_dblen, masked_dblen, 1U);

	*masked_dblen_p = masked_dblen;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_verify_masked_db(uint32_t *lb_mask_p,
					 uint32_t rsa_byte_size,
					 uint8_t leftmost_byte,
					 uint32_t em_len)
{
	status_t ret = NO_ERROR;
	uint32_t tmp = 0U;
	uint32_t lowest_bits = 0U;
	uint32_t lb_mask = 0U;
	uint32_t first_octet_masked_db = 0U;
	uint32_t rsa_bitsize = rsa_byte_size << 3U;
	uint32_t em_bitlen = em_len << 3U;

	LTRACEF("entry\n");

	if (NULL == lb_mask_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Step 6. If the leftmost 8em_len - emBits bits of the leftmost
	 * octet in masked_db are not all equal to zero, output "inconsistent"
	 * and stop.
	 *
	 * 8em_len - emBits = 8*256 - (2048 - 1) = 1 (assuming 2048 key size and sha256)
	 */

	// XXX TODO: Hmm... lowest_bits is always value 1 here?
	//
	CCC_SAFE_SUB_U32(rsa_bitsize, rsa_bitsize, 1U);
	CCC_SAFE_SUB_U32(lowest_bits, em_bitlen, rsa_bitsize);

	if (lowest_bits >= 8U) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	/* Due to coverity need to cast the constant.
	 */
	lb_mask = ((uint32_t)0xFFU << (8U - lowest_bits)) & 0xFFU;

	/* XX Because of the above, this checks that the upper bit of the leftmost byte
	 * XX  is zero (i.e. that the modulus size value is positive)
	 */
	tmp = leftmost_byte;
	first_octet_masked_db = (tmp & lb_mask);
	if (first_octet_masked_db != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	*lb_mask_p = lb_mask;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_process_m_prime(
				const struct se_data_params *input_params,
				uint8_t *m_prime, uint32_t *m_prime_blen_p,
				const uint8_t *db, uint32_t m_prime_max_len,
				uint32_t db_salt_offset, uint32_t em_len,
				uint32_t masked_dblen)
{
	status_t ret = NO_ERROR;
	status_t rbad = NO_ERROR;
	uint32_t m_prime_blen = 0U;
	uint32_t final_m_prime_len = 0U;
	uint32_t hlen = input_params->src_digest_size;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	(void)em_len;

	/* Step 12. Let M' = 0x 00 00 00 00 00 00 00 00 || mHash || salt;
	 * Set eight initial octets to 0.
	 */
	ret = rsa_pss_setup_m_prime(input_params, m_prime, hlen, &rbad);
	CCC_ERROR_CHECK(ret);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	/* TBUF(1) is part (m_prime)
	 * DB(2) is full (db)
	 * CONTEXT->BUF is full (H)
	 */

	rbad = ERR_BAD_STATE;

	/* If salt present, final M' m_prime_blen is salt len longer
	 */
	if (hlen > MAX_DIGEST_LEN) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	m_prime_blen = RSA_PSS_M_PRIME_PADDING_LENGTH + hlen;

	LTRACEF("masked_dblen=%u, db_salt_offset=%u\n", masked_dblen, db_salt_offset);

	if (masked_dblen > db_salt_offset) {
		uint32_t slen = masked_dblen - db_salt_offset;

#if MAX_RSA_BYTE_SIZE < (RSA_PSS_M_PRIME_PADDING_LENGTH + MAX_DIGEST_LEN)
	/* This used to be a runtime test but colwerted to compile
	 * time error to avoid unreachable code in all sensible
	 * configurations.
	 */
#error "MAX_RSA_BYTE_SIZE must be longer than (MAX_DIGEST_LEN+8) bytes"
#endif

		final_m_prime_len = m_prime_blen + slen;
		if (final_m_prime_len > m_prime_max_len) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
					     LTRACEF("RSA-PSS salt too long: hlen:%u slen:%u em_len:%u\n",
						     hlen, slen, em_len));
		}

		/* TBUF(1) is part (m_prime)
		 * DB(2) is full (db)
		 * CONTEXT->BUF is full (H)
		 */

		/* m_prime buffer max size RSA_SIZE + M_PRIME_SIZE_EXTENSION, i.e.
		 * value of macro M_PRIME_MAX_LENGTH(RSA_SIZE), so this fits.
		 * [ 8 + HLEN + SLEN <= M_PRIME_MAX_LENGTH(RSA_SIZE) ]
		 *
		 * Append salt to current M' value
		 */
		for (inx = 0U; inx < slen; inx++) {
			m_prime[m_prime_blen + inx] = db[db_salt_offset + inx];
		}

		FI_BARRIER(u32, inx);

		CCC_SAFE_ADD_U32(m_prime_blen, m_prime_blen, inx);

		if (m_prime_blen != final_m_prime_len) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
					     LTRACEF("FI in salt copy\n"));
		}
	}

	*m_prime_blen_p = m_prime_blen;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_m_prime_generation(
				const struct se_data_params *input_params,
				uint8_t *m_prime, uint32_t *m_prime_blen_p,
				const uint8_t *db, uint32_t m_prime_max_len,
				uint32_t em_len, uint32_t masked_dblen)
{
	status_t ret = NO_ERROR;
	uint32_t db_salt_offset = 0U;

	LTRACEF("entry\n");


	/* Step 10. Verify that the (arbitrary 0..N) PS bytes are zero in DB.
	 *
	 * DB == PS || 0x01 || salt
	 *
	 * length(DB) == masked_dblen
	 * length(PS) == 0..masked_dblen-1 [bounds inclusive]
	 *
	 * Skip optional leading zero bytes and verify that the
	 * byte after PS has value 0x01.
	 */

	ret = rsa_pss_verify_check_db_up_to_salt(db, masked_dblen, &db_salt_offset);
	CCC_ERROR_CHECK(ret);

	/* Step 11. Let salt be the last slen octets of DB.
	 *
	 * Step 12. Let M' = 0x 00 00 00 00 00 00 00 00 || mHash || salt;
	 * Set eight initial octets to 0.
	 */

	ret = rsa_pss_process_m_prime(input_params, m_prime, m_prime_blen_p,
				      db, m_prime_max_len, db_salt_offset,
				      em_len, masked_dblen);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_verify_safe_setup(const struct se_rsa_context *rsa_context,
					  uint8_t **mem_p,
					  uint32_t *m_prime_max_len_p)
{
	status_t ret = NO_ERROR;
	uint8_t *mem = NULL;

	LTRACEF("entry\n");

	if (rsa_context->ec.rsa_size > MAX_RSA_BYTE_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	mem = CMTAG_MEM_GET_BUFFER(rsa_context->ec.cmem,
				   CMTAG_ALIGNED_BUFFER,
				   RSA_EM_BUFFER_POW2_ALIGN,
				   (2U * rsa_context->ec.rsa_size));
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	*mem_p = mem;
	*m_prime_max_len_p = M_PRIME_MAX_LENGTH(rsa_context->ec.rsa_size);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_verify_digest(const struct se_rsa_context *rsa_context,
				  uint8_t *m_prime, const uint8_t *H_val,
				  uint32_t hlen, uint32_t m_prime_blen,
				  uint32_t rsa_size,
				  te_crypto_algo_t md_algorithm,
				  status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	uint8_t *digest = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	/* Step 13. Let H' = Hash(M') */

	/* Reuse the m_prime for digest value */
	digest = m_prime;

	input.src	  = m_prime;
	input.input_size  = m_prime_blen;
	input.dst	  = digest;
	input.output_size = rsa_size;

	/* TBUF(1) is full (m_prime -> digest)
	 * DB(2) is NC
	 * CONTEXT->BUF is full (H)
	 */

	/* This holds because m_prime is in tbuf (contiguous) */
	DEBUG_ASSERT_PHYS_DMA_OK(input.src, input.input_size);

	/* Callwlate digest of m_prime (overwrites m_prime)
	 */
	ret = sha_digest(rsa_context->ec.cmem, TE_CRYPTO_DOMAIN_KERNEL,
			 &input, md_algorithm);
	CCC_ERROR_CHECK(ret);

	CCC_RANDOMIZE_EXELWTION_TIME;

	/* Step 14. If H = H' output "consistent".
	 * Otherwise, output "inconsistent".
	 *
	 * H is in "em", digest is in tbuf
	 */

	/* TBUF(1) is full (digest) [digest(M')]
	 * DB(2) is NC
	 * CONTEXT->BUF is full (H) [H in EM]
	 */

	if (!BOOL_IS_TRUE(se_util_vec_match(H_val, digest, hlen, CFALSE, rbad_p))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	if (NO_ERROR != *rbad_p) {
		CCC_ERROR_WITH_ECODE(*rbad_p);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_check_state(status_t rbad, const uint8_t *em,
				const uint8_t *tbuf, const uint8_t *db,
				const uint8_t *masked_db)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad,
				     LTRACEF("Inconsistent error state\n"));
	}

	if ((NULL != db) ||
	    (NULL != masked_db) ||
	    (NULL != em) ||
	    (NULL != tbuf)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Inconsistent exelwtion path\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_verify_finalize(const struct se_rsa_context *rsa_context,
					uint8_t *mem_arg, const uint8_t *em,
					const uint8_t *tbuf, const uint8_t *db,
					const uint8_t *masked_db, status_t rbad,
					status_t prev_ret)
{
	status_t ret = prev_ret;
	uint8_t *mem = mem_arg;

	LTRACEF("entry\n");

	if (NULL != mem) {
		/* no secrets in mem, just release */
		CMTAG_MEM_RELEASE(rsa_context->ec.cmem,
				  CMTAG_ALIGNED_BUFFER,
				  mem);
	}

	if (NO_ERROR == ret) {
		ret = rsa_check_state(rbad, em, tbuf, db, masked_db);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* validate that the PSS encoding is correct and the
 * signature matches the message digest
 *
 * Deciphered signature (message representative) m is in
 * rsa_context->buf in LITTLE ENDIAN
 */
static status_t rsa_pss_verify(
	const struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	uint8_t *mem = NULL;
	uint8_t *em = NULL;
	const uint8_t *H_val = NULL;
	uint8_t *m_prime = NULL;
	const uint8_t *masked_db = NULL;
	uint32_t masked_dblen = 0U;
	uint32_t em_len = 0U;
	uint32_t lb_mask = 0U;

	/* This was verified by the caller... */
	uint32_t hlen = input_params->src_digest_size;

	uint8_t *tbuf = NULL;
	uint8_t *db = NULL;

	uint32_t m_prime_blen = 0U;
	uint32_t m_prime_max_len = 0U;
	uint32_t tmp = 0U;
	bool     dynamic_salt =
		((rsa_context->ec.rsa_flags & RSA_FLAGS_DYNAMIC_SALT) != 0U);

	LTRACEF("entry\n");

	/**
	 *  1. Length checking: If the length of the signature S is not k octets,
	 *  where k is the length in octets of the RSA modulus n, output "invalid
	 *  signature".
	 *
	 *  The length of the signature S is assumed to be of the correct length.
	 *  The onus is on the calling function.
	 *
	 * Done (by the caller).
	 */

	/**
	 *  2. a) Colwert the signature S to an integer signature representative:
	 *		s = OS2IP (S).
	 *
	 *  This is not necessary since the integer signature representative
	 *  is already in an octet byte stream.
	 *
	 * Done (by the caller).
	 */

	/**
	 *  2. b) Apply the RSAVP1 verification primitive to the
	 *		RSA public key (n, e)
	 *		and the signature representative s to produce an integer message
	 *		representative m:
	 *			  m = RSAVP1 ((n, e), s)
	 *			  m = s^e mod n
	 *
	 *		If RSAVO1 output "signature representative out of range", output
	 *		"invalid signature" and stop.
	 *
	 * Note: s needs to be in range [0..n-1] inclusive.
	 *
	 * Done (by the caller).
	 */

	/* At this point the pubkey deciphered RSA value (message representative)
	 * is in rsa_context->buf as little endian after the RSA engine operation.
	 *
	 * Turn it to big endian encoded message (em), as the rest of the
	 * code expects BE data.
	 *
	 * First, get space for the contiguous work buffer (tbuf) and message
	 * representative (em).
	 *
	 * This must be contiguous for rsa_context->ec.rsa_size+6 bytes since
	 * it is passed to SHA DMA.
	 *
	 * The memory pools normally work in power of twos, so align
	 * with RSA_EM_BUFFER_POW2_ALIGN
	 */
	ret = rsa_pss_verify_safe_setup(rsa_context, &mem, &m_prime_max_len);
	CCC_ERROR_CHECK(ret);

	/* TBUF is large enough to hold M' value when salt extends the
	 * length past RSA_SIZE. It needs to be contiguous since M'
	 * is read with the SHA DMA engine.
	 */
	tbuf = &mem[0U];

	/* DB does not need to be RSA_SIZE long. Use a smaller
	 * buffer for it to extend TBUF.
	 *
	 * DB is (RSA_SIZE - HLEN - 1) bytes long => this is ok.
	 * (All hashes are longer than 5 bytes)
	 */
	db = &mem[m_prime_max_len];

	/* TBUF(1) is empty
	 * DB(2) is empty (db)
	 * CONTEXT->BUF is full (reversed em, result of RSA pubkey operation)
	 */

	/**
	 * 2. c) Colwert the message representative m to an encoded message em of
	 *	   length em_len = Ceiling( (modBits - 1U) / 8U) octets, where modBits
	 *	   is the length in bits of the RSA modulus n:
	 *		  em = I2OSP(m, em_len)
	 *
	 * [In big endian order:]
	 *
	 * EM = maskedDB || H || 0xbc
	 */
	ret = se_util_reverse_list(rsa_context->buf, rsa_context->buf, rsa_context->ec.rsa_size);
	CCC_ERROR_CHECK(ret);

	/* EM is now in big endian */
	em = &rsa_context->buf[0];

	/* TBUF(1) is empty
	 * DB(2) is empty (db)
	 * CONTEXT->BUF is full (em)
	 */

	/**
	 * 3. emSA-PSS verification: Apply the emSA-PSS verification operation
	 *	to the message M and the encoded message em to determine whether
	 *	they are consistent:
	 *	  Result = emSA-PSS-VERIFY(M, em, modBits -1U)
	 *
	 *	if [Result is "consistent"] then
	 *		  output "Valid Signature"
	 *	else
	 *		  output "Invalid Signature"
	 */

	/* Step 1. If the length of M is greater than the input limitation of
	 * the hash function, output "inconsistent" and stop.
	 *
	 * Step 2. Let mHash = Hash(M), an octet string of length hlen.
	 *
	 * Step 3. If em_len < hlen + slen + 2U, output "inconsistent" and stop.
	 *
	 * em_len < hlen + slen + 2U
	 *
	 * Ceiling(emBits/8U) < hlen + slen + 2U
	 * Ceiling(modBits-1U/8U) < hlen + slen + 2U
	 * 256 octets < 32 octets + 32 octets + 2U
	 * (assuming SHA256 as the hash function)
	 *
	 * Step 4. If the rightmost octet of em does not have hexadecimal
	 * value 0xbc, output "inconsistent" and stop.
	 *
	 * EM = maskedDB || H || 0xbc
	 */

	ret = rsa_pss_verify_em(rsa_context, em, &em_len, hlen, dynamic_salt);
	CCC_ERROR_CHECK(ret);

	/* Step 5. Let masked_db be the leftmost em_len - hlen - 1 octets
	 * of em, and let H be the next hlen octets.
	 *
	 * EM = maskedDB || H || 0xbc
	 */
	ret = rsa_pss_set_masked_dblen(&masked_dblen, em_len, hlen);
	CCC_ERROR_CHECK(ret);
	masked_db = em;
	H_val = &em[masked_dblen];

	/* TBUF(1) is empty
	 * DB(2) is empty (db)
	 * CONTEXT->BUF is full (em, masked_db, H)
	 */

	/* Step 6. If the leftmost 8em_len - emBits bits of the leftmost
	 * octet in masked_db are not all equal to zero, output "inconsistent"
	 * and stop.
	 *
	 * 8em_len - emBits = 8*256 - (2048 - 1) = 1 (assuming 2048 key size and sha256)
	 */

	ret = rsa_pss_verify_masked_db(&lb_mask, rsa_context->ec.rsa_size,
				       masked_db[0], em_len);
	CCC_ERROR_CHECK(ret);

	/* Step 7. Let dbMask = MGF(H, em_len - hlen - 1).
	 *
	 * Uses tbuf as counter source for SHA engine.
	 * Constructs first the dbMask and then DB to buffer db.
	 *
	 * Step 8. Let DB = masked_db XOR dbMask, now done in the mask generation
	 * function above (due to pmccabe).
	 *
	 * The above XOR is done in the call, result written to DB.
	 *
	 * masked_db value (residing in EM) is not used after this call.
	 */

	/* TBUF(1) is empty/used, [SHA DMA (cbuffer/counter)];
	 * DB(2) is empty (db)
	 * CONTEXT->BUF is full (em, masked_db, H)
	 */

	ret = se_mgf_mask_generation(rsa_context->ec.cmem,
				     H_val, hlen, db, masked_dblen,
				     rsa_context->md_algorithm, hlen,
				     tbuf, rsa_context->ec.rsa_size,
				     masked_db);
	if (NO_ERROR != ret) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	/* TBUF(1) is empty
	 * DB(2) is full (db)
	 * CONTEXT->BUF is full (em, masked_db, H)
	 */

	masked_db = NULL;
	em = NULL;

	/* TBUF(1) is empty
	 * DB(2) is full (db)
	 * CONTEXT->BUF is full (H)
	 */

	LOG_HEXBUF("DB (XORed)", db, masked_dblen);

	ret = rsa_pss_verify_slen(rsa_context, dynamic_salt, masked_dblen);
	CCC_ERROR_CHECK(ret);

	/* Step 9. Set the leftmost 8em_len - emBits bits of the leftmost
	 * octet in DB to zero.
	 */
	tmp = db[0];
	db[0] = BYTE_VALUE(tmp & ~lb_mask);

	/* Step 10. Verify that the (arbitrary 0..N) PS bytes are zero in DB.
	 *
	 *
	 * Step 11. Let salt be the last slen octets of DB.
	 *
	 * Step 12. Let M' = 0x 00 00 00 00 00 00 00 00 || mHash || salt;
	 * Set eight initial octets to 0.
	 */

	/* The caller RSA buffer is empty/unused => use it for m_prime
	 * value construction with the SE SHA engine (DMA src access).
	 *
	 * Re-uses tbuf for m_prime
	 * (contiguous, size is M_PRIME_MAX_LENGTH(rsa_size))
	 */
	m_prime = &tbuf[0];

	/* TBUF(1) is empty (m_prime)
	 * DB(2) is full (db)
	 * CONTEXT->BUF is full (H)
	 */

	ret = rsa_pss_m_prime_generation(input_params, m_prime, &m_prime_blen,
					 db, m_prime_max_len, em_len,
					 masked_dblen);
	CCC_ERROR_CHECK(ret);

	db = NULL;

	/* TBUF(1) is full (m_prime)
	 * DB(2) is NC
	 * CONTEXT->BUF is full (H)
	 */

	LTRACEF("number of bytes in M'=%u\n", m_prime_blen);

	/* Step 13. Let H' = Hash(M')
	 * Step 14. If H = H' output "consistent".
	 * Otherwise, output "inconsistent".
	 *
	 * H is in "em", digest is in tbuf
	 */
	ret = rsa_verify_digest(rsa_context,
				m_prime, H_val, hlen, m_prime_blen,
				rsa_context->ec.rsa_size, rsa_context->md_algorithm,
				&rbad);
	CCC_ERROR_CHECK(ret); /* rbad is same with ret now */

	tbuf = NULL;

	/* TBUF(1) is NC
	 * DB(2) is NC
	 * CONTEXT->BUF is NC
	 */

	FI_BARRIER(u8_ptr, db);
	FI_BARRIER_CONST(u8_ptr, masked_db);
	FI_BARRIER(u8_ptr, em);
	FI_BARRIER(u8_ptr, tbuf);

fail:
	ret = rsa_pss_verify_finalize(rsa_context,
				      mem, em, tbuf, db, masked_db, rbad, ret);
	/* FALLTHROUGH */
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Verify rsa data < modulus.
 * Data length must be RSA_SIZE here: caller verified.
 *
 * This can only be checked in case the modulus exists,
 * this is false only when a pre-loaded key is used.
 *
 * Even so the modulus may still exist: if client has provided it and
 * set a proper flag => in that case RSAVP1 s < (n) comparison
 * is performed even in that case.
 */
status_t rsa_verify_data_range(const struct se_engine_rsa_context *econtext,
			       const uint8_t *data,
			       bool big_endian)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (((econtext->rsa_flags & RSA_FLAGS_USE_PRESET_KEY) == 0U) ||
	    ((econtext->rsa_flags & RSA_FLAGS_KMAT_VALID) != 0U)) {
		status_t rbad = ERR_BAD_STATE;
		uint32_t eflags = 0U;

		if ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) == 0U) {
			eflags |= CMP_FLAGS_VEC1_LE;
		}

		if (!BOOL_IS_TRUE(big_endian)) {
			eflags |= CMP_FLAGS_VEC2_LE;
		}

		/* True if v1 > v2 */
		if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(econtext->rsa_modulus,
							 data,
							 econtext->rsa_size,
							 eflags, &rbad))) {
			CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
		}

		if (NO_ERROR != rbad) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN

#define MAX_DIGEST_INFO_PREFIX_LEN 20U

/* SHA-1 digest length is 20, all other SHA digests are longer
 *
 * XXX MD5 not supported yet...
 */
#define MIN_RSA_SIG_DIGEST_LEN 20U

static status_t rsa_sig_pkcs1_v1_5_encode_em(
		const struct se_data_params *input_params,
		const struct se_rsa_context *rsa_context,
		const uint8_t *di_prefix,
		const uint32_t di_len,
		uint8_t *em,
		bool is_verify)
{
	status_t ret = NO_ERROR;
	uint32_t em_len = rsa_context->ec.rsa_size;
	uint32_t hlen = input_params->src_digest_size;
	uint32_t tlen = 0;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if ((hlen < MIN_RSA_SIG_DIGEST_LEN) || (hlen > MAX_DIGEST_LEN) ||
	    (di_len > MAX_DIGEST_INFO_PREFIX_LEN)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* digestInfo len + hash len */
	tlen = di_len + hlen;

	if (em_len < tlen + 11U) {
		if (BOOL_IS_TRUE(is_verify)) {
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
					     LTRACEF("Intended encoded message length too short\n"));
		}
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("Intended encoded message length too short\n"));
	}

	/* Encode according to RFC-8017 EMSA-PKCS1-v1_5 */
	em[0U] = 0x00U;
	em[1U] = 0x01U;

	/* Add PS (0xFF bytes), at least 8 bytes (starting offset +2 => end offset +2) */
	for (inx = 2U; inx < (em_len - tlen - 3U + 2U); inx++) {
		em[inx] = 0xFFU;
	}

	FI_BARRIER(u32, inx);

	if (inx != (em_len - tlen - 3U + 2U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	em[inx] = 0x00U;
	inx++;

	/* di_len is ensured NOT greater than size of di_prefix */
	se_util_mem_move(&em[inx], di_prefix, di_len);
	CCC_SAFE_ADD_U32(inx, inx, di_len);

	se_util_mem_move(&em[inx], input_params->src_digest, hlen);

	LOG_HEXBUF("encoded EM value (BE)", em, rsa_context->ec.rsa_size);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t rsa_sig_pkcs1_v1_5_encode(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context,
	uint8_t *em,
	bool is_verify)
{
	status_t ret = NO_ERROR;

	/* Digest follows immediately after di_prefix field.
	 *
	 * Using DER encoded digestInfo fields as in the RFC-8017. BER would
	 * be more generic and according to the RFC in some unlikely cases
	 * oddly coded signatures using BER encoding may fail to be verified
	 * (in theory). Such is life... Adding a generic ASN.1 parser for this
	 * reason would be hilarious.
	 */
	static const struct digest_info_s {
		te_crypto_algo_t digest_algo;
		uint32_t	 di_len;
		uint8_t		 di_prefix[MAX_DIGEST_INFO_PREFIX_LEN];
	} digest_info[] = {
		{ TE_ALG_MD5, 18,
		  { 0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05, 0x05, 0x00,
		    0x04, 0x10, 0x00, 0x00, }},
		{ TE_ALG_SHA1, 15,
		  { 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14, 0x00,
		    0x00, 0x00, 0x00, 0x00, }},
		{ TE_ALG_SHA224, 19,
		  { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05,
		    0x00, 0x04, 0x1c, 0x00, }},
		{ TE_ALG_SHA256, 19,
		  { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
		    0x00, 0x04, 0x20, 0x00, }},
		{ TE_ALG_SHA384, 19,
		  { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
		    0x00, 0x04, 0x30, 0x00, }},
		{ TE_ALG_SHA512, 19,
		  { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
		    0x00, 0x04, 0x40, 0x00, }},
#if 0
		{ TE_ALG_SHA512_224, 19,
		  { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x05, 0x05,
		    0x00, 0x04, 0x1c, 0x00, }},
		{ TE_ALG_SHA512_256, 19
		  { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x06, 0x05,
		    0x00, 0x04, 0x20, 0x00, }},
#endif
		{ TE_ALG_NONE, 0, { 0x00 }},
	};

	const struct digest_info_s *di = &digest_info[0U];

	LTRACEF("entry\n");

	while (di->digest_algo != TE_ALG_NONE) {
		if (di->digest_algo == rsa_context->md_algorithm) {
			break;
		}
		di++;
	}

	if (di->digest_algo == TE_ALG_NONE) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("RSA verify digest algorithm unsupported\n"));
	}

	ret = rsa_sig_pkcs1_v1_5_encode_em(input_params, rsa_context,
					   &di->di_prefix[0],
					   di->di_len, em,
					   is_verify);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* validate that the PKCS#1v1.5 encoding is correct and the
 * signature matches the message digest (as in RFC-8017).
 *
 * Deciphered signature (message representative) m is in
 * rsa_context->buf in LITTLE ENDIAN
 */
static status_t rsa_pkcs1_v1_5_verify(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint8_t *em = NULL;

	LTRACEF("entry\n");

	em = CMTAG_MEM_GET_BUFFER(rsa_context->ec.cmem,
				  CMTAG_BUFFER,
				  0U,
				  rsa_context->ec.rsa_size);
	if (NULL == em) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	LOG_HEXBUF("RSA result (LE)", rsa_context->buf, rsa_context->ec.rsa_size);

	ret = rsa_sig_pkcs1_v1_5_encode(input_params, rsa_context, em, true);
	CCC_ERROR_CHECK(ret);

	CCC_RANDOMIZE_EXELWTION_TIME;

	/* EM big endian encoding complete.
	 * rsa_context->buf is in little endian, so use endian swapping matching.
	 */
	if (!BOOL_IS_TRUE(se_util_vec_match(em, rsa_context->buf, rsa_context->ec.rsa_size,
					    CTRUE, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	FI_BARRIER(status, rbad);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	FI_BARRIER(status, rbad);
fail:
	if (NULL != em) {
		CMTAG_MEM_RELEASE(rsa_context->ec.cmem,
				  CMTAG_BUFFER,
				  em);
	}

	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN */

static status_t se_rsa_process_verify_check_input_params(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == input_params->src) || (0U == input_params->input_size) ||
	    (NULL == input_params->src_signature) ||
	    (input_params->src_signature_size != rsa_context->ec.rsa_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_process_verify_check_keytype(
	const struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Allow verify with a private key only if the public exponent
	 * is specified by caller.
	 */
	switch (rsa_context->ec.rsa_keytype) {
	case KEY_TYPE_RSA_PUBLIC:
		break;

	case KEY_TYPE_RSA_PRIVATE:
		if ((0U == rsa_context->ec.rsa_pub_expsize) ||
		    (NULL == rsa_context->ec.rsa_pub_exponent)) {
			LTRACEF("RSA public exponent not specified\n");
			ret = SE_ERROR(ERR_NOT_VALID);
			break;
		}
		break;

	default:
		LTRACEF("RSA verify with wrong key type %u\n",
			(uint32_t)rsa_context->ec.rsa_keytype);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_process_verify_check_args(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context,
	const uint8_t *digest)
{
	status_t ret = NO_ERROR;
	uint32_t bit_size = 0U;

	LTRACEF("entry\n");

	if ((NULL == rsa_context) || (NULL == digest)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = se_rsa_process_verify_check_input_params(input_params, rsa_context);
	CCC_ERROR_CHECK(ret);

	if (rsa_context->ec.rsa_size > MAX_RSA_BYTE_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	bit_size = rsa_context->ec.rsa_size * 8U;
	if (!BOOL_IS_TRUE(IS_VALID_RSA_KEYSIZE_BITS(bit_size))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (rsa_context->ec.rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = se_rsa_process_verify_check_keytype(rsa_context);
	CCC_ERROR_CHECK(ret);

	if (rsa_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("RSA rsa_context buffer can not be used for RSA verify\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Verify signature representative < modulus
 *
 * Can only be verified in case the modulus exists,
 * this is false only when a pre-loaded key is used.
 *
 * Even so the modulus may still exist: if client has provided it and
 * set a proper flag => in that case RSAVP1 s < (n) comparison
 * is performed even in that case.
 */
static status_t rsa_verify_signature_in_range(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	ret = rsa_verify_data_range(
		&rsa_context->ec,
		input_params->src_signature,
		((rsa_context->ec.rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U));
	CCC_ERROR_CHECK(ret);

fail:
	if (NO_ERROR != ret) {
		/* Signature checks primarily return this error */
		ret = SE_ERROR(ERR_SIGNATURE_ILWALID);
	}

	return ret;
}

static void cleanup_rsa_verify(struct se_rsa_context *rsa_context,
			       struct se_data_params *input_params,
			       uint8_t *digest)
{
	if (NULL != digest) {
		se_util_mem_set(digest, 0U, MAX_DIGEST_LEN);
	}

	if (NULL != input_params) {
		input_params->src_digest = NULL;
	}

	if (NULL != rsa_context) {
		se_util_mem_set(rsa_context->buf, 0U, sizeof_u32(rsa_context->buf));
		rsa_context->used = 0U;
	}
}

/* Support function for rsa_process_verify():
 * First part of signature verification and parameter checks.
 * If requested also callwlates the message digest.
 *
 * Check signature value if possible. However, if the modulus only
 * exists in HW keyslot this can not be checked now. CCC allows the
 * RSA key material to be provided in both the HW keyslot (for speed)
 * and in key parameters because of this (for verifying signature value).
 * Signature numeric value must be in range 0..n-1 [inclusive].
 *
 * Place the signature to rsa_context->buf in little endian and start the
 * RSA engine operation on the aligned buffer. [ PKA0 engine uses DMA to
 * read it, hence the signature must not cross page boundaries with
 * phys addr access ]
 *
 * The rest of RSA signature validation is handled by the caller,
 * dealing with the result of modular exponentiation.
 */
static status_t rsa_process_verify_start(struct se_data_params *input_params,
					 struct se_rsa_context *rsa_context,
					 uint8_t *digest)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t hlen = 0U;

	LTRACEF("entry\n");

	ret = se_rsa_process_verify_check_args(input_params,
					       rsa_context,
					       digest);
	CCC_ERROR_CHECK(ret);

	ret = get_message_digest_size(rsa_context->md_algorithm, &hlen);
	CCC_ERROR_CHECK(ret);

	/* In case the input needs to be digested first, this bit
	 * must be set.
	 *
	 * XXX RSA sig verify only supports SHA digests lwrrently...
	 */
	if ((rsa_context->ec.rsa_flags & RSA_FLAGS_DIGEST_INPUT_FIRST) != 0U) {
		input.src	  = input_params->src;
		input.input_size  = input_params->input_size;
		input.dst	  = digest;
		input.output_size = hlen;

		ret = sha_digest(rsa_context->ec.cmem, rsa_context->ec.domain,
				 &input, rsa_context->md_algorithm);
		CCC_ERROR_CHECK(ret);

		/* Do the RSA signature validation with the digest
		 * callwlated above.
		 */
		input_params->src_digest      = digest;
		input_params->src_digest_size = hlen;
	}

	/* Digest is correct size? */
	if (input_params->src_digest_size != hlen) {
		LTRACEF("Digest length %u does not match md algorithm size %u\n",
			input_params->src_digest_size, hlen);
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	/**
	 *  1. Length checking: If the length of the signature S is not k octets,
	 *  where k is the length in octets of the RSA modulus n, output "invalid
	 *  signature".
	 *
	 *  The length of the signature S is assumed to be of the correct length.
	 *  The onus is on the calling function.
	 */

	/**
	 *  2. a) Colwert the signature S to an integer signature representative:
	 *		s = OS2IP (S).
	 *
	 *  This is not necessary since the integer signature representative
	 *  is already in an octet byte stream.
	 */

	/**
	 *  2. b) Apply the RSAVP1 verification primitive to the
	 *		RSA public key (n, e)
	 *		and the signature representative s to produce an integer message
	 *		representative m:
	 *			  m = RSAVP1 ((n, e), s)
	 *			  m = s^e mod n
	 *
	 *		If RSAVO1 output "signature representative out of range", output
	 *		"invalid signature" and stop.
	 *
	 */

	/* If possible, verify that s is in range [0..n-1] inclusive
	 *
	 * It is possible to verify this when not using a pre-loaded
	 * keyslot key OR the client has provided the key material
	 * anyway and the RSA_FLAGS_KMAT_VALID flag is set.
	 *
	 * The keyslots are unreadable => can not access the modulus from there.
	 */
	ret = rsa_verify_signature_in_range(input_params, rsa_context);
	CCC_ERROR_CHECK(ret);

	/* Callwlate m = s^e mod n
	 *
	 * Place the data (in little endian) to rsa_context->buf,
	 * decipher the RSA in place back to rsa_context->buf as well.
	 */
	rsa_context->used = rsa_context->ec.rsa_size;

	if ((rsa_context->ec.rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U) {
		ret = se_util_reverse_list(rsa_context->buf,
					   input_params->src_signature,
					   rsa_context->ec.rsa_size);
		CCC_ERROR_CHECK(ret);

		input.src = rsa_context->buf;
	} else {
#if HAVE_NC_REGIONS
		switch (rsa_context->ec.engine->e_id) {
		case CCC_ENGINE_SE0_PKA0:
		case CCC_ENGINE_SE1_PKA0:
			/* Make sure the RSA input is not crossing
			 * a page boundary with SE PKA0 engines =>
			 * copy it to rsa_context->buf
			 */
			se_util_mem_move(rsa_context->buf,
					 input_params->src_signature,
					 rsa_context->ec.rsa_size);
			input.src = rsa_context->buf;
			break;

		default:
			input.src = input_params->src_signature;
			break;
		}
#else
		input.src = input_params->src_signature;
#endif
	}

	/* No DMA writes to dst, no contiguous requirements.
	 */
	input.input_size  = rsa_context->ec.rsa_size;
	input.dst	  = rsa_context->buf;
	input.output_size = rsa_context->ec.rsa_size;

	{
		te_crypto_algo_t saved_algo = TE_ALG_NONE;
		HW_MUTEX_TAKE_ENGINE(rsa_context->ec.engine);

		/* Modify the engine rsa_context for a while to do plain RSA
		 * exponentiation with the sig validation rsa_context RSA engine,
		 * then change it back.
		 *
		 * Since the RSA engine operation does not modify the RSA
		 * engine rsa_context => no need to save anything else and this is
		 * considered OK.
		 */
		saved_algo = rsa_context->ec.algorithm;
		rsa_context->ec.algorithm = TE_ALG_MODEXP;

		/* checked outside mutex */
		ret = CCC_ENGINE_SWITCH_RSA(rsa_context->ec.engine, &input, &rsa_context->ec);

		rsa_context->ec.algorithm = saved_algo;

		HW_MUTEX_RELEASE_ENGINE(rsa_context->ec.engine);

		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Requires: <DIGEST,DIGESTLEN,SIGNATURE,SIGLEN> and the RSA context
 *
 * or (if s->sig_context.rsa_flags has RSA_FLAGS_DIGEST_INPUT_FIRST set)
 *
 * Requires: <MESSAGE,MSGLEN,SIGNATURE,SIGLEN> and the RSA context
 *
 * If data is in BIG ENDIAN (it is by default)
 * input_params->src_sigature is reversed to rsa_context->buf; and either
 * way, sig is then deciphered to rsa_context->buf and padding processed.
 *
 * NOTE: This code is not data size optimized yet => TODO! (most
 * likely could do with less work buffers); lwrrently the rsa_context->buf
 * is used for SE engine input data since that buffer needs to fit in
 * one page. The other buffers are not accessed via phys addrs.
 */
static status_t rsa_process_verify(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint8_t  *digest = NULL;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Async verify can not use a local buffer (change if feature added) */
	uint8_t sync_digest[MAX_DIGEST_LEN];
	digest = &sync_digest[0U];

	{
		ret = rsa_process_verify_start(input_params, rsa_context, digest);
		CCC_ERROR_CHECK(ret);
	}

	{
		/*
		 * At this point plaintext RSA value is
		 * in rsa_context->buf, length is (rsa_size in bytes)
		 *
		 * The decode the deciphered signature and verify that the
		 * digest in input_src->src_digest matches the signature.
		 */
		switch (rsa_context->ec.algorithm) {
		case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
		case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
		case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
		case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
		case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
			ret = rsa_pss_verify(input_params, rsa_context);
			break;

		case TE_ALG_RSASSA_PKCS1_V1_5_SHA1:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA224:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
		case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:
#if HAVE_RSA_PKCS_VERIFY
			ret = rsa_pkcs1_v1_5_verify(input_params, rsa_context);
#else
			ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
			break;

		case TE_ALG_RSASSA_PKCS1_V1_5_MD5:
			ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
			break;

		default:
			LTRACEF("Algorithm 0x%x not supported for RSA sig verify\n",
				rsa_context->ec.algorithm);
			ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
			break;
		}

		CCC_ERROR_CHECK(ret);
		rbad = NO_ERROR;
	}

	FI_BARRIER(status, rbad);
fail:
	cleanup_rsa_verify(rsa_context, input_params, digest);

	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_rsa_process_verify(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = rsa_process_verify(input_params, rsa_context);
	LTRACEF("eit: %d\n",ret );

	return ret;
}

status_t se_rsa_process_sign(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = ERR_NOT_SUPPORTED;
	LTRACEF("entry\n");

	(void)ret;

#if HAVE_RSA_SIGN
	ret = rsa_process_sign(input_params, rsa_context);
#else
	(void)input_params;
	(void)rsa_context;
	LTRACEF("RSA signing not supported\n");
	ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */
