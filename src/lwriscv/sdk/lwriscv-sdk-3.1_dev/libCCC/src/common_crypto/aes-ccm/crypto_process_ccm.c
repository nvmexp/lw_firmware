/*
 * Copyright (c) 2019-2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/**
 * @file   crypto_process_ccm.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019
 *
 * @brief Support for authenticating encryption (AE) mode AES-CCM
 *
 * lwlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38c.pdf
 *
 * With the current formatting function defined by NIST the following
 * statements are always true:
 *
 * t is an element of {4,6,8,10,12,14,16};
 * q is an element of {2,3,4,5,6,7,8};
 * n is an element of {7,8,9,10,11,12,13};
 * n+q=15
 * a < 2^64
 *
 * If not true => invalid input is rejected.
 *
 * P => payload passed through AES-CTR
 * Q => a bit string representation of length of P
 * t => octet length of TAG (Tlen)
 * q => octet length of binary representation of the octet length of the payload
 * a => length of AAD data
 * N => Nonce value
 * n => byte length of Nonce
 *
 * B0 block "Flags" octet format (byte 0 of big endian B0):
 *
 * +----------------------------------------------------------------------------+
 * | Bit number|  7     | 6     | 5     | 4     | 3     | 2     | 1     | 0     |
 * +----------------------------------------------------------------------------+
 * | Content   |reserved| Adata |        t-2/2          |         q-1           |
 * +----------------------------------------------------------------------------+
 *
 * Bit 7 => always zero
 * Bit 6 => flag for AAD presense
 *
 * Bits [5..3] => one of the valid values of element t above
 * Bits [2..0] => one of the valid values of element q above
 *
 * B0 block format (16 bytes):
 *
 * +----------------------------------------------------+
 * | Octet # |  0     |  [1 .. (15-q)] | [(16-q) .. 15] |
 * +----------------------------------------------------+
 * | Content | Flags  |      N         |      Q         |
 * +----------------------------------------------------+
 */
#include <crypto_system_config.h>

#if HAVE_AES_CCM

#include <aes-ccm/crypto_ops_ccm.h>
#include <crypto_cipher_proc.h>
#include <tegra_se_gen.h> /* For counter increment function */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define FLAGS_AAD_SHIFT	6U
#define FLAGS_T_SHIFT	3U
#define FLAGS_Q_SHIFT	0U

static status_t check_msg_len(uint32_t q, uint32_t msg_blen)
{
	status_t ret = NO_ERROR;
	bool ok = false;

	LTRACEF("entry\n");

	switch (q) {
	case 2U:
		ok = (msg_blen < 65536U);
		break;
	case 3U:
		ok = (msg_blen < 16777216U);
		break;
	case 4U:
	case 5U:
	case 6U:
	case 7U:
	case 8U:
		ok = true;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(ok)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief B0 block "Flags" octet format (byte 0 of big endian B0):
 *
 * +----------------------------------------------------------------------------+
 * | Bit number|  7     | 6     | 5     | 4     | 3     | 2     | 1     | 0     |
 * +----------------------------------------------------------------------------+
 * | Content   |reserved| Adata |      (t-2)/2          |         q-1           |
 * +----------------------------------------------------------------------------+
 *
 * Bit 7 => always zero
 * Bit 6 => flag for AAD presense
 *
 * Bits [5..3] => one of the valid values of element t above
 * Bits [2..0] => one of the valid values of element q above
 *
 * I.e. same as:
 * +----------------------------------------------------------------------------+
 * | Bit number|  7     | 6     | 5     | 4     | 3     | 2     | 1     | 0     |
 * +----------------------------------------------------------------------------+
 * | Content   |reserved| Adata |      (t-2)/2          | (15 - nonce_len) - 1  |
 * +----------------------------------------------------------------------------+
 *
 */
static status_t ccm_encode_b0_flags(uint32_t *flags_p,
				    uint32_t nonce_blen,
				    uint32_t tag_blen,
				    bool aad_present,
				    uint32_t msg_blen)
{
	status_t ret = NO_ERROR;
	uint32_t t_enc = 0U;
	uint32_t q_enc = 0U;
	uint32_t a_enc = 0U;
	uint32_t q = 0U;

	LTRACEF("entry\n");

	if (NULL == flags_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!AES_CCM_TAG_LEN_VALID(tag_blen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CCM TAG length invalid %u\n", tag_blen));
	}

	if (!AES_CCM_NONCE_LEN_VALID(nonce_blen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CCM NONCE length invalid %u\n", nonce_blen));
	}

	t_enc = (tag_blen - 2U) / 2U;
	q     = (15U - nonce_blen);
	q_enc = q - 1U;
	a_enc = (BOOL_IS_TRUE(aad_present) ? 1U : 0U);

	ret = check_msg_len(q, msg_blen);
	CCC_ERROR_CHECK(ret);

	*flags_p = (a_enc << FLAGS_AAD_SHIFT) | (t_enc << FLAGS_T_SHIFT) | (q_enc << FLAGS_Q_SHIFT);

	LTRACEF("AES-CCM B0 flags 0x%x\n", *flags_p);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#ifdef NOT_YET

/* Are these ever needed? Maybe not...
 */

/* B0 flags field is 8 bit field, passed as 32 bit unsigned value
 */
static uint32_t ccm_b0_flags_get_q(uint32_t b0_flags)
{
	return ((b0_flags >> FLAGS_Q_SHIFT) & 0x7U) + 1U;
}

/* B0 flags field is 8 bit field, passed as 32 bit unsigned value
 */
static uint32_t ccm_b0_flags_get_t(uint32_t b0_flags)
{
	return (((b0_flags >> FLAGS_T_SHIFT) & 0x7U) + 2U) * 2U;
}

#endif /* NOT_YET */

/**
 * @brief B0 block format (16 bytes):
 *
 * +----------------------------------------------------+
 * | Octet # |  0     |  [1 .. (15-q)] | [(16-q) .. 15] |
 * +----------------------------------------------------+
 * | Content | Flags  |      N         |      Q         |
 * +----------------------------------------------------+
 *
 * Q contains the msg length.
 *
 * In Bytes
 */
static status_t ccm_encode_b0(uint8_t *b0_block,
			      const uint8_t *nonce,
			      uint32_t nonce_blen,
			      uint32_t tag_blen,
			      uint32_t aad_blen,
			      uint32_t msg_blen)
{
	status_t ret = NO_ERROR;
	uint64_t Q =  (uint64_t)msg_blen;
	uint32_t b0_flags = 0U;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	LTRACEF("NONCE len: %u, TAG len: %u, AAD len: %u, MSG len: %u\n",
		nonce_blen, tag_blen, aad_blen, msg_blen);

	ret = ccm_encode_b0_flags(&b0_flags, nonce_blen,
				  tag_blen, (aad_blen > 0U),
				  msg_blen);
	CCC_ERROR_CHECK(ret);

	b0_block[0U] = BYTE_VALUE(b0_flags);

	if ((nonce_blen < CCM_NONCE_MIN_LEN) ||
	    (nonce_blen > CCM_NONCE_MAX_LEN)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	for (inx = 0U; inx < nonce_blen; inx++) {
		b0_block[1U + inx] = nonce[inx];
	}

	for (inx = 0U; inx < (15U - nonce_blen); inx++) {
		uint32_t tmp32 = se_util_lsw_u64((Q >> (inx * 8U)));;
		b0_block[15U - inx] = BYTE_VALUE(tmp32);
	}

	LOG_HEXBUF("AES-CCM encoded B0 block\n", b0_block, 16U);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief Msg length fitting q already verified in B0 encoding,
 * not done here.
 *
 * CTR0 block "Flags" octet format (byte 0 of big endian CTR0 block):
 *
 * +----------------------------------------------------------------------------+
 * | Bit number|  7     | 6        | 5     | 4     | 3     | 2     | 1     | 0  |
 * +----------------------------------------------------------------------------+
 * | Content   |reserved| reserved |  0    | 0     | 0     |      q-1           |
 * +----------------------------------------------------------------------------+
 *
 * Bit 7 => always zero
 * Bit 6 => always zero
 *
 * Bits [5..3] => always zero
 * Bits [2..0] => one of the valid values of element q above (same as in B0 flags)
 */
static status_t ccm_encode_ctr0_flags(uint32_t *flags_p,
				      uint32_t nonce_blen)
{
	status_t ret = NO_ERROR;
	uint32_t q = 0U;

	LTRACEF("entry\n");

	if (NULL == flags_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!AES_CCM_NONCE_LEN_VALID(nonce_blen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CCM NONCE length invalid %u\n", nonce_blen));
	}

	q = (15U - nonce_blen) - 1U;

	*flags_p = (q << FLAGS_Q_SHIFT);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief CTR0 block format (16 bytes):
 *
 * +----------------------------------------------------+
 * | Octet # |  0     |  [1 .. (15-q)] | [(16-q) .. 15] |
 * +----------------------------------------------------+
 * | Content | Flags  |      N         |      Q         |
 * +----------------------------------------------------+
 *
 * Q contains zero value in CTR0.
 *
 * How much does Q have to increment in AES-CTR mode =>
 * CTRm is last counter where m = CEIL(Plen / 128) =>
 * increment by 1 for every 128 bit block.
 */
static status_t ccm_encode_ctr0(uint8_t *ctr0_block,
				const uint8_t *nonce,
				uint32_t nonce_blen)
{
	status_t ret = NO_ERROR;
	uint32_t ctr0_flags = 0U;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if ((NULL == ctr0_block) || (NULL == nonce)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ccm_encode_ctr0_flags(&ctr0_flags, nonce_blen);
	CCC_ERROR_CHECK(ret);

	ctr0_block[0U] = BYTE_VALUE(ctr0_flags);

	for (inx = 0U; inx < nonce_blen; inx++) {
		ctr0_block[1U + inx] = nonce[inx];
	}

	for (inx = 0U; inx < (15U - nonce_blen); inx++) {
		ctr0_block[15U - inx] = 0U;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Encode AAD length to aad_lenbuf.
 * The length of encoding depends on the AAD_BLEN64 value.
 *
 * Function returns the number of bytes used in encoding,
 * the specs set the AAD length encoding limits as follows:
 *
 * (0)          <  a < (2^16-2^8) => two bytes, no prefix
 * (2^16 - 2^8) <= a < (2^32)     => six bytes, prefix: 0xFF || 0xFE
 * (2^32)       <= a < (2^64)     => ten bytes, prefix: 0xFF || 0xFF
 *
 * Note that zero length AAD is not encoded at all; bit 6
 * in B0 flags is zero if there is no AAD.
 */
static uint32_t ccm_encode_aad_length(uint8_t *aad_lenbuf,
				      uint64_t aad_blen64)
{
	uint32_t albytes = 0U;
	uint64_t alen = aad_blen64;
	uint32_t inx = 0U;
	uint32_t prefix = 0U;
	uint32_t aad_len = 0U;

	if (aad_blen64 > 0ULL) {
		if (aad_blen64 < AAD_LEN_TWOBYTE_LIMIT) {
			albytes = 2U;
		} else {
			prefix  = 2U;
			aad_lenbuf[0] = 0xFFU;

			if (((aad_blen64 >> 32U) & 0xFFFFFFFFUL) == 0UL) {
				albytes = 4U;
				aad_lenbuf[1] = 0xFEU;
			} else {
				albytes = 8U;
				aad_lenbuf[1] = 0xFFU;
			}
		}

		aad_len = prefix + albytes;
		for (inx = 1U; inx <= albytes; inx++) {
			/* prefix is either 0 or 2. albytes is 2, 4 or 8.
			 * Never wrap */
			uint32_t alen32 = se_util_lsw_u64(alen);
			uint32_t aad_inx = aad_len - inx;
			aad_lenbuf[aad_inx] = BYTE_VALUE(alen32);
			alen = (alen >> 8U);
		}
	}

	return (aad_len);
}

static status_t ccm_zero_pad_wblock(uint8_t *wbuf, uint32_t *offset_p)
{
	status_t ret = NO_ERROR;
	uint32_t offset = *offset_p;
	uint32_t unaligned = offset % SE_AES_BLOCK_LENGTH;

	LTRACEF("entry\n");

	LTRACEF("ZPAD: offset %u, unaligned %u\n", offset, unaligned);
	LOG_HEXBUF("ZPAD(before)", &wbuf[offset], 16);

	if (0U != unaligned) {
		uint32_t inx = 0U;
		uint32_t zero_pad = SE_AES_BLOCK_LENGTH - unaligned;
		uint32_t offset_padded = 0U;

		offset_padded = offset + zero_pad;
		/* check if within the range or wrap */
		if ((CCC_AES_CCM_WORK_BUFFER_SIZE <= offset_padded) || (offset_padded < offset)) {
			CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
		}

		for (; inx < zero_pad; inx++) {
			/* "offset + zero_pad" has been checked as offset_padded */
			wbuf[offset + inx] = 0U;
		}

		*offset_p = offset_padded;
	}

	LOG_HEXBUF("ZPAD(after)", &wbuf[offset], 16);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_append_aad(struct se_aes_context *aes_context, uint32_t *offset_p)
{
	status_t ret = NO_ERROR;

	uint32_t aad_count = 0U;
	uint32_t offset = *offset_p;

	LTRACEF("entry\n");

	aad_count = ccm_encode_aad_length(&aes_context->ec.ccm.wbuf[SE_AES_BLOCK_LENGTH],
					  (uint64_t)aes_context->ccm.aad_len);

	if ((aes_context->ccm.aad_len >= CCC_AES_CCM_WORK_BUFFER_SIZE) ||
	    (aad_count >= CCC_AES_CCM_WORK_BUFFER_SIZE) ||
	    (offset >= CCC_AES_CCM_WORK_BUFFER_SIZE) ||
	    (CCC_AES_CCM_WORK_BUFFER_SIZE <= (offset + (aad_count + aes_context->ccm.aad_len)))) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	offset += aad_count;

	se_util_mem_move(&aes_context->ec.ccm.wbuf[offset],
			 aes_context->ccm.aad,
			 aes_context->ccm.aad_len);

	offset += aes_context->ccm.aad_len;

	ret = ccm_zero_pad_wblock(&aes_context->ec.ccm.wbuf[0], &offset);
	CCC_ERROR_CHECK(ret);

	*offset_p = offset;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static void aes_econtext_release(struct se_engine_aes_context *aes_ectx)
{
	if (NULL != aes_ectx) {
		aes_ectx->se_ks.ks_keydata = NULL;
		CMTAG_MEM_RELEASE(aes_ectx->cmem,
				  CMTAG_ECONTEXT,
				  aes_ectx);
	}
}

/* Callwlate AES-CCM tag with CBC-MAC from data in aes_context->ccm.wbuf
 */
static status_t ccm_callwlate_tag(struct se_aes_context *aes_context,
				  uint32_t data_len,
				  uint8_t *tag)
{
	status_t ret = NO_ERROR;
	struct se_engine_aes_context *aes_ectx = NULL;
	struct se_data_params input;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	LTRACEF("XXX AES-CCM callwlating CBC-MAC for WBUF\n");
	LOG_HEXBUF("CBC-MAC src", aes_context->ec.ccm.wbuf, data_len);

	aes_ectx = CMTAG_MEM_GET_OBJECT(aes_context->ec.cmem,
					CMTAG_ECONTEXT,
					0U,
					struct se_engine_aes_context,
					struct se_engine_aes_context *);
	if (NULL == aes_ectx) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Engine code for CBC-MAC and CMAC-AES is identical with the
	 * current HW: the last block is not processed by HW.
	 *
	 * So not necessary to implement specific CBC-MAC support,
	 * use the CMAC-AES engine code as is.
	 */
	ret = aes_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY,
						TE_ALG_CMAC_AES,
						aes_ectx,
						aes_context->ec.cmem);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to init AES ectx for CBC-MAC\n"));

	engine = aes_ectx->engine;
	aes_ectx->is_last = true;

	/* When a MAC is callwlated with DST == NULL the result will
	 * be left in ectx->aes_mac.phash
	 */
	input.src = aes_context->ec.ccm.wbuf;
	input.input_size = data_len;
	input.dst = NULL;
	input.output_size = 0U;

	/* Copy key setup from caller engine context
	 */
	CCC_OBJECT_ASSIGN(aes_ectx->se_ks, aes_context->ec.se_ks);

	/* If caller wants to leave the key in keyslot
	 * leave it there in all cases.
	 */
	aes_ectx->aes_flags |= (aes_context->ec.aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT);

	if ((aes_context->ec.aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		uint32_t key_flags = AES_FLAGS_USE_PRESET_KEY;

		/* If caller does not want to leave it, tag
		 * callwlation must anyway leave it there for the
		 * ENCRYPT because encryption is done after tag
		 * generation (can not set the key value for it
		 * later since it only exists in keyslot).
		 *
		 * On decipher the tag callwlation must erase the key,
		 * since it is done after deciphering (unless caller
		 * leaves it, as set above).
		 */
		if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
			key_flags |= AES_FLAGS_LEAVE_KEY_TO_KEYSLOT;
		}

		aes_ectx->aes_flags |= key_flags;
	}

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = engine_cmac_process_block_locked(&input, aes_ectx);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret,
			LTRACEF("AES-CCM CBC_MAC failed: %d\n", ret));

	se_util_mem_move (tag,
			  (uint8_t *)&aes_ectx->aes_mac.phash[0],
			  aes_context->ec.ccm.tag_len);

	LTRACEF("AES-CCM tag length %u (first bytes of the following value)\n",
		aes_context->ec.ccm.tag_len);

	LOG_HEXBUF("AES-CCM callwlated TAG", tag, aes_context->ec.ccm.tag_len);

fail:
	aes_econtext_release(aes_ectx);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Write CCM CTR0 to context buf (DMA aligned) for S0 callwlation.
 * CTR0 used now => increments to CTR1.
 *
 * s0 = aes_ecb_encrypt_block(CTR0)
 */
static status_t ccm_callwlate_s0(struct se_aes_context *aes_context,
				 uint8_t *s0)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	se_util_mem_move(&aes_context->buf[0],
			 (const uint8_t *)&aes_context->ec.ccm.counter[0],
			 SE_AES_BLOCK_LENGTH);

	LOG_HEXBUF("XXX CTR0", &aes_context->buf[0], SE_AES_BLOCK_LENGTH);

	/* AES-ECB cipher CTR0 in place in context buf => S0
	 */
	ret = aes_ecb_encrypt_block(&aes_context->ec,
				    &aes_context->buf[0],
				    &aes_context->buf[0]);
	CCC_ERROR_CHECK(ret);

	se_util_mem_move(s0, &aes_context->buf[0], SE_AES_BLOCK_LENGTH);

	LOG_HEXBUF("XXX AES-CCM S0", s0, SE_AES_BLOCK_LENGTH);

	/* CCM counter increment CTR0 => CTR1
	 */
	ret = se_update_linear_counter(&aes_context->ec, 1U);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_setup_check_args(const struct se_data_params *input_params,
				     const struct se_aes_context *aes_context,
				     const uint8_t * s0)
{
	status_t ret = NO_ERROR;
	uint32_t payload_len = 0U;

	LTRACEF("entry\n");

	if ((NULL == aes_context) || (NULL == input_params) ||
	    (NULL == s0)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	payload_len = input_params->input_size;

	if (TE_ALG_AES_CCM != aes_context->ec.algorithm) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (NULL == aes_context->ccm.nonce) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		if (payload_len < aes_context->ec.ccm.tag_len) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_set_input_data(const struct se_data_params *input_params,
				   struct se_aes_context *aes_context,
				   uint32_t *offset_p)
{
	status_t ret = NO_ERROR;
	uint32_t buffer_size = 0U;
	uint32_t offset = 0U;

	LTRACEF("entry\n");

	offset = *offset_p;

	if (input_params->input_size > 0U) {
		CCC_SAFE_ADD_U32(buffer_size, input_params->input_size, offset);
		if (CCC_AES_CCM_WORK_BUFFER_SIZE <= buffer_size) {
			CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
		}

		/* Includes also the PROCESSED TAG if this is decipher
		 */
		se_util_mem_move(&aes_context->ec.ccm.wbuf[offset],
				 input_params->src,
				 input_params->input_size);

		CCC_SAFE_ADD_U32(offset, offset, input_params->input_size);

		if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
			ret = ccm_zero_pad_wblock(&aes_context->ec.ccm.wbuf[0], &offset);
			CCC_ERROR_CHECK(ret);
		}
	}

	*offset_p = offset;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_construct_ctr0_and_s0(struct se_aes_context *aes_context,
					  uint8_t *s0)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = ccm_encode_ctr0((uint8_t *)aes_context->ec.ccm.counter,
			      aes_context->ccm.nonce,
			      aes_context->ec.ccm.nonce_len);
	CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM ctr encoding failed\n"));

	LOG_HEXBUF("AES-CCM ctr0", aes_context->ec.ccm.counter, 16U);

	ret = ccm_callwlate_s0(aes_context, s0);
	CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM S0 callwlation failed\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief Construct as much of CCM data to WBUF (aes_context->ec.ccm.wbuf)
 * as possible for further processing by caller.
 *
 * Value of various CCM meta-blocks are also constructed by this
 * call: S0, B0, CTR0 blocks for CCM value callwlation.
 *
 * For encrypt the data placed to WBUF includes vector B0, AAD data +
 * AAD padding and cleartext + padding.
 *
 * For decrypt the data placed to WBUF includes vector B0, AAD data +
 * AAD padding and ciphertext and TAG value and padding.
 *
 * CTR0 counter is constructed to aes_context->ec.ccm.counter buffer.
 * S0 vector is constructed to s0 parameter buffer.
 *
 * AAD data buffer ref and length are in aes_context->ccm object.
 *
 * @param input_params input/output data buffers and lengths
 * @param aes_context AES-CCM context with the caller setup values
 * @param wbuf_len_p 16 byte aligned wbuf data length
 * @param aad_len_p 16 byte aligned AAD data length
 * @param s0 constructed S0 vector value
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t ccm_setup(const struct se_data_params *input_params,
			  struct se_aes_context *aes_context,
			  uint32_t *wbuf_len_p,
			  uint32_t *aad_len_p,
			  uint8_t *s0)
{
	status_t ret = NO_ERROR;
	uint32_t offset = 0U;
	uint32_t payload_len = 0U;

	LTRACEF("entry\n");

	ret = ccm_setup_check_args(input_params, aes_context, s0);
	CCC_ERROR_CHECK(ret);

	payload_len = input_params->input_size;

	if (!BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		/* Processed tag is at end of ciphertext, but it must not
		 * be deciphered.
		 */
		CCC_SAFE_SUB_U32(payload_len, payload_len,  aes_context->ec.ccm.tag_len);
	}

	*aad_len_p = 0U;

	ret = ccm_encode_b0(aes_context->ec.ccm.wbuf,
			    aes_context->ccm.nonce,
			    aes_context->ec.ccm.nonce_len,
			    aes_context->ec.ccm.tag_len,
			    aes_context->ccm.aad_len,
			    payload_len);
	CCC_ERROR_CHECK(ret,
			LTRACEF("B0 encoding failed\n"));

	offset = SE_AES_BLOCK_LENGTH;

	/* Append encoded AAD to WBUF if present
	 */
	if (aes_context->ccm.aad_len > 0U) {
		uint32_t aad_start = offset;
		uint32_t data_len = 0U;

		ret = ccm_append_aad(aes_context, &offset);
		CCC_ERROR_CHECK(ret, LTRACEF("Failed to handle AAD\n"));

		CCC_SAFE_SUB_U32(data_len, offset, aad_start);
		*aad_len_p = data_len;
	}

	// XXX FIXME: Now assumes all data fits in WBUF!!!!
	// XXX ASSUMPTION FOR FIRST SIMPLE IMPLEMENTATION =>
	// XXX NOT TRUE LATER => FIXME

	ret = ccm_set_input_data(input_params, aes_context, &offset);
	CCC_ERROR_CHECK(ret);

	LTRACEF("AES-CCM data len %u (with B0 block)\n", offset);
	LOG_HEXBUF("AES-CCM wbuf data", aes_context->ec.ccm.wbuf, offset);

	*wbuf_len_p = offset;

	/* RUN CBC-MAC with WBUF (without additional length encodings, length already in B0
	 * and payload length in front of payload)
	 *
	 * mac = CBC_MAC(wbuf)
	 */

	/* Construct CTR0 and S0
	 */
	ret = ccm_construct_ctr0_and_s0(aes_context, s0);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* functions for processing AES-CCM */

static status_t aes_ccm_process_check_args_cipher(
	const struct se_data_params *input_params,
	const struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint32_t output_size = 0U;

	LTRACEF("entry\n");

	/* Ciphered output size is length(input) + length(tag)
	 * Also with NULL payload the TAG is output to DST.
	 */
	CCC_SAFE_ADD_U32(output_size, input_params->input_size, aes_context->ec.ccm.tag_len);
	if ((NULL == input_params->dst) ||
	    (input_params->output_size < output_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-CCM output buffer too small\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_ccm_process_check_args_decipher(
	const struct se_data_params *input_params,
	const struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Input can not be NULL. It contains at least the TAG at end. */
	if ((NULL == input_params->src) ||
	    (input_params->input_size < aes_context->ec.ccm.tag_len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-CCM input invalid\n"));
	}

	if (input_params->input_size > aes_context->ec.ccm.tag_len) {
		uint32_t pload_len = input_params->input_size - aes_context->ec.ccm.tag_len;

		if ((NULL == input_params->dst) ||
		    (input_params->output_size < pload_len)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AES-CCM output buffer too small\n"));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_ccm_process_check_buffer(const struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/*
	 * XXXX Arbitrary constant buffer size used for CCM work buffer size!!!
	 * XXXX The current size is large enough for all presented requirements.
	 *
	 * XXXX For generic support => TODO: Add support for sliced processing.
	 * XXXX AES-MAC engine and AES-CTR engine both support that already,
	 * XXXX implement if required.
	 */
	if ((NULL == aes_context->ec.ccm.wbuf) ||
	    (aes_context->ec.ccm.wbuf_size < CCC_AES_CCM_WORK_BUFFER_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AES-CCM work buffer not set\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_ccm_process_check_args(
	const struct se_data_params *input_params,
	const struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	if ((NULL == aes_context) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == input_params->src) && (input_params->input_size > 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		ret = aes_ccm_process_check_args_cipher(input_params, aes_context);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = aes_ccm_process_check_args_decipher(input_params,
							  aes_context);
		CCC_ERROR_CHECK(ret);
	}

	if (!BOOL_IS_TRUE(aes_context->ec.is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AES-CCM does not support update calls\n"));
	}

	ret = aes_ccm_process_check_buffer(aes_context);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief Append (TAG xor S0) to the end of encrypted data on cipher =>
 * first length(TAG) bytes of the value.
 *
 * S0  is in s0
 * TAG is in tag
 *
 * PROCESSED_TAG = s0 XOR TAG
 */
static status_t ccm_append_processed_tag(struct se_aes_context *aes_context,
					 uint32_t *data_len_p,
					 const uint8_t *s0,
					 const uint8_t *tag)
{
	status_t ret = NO_ERROR;
	uint32_t tag_offset = *data_len_p;
	uint32_t aad_padded_len_extend = 0U;
	uint32_t data_len = 0U;
	uint32_t inx = 0U;

	/* For FI */
	*data_len_p = 0U;

	LTRACEF("entry: TAG OFFSET %u\n", tag_offset);

	LOG_HEXBUF("CCM WBUF", &aes_context->ec.ccm.wbuf[0], tag_offset);

	data_len = aes_context->ec.ccm.tag_len + tag_offset;

	/* check if within the range or wrap */
	if ((CCC_AES_CCM_WORK_BUFFER_SIZE <= data_len) || (data_len < tag_offset)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	for (; inx < aes_context->ec.ccm.tag_len; inx++) {
		/* data_len is tag_len + tag_offset. It has been checked. */
		aes_context->ec.ccm.wbuf[tag_offset + inx] =
					BYTE_VALUE((uint32_t)s0[inx] ^ tag[inx]);
	}

	FI_BARRIER(u32, inx);

	if (inx != aes_context->ec.ccm.tag_len) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	CCC_SAFE_ADD_U32(aad_padded_len_extend, \
			 SE_AES_BLOCK_LENGTH, aes_context->ec.ccm.aad_padded_len);
	CCC_SAFE_SUB_U32(data_len, data_len, aad_padded_len_extend);
	*data_len_p = data_len;

	LOG_HEXBUF("CCM WBUF+t^s0",
		   &aes_context->ec.ccm.wbuf[0], tag_offset + aes_context->ec.ccm.tag_len);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief Check that the TAG is correct on decipher.
 * Return error if not.
 */
static status_t ccm_compare_tag(struct se_aes_context *aes_context,
				const uint8_t *decoded_tag,
				const uint8_t *tag)
{
	status_t ret = ERR_ILWALID_ARGS;
	uint32_t tag_len = 0U;
	status_t rbad = ERR_BAD_STATE;
	bool match = false;

	LTRACEF("entry\n");

	tag_len = aes_context->ec.ccm.tag_len;

	if (!AES_CCM_TAG_LEN_VALID(tag_len) || (NULL == decoded_tag) || (NULL == tag)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	match = se_util_vec_match(decoded_tag, tag, tag_len,
				  CFALSE, &rbad);
	if (BOOL_IS_TRUE(match)) {
		ret = NO_ERROR;
	}

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = SE_ERROR(ERR_BAD_STATE);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_verify_tag(uint32_t text_len,
			       struct se_aes_context *aes_context,
			       uint32_t *data_len_p,
			       const uint8_t *block,
			       uint8_t *tag)
{
	status_t ret = NO_ERROR;
	uint32_t data_len = *data_len_p;

	LTRACEF("entry\n");

	/* For FI */
	*data_len_p = 0U;

	/* ciphertext/plaintext length from client used.
	 *
	 * CIPHERTEXT includes the tag value on decipher.
	 *
	 * Zero the TAG and the remaining of the AES block for TAG callwlation
	 */
	ret = ccm_callwlate_tag(aes_context, data_len, &tag[0]);
	CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM tag callwlation failed on encrypt\n"));

	/* xored tag dropped from text length
	 */
	data_len = text_len - aes_context->ec.ccm.tag_len;

	ret = ccm_compare_tag(aes_context, &block[0], &tag[0]);
	CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM tag verify on decipher\n"));

	LTRACEF("Post processed length: %u\n", data_len);

	FI_BARRIER(u32, data_len);

	*data_len_p = data_len;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/** @brief Copy the data out to caller:
 *  cipher => ciphertext + (TAG xor S0)
 *  decipher => plaintext (only after TAG validation succeeds)
 */
static status_t ccm_handle_result(struct se_data_params *input_params,
				  struct se_aes_context *aes_context,
				  uint32_t result_len)
{
	status_t ret = NO_ERROR;
	uint32_t offset = 0U;
	uint32_t min_len = aes_context->ec.ccm.tag_len;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		/* Decipher input_size includes tag at end */
		min_len = input_params->input_size - aes_context->ec.ccm.tag_len;
	}

	if (result_len < min_len) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS, LTRACEF("AES-CCM output too short\n"));
	}

	if (result_len > input_params->output_size) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("AES-CCM output does not fit in output buffer\n"));
	}

	CCC_SAFE_ADD_U32(offset, SE_AES_BLOCK_LENGTH, aes_context->ec.ccm.aad_padded_len);

	LOG_HEXBUF("XXX CCM data", &aes_context->ec.ccm.wbuf[offset], result_len);

	se_util_mem_move(input_params->dst,
			 &aes_context->ec.ccm.wbuf[offset],
			 result_len);

	input_params->output_size = result_len;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* @brief In place AES-CTR process lock aligned payload_len data in
 * wbuf (both length and address are block aligned).
 */
static status_t ccm_aes_process_data(struct se_aes_context *aes_context,
				     uint32_t payload_offset,
				     uint32_t payload_len)
{
	status_t ret = NO_ERROR;
	struct se_data_params ccm_input;
	uint8_t *data = &aes_context->ec.ccm.wbuf[payload_offset];

	LTRACEF("entry\n");

	LTRACEF("AES-CCM payload %p, payload_len: %u, payload_offset: %u, tag_len : %u\n",
		data, payload_len, payload_offset, aes_context->ec.ccm.tag_len);

	if ((payload_len % SE_AES_BLOCK_LENGTH) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	if (payload_len > 0U) {
		uint32_t saved_flags = aes_context->ec.aes_flags;

		ccm_input.src = data;
		ccm_input.input_size = payload_len;

		ccm_input.dst = data;
		ccm_input.output_size = ccm_input.input_size;

		/* If using pre-existing keyslot value on AES-CCM
		 * decipher, the key must be left in place because TAG
		 * validation needs it.
		 *
		 * This requires special handling on errors if the
		 * caller's intention is not to leave the key in
		 * place when AES-CCM completes.
		 */
		if (!BOOL_IS_TRUE(aes_context->ec.is_encrypt) &&
		    ((aes_context->ec.aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U)) {
			aes_context->ec.aes_flags |= AES_FLAGS_LEAVE_KEY_TO_KEYSLOT;
		}

		/* AES-CCM cipher/decipher TEXT in wbuf (in place)
		 * This is using AES HW engine in CTR mode.
		 */
		ret = se_aes_process_input(&ccm_input, aes_context);

		aes_context->ec.aes_flags = saved_flags;

		CCC_ERROR_CHECK(ret);

		LOG_HEXBUF("XXXX AES CCM aes out", ccm_input.dst, ccm_input.output_size);
	} else {
		LTRACEF("AES-CCM no data to pass through AES\n");
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Erase key in case there is a sequence error when key even could be
 * in keyslot due to error retrun (unless caller wants to leave it there).
 *
 * CCM uses two "engine step" operations, one with AES-CTR and one
 * with AES-MAC both using the same key.
 *
 * When using pre-existing key (not programmed by SW) then the normal
 * code paths take care of erasing a keyslot if key is not meant to be
 * left there; but on error cases it needs to be explicitly erased
 * when a sequence error oclwrs.
 *
 * The keyslot erase considered on error cases when returning from AES-CCM.
 */
static void cleanup_ccm_key_on_seq_error(bool seq_error, struct se_aes_context *aes_context)
{
	if (BOOL_IS_TRUE(seq_error) && (NULL != aes_context)) {
		if ((aes_context->ec.aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U) {
			(void)se_clear_device_aes_keyslot(aes_context->ec.engine->e_dev->cd_id,
							  aes_context->ec.se_ks.ks_slot);
		}
	}
}

static status_t ccm_process_encrypt(struct se_aes_context *aes_context,
				    uint8_t *tag, uint32_t wbuf_len,
				    bool *seq_error_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		LTRACEF("XXX CCM encrypt: CBC-MAC data len %u (padded)\n",
			wbuf_len);

		if ((wbuf_len % SE_AES_BLOCK_LENGTH) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}

		*seq_error_p = true;

		ret = ccm_callwlate_tag(aes_context, wbuf_len, &tag[0]);
		CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM tag callwlation failed on encrypt\n"));
	} else {
		LTRACEF("XXX CCM decrypt: data len %u (with processed tag)\n",
			wbuf_len);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_process_data(struct se_aes_context *aes_context,
				 uint32_t *process_len_p,
				 uint32_t *tag_offset_p,
				 uint8_t *tag_decoded, uint8_t *s0,
				 uint32_t input_size,
				 uint32_t payload_offset)
{
	status_t ret = NO_ERROR;
	uint32_t process_len = 0U;
	uint32_t tag_offset = 0U;
	uint32_t buf_offset = 0U;

	LTRACEF("entry\n");

	process_len = *process_len_p;

	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		/* For encrypt: tag after the payload
		 *
		 * process_len is already set to end of plaintext padding.
		 */
		CCC_SAFE_ADD_U32(tag_offset, payload_offset, input_size);
	} else {
		uint32_t inx = 0U;
		uint32_t padded_dlen = 0U;

		/* For decipher: the input_size includes the processed tag.
		 */
		CCC_SAFE_ADD_U32(tag_offset, payload_offset, input_size);
		CCC_SAFE_SUB_U32(tag_offset, tag_offset,
				 aes_context->ec.ccm.tag_len);

		for (; inx < aes_context->ec.ccm.tag_len; inx++) {
			CCC_SAFE_ADD_U32(buf_offset, tag_offset, inx);
			tag_decoded[inx] =
				BYTE_VALUE((uint32_t)s0[inx] ^ aes_context->ec.ccm.wbuf[buf_offset]);
		}

		LTRACEF("CCM decipher tag offset %u, length %u\n", tag_offset, aes_context->ec.ccm.tag_len);
		LOG_HEXBUF("decipher: TAG after decoding", tag_decoded, aes_context->ec.ccm.tag_len);

		padded_dlen = tag_offset;

		/* Zero pad ciphertext, if required clearing the TAG for CBC-MAC
		 */
		ret = ccm_zero_pad_wblock(&aes_context->ec.ccm.wbuf[0], &padded_dlen);
		CCC_ERROR_CHECK(ret);

		/* Tag is not passed through AES or CBC-MAC.
		 */
		CCC_SAFE_SUB_U32(process_len, padded_dlen, payload_offset);
	}

	*process_len_p = process_len;
	*tag_offset_p = tag_offset;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_postprocess_data(struct se_aes_context *aes_context,
				     uint8_t *tag_decoded, uint8_t *s0,
				     uint8_t *tag, uint32_t *process_len_p,
				     uint32_t input_size)
{
	status_t ret = NO_ERROR;
	uint32_t process_len = *process_len_p;
	uint32_t total_len = 0U;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {
		ret = ccm_append_processed_tag(aes_context, &process_len,
					       &s0[0], &tag[0]);
		CCC_ERROR_CHECK(ret);

		/* xored tag added to plaintext, verify data length is consistent.
		 */
		CCC_SAFE_ADD_U32(total_len, input_size,
				 aes_context->ec.ccm.tag_len);
		if (process_len != total_len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
	} else {
		/* Zero pad deciphered data for CBC-MAC.
		 */
		ret = ccm_zero_pad_wblock(&aes_context->ec.ccm.wbuf[0], &process_len);
		CCC_ERROR_CHECK(ret);

		ret = ccm_verify_tag(input_size, aes_context,
				     &process_len, &tag_decoded[0], &tag[0]);
		CCC_ERROR_CHECK(ret);

		/* xored tag dropped from plaintext, verify data length.
		 * (it is correct unless something odd happens)
		 */
		if (process_len != (input_size - aes_context->ec.ccm.tag_len)) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
	}

	*process_len_p = process_len;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ccm_process_input_start(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context,
	uint8_t *s0, uint8_t *tag,
	uint32_t *wbuf_len_p,
	bool *seq_error_p)
{
	status_t ret = NO_ERROR;
	uint32_t wbuf_len = 0U;

	LTRACEF("entry\n");

	if (NULL == wbuf_len_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ccm_setup(input_params, aes_context, &wbuf_len,
			&aes_context->ec.ccm.aad_padded_len, s0);
	CCC_ERROR_CHECK(ret, LTRACEF("AES-CCM setup failed\n"));

	if ((aes_context->ec.ccm.aad_padded_len % SE_AES_BLOCK_LENGTH) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AAD padding failed, len %u",
					     aes_context->ec.ccm.aad_padded_len));
	}

	LTRACEF("XXX CCM aad padded len %u\n", aes_context->ec.ccm.aad_padded_len);

	ret = ccm_process_encrypt(aes_context, tag, wbuf_len, seq_error_p);
	CCC_ERROR_CHECK(ret);

	*wbuf_len_p = wbuf_len;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static void ccm_clean_context(struct se_aes_context *aes_context,
			      uint32_t wbuf_len)
{
	if ((NULL != aes_context) && (NULL != aes_context->ec.ccm.wbuf) &&
	    (wbuf_len > 0U)) {
		se_util_mem_set(aes_context->ec.ccm.wbuf, 0U, wbuf_len);
	}
}

/**
 * @brief Code assumes WBUF is large enough to hold data added to it.
 *
 * This is only true for the restricted use case lwrrently defined.
 *
 * TODO: Process data in chunks if too big. Note that deciphered
 * data can not be passed to client before tag validated
 * successfully!
 */
status_t se_aes_ccm_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint32_t wbuf_len = 0U;
	uint32_t payload_offset = 0U;
	uint8_t tag[SE_AES_BLOCK_LENGTH] = { 0x0U };
	uint8_t s0[SE_AES_BLOCK_LENGTH] = { 0x0U };
	uint8_t tag_decoded[SE_AES_BLOCK_LENGTH] = { 0x0U };
	uint32_t process_len = 0U;
	uint32_t tag_offset = 0U;
	bool seq_err = false;

	LTRACEF("entry\n");

	ret = aes_ccm_process_check_args(input_params, aes_context);
	CCC_ERROR_CHECK(ret);

	/* CCM only support a sigle dofinal operation (single shot
	 * operation); thus CCM can be set up this late.
	 *
	 * Save aad block padded len to find the ciphered/deciphered
	 * data start in wbuf
	 */
	ret = ccm_process_input_start(input_params, aes_context, &s0[0], &tag[0],
				      &wbuf_len, &seq_err);
	CCC_ERROR_CHECK(ret);

	/* CT or PT start offset is after block padded AAD data.
	 *
	 * For encryption wbuf_len includes possible zero padding of
	 * plaintext (size aligned).
	 *
	 * For decryption wbuf_len includes tag length (size possibly
	 * unaligned).
	 */
	CCC_SAFE_ADD_U32(payload_offset, SE_AES_BLOCK_LENGTH, \
			 aes_context->ec.ccm.aad_padded_len);

	CCC_SAFE_SUB_U32(process_len, wbuf_len, payload_offset);

	ret = ccm_process_data(aes_context, &process_len, &tag_offset,
			       &tag_decoded[0], &s0[0],
			       input_params->input_size, payload_offset);
	CCC_ERROR_CHECK(ret);

	if ((process_len % SE_AES_BLOCK_LENGTH) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Data padding failed, process length %u\n",
					     process_len));
	}

	LTRACEF("CCM wbuf len: %u, aligned process_len for AES: %u\n",
		wbuf_len, process_len);

	seq_err = true;

	/* AES-CTR process payload bytes of data in wbuf@payload_offset
	 *
	 * Length of data to AES-CTR == output ciphered data length
	 *
	 * PADDED_TEXT = (B0/S0 + AAD + TEXT) - AAD - B0/S0;
	 *
	 * Processes full blocks with AES-CTR, actual data may be shorter.
	 */
	ret = ccm_aes_process_data(aes_context, payload_offset, process_len);
	CCC_ERROR_CHECK(ret);

	/* Reject excess bytes from AES-CTR output */
	process_len = tag_offset;

	ret = ccm_postprocess_data(aes_context, &tag_decoded[0], &s0[0], &tag[0],
				   &process_len, input_params->input_size);
	CCC_ERROR_CHECK(ret);

	seq_err = false;

	ret = ccm_handle_result(input_params, aes_context, process_len);
	CCC_ERROR_CHECK(ret);
fail:
	cleanup_ccm_key_on_seq_error(seq_err, aes_context);

	se_util_mem_set(s0, 0U, sizeof_u32(s0));
	se_util_mem_set(tag, 0U, sizeof_u32(tag));
	se_util_mem_set(tag_decoded, 0U, sizeof_u32(tag_decoded));
	ccm_clean_context(aes_context, wbuf_len);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CCM */
