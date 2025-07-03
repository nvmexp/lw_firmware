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

#if HAVE_RSA_SIGN
#if CCC_WITH_RSA

#include <crypto_ops.h>

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

/* Override this macro to set the RSA PSS salt length
 * Default is to match digest length with salt length.
 *
 * TODO: Add RSA INIT parameter for setting salt length if required.
 */
#ifndef RSA_PSS_SALT_LENGTH
#define RSA_PSS_SALT_LENGTH(alg, hl) (hl)
#endif

static status_t se_rsa_process_sign_check_key(struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_VALID_RSA_KEYSIZE_BITS(rsa_context->ec.rsa_size << 3U))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* RSA signing needs an RSA private key
	 */
	//
	// XXXXXXXXX FIXME: If the PKA1 is used it does not need a keyslot =>
	// REMEMBER TO VERIFY my recent change where I set this to invalid value
	// for PKA1 !!!!
	//
	if ((rsa_context->ec.rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) ||
	    (rsa_context->ec.rsa_keytype != KEY_TYPE_RSA_PRIVATE)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_process_sign_check_args(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == rsa_context)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == input_params->src) || (0U == input_params->input_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == input_params->dst) ||
	    (rsa_context->ec.rsa_size > input_params->output_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (rsa_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("RSA context buffer can not be used for RSA signing\n"));
	}

	ret = se_rsa_process_sign_check_key(rsa_context);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_RSA_PSS_SIGN

/* callwlates the length for work buffer and get the zeroed work buffer
 * used by caller.
 */
static status_t rsa_pss_safe_set_encode_mem(struct context_mem_s *cmem,
					    uint8_t **mem_p, uint32_t rsa_size,
					    uint32_t hlen,
					    uint32_t *pss_encode_mem_len_p)
{
	status_t ret = NO_ERROR;
	uint8_t *mem = NULL;
	uint32_t pss_encode_mem_len = 0U;

	LTRACEF("entry\n");

	if ((rsa_size > MAX_RSA_BYTE_SIZE) ||
	    (hlen > MAX_DIGEST_LEN)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	pss_encode_mem_len = (2U * rsa_size) + hlen;

	mem = CMTAG_MEM_GET_BUFFER(cmem,
				   CMTAG_BUFFER,
				   0U,
				   pss_encode_mem_len);
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	*mem_p = mem;
	*pss_encode_mem_len_p = pss_encode_mem_len;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_em_len_check(uint32_t em_len, uint32_t hlen,
				     uint32_t sLen)
{
	status_t ret    = NO_ERROR;
	uint32_t min_em_len = 0U;

	LTRACEF("entry\n");

	if ((hlen > MAX_DIGEST_LEN) || (sLen > em_len) ||
	    (em_len > MAX_RSA_BYTE_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	min_em_len = hlen + sLen + 2U;

	if (em_len < min_em_len) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_prepare_em(const struct se_data_params *input_params,
				   uint8_t *em, uint32_t *em_len_p,
				   uint32_t sLen, uint32_t hlen,
				   uint32_t rsa_size)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t em_len = 0U;
	uint32_t rsa_bitsize = 0U;

	LTRACEF("entry\n");

	if ((rsa_size > MAX_RSA_BYTE_SIZE) || (0U == rsa_size)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	/* EM length, not longer than RSA bytesize */
	rsa_bitsize = rsa_size * 8U;
	CCC_SAFE_ICEIL_U32(em_len, (rsa_bitsize - 1U), 8U);

	ret = rsa_pss_em_len_check(em_len, hlen, sLen);
	CCC_ERROR_CHECK(ret);

	for (inx = 0U; inx < 8U; inx++) {
		em[inx] = 0x00U;
	}

	FI_BARRIER(u32, inx);

	if (inx != 8U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	for (inx = 0U; inx < hlen; inx++) {
		em[8U + inx] = input_params->src_digest[inx];
	}

	FI_BARRIER(u32, inx);

	if (inx != hlen) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	*em_len_p = em_len;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* If sLen > 0 generate random salt of sLen bytes to &em[8+hlen]
 *
 * sLen validated by caller.
 */
static status_t rsa_pss_gen_em_salt(uint8_t *em,
				    uint32_t sLen, uint32_t hlen,
				    uint8_t **salt_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == em) || (0U == hlen) || (NULL == salt_p)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	*salt_p = NULL;

	if (sLen > 0U) {
		/* Random salt generated to em[8U+hlen]
		 */
		uint8_t *salt = &em[8U + hlen];

#if defined(CCC_GENERATE_RANDOM)
		ret = CCC_GENERATE_RANDOM(salt, sLen);
		CCC_ERROR_CHECK(ret);
#else
#error "RSA-PSS needs a random number generator"
#endif

		*salt_p = salt;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_hash_generation(struct context_mem_s *cmem,
					uint8_t *H, const uint8_t *em,
					uint32_t sLen, uint32_t hlen,
					te_crypto_algo_t md_algorithm)
{
	status_t ret = NO_ERROR;
	uint32_t m_prime_len = 0U;
	struct se_data_params input;

	LTRACEF("entry\n");

	if ((hlen > MAX_DIGEST_LEN) || (sLen > MAX_RSA_BYTE_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	m_prime_len = 8U + hlen + sLen;

	input.src	  = em;
	input.input_size  = m_prime_len;
	input.dst	  = H;
	input.output_size = hlen;

	LOG_HEXBUF("H'", em, input.input_size);

	/* Digest h := digest(H')
	 */
	ret = sha_digest(cmem, TE_CRYPTO_DOMAIN_KERNEL,
			 &input, md_algorithm);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("H", H, hlen);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_enc_db(uint8_t *db, const uint8_t *salt,
			       uint32_t hlen, uint32_t sLen, uint32_t em_len)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t ps_len = 0U;

	LTRACEF("entry\n");

	if ((hlen > MAX_DIGEST_LEN) || (sLen > MAX_RSA_BYTE_SIZE) ||
	    (em_len < (hlen + sLen + 2U))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	ps_len = em_len - hlen - sLen - 2U;

	db[ps_len] = 0x01U;

	if (sLen > 0U) {
		uint32_t salt_offset = ps_len + 1U;
		/* Salt was generated to em, see above
		*/
		for (inx = 0U; inx < sLen; inx++) {
			/* ps_len add sLen never exceed em_len */
			uint32_t db_inx = salt_offset + inx;
			db[db_inx] = salt[inx];
		}

		FI_BARRIER(u32, inx);

		if (inx != sLen) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_safe_set_db_len(uint32_t *db_len_p, uint32_t em_len,
					uint32_t hlen)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (em_len <= hlen) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	*db_len_p = em_len - hlen - 1U;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;

}

static status_t rsa_pss_prepare_masked_db(uint8_t *masked_db, uint32_t rsa_bsize,
					  uint32_t em_len)
{
	status_t ret    = NO_ERROR;
	uint32_t lowest_bits = 0U;
	uint32_t lb_mask = 0U;
	uint32_t tmp = 0U;

	LTRACEF("entry\n");

	if ((em_len > MAX_RSA_BYTE_SIZE) ||
	    (rsa_bsize > MAX_RSA_BYTE_SIZE) ||
	    (em_len < rsa_bsize)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	// XXX TODO: Hmm... lowest_bits is always value 1 here?
	//
	lowest_bits = (8U * em_len) - ((rsa_bsize * 8U) - 1U);

	if (lowest_bits >= 8U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
	lb_mask = ((uint32_t)0xFFU << (8U - lowest_bits)) & 0xFFU;

	tmp = masked_db[0];
	masked_db[0] = BYTE_VALUE(tmp & ~lb_mask);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_encode_em(uint8_t *em,
				  const uint8_t *masked_db,
				  uint32_t db_len,
				  const uint8_t *H,
				  uint32_t hlen,
				  uint32_t rsa_bsize)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	LOG_HEXBUF("masked_DB", masked_db, db_len);

	if ((hlen > MAX_DIGEST_LEN) || (db_len > MAX_RSA_BYTE_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	for (inx = 0U; inx < db_len; inx++) {
		em[inx] = masked_db[inx];
	}

	FI_BARRIER(u32, inx);

	if (db_len != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	for (inx = 0U; inx < hlen; inx++) {
		em[db_len + inx] = H[inx];
	}

	FI_BARRIER(u32, inx);

	if (hlen != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	em[db_len + hlen] = 0xblw;

	if ((db_len + hlen + 1U) != rsa_bsize) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("index %u does not match RSA size %u\n",
					      (db_len + hlen + 1U), rsa_bsize));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_pss_construct_em(uint8_t *em,
				     uint8_t *masked_db,
				     uint32_t db_len,
				     uint32_t em_len,
				     const uint8_t *H,
				     uint32_t hlen,
				     uint32_t rsa_bsize)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = rsa_pss_prepare_masked_db(masked_db, rsa_bsize, em_len);
	CCC_ERROR_CHECK(ret);

	ret = rsa_pss_encode_em(em, masked_db, db_len, H, hlen, rsa_bsize);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static void rsa_pss_cleanup(struct context_mem_s *cmem,
			    uint8_t *mem, uint32_t mlen)
{
	if (NULL != mem) {
		se_util_mem_set(mem, 0U, mlen);
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_BUFFER,
				  mem);
	}
}

/* EMSA-PSS-ENCODE (M, emBits)
 *
 * sLen == salt len
 * em is a contiguous buffer of at least RSA_SIZE bytes readable by SE DMA engines.
 */
static status_t rsa_pss_encode(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context,
	uint8_t *em,
	uint32_t sLen)
{
	status_t ret    = NO_ERROR;
	uint32_t pss_encode_mem_len = 0U;
	uint32_t em_len = 0U;
	uint32_t hlen   = input_params->src_digest_size;
	uint32_t db_len = 0U;
	uint8_t *db     = NULL;
	uint8_t *mem    = NULL;
	uint8_t *masked_db = NULL;
	uint8_t *H      = NULL;
	uint8_t *salt   = NULL;
	uint32_t offset = 0U;
	struct context_mem_s *cmem = rsa_context->ec.cmem;

	LTRACEF("entry\n");

	ret = rsa_pss_safe_set_encode_mem(cmem,
					  &mem, rsa_context->ec.rsa_size, hlen,
					  &pss_encode_mem_len);
	CCC_ERROR_CHECK(ret);

	/* rsa_size * 2 + hlen is pss_encode_mem_len that is checked in
	 * function rsa_pss_safe_set_encode_mem. It will not wrap.
	 */
	db        = &mem[offset];
	offset   += rsa_context->ec.rsa_size;

	masked_db = &mem[offset];
	offset   += rsa_context->ec.rsa_size;

	H	  = &mem[offset];
	offset	 += hlen;

	if (offset != pss_encode_mem_len) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/*
	 * Done by caller:
	 *
	 * 1.  If the length of M is greater than the input limitation for the
	 *     hash function (2^61 - 1 octets for SHA-1), output "message too
	 *     long" and stop.
	 *
	 *  2.  Let mHash = Hash(M), an octet string of length hLen.
	 *
	 * 3.  If emLen < hLen + sLen + 2, output "encoding error" and stop.
	 *
	 * Copy digest to em after 8 zero byte prefix.
	 */
	ret = rsa_pss_prepare_em(input_params, em, &em_len, sLen,
				 hlen, rsa_context->ec.rsa_size);
	CCC_ERROR_CHECK(ret);

	/* 4.  Generate a random octet string salt of length sLen; if sLen = 0,
	 * then salt is the empty string.
	 *
	 * 5.  Let
	 * M' = (0x)00 00 00 00 00 00 00 00 || mHash || salt;
	 */
	ret = rsa_pss_gen_em_salt(em, sLen, hlen, &salt);
	CCC_ERROR_CHECK(ret);

	/*
	 * 6.  Let H = Hash(M'), an octet string of length hLen.
	 */
	ret = rsa_pss_hash_generation(cmem, H, em, sLen, hlen,
				      rsa_context->md_algorithm);
	CCC_ERROR_CHECK(ret);

	/*
	 * 7.  Generate an octet string PS consisting of emLen - sLen - hLen - 2
	 *     zero octets. The length of PS may be 0.
	 *
	 * 8.  Let DB = PS || 0x01 || salt; DB is an octet string of length
	 *     emLen - hLen - 1.
	 *
	 * DB was filled with zeros when created, no need to zero here.
	 */
	ret = rsa_pss_enc_db(db, salt, hlen, sLen, em_len);
	CCC_ERROR_CHECK(ret);

	/* salt is not used after this */
	salt = NULL;
	(void)salt;

	/* em is now free */

	ret = rsa_pss_safe_set_db_len(&db_len, em_len, hlen);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("DB", db, db_len);

	/* Combined =>
	 * 9.  Let dbMask = MGF(H, emLen - hLen - 1).
	 * 10. Let maskedDB = DB \xor dbMask.
	 *
	 * em is used as scrath buffer.
	 * masked_db := db XOR dbmask.
	 */
	ret = se_mgf_mask_generation(cmem,
				     H, hlen, masked_db, db_len,
				     rsa_context->md_algorithm, hlen,
				     em, em_len,
				     db);
	if (NO_ERROR != ret) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Step 11. Set the leftmost 8em_len - emBits bits of the leftmost
	 *          octet in DB to zero.
	 */
	ret = rsa_pss_construct_em(em, masked_db, db_len, em_len,
				   H, hlen, rsa_context->ec.rsa_size);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("EM (cleartext)", em, rsa_context->ec.rsa_size);
fail:
	rsa_pss_cleanup(cmem, mem, pss_encode_mem_len);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_PSS_SIGN */

static void cleanup_rsa_sign(struct se_rsa_context *rsa_context,
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
		// possibly optimize number of bytes cleared...
		se_util_mem_set(rsa_context->buf, 0U, sizeof_u32(rsa_context->buf));
		rsa_context->used = 0U;
	}
}

static status_t rsa_modexp_context_buf(struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	te_crypto_algo_t saved_algo = TE_ALG_NONE;

	LTRACEF("entry\n");

	/* No DMA writes to dst, no contiguous requirements.
	 *
	 * dst = RSA(K, rsa_context->buf)
	 */
	input.input_size  = rsa_context->used;
	input.src	  = rsa_context->buf;
	input.output_size = rsa_context->ec.rsa_size;
	input.dst	  = rsa_context->buf;

	HW_MUTEX_TAKE_ENGINE(rsa_context->ec.engine);

	/* Modify the engine context for a while to do plain RSA
	 * exponentiation with the sig validation context RSA engine,
	 * then change it back.
	 *
	 * Since the RSA engine operation does not modify the RSA
	 * engine context => no need to save anything else and this is
	 * considered OK.
	 */
	saved_algo = rsa_context->ec.algorithm;
	rsa_context->ec.algorithm = TE_ALG_MODEXP;

	ret = CCC_ENGINE_SWITCH_RSA(rsa_context->ec.engine, &input, &rsa_context->ec);

	rsa_context->ec.algorithm = saved_algo;

	HW_MUTEX_RELEASE_ENGINE(rsa_context->ec.engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_sign_encode_m(struct se_rsa_context *rsa_context,
				  const struct se_data_params *sign_input,
				  uint32_t hlen)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Digest is correct size? */
	if (sign_input->src_digest_size != hlen) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Digest length %u does not match md algorithm size %u\n",
					     sign_input->src_digest_size, hlen));
	}

	/**
	 * Encoding: Encode the k octet message M according to the
	 *  encoding method selected with the algorithm (PKCS#1v1.5 or PSS)
	 *  into the rsa_context->buf
	 */
	switch (rsa_context->ec.algorithm) {
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
#if HAVE_RSA_PSS_SIGN
		ret = rsa_pss_encode(sign_input,
				     rsa_context,
				     rsa_context->buf,
				     RSA_PSS_SALT_LENGTH(rsa_context->ec.algorithm,
							 hlen));
#else
		LTRACEF("RSASSA_PKCS1_PSS_MGF1 SHA signing not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

#if HAVE_RSA_PKCS1V15_CIPHER
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA1:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA224:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:
#if HAVE_RSA_PKCS_SIGN
		ret = rsa_sig_pkcs1_v1_5_encode(sign_input,
						rsa_context,
						rsa_context->buf,
						false);
#else
		LTRACEF("RSASSA_PKCS1_V1_5 SHA signing not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;
#endif /* HAVE_RSA_PKCS1V15_CIPHER */

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		LTRACEF("Algorithm 0x%x not supported for RSA sign\n",
			rsa_context->ec.algorithm);
		break;
	}
	CCC_ERROR_CHECK(ret, LTRACEF("RSA signature encoding failed\n"));

	/* OK encoding result: RSA_SIZE data encoding in rsa_context->buf
	 */
	rsa_context->used = rsa_context->ec.rsa_size;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t rsa_process_sign(struct se_data_params *input_params,
			  struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;
	struct se_data_params input;
	struct se_data_params *sign_input = input_params;
	uint8_t digest[MAX_DIGEST_LEN];

	LTRACEF("entry\n");

	ret = se_rsa_process_sign_check_args(input_params,
					     rsa_context);
	CCC_ERROR_CHECK(ret);

	ret = get_message_digest_size(rsa_context->md_algorithm, &hlen);
	CCC_ERROR_CHECK(ret);

	/* In case the input needs to be digested first, this bit
	 * must be set.
	 *
	 * XXX This RSA sig creation does not support other than SHA digests now.
	 *     (could add also WHIRLPOOL...)
	 */
	if ((rsa_context->ec.rsa_flags & RSA_FLAGS_DIGEST_INPUT_FIRST) != 0U) {
		input.src	   = input_params->src;
		input.input_size   = input_params->input_size;
		input.dst	   = &digest[0];
		input.output_size  = hlen;

		ret = sha_digest(rsa_context->ec.cmem, rsa_context->ec.domain,
				 &input, rsa_context->md_algorithm);
		CCC_ERROR_CHECK(ret);

		/* Do the RSA signature with the digest
		 * callwlated above instead of the message ref'd to in
		 * input_params.
		 */
		input.src_digest      = &digest[0];
		input.src_digest_size = hlen;
		input.dst	      = input_params->dst;
		input.output_size     = input_params->output_size;

		sign_input = &input;
	}

	ret = rsa_sign_encode_m(rsa_context, sign_input, hlen);
	CCC_ERROR_CHECK(ret);

	/**
	 *  2. a) Colwert the encoded message EM to an integer message
	 *  representative m:
	 *		m = OS2IP (EM).
	 *
	 *  This is not necessary since the integer signature representative
	 *  is already in an octet byte stream.
	 *
	 * m is in rsa_context->buf (in big endian byte order)
	 */

	/**
	 *  2. b) Apply the RSASP1 signature primitive to the RSA private key K
	 *        and the message representative m to produce an
	 *        integer signature representative s:
	 *			  s = RSASP1 (K, m)
	 *
	 * 2.  c) Colwert the signature representative s to a signature S of
	 *        length k octets.
	 *
	 *                        S = I2OSP (s, k)
	 */

	/* colwert rsa_context->buf to little endian (engine requirement) */
	ret = se_util_reverse_list(rsa_context->buf, rsa_context->buf, rsa_context->used);
	CCC_ERROR_CHECK(ret);

	/* Check that LE data < modulus */
	ret = rsa_verify_data_range(&rsa_context->ec, rsa_context->buf, false);
	CCC_ERROR_CHECK(ret);

	/* MODEXP context buffer */
	ret = rsa_modexp_context_buf(rsa_context);
	CCC_ERROR_CHECK(ret);

	/**
	 * 3. Output the signature S
	 *
	 * Signature data is in rsa_context->buf now,
	 * pass it to input_params->dst in correct byte order.
	 */
	input_params->output_size = 0U;

	/* Reverse back to big endian.
	 *
	 * Note: EM was expected to be big endian on input, so
	 * the output signature is also colwerted to big
	 * endian here.
	 *
	 * Input was either message or digest: Both are
	 * treated to be big endian byte order on input when
	 * signed.
	 */
	ret = se_util_reverse_list(input_params->dst,
				   rsa_context->buf,
				   rsa_context->used);
	CCC_ERROR_CHECK(ret);

	input_params->output_size = rsa_context->used;
fail:
	cleanup_rsa_sign(rsa_context, input_params, digest);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */
#endif /* HAVE_RSA_SIGN */
