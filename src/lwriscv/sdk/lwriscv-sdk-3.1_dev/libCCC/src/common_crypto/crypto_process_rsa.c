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

#if HAVE_RSA_CIPHER

#include <crypto_ops.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define TE_RSA_MODE_PADDING(algo)	m_is_rsa_mode_padding(algo)

#define RBUF_SIZE CACHE_LINE

static bool m_is_rsa_mode_padding(te_crypto_algo_t algo)
{
	return (((algo) == TE_ALG_RSAES_PKCS1_V1_5)             ||
		((algo) == TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1)   ||
		((algo) == TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224) ||
		((algo) == TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256) ||
		((algo) == TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384) ||
		((algo) == TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512));
}

#if HAVE_RSA_PKCS1V15_CIPHER
static status_t get_pseudorandom_nonzero_bytes(struct context_mem_s *cmem,
					       uint32_t len, uint8_t *dst)
{
	status_t ret = NO_ERROR;
	uint32_t count = 0U;
	bool done = false;
	uint8_t *rbuf = CMTAG_MEM_GET_BUFFER(cmem,
					     CMTAG_ALIGNED_BUFFER,
					     CACHE_LINE,
					     RBUF_SIZE);
	if (NULL == rbuf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	do {
		uint32_t inx = 0U;

#if defined(CCC_GENERATE_RANDOM)
		/* status_t CCC_GENERATE_RANDOM(uint8_t *buffer, uint32_t length) */
		ret = CCC_GENERATE_RANDOM(rbuf, RBUF_SIZE);
#else
#error "RSA padding needs a random number generator"
#endif
		if (NO_ERROR != ret) {
			LTRACEF("Failed to generate %u long random value: %d\n", len, ret);
			break;
		}

		for (inx = 0U; inx < RBUF_SIZE; inx++) {
			if (count >= len) {
				done = true;
				break;
			}

			if (0x00U != rbuf[inx]) {
				dst[count] = rbuf[inx];
				count++;
			}
		}
	} while(!BOOL_IS_TRUE(done));

	CCC_ERROR_CHECK(ret);


fail:
	if (NULL != rbuf) {
		se_util_mem_set(rbuf, 0U, RBUF_SIZE);
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ALIGNED_BUFFER,
				  rbuf);
	}
	return ret;
}

/* PAD data to little endian rsa_context->buf */
static status_t rsa_pkcs1v5_pad_to_le(
	struct se_rsa_context *rsa_context,
	const struct se_data_params *input_params,
	bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = input_params->input_size;
	uint32_t inx = 0U;
	uint8_t BT = 0x00U;
	uint32_t PS_length = 0U;
	uint32_t required_pad_length = 0U;

	LTRACEF("entry\n");

	switch(rsa_context->ec.rsa_keytype) {
	case KEY_TYPE_RSA_PUBLIC:
		/* Public key BT value */
		BT = 0x02U;
		required_pad_length = 8U;
		break;

	case KEY_TYPE_RSA_PRIVATE:
		/* RFC-8017 no longer defines this => not supported in CCC.
		 *
		 * Older RFC allowed private key encryption with PCS1v1.5 with:
		 * BT 0x01
		 * PS_byte nonzero pseudorandom byte
		 * pad length 8
		 */
		LTRACEF("RSA private key encryption with PKCS1-V1_5 padding is not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (bytes_left > (rsa_context->ec.rsa_size - 3U - required_pad_length)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("Data too long (%u bytes) for adding PKCS#1v1.5 padding\n",
					     input_params->input_size));
	}

	if (0U != rsa_context->used) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* XXX This could be optimized by writing bytes backward in LE mode => later... */

	PS_length = rsa_context->ec.rsa_size - 3U - bytes_left;

	inx = 0U;
	rsa_context->buf[inx] = 0x00U;
	inx++;
	rsa_context->buf[inx] = BT;
	inx++;

	ret = get_pseudorandom_nonzero_bytes(rsa_context->ec.cmem,
					     PS_length, &rsa_context->buf[inx]);
	CCC_ERROR_CHECK(ret);

	inx += PS_length;

	rsa_context->buf[inx] = 0x00U;
	inx++;

	if (BOOL_IS_TRUE(data_big_endian)) {
		se_util_mem_move(&rsa_context->buf[inx], input_params->src, bytes_left);
	} else {
		ret = se_util_reverse_list(&rsa_context->buf[inx], input_params->src, bytes_left);
		CCC_ERROR_CHECK(ret);
	}

	rsa_context->used = bytes_left + inx;

	if (rsa_context->used != rsa_context->ec.rsa_size) {
		LTRACEF("Padded data size error: %u (data) + %u (meta) != %u (rsa size)\n",
			bytes_left, inx, rsa_context->ec.rsa_size);
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	/* colwert rsa_context->buf to little endian (engine requirement) */
	ret = se_util_reverse_list(rsa_context->buf, rsa_context->buf, rsa_context->used);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LTRACEF("RSA pkcs1-v1_5 padded data (RSA_SIZE %u) (little endian for engine)\n",
		rsa_context->ec.rsa_size);
	LOG_HEXBUF("RSA pkcs1v5 padded data", rsa_context->buf, rsa_context->used);
#endif

fail:
	if (NO_ERROR != ret) {
		se_util_mem_set(rsa_context->buf, 0U, sizeof_u32(rsa_context->buf));
	}

	LTRACEF("exit: ret=%d\n", ret);
	return ret;
}

static status_t rsa_pkcs1v5_unpad_handle_pad(const struct se_rsa_context *rsa_context,
					     uint32_t *required_pad_length_p)
{
	status_t ret = NO_ERROR;
	uint32_t required_pad_length = 0U;
	uint32_t inx = 0U;
	uint8_t  BT = 0x00U;

	LTRACEF("entry\n");

	switch(rsa_context->ec.rsa_keytype) {
	case KEY_TYPE_RSA_PRIVATE:
		/* Public key BT value (data ciphered with public key, deciphered with private key) */
		BT = 0x02U;
		required_pad_length = 8U;
		break;

	case KEY_TYPE_RSA_PUBLIC:
		/* RFC-8017 no longer defines this => not supported (unpadding data with public key after
		 * it was ciphered with private key)
		 */
		LTRACEF("PKCS#1v1_5 decoding for RSA public key deciphered data is not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if ((rsa_context->buf[0] != 0x00U) || (rsa_context->buf[1] != BT)) {
		LTRACEF("Prefix error in RSA pubkey ciphered data\n");
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* At least 2+8 bytes are metadata in PKCS#1_1v5 [ bytes 0..9 ] */
	for (inx = 2U; inx < (2U+required_pad_length); inx++) {
		if (0x00U == rsa_context->buf[inx]) {
			LTRACEF("Zero as preamble pad byte %u\n", inx);
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

	if (inx != (2U+required_pad_length)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	*required_pad_length_p = required_pad_length;

fail:
	LTRACEF("exit: ret=%d\n", ret);
	return ret;
}

/* Input data always in big endian */
static status_t rsa_pkcs1v5_unpad(
	const struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t required_pad_length = 0U;
	uint32_t index_of_zero = 0U;

	LTRACEF("entry\n");

	ret = rsa_pkcs1v5_unpad_handle_pad(rsa_context, &required_pad_length);
	CCC_ERROR_CHECK(ret);

	/* first 0x00 terminated padding after this point */
	index_of_zero = 0U;

	/* bytes [ 9 .. index_of_zero ] */
	for (inx = 2U + required_pad_length; inx < rsa_context->ec.rsa_size; inx++) {
		if (0x00U == rsa_context->buf[inx]) {
			index_of_zero = inx;
			break;
		}
	}

	// XX Is the 0x00U mandatory at the end (if not present, is it a padding error?)
	// Now both conditions are accepted as "empty data".
	//
	if ((0U == index_of_zero) || ((index_of_zero + 1U) >= rsa_context->ec.rsa_size)) {
		LTRACEF("Empty RSA data after padding removed\n");
		input_params->output_size = 0U;
	} else {
		/* Data is in [ index_of_zero + 1 .. rsa_context->ec.rsa_size - 1 ] */
		uint32_t data_length = rsa_context->ec.rsa_size - index_of_zero - 1U;

		LTRACEF("RSA data length %u, padding (8 + %u pad bytes)\n",
			data_length, (index_of_zero + 1U) - (2U + required_pad_length));

		if (BOOL_IS_TRUE(data_big_endian)) {
			se_util_mem_move(input_params->dst,
					 &rsa_context->buf[index_of_zero + 1U],
					 data_length);
		} else {
			ret = se_util_reverse_list(input_params->dst,
						   &rsa_context->buf[index_of_zero + 1U],
						   data_length);
			CCC_ERROR_CHECK(ret);
		}
		input_params->output_size = data_length;
	}

fail:
	LTRACEF("exit, ret=%u\n", ret);
	return ret;
}
#endif /* HAVE_RSA_PKCS1V15_CIPHER */

#if HAVE_RSA_OAEP

/* For both encoding and decoding RSA OAEP
 *
 * Min cbuffer size would be (k - hlen - 1 + 4); just use rsa size for it now.
 */
#define RSA_OAEP_CBUFFER_SIZE (rsa_context->ec.rsa_size)

/* PAD data to litle endian rsa_context->buf */
static status_t rsa_oaep_pad_to_le_intern(
	struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	bool data_big_endian,
	uint8_t *mem,
	uint32_t hlen)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t PS_length = 0U;
	const uint32_t k = rsa_context->ec.rsa_size;
	uint8_t *ptr = NULL;
	uint8_t *seed = NULL;
	uint8_t *tbuf = NULL;
	uint8_t *cbuffer = NULL;
	uint32_t offset = 0U;
	uint32_t DB_len = 0U;
	bool use_empty_label = true;
	struct context_mem_s *cmem = rsa_context->ec.cmem;

	LTRACEF("entry\n");

	tbuf    = &mem[offset];
	offset += DMA_ALIGN_SIZE(rsa_context->ec.rsa_size);

	cbuffer = &mem[offset];
	offset += RSA_OAEP_CBUFFER_SIZE;

	seed    = &mem[offset];
	offset += hlen;

	/* hash too big for the rsa size?
	 */
	if (k < ((2U * hlen) - 2U)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("Digest size too big for the RSA size\n"));
	}

	/* 7.1.1 length checking
	 */
	if (input_params->input_size > (k - (2U * hlen) - 2U)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("Message too big for the RSA size\n"));
	}

	/* construct
	 *  DB = lhash || PS || 0x01 || M.
	 *
	 * length(PS) may be zero.
	 * length(DB) = k - hlen - 1
	 */

	DB_len = (k - hlen - 1U);

	/* ptr is location(lhash)
	 */
	ptr = tbuf;

#if HAVE_OAEP_LABEL
	if (NULL != rsa_context->ec.oaep_label_digest) {
		LTRACEF("OAEP optional label digest used\n");
		use_empty_label = false;
		se_util_mem_move(ptr, rsa_context->ec.oaep_label_digest, hlen);
	}
#endif

	if (BOOL_IS_TRUE(use_empty_label)) {
		/* Digest NULL vector (no bytes)
		 * Results in a constant value.
		 */
		struct se_data_params input;

		input.src         = NULL;
		input.input_size  = 0U;
		input.dst         = ptr;
		input.output_size = hlen;

		ret = sha_digest(rsa_context->ec.cmem, TE_CRYPTO_DOMAIN_KERNEL,
				 &input, rsa_context->md_algorithm);
		CCC_ERROR_CHECK(ret);
	}

	PS_length = k - input_params->input_size - (2U * hlen) - 2U;

	/* ptr = location PS
	 */
	ptr = &ptr[hlen];
	for (inx = 0U; inx < PS_length; inx++) {
		ptr[inx] = 0x00U;
	}

	/* ptr = location 0x01
	 */
	ptr[inx] = 0x01U;
	inx++;

	/* ptr = location M
	 */
	ptr = &ptr[inx];
	if (BOOL_IS_TRUE(data_big_endian)) {
		for (inx = 0U; inx < input_params->input_size; inx++) {
			ptr[inx] = input_params->src[inx];
		}
	} else {
		uint32_t last = input_params->input_size - 1U;
		for (inx = 0U; inx < input_params->input_size; inx++) {
			ptr[inx] = input_params->src[last - inx];
		}
	}

	LOG_HEXBUF("OAEP(enc) DB", tbuf, DB_len);

	/* Generate random seed of length hlen
	 */
#if defined(CCC_GENERATE_RANDOM)
	ret = CCC_GENERATE_RANDOM(seed, hlen);
#else
#error "RSA-OAEP needs a random number generator"
#endif
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("OAEP(enc) seed", seed, hlen);

	/* Let:
	 * dbMask = MGF(seed, k - hlen - 1)
	 * maskedDB = DB ^ dbMask
	 */
	ret = se_mgf_mask_generation(cmem, seed, hlen,
				     rsa_context->buf, DB_len,
				     rsa_context->md_algorithm, hlen,
				     cbuffer, RSA_OAEP_CBUFFER_SIZE,
				     tbuf);
	CCC_ERROR_CHECK(ret);

	/* tbuf <= maskedDB
	 */
	se_util_mem_move(tbuf, rsa_context->buf, DB_len);

	/* Let:
	 * seedMask = MGF(maskedDB, hlen)
	 * maskedSeed = seed ^ seedMask
	 *
	 * tbuf contains the maskedDB (DB_len bytes).
	 * maskedSeed set to (seedMask xor seed) into rsa_context buf.
	 */
	ret = se_mgf_mask_generation(cmem, tbuf, DB_len,
				     &rsa_context->buf[1U], hlen,
				     rsa_context->md_algorithm, hlen,
				     cbuffer, RSA_OAEP_CBUFFER_SIZE,
				     seed);
	CCC_ERROR_CHECK(ret);

	/*
	 * EM = 0x00 || maskedSeed || maskedDB
	 */
	rsa_context->buf[0U] = 0x00U;
	se_util_mem_move(&rsa_context->buf[ 1U + hlen ], tbuf, DB_len);

	LOG_HEXBUF("OAEP(enc) EM (big endian)", rsa_context->buf, k);

	/* Colwert EM to little endian
	 */
	ret = se_util_reverse_list(rsa_context->buf, rsa_context->buf, k);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("OAEP(enc) EM (little endian)", rsa_context->buf, k);

	rsa_context->used = k;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* PAD data to litle endian rsa_context->buf */
static status_t rsa_oaep_pad_to_le(
	struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;
	uint8_t *mem = NULL;
	struct context_mem_s *cmem = rsa_context->ec.cmem;

	LTRACEF("entry\n");

	ret = get_message_digest_size(rsa_context->md_algorithm, &hlen);
	CCC_ERROR_CHECK(ret);

	/* hlen must be set before using OAEP_ENCODE_MEM_SIZE
	 */
#define OAEP_ENCODE_MEM_SIZE (DMA_ALIGN_SIZE(rsa_context->ec.rsa_size) + RSA_OAEP_CBUFFER_SIZE + hlen)
	mem = CMTAG_MEM_GET_BUFFER(cmem,
				   CMTAG_ALIGNED_BUFFER,
				   CACHE_LINE,
				   OAEP_ENCODE_MEM_SIZE);
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = rsa_oaep_pad_to_le_intern(rsa_context, input_params,
					data_big_endian, mem, hlen);
	CCC_ERROR_CHECK(ret);
fail:
	if (NULL != mem) {
		se_util_mem_set(mem, 0U, OAEP_ENCODE_MEM_SIZE);
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ALIGNED_BUFFER,
				  mem);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define CHECK_ERROR_CONDVAR_NOTRACE(econd)				\
	do { cond_failed = ((econd) || cond_failed);			\
	     cond_ok = (cond_ok && !cond_failed); } while (CFALSE)

#if LOCAL_TRACE
#define CHECK_ERROR_CONDVAR(econd)					\
	do {								\
		bool local_cond = (econd);				\
		CHECK_ERROR_CONDVAR_NOTRACE(local_cond);		\
		if (BOOL_IS_TRUE(local_cond)) { LTRACEF("==> Condition error: %s\n", #econd); } \
	} while (CFALSE)
#else
#define CHECK_ERROR_CONDVAR(econd) CHECK_ERROR_CONDVAR_NOTRACE(econd)
#endif

static status_t rsa_oaep_move_decoded_result(uint8_t *dst,
					     const uint8_t *src,
					     bool data_big_endian,
					     uint32_t msg_len)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (msg_len > 0U) {
		if (BOOL_IS_TRUE(data_big_endian)) {
			se_util_mem_move(dst, src, msg_len);
		} else {
			ret = se_util_reverse_list(dst, src, msg_len);
			CCC_ERROR_CHECK(ret);
		}

		LOG_HEXBUF("OAEP(dec): MSG", dst, msg_len);
	} else {
		LTRACEF("OAEP(dec): empty message\n");
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_oaep_unpad_intern(
	struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	bool data_big_endian,
	uint8_t *mem,
	uint32_t hlen)
{
	status_t ret = NO_ERROR;

	/* CHECK_ERROR_CONDVAR macro uses these variables
	 */
	bool cond_failed = false;
	bool cond_ok     = true;

	uint32_t inx = 0U;
	const uint32_t k = rsa_context->ec.rsa_size;
	uint8_t *EM = rsa_context->buf;	/* Big endian EM value */
	uint8_t *lhash = NULL;
	uint8_t *tbuf = NULL;
	uint8_t *cbuffer = NULL;
	uint32_t offset = 0U;
	bool in_PS = true;
	bool DB_ilwalid = false;
	uint32_t one_index = 0U;
	uint32_t msg_len = 0U;
	uint32_t DB_len = 0U;
	bool use_empty_label = true;
	struct context_mem_s *cmem = rsa_context->ec.cmem;

	LTRACEF("entry\n");

	tbuf    = &mem[offset];
	offset += DMA_ALIGN_SIZE(rsa_context->ec.rsa_size);

	cbuffer = &mem[offset];
	offset += RSA_OAEP_CBUFFER_SIZE;

	lhash   = &mem[offset];
	offset += hlen;

#if HAVE_OAEP_LABEL
	if (NULL != rsa_context->ec.oaep_label_digest) {
		LTRACEF("OAEP optional label digest used\n");
		use_empty_label = false;
		se_util_mem_move(lhash, rsa_context->ec.oaep_label_digest, hlen);
	}
#endif

	if (BOOL_IS_TRUE(use_empty_label)) {
		/* Digest NULL vector (no bytes)
		 * Results in a constant value.
		 */
		struct se_data_params input;

		/* let: lhash = digest(LABEL)
		 * If LABEL not specified, use empty string as LABEL.
		 */
		input.input_size  = 0U;
		input.src         = NULL;
		input.output_size = hlen;
		input.dst         = lhash;

		ret = sha_digest(cmem, TE_CRYPTO_DOMAIN_KERNEL,
				 &input, rsa_context->md_algorithm);
		CHECK_ERROR_CONDVAR(NO_ERROR != ret);
	}

	/* Must not let detected errors affect the exelwtion time of
	 * the following code in this function. Exelwtion time must
	 * not depend on the content of the padded data.
	 */
	CHECK_ERROR_CONDVAR(rsa_context->used != k);

	CHECK_ERROR_CONDVAR(k < ((2U * hlen) + 2U));

	/* input_params->src == EM
	 *
	 * Let:
	 * EM = Y || maskedSeed || maskedDB
	 *
	 * where:
	 * length(Y) == 1
	 * length(maskedSeed) == hlen
	 * length(maskedDB) == k - hlen - 1
	 *
	 * If Y is nonzero => "decryption error"
	 */
	CHECK_ERROR_CONDVAR(EM[0U] != 0x00U);

	{
		/* Let:
		 * seedMask = MGF(maskedDB, hlen)
		 * seed = maskedSeed ^ seedMask
		 */
		uint8_t *maskedDB = &EM[ 1U + hlen ];

		/* dbMask length is same as DB_len == (k - hlen -1)
		 */
		DB_len = (k - hlen - 1U);

		/* DB_len bytes of maskedDB are used as seed.
		 * On return TBUF contains the unmasked SEED (hlen bytes).
		 *
		 * seed = maskedSeed ^ MGF(maskedDB, hlen)
		 *
		 * maskedSeed == &EM[1U]
		 */
		ret = se_mgf_mask_generation(cmem, maskedDB, DB_len,
					     tbuf, hlen,
					     rsa_context->md_algorithm, hlen,
					     cbuffer, RSA_OAEP_CBUFFER_SIZE,
					     &EM[ 1U ]);
		CHECK_ERROR_CONDVAR(NO_ERROR != ret);

		/* Seed is now in &tbuf[0] (hlen bytes).
		 */
		LOG_HEXBUF("OAEP(dec): seed", tbuf, hlen);

		/* Construct the DB value (DB_len bytes) to tbuf.
		 *
		 * dbMask = MGF(seed, j - hlen - 1)
		 * DB = maskedDB ^ dbMask
		 */
		ret = se_mgf_mask_generation(cmem, tbuf, hlen,
					     tbuf, DB_len,
					     rsa_context->md_algorithm, hlen,
					     cbuffer, RSA_OAEP_CBUFFER_SIZE,
					     maskedDB);
		CHECK_ERROR_CONDVAR(NO_ERROR != ret);
	}

	/* EM is not longer used => rsa_context->buf is free here. */
	rsa_context->used = 0U;

	/* DB is now in &tbuf[0] (DB_len bytes).
	 *
	 * DB = lHash' || PS || 0x01 || M
	 */
	LOG_HEXBUF("OAEP(dec): DB", tbuf, DB_len);

	/* RFC-8017:
	 * If there is no octet with hexadecimal value 0x01 to
	 * separate PS from M, if lHash does not equal lHash', or if
	 * Y is nonzero, output "decryption error" and stop.
	 */
	{
		bool match = se_util_vec_match(lhash, tbuf, hlen, CFALSE, &ret);
		CHECK_ERROR_CONDVAR(!match);
		CHECK_ERROR_CONDVAR(NO_ERROR != ret);
	}

	for (inx = hlen; inx < DB_len; inx++) {
		uint8_t ch = tbuf[inx];

		if (BOOL_IS_TRUE(in_PS)) {
			in_PS = false;

			if (0x01U == ch) {
				one_index = inx;
			} else if (0x00U == ch) {
				in_PS = true;
			} else {
				DB_ilwalid = true;
			}
		} else {
			msg_len++;
		}
	}

	CHECK_ERROR_CONDVAR(one_index < hlen);
	CHECK_ERROR_CONDVAR(BOOL_IS_TRUE(DB_ilwalid));
	CHECK_ERROR_CONDVAR((((one_index + 1U) + msg_len) != DB_len));
	CHECK_ERROR_CONDVAR(input_params->output_size < msg_len);

	input_params->output_size = 0U;

	if ((NO_ERROR != ret) || BOOL_IS_TRUE(cond_failed) || !BOOL_IS_TRUE(cond_ok)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("OAEP decoding failed\n"));
	}

	ret = rsa_oaep_move_decoded_result(input_params->dst,
					   &tbuf[one_index + 1U],
					   data_big_endian,
					   msg_len);
	CCC_ERROR_CHECK(ret);

	input_params->output_size = msg_len;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Input data always in big endian */
/* RFC-8017:
 * Note: Care must be taken to ensure that an opponent cannot
 * distinguish the different error conditions in Step 3.g, whether by
 * error message or timing, and, more generally, that an opponent
 * cannot learn partial information about the encoded message EM.
 * Otherwise, an opponent may be able to obtain useful information
 * about the decryption of the ciphertext C, leading to a chosen-
 * ciphertext attack such as the one observed by Manger [MANGER].
 */
static status_t rsa_oaep_unpad(
	struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;
	uint8_t *mem = NULL;
	struct context_mem_s *cmem = rsa_context->ec.cmem;

	LTRACEF("entry\n");

	ret = get_message_digest_size(rsa_context->md_algorithm, &hlen);
	CCC_ERROR_CHECK(ret);

	/* hlen must be set before using OAEP_DECODE_MEM_SIZE
	 */
#define OAEP_DECODE_MEM_SIZE (DMA_ALIGN_SIZE(rsa_context->ec.rsa_size) + RSA_OAEP_CBUFFER_SIZE + hlen)

	mem = CMTAG_MEM_GET_BUFFER(cmem,
				   CMTAG_ALIGNED_BUFFER,
				   CACHE_LINE,
				   OAEP_DECODE_MEM_SIZE);
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = rsa_oaep_unpad_intern(rsa_context, input_params,
				    data_big_endian, mem, hlen);
	CCC_ERROR_CHECK(ret);

fail:
	if (NULL != mem) {
		se_util_mem_set(mem, 0U, OAEP_DECODE_MEM_SIZE);
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ALIGNED_BUFFER,
				  mem);
	}

	LTRACEF("exit\n");
	return ret;
}
#endif /* HAVE_RSA_OAEP */

static status_t se_rsa_engine_call(struct se_rsa_context *rsa_context,
				   const uint8_t *buf, uint8_t *dest,
				   uint32_t bytes)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.src = buf;
	input.input_size = bytes;
	input.dst = dest;
	input.output_size = rsa_context->ec.rsa_size;

	/* Callee RSA engine output always written via registers, not via DMA.
	 */
	DEBUG_ASSERT_PHYS_DMA_OK(buf, bytes);

	HW_MUTEX_TAKE_ENGINE(rsa_context->ec.engine);
	ret = CCC_ENGINE_SWITCH_RSA(rsa_context->ec.engine, &input, &rsa_context->ec);
	HW_MUTEX_RELEASE_ENGINE(rsa_context->ec.engine);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_input_check_args(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	/* Two cipher directions: encrypt && decrypt => this affects padding etc.
	 *
	 * NOTE: Added output size test here. This requires that output buffer must
	 * be at least as long as RSA_SIZE... However, when deciphering and unpadding:
	 * the RSA result will be shorter than RSA SIZE, so in this particular case
	 * this test is too strict.
	 *
	 * If this is a problem for you => please let me know!
	 */
	if ((NULL == rsa_context) || (NULL == input_params) ||
	    (NULL == input_params->src) || (NULL == input_params->dst) ||
	    (input_params->output_size < rsa_context->ec.rsa_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

#if HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER
static status_t rsa_process_input_remove_padding(struct se_rsa_context *rsa_context,
						 struct se_data_params *input_params,
						 bool data_big_endian)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)data_big_endian;

	/* Final unpadded data destination */
	if (NULL == input_params->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Colwert result to big endian (for removing padding). After
	 * SE HW operation it is always in little endian.
	 *
	 * If the unpad functions would instead operate in LITTLE ENDIAN => would save
	 * one data copy (TODO). Need to process data backwards...
	 */
	ret = se_util_reverse_list(rsa_context->buf, rsa_context->buf, rsa_context->ec.rsa_size);
	CCC_ERROR_CHECK(ret);

	switch (rsa_context->ec.algorithm) {
	case TE_ALG_RSAES_PKCS1_V1_5:
#if HAVE_RSA_PKCS1V15_CIPHER
		ret = rsa_pkcs1v5_unpad(rsa_context, input_params, data_big_endian);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
#if HAVE_RSA_OAEP
		ret = rsa_oaep_unpad(rsa_context, input_params, data_big_endian);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_NOT_VALID);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_input_add_padding(struct se_rsa_context *rsa_context,
					      struct se_data_params *input_params,
					      bool data_big_endian)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)data_big_endian;
	(void)input_params;

	switch (rsa_context->ec.algorithm) {
	case TE_ALG_RSAES_PKCS1_V1_5:
#if HAVE_RSA_PKCS1V15_CIPHER
		ret = rsa_pkcs1v5_pad_to_le(rsa_context, input_params, data_big_endian);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
#if HAVE_RSA_OAEP
		ret = rsa_oaep_pad_to_le(rsa_context, input_params, data_big_endian);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_NOT_VALID);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER */

static status_t rsa_process_input_postprocess(
	struct se_rsa_context *rsa_context,
	struct se_data_params *input_params,
	uint8_t *rsa_result_data,
	bool data_big_endian,
	bool remove_padding)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* After RSA operation the data is in LE byte order; length here rsa_context->rsa-size.
	 * If the call colwerts it to big endian => callers need to be modified accordingly.
	 */
	if (BOOL_IS_TRUE(remove_padding)) {
#if HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER
		/* Deciphered data bytes (length rsa_context->ec.rsa_size) are
		 * in rsa_result_data (here data is in rsa_context->buf).
		 *
		 * In case of padding error, erase the intermediate
		 * result bytes and return failure.
		 *
		 * All unpadding algorithms use the rsa_context->buf as
		 * source and unpad data to input_params->dst
		 * destination. input_params->output_size is set to
		 * unpadded result length.
		 */
		if (rsa_result_data != rsa_context->buf) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		ret = rsa_process_input_remove_padding(rsa_context, input_params, data_big_endian);
		CCC_ERROR_CHECK(ret);

		/* Now result bytes are in input_params->dst and output_size is the
		 * number of valid bytes in the result (after removing padding).
		 */
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("RSA padding is not supported"));
#endif
	} else {
		if (rsa_result_data != input_params->dst) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		if (BOOL_IS_TRUE(data_big_endian)) {
			ret = se_util_reverse_list(rsa_result_data,
						   rsa_result_data, rsa_context->ec.rsa_size);
			CCC_ERROR_CHECK(ret);
		}
		input_params->output_size = rsa_context->ec.rsa_size;

		/* Now exactly rsa_size bytes are in input_params->dst and output_size is rsa_size.
		 */
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_data_alignment_check(
			const struct se_rsa_context *rsa_context,
			const uint32_t bytes_left)
{
	status_t ret = NO_ERROR;
	uint32_t unaligned_bytes = bytes_left % rsa_context->ec.rsa_size;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(rsa_context->is_encrypt)) {
		/* Ciphering: Either size is ok or using padding or error.
		 *
		 * In case of a padding mode the padding must fit and data must be shorter than
		 * modulus size. Since each padding algorithm requires different amount of space,
		 * such problemas are trapped in the pad handlers, not here.
		 *
		 * Padding encryption modes use the input_params->src data and write the padded
		 * source to rsa_context->buf which is then passed through RSA and the ciphered
		 * output is written to input_params->dst.
		 */
		CCC_LOOP_BEGIN {
#if HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER
			if (TE_RSA_MODE_PADDING(rsa_context->ec.algorithm)) {
				CCC_LOOP_STOP;
			}
#endif
			if (unaligned_bytes != 0U) {
				/* non-padding asymmetric cipher */
				CCC_ERROR_END_LOOP_WITH_ECODE(ERR_BAD_LEN,
						      LTRACEF("RSA(%u) cipher, non-padding mode data not aligned, %u stray bytes\n",
								rsa_context->ec.algorithm, unaligned_bytes));
			}
			CCC_LOOP_STOP;
		} CCC_LOOP_END;
		CCC_ERROR_CHECK(ret);
	} else {
		/* Deciphering: Data size must equal modulus "length in bytes" in all cases. */
		if (bytes_left != rsa_context->ec.rsa_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     LTRACEF("RSA(%u) decipher, data size (%u bytes) incorrect (not %u)\n",
						     rsa_context->ec.algorithm, bytes_left,
						     rsa_context->ec.rsa_size));
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_check_data_length(
			const struct se_rsa_context *rsa_context,
			const uint32_t bytes_left,
			const bool add_padding)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(add_padding)) {
		if (bytes_left < rsa_context->ec.rsa_size) {
			/* Not worth proceeding unless we are padding this to RSA size */
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     LTRACEF("RSA %s input too short (%u), not padding\n",
						     (rsa_context->is_encrypt ? "cipher" : "decipher"),
						     bytes_left));
		}
	}

	if (bytes_left > rsa_context->ec.rsa_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("RSA(%u) %s : input data (%u bytes) too long, modulus size %u bits (%u bytes)\n",
					     rsa_context->ec.algorithm,
					     (rsa_context->is_encrypt ? "cipher" : "decipher"),
					     bytes_left, (rsa_context->ec.rsa_size * 8U),
					     rsa_context->ec.rsa_size));
	}

	/* Error checks: data alignment
	 */
	ret = rsa_process_data_alignment_check(rsa_context, bytes_left);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_do_rsa(struct se_rsa_context *rsa_context,
				   const uint8_t *rsa_source_data,
				   uint8_t *rsa_result_data,
				   uint32_t bytes_left)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Verify that data is in range 0..N-1 (N == modulus)
	 * Here data is in little endian byte order.
	 */
	ret = rsa_verify_data_range(&rsa_context->ec, rsa_source_data, false);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular exponentiation data out of range [0..n-1]\n"));

	LTRACEF("RSA bytes=%u in=%p out=%p\n", bytes_left, rsa_source_data, rsa_result_data);

	/* At this point => data must be in little endian byte order.
	 * Do RSA operation.
	 */
	ret = se_rsa_engine_call(rsa_context, rsa_source_data,
				 rsa_result_data, bytes_left);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA modular exp failed: %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_process_input_start(struct se_data_params *input_params,
					struct se_rsa_context *rsa_context,
					uint8_t *rsa_result_data,
					bool add_padding)
{
	status_t ret = NO_ERROR;
	bool data_big_endian = false;
	const uint8_t *rsa_source_data = NULL;
	uint32_t bytes_left  = 0U;

	LTRACEF("entry\n");

	ret = rsa_process_input_check_args(input_params, rsa_context);
	CCC_ERROR_CHECK(ret);

	rsa_source_data = input_params->src;

	if ((rsa_context->ec.rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U) {
		LTRACEF("RSA input data treated as big endian\n");
		data_big_endian = true;
	}

	bytes_left = input_params->input_size;

#if SE_RD_DEBUG
	if (0U == bytes_left) {
		LTRACEF("RSA(0x%x) %s : has no data\n", rsa_context->ec.algorithm,
			(rsa_context->is_encrypt ? "cipher" : "decipher"));
	}
#endif

	ret = rsa_process_check_data_length(rsa_context, bytes_left, add_padding);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(add_padding)) {
#if HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER
		/* After padding calls => data is little endian in rsa_context->buf */
		ret = rsa_process_input_add_padding(rsa_context, input_params,
						    data_big_endian);
		CCC_ERROR_CHECK(ret);

		rsa_source_data = rsa_context->buf;
		bytes_left      = rsa_context->used;
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("RSA padding is not supported"));
#endif /* HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER */
	} else {
		if (BOOL_IS_TRUE(data_big_endian)) {
			ret = se_util_reverse_list(rsa_context->buf, rsa_source_data, bytes_left);
			CCC_ERROR_CHECK(ret);

			rsa_context->used   = bytes_left;
			rsa_source_data = rsa_context->buf;
		}
	}

	if (bytes_left != rsa_context->ec.rsa_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("RSA(%u) input size %u != modulus byte size %u\n",
					     rsa_context->ec.algorithm, bytes_left,
					     rsa_context->ec.rsa_size));
	}

#if HAVE_NC_REGIONS
	/* If this is one of the SE engines that uses DMA to read RSA
	 * input data then it is simpler to just copy the (max 256)
	 * bytes of data to the contiguous rsa_context->buf if the data is
	 * not already there.
	 *
	 * This makes sure it does not cross non-adjacent page
	 * boundaries and the DMA can read it when using these
	 * engines.
	 */
	switch (rsa_context->ec.engine->e_id) {
	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		if (rsa_source_data != rsa_context->buf) {
			se_util_mem_move(rsa_context->buf, rsa_source_data, bytes_left);
			rsa_source_data = rsa_context->buf;
			rsa_context->used = bytes_left;
		}
		break;

	default:
		/* No action required */
		break;
	}
#endif /* HAVE_NC_REGIONS */

	ret = rsa_process_do_rsa(rsa_context, rsa_source_data,
				 rsa_result_data, bytes_left);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RSA supports only one-pass operations, i.e. there is only
 * one dofinal() call for every initialized RSA rsa_context and then the
 * results are complete.
 *
 * So there is no need for a context gather buffer for RSA operations.
 * Of course, for completeness it could be done and the RSA operation data
 * could be input in chunks but I see no real need for this => not supported.
 *
 * This means that any call to this function must be final and produce
 * either a result or end in failure.
 *
 * This function internally deals with the RSA input data crossing a page boundary
 * (in case the SE engine PKA0 is used).
 */
static status_t rsa_process_input(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret	     = NO_ERROR;
	bool add_padding     = false;
	bool remove_padding  = false;
	uint8_t *rsa_result_data = NULL;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	rsa_result_data = input_params->dst;

	/*
	 * When deciphering, padding algorithms pass the input_params->src as input to RSA
	 * which writes the output to rsa_context->buf. That buffer is then unpadded to input_params->dst.
	 *
	 * This way the unpadding errors will be caught before any data bytes are passed back
	 * to caller.
	 */
	if (TE_RSA_MODE_PADDING(rsa_context->ec.algorithm)) {
		if (BOOL_IS_TRUE(rsa_context->is_encrypt)) {
			LTRACEF("RSA add padding\n");
			add_padding = true;
		} else {
			LTRACEF("RSA remove padding\n");
			rsa_result_data = rsa_context->buf;
			remove_padding = true;
		}
	}

	ret = rsa_process_input_start(input_params, rsa_context,
				      rsa_result_data, add_padding);
	CCC_ERROR_CHECK(ret);

	{
		bool data_big_endian = false;

		if ((rsa_context->ec.rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U) {
			LTRACEF("RSA input data treated as big endian\n");
			data_big_endian = true;
		}

		/* Postprocess the RSA result data */
		ret = rsa_process_input_postprocess(rsa_context, input_params,
						    rsa_result_data, data_big_endian,
						    remove_padding);
		CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
		LOG_HEXBUF("RSA FINAL RESULT (dst)", input_params->dst, input_params->output_size);
#endif
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Processing RSA encrypt / decrypt operations (plain or padded).
 * This is not used for signature operations.
 */
status_t se_rsa_process_input(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = rsa_process_input(input_params, rsa_context);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#endif /* HAVE_RSA_CIPHER */
