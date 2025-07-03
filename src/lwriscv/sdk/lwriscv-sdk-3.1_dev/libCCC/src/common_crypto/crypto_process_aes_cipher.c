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

/*
 * This layer takes care of processing and buffering data when several
 * calls feed the inbound data in parts (i.e. zero or more UPDATE()
 * calls followed by one DOFINAL() call).
 *
 * Such a sequence of multiple calls is supported for digests, macs and
 * block/stream cipher functions.
 *
 * Functions in this file are called by the crypto_<md,cipher,mac>.c
 * to process the data, possibly via the crypto_ta_api.c which is dealing
 * with the TA user space addresses (if the crypto call domain is not the
 * secure OS).
 *
 * So this module deals with kernel virtual addresses only. This calls
 * the SE engine HW functions in tegra_se.c, which maps the KV addresses
 * to phys addresses before passing them to the engine.
 */

/**
 * @file   crypto_process_aes_cipher.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Processing layer for AES cipher operations
 */

#include <crypto_system_config.h>

#if HAVE_SE_AES

#include <crypto_cipher_proc.h>

#if CCC_WITH_AES_GCM
#include <aes-gcm/crypto_ops_gcm.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CMAC_AES
static void aes_ecb_encrypt_release(struct se_engine_aes_context *aes_ectx)
{
	if (NULL != aes_ectx) {
		aes_ectx->se_ks.ks_keydata = NULL;
		CMTAG_MEM_RELEASE(aes_ectx->cmem,
				  CMTAG_ECONTEXT,
				  aes_ectx);
	}
}

/* Encrypt a single aes block of data with AES-ECB
 *
 * Used e.g. by:
 * CMAC
 * GCM
 * AES-CCM (requires HAVE_CMAC_AES)
 */
status_t aes_ecb_encrypt_block(
	struct se_engine_aes_context *econtext,
	uint8_t *aes_out,
	const uint8_t *aes_in)
{
	status_t ret = NO_ERROR;

	struct se_data_params input;
	struct se_engine_aes_context *aes_ectx = NULL;
	bool caller_kslot = false;

	LTRACEF("entry\n");

	if ((NULL == aes_in) || (NULL == econtext) ||
	    (NULL == aes_out)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	DEBUG_ASSERT_PHYS_DMA_OK(aes_in, SE_AES_BLOCK_LENGTH);
	DEBUG_ASSERT_PHYS_DMA_OK(aes_out, SE_AES_BLOCK_LENGTH);

	aes_ectx = CMTAG_MEM_GET_OBJECT(econtext->cmem,
					CMTAG_ECONTEXT,
					0U,
					struct se_engine_aes_context,
					struct se_engine_aes_context *);
	if (NULL == aes_ectx) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* ECB nopad mode does not use IV and does NOT PAD */
	/* Set the CMEM ref to aes engine context from caller engine context */
	ret = aes_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY, TE_ALG_AES_ECB_NOPAD,
						aes_ectx,
						econtext->cmem);
	CCC_ERROR_CHECK(ret);

	/* Default is to use the same keyslot for this AES-ECB and the
	 * original operation. This only works if the original key can
	 * be used for it at this point of time (i.e. that the
	 * operations context was saved outside keyslot or the previous
	 * operations has completed or not started yet).
	 *
	 * E.g. with CMAC key is used for both CMAC and AES-ECB.
	 */
	CCC_OBJECT_ASSIGN(aes_ectx->se_ks, econtext->se_ks);
	caller_kslot = true;

	/* Allow the caller to use existing keys from keyslots.
	 */
	if ((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* This is used for "implicit AES operations", so do
		 * not erase the keyslot
		 */
		aes_ectx->aes_flags = (AES_FLAGS_LEAVE_KEY_TO_KEYSLOT | AES_FLAGS_USE_PRESET_KEY);
		LTRACEF("Use existing key in AES keyslot for algo implied AES-ECB operation\n");
	} else {
#if HAVE_CRYPTO_KCONFIG
		/* If subsystem supports runtime dynamic keyslot, use that instead.
		 * If that fails, use the above default.
		 */
		ret = aes_engine_get_dynamic_keyslot(econtext,
						     &aes_ectx->se_ks.ks_slot);
		if (NO_ERROR == ret) {
			caller_kslot = false;
		} else {
			LTRACEF("AES dynamic keyslot not available, using default\n");
			CCC_OBJECT_ASSIGN(aes_ectx->se_ks, econtext->se_ks);
			ret = NO_ERROR;
			LTRACEF("Use caller AES keyslot for algo implied AES-ECB operation\n");
		}
#else
		/* Use the default set above */
		LTRACEF("Use caller AES keyslot for algo implied AES-ECB operation\n");
#endif
	}

	/* If the key must be left to caller keyslot and we use that slot,
	 * do not erase that key here.
	 */
	if (BOOL_IS_TRUE(caller_kslot)) {
		aes_ectx->aes_flags |= (econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT);
	}

	aes_ectx->is_last    = true;
	aes_ectx->is_encrypt = true;

	aes_ectx->se_ks.ks_keydata = econtext->se_ks.ks_keydata;
	aes_ectx->se_ks.ks_bitsize = econtext->se_ks.ks_bitsize;

	input.src	  = aes_in;
	input.input_size  = SE_AES_BLOCK_LENGTH;
	input.dst	  = aes_out;
	input.output_size = SE_AES_BLOCK_LENGTH;

	HW_MUTEX_TAKE_ENGINE(aes_ectx->engine);
	ret = CCC_ENGINE_SWITCH_AES(aes_ectx->engine, &input, aes_ectx);
	HW_MUTEX_RELEASE_ENGINE(aes_ectx->engine);

	CCC_ERROR_CHECK(ret);

fail:
	aes_ecb_encrypt_release(aes_ectx);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES */

#if HAVE_AES_PADDING
static status_t pkcs7_pad_aes(uint8_t *block, uint32_t bytes)
{
	status_t ret = NO_ERROR;
	uint8_t pad = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	if ((NULL == block) || (bytes > SE_AES_BLOCK_LENGTH)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	pad = (uint8_t)(SE_AES_BLOCK_LENGTH - bytes);

	for (i = bytes; i < SE_AES_BLOCK_LENGTH; i++) {
		block[i] = pad;
	}

	FI_BARRIER(u32, i);

	if (SE_AES_BLOCK_LENGTH != i) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pkcs7_unpad_aes(const uint8_t *block, uint32_t *bytes_p)
{
	status_t ret = NO_ERROR;
	uint8_t pad;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	if ((NULL == block) || (NULL == bytes_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	pad = block[SE_AES_BLOCK_LENGTH - 1U];
	if ((pad <= 0U) || (pad > SE_AES_BLOCK_LENGTH)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	*bytes_p = 0U;	// set invalid pad value

	for (i = (SE_AES_BLOCK_LENGTH - (uint32_t)pad); i < (SE_AES_BLOCK_LENGTH-1U); i++) {
		if (block[i] != pad) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
		}
	}
	*bytes_p = pad;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_PADDING */

static status_t se_aes_engine_data(const engine_t *engine,
				   struct se_data_params *input,
				   struct se_engine_aes_context *econtext,
				   uint32_t *processed_p)
{
	status_t ret = NO_ERROR;
	struct se_data_params pinput;
	uint32_t bytes_left = input->input_size;
	uint32_t processed_in = 0U;
	uint32_t processed_out = 0U;

	bool is_last = econtext->is_last;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	econtext->is_last = false;

	CCC_LOOP_BEGIN {
		uint32_t bcount = 0U;

		if (input->output_size < processed_out) {
			CCC_ERROR_END_LOOP_WITH_ECODE(ERR_BAD_LEN);
		}
		pinput.src = &input->src[processed_in];
		pinput.dst = &input->dst[processed_out];
		pinput.output_size = input->output_size - processed_out;

		if (bytes_left <= MAX_AES_ENGINE_CHUNK_SIZE) {
			bcount = bytes_left;
			econtext->is_last = is_last;
		} else {
			bcount = MAX_AES_ENGINE_CHUNK_SIZE;
		}

		pinput.input_size = bcount;

		HW_MUTEX_TAKE_ENGINE(engine);
		ret = CCC_ENGINE_SWITCH_AES(engine, &pinput, econtext);
		HW_MUTEX_RELEASE_ENGINE(engine);

		CCC_ERROR_END_LOOP(ret);

		ret = CCC_ADD_U32(processed_in, processed_in, bcount);
		CCC_ERROR_END_LOOP(ret);
		bytes_left    -= bcount;
		ret = CCC_ADD_U32(processed_out, processed_out, pinput.output_size);
		CCC_ERROR_END_LOOP(ret);

		if (0U == bytes_left) {
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	*processed_p = processed_out;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Handle the contiguous chunk of data and pass it to selected
 * AES engine level handler.
 *
 * Push the contiguous data chunk through the engine level for
 * all AES cipher modes (CMAC is not handled here).
 *
 * The engine has a size limitation of 16 MB per chunk, so this
 * function auto-splits the data into suitable chunks if the chunk is
 * larger.
 *
 * @param engine Selected engine to process the request
 * @param input Normalized engine input object
 * @param econtext AES engine call context
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t se_aes_engine(const engine_t *engine,
			      struct se_data_params *input,
			      struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t processed_out = 0U;

	LTRACEF("entry\n");

	if ((NULL == input->src) || (NULL == input->dst)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("in/out buffer error\n"));
	}

	ret = se_aes_engine_data(engine, input, econtext, &processed_out);
	CCC_ERROR_CHECK(ret);

	input->output_size = processed_out;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Setup AES engine call arguments and call AES handler
 *
 * @param ctx AES process context, defines the state for the operation
 * @param in Input data buffer
 * @param inlen Input bytes for this call
 * @param out Output data buffer
 * @param outlen Space left in output buffer
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t se_aes_engine_call(struct se_aes_context *aes_context,
				   const uint8_t *in, uint32_t inlen,
				   uint8_t *out, uint32_t outlen)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	input.src = in;
	input.input_size = inlen;
	input.dst = out;
	input.output_size = outlen;

	DEBUG_ASSERT_PHYS_DMA_OK(in, inlen);

	if ((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U) {
		/* This is allowed to be <0, NULL>
		 * No DMA output to memory when target is keyslot.
		 */
		DEBUG_ASSERT_PHYS_DMA_OK(out, outlen);
	}

	ret = se_aes_engine(aes_context->ec.engine, &input, &aes_context->ec);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Pass data to AES engines and handle optional pre/postprocess
 * data for the call.
 *
 * Some algorithms require pre-processing/post-processing of the data
 * stream; this processing is handled in this function.
 *
 * One example of this is the hybrid implementation of AES-GCM
 * algorithm, which uses the AES-CTR mode engine for data ciphering
 * but HW has no support for GHASH callwlation. This function passes
 * the ciphered data (before decipher it or after ciphering plaintext)
 * to the SW implementation of GHASH to callwlate the TAG for the AE
 * mode.
 *
 * @param aes_context AES process context, defines the state for the operation
 * @param in Input data buffer
 * @param out Output data buffer
 * @param outlen Space left in output buffer
 * @param bytes Input bytes for this call
 * @param ae_bcount Number of bytes passed to pre/post processing handlers
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t do_aes_and_check(struct se_aes_context *aes_context,
				 const uint8_t *in,
				 uint8_t *out,
				 uint32_t outlen,
				 uint32_t bytes,
				 uint32_t ae_bcount)
{
	status_t ret = NO_ERROR;
	uint32_t num_bytes = bytes;

	(void)ae_bcount;

	LTRACEF("entry\n");

#if CCC_WITH_AES_GCM
	/* BYTES is the padded byte length and AE_BCOUNT is the actual
	 * number of bytes, these may differ when handling the last
	 * block. Some engines need to know the actual number
	 * (e.g. AES-GCM) other engines use the padded length.
	 */
	if (BOOL_IS_TRUE(aes_context->ec.is_last) &&
	    TE_AES_GCM_MODE(aes_context->ec.algorithm)) {
		/* HW GCM pads the last byte with zeros internally
		 * and it requires the block length to be passed
		 * in correctly (the TAG callwlation includes the
		 * message length, so the lengths must be correct).
		 */
		num_bytes = ae_bcount;
	}
#endif

	ret = se_aes_engine_call(aes_context, in, num_bytes, out, outlen);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES crypto failed: %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t check_aes_alignment_cipher(const struct se_aes_context *aes_context,
					   struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (TE_AES_MODE_PADDING(aes_context->ec.algorithm)) {
		aistate->ai_add_padding = true;
	} else {
		if (aistate->ai_unaligned_bytes != 0U) {
			/* non-padding block cipher mode
			 * (padding block ciphers handled
			 * above)
			 */
			if (TE_AES_BLOCK_CIPHER_MODE(aes_context->ec.algorithm)) {
				CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
						     CCC_ERROR_MESSAGE("Unpadded block cipher(%u) mode data not aligned, %u stray bytes\n",
								   aes_context->ec.algorithm,
								   aistate->ai_unaligned_bytes));
			}
		}

		if (BOOL_IS_TRUE(aes_context->ec.is_first) && (0U == aistate->ai_bytes_left)) {
			if (TE_AES_BLOCK_CIPHER_MODE(aes_context->ec.algorithm)) {
				/* non-padding block cipher mode, no data. */
				CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
						     CCC_ERROR_MESSAGE("Non-padding block cipher(%u) mode has no data\n",
								   aes_context->ec.algorithm));
			}
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t check_aes_alignment_decipher(const struct se_aes_context *aes_context,
					     const struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (TE_AES_BLOCK_CIPHER_MODE(aes_context->ec.algorithm)) {
		if (aistate->ai_unaligned_bytes != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     CCC_ERROR_MESSAGE("Data to decipher(%u) mode not aligned, %u stray bytes remain\n",
							   aes_context->ec.algorithm,
							   aistate->ai_unaligned_bytes));
		}

		if (BOOL_IS_TRUE(aes_context->ec.is_first) && (0U == aistate->ai_bytes_left)) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     CCC_ERROR_MESSAGE("Block decipher(algo 0x%x) mode has no data (can't unpad)\n",
							   aes_context->ec.algorithm));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Check that the last chunk data alignment is correct for the cipher mode.
 *
 * Check that the data is AES block aligned if not using a padding or streaming cipher mode.
 * Flag aistate field when padding mode used.
 *
 * @param aes_context AES process context, defines the state for the operation
 * @param aistate state of current process handler call
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t check_aes_alignment(const struct se_aes_context *aes_context,
				    struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {

		/* Ciphering */
		ret = check_aes_alignment_cipher(aes_context, aistate);
		CCC_ERROR_CHECK(ret);

	} else {

		/* Deciphering */
		ret = check_aes_alignment_decipher(aes_context, aistate);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* This function is used for both aes cipher and decipher*/
static status_t process_last_aes_data_block(struct se_aes_context *aes_context,
					    struct aes_istate *aistate,
					    const uint8_t *last_block, uint8_t *dst,
					    uint32_t outlen, uint32_t numbytes)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (numbytes > AES_BLOCK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = do_aes_and_check(aes_context, last_block, dst,
			       outlen, numbytes,
			       aistate->ai_bytes_left);
	CCC_ERROR_CHECK(ret, LTRACEF("AES crypto failed: %d\n", ret));

	if (dst != aistate->ai_input_dst) {
		LTRACEF("stream cipher 0x%x copying final %u bytes (enc=%u)\n",
			aes_context->ec.algorithm, aistate->ai_bytes_left,
			aes_context->ec.is_encrypt);
		se_util_mem_move(aistate->ai_input_dst, dst,
				 aistate->ai_bytes_left);
	}

	CCC_SAFE_ADD_U32(aes_context->dst_total_written, aes_context->dst_total_written,
			 aistate->ai_bytes_left);
	if (aistate->ai_out_len < aistate->ai_bytes_left) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	aistate->ai_out_len -= aistate->ai_bytes_left;
	aistate->ai_bytes_left = 0U;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_last_block_cipher(struct se_aes_context *aes_context,
				      struct aes_istate *aistate,
				      uint8_t *last_block)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Ciphering */

	/* Block ciphers: CBC, ECB */
	if (BOOL_IS_TRUE(aistate->ai_add_padding)) {
		/* Data may grow by up to one full AES block
		 *  when padded => dst must be able to cope
		 *  with it!
		 *
		 * Also data expansion length need to be
		 *  passed to caller => use
		 *  input_params->output_size for this.
		 */
#if HAVE_AES_PADDING
		ret = pkcs7_pad_aes(last_block, aistate->ai_bytes_left);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Ciphering with %u mode pkcs#7 padding error: %d\n",
					      aes_context->ec.algorithm, ret));

		/* DST buffer grows (SE_AES_BLOCK_LENGTH -
		 * bytes_left) longer than SRC buffer.
		 */
		aistate->ai_bytes_left = SE_AES_BLOCK_LENGTH;
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("AES PKCS#7 padding not enabled\n"));
#endif
	}

	/* bytes_left is 16 here for block ciphers and for
	 * stream ciphers it may be anything from 1..15
	 * (inclusive)
	 */
	if (aistate->ai_bytes_left > 0U) {
		uint8_t *dst      = aistate->ai_input_dst;
		uint32_t numbytes = aistate->ai_bytes_left;
		uint32_t outlen   = aistate->ai_out_len;

		/* Padding block ciphers: CBC, ECB with 16
		 * bytes or AES stream mode ciphers with 1..15
		 * bytes
		 *
		 * If done: this is the last AES operation for
		 * encrypt dofinal()
		 */
		if (BOOL_IS_TRUE(aistate->ai_stream_cipher)) {
			/* For stream ciphers this holds here:
			 * aistate->ai_bytes_left < SE_AES_BLOCK_LENGTH
			 *
			 * The last_block was already
			 * zero padded above to 16
			 * bytes; so pass that as full
			 * block AES engine and set
			 * destination to internal
			 * buffer to avoid overshooting
			 * dst (by padded bytes).
			 * Then copy the correct
			 * amount of ciphered partial
			 * block to dst.
			 */
			dst = aes_context->buf;
			numbytes = SE_AES_BLOCK_LENGTH;
			outlen = CCC_DMA_MEM_AES_BUFFER_SIZE;
		}

		ret = process_last_aes_data_block(aes_context, aistate, last_block,
						  dst, outlen, numbytes);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_last_block_decipher(struct se_aes_context *aes_context,
					struct aes_istate *aistate,
					const uint8_t *last_block,
					const uint8_t *unpad_last_full_block)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Deciphering.
	 *
	 * Data never grows when doing AES decipher; but it
	 * may shrink.
	 */

	/* Only no data remaining for block ciphers OR last
	 * block partially full for stream ciphers are
	 * possible at this point.
	 */
	if (TE_AES_BLOCK_CIPHER_MODE(aes_context->ec.algorithm)) {

		/* Length errors checked by caller */
		DEBUG_ASSERT(0U == aistate->ai_bytes_left);
		DEBUG_ASSERT(0U == aistate->ai_unaligned_bytes);

		// CBC, ECB mode with padding => strip off the pad bytes in DST
		if (TE_AES_MODE_PADDING(aes_context->ec.algorithm)) {
#if HAVE_AES_PADDING
			uint32_t pad_bytes = 0U;

			if (NULL == unpad_last_full_block) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						     LTRACEF("AES decipher pad block NULL\n"));
			}

			/* Remove padding; e.g. ADJUST amount of bytes returned
			 * to caller (remove pad bytes from last deciphered block)
			 */
			ret = pkcs7_unpad_aes(unpad_last_full_block, &pad_bytes);
			CCC_ERROR_CHECK(ret,
					CCC_ERROR_MESSAGE("Deciphering with %u mode pkcs#7 unpadding error: %d\n",
						      aes_context->ec.algorithm, ret));

			/* The caller shrinks the total byte count by this
			 * many bytes
			 */
			aistate->ai_strip_pad_bytes = pad_bytes;
#else
			(void)unpad_last_full_block;
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("AES PKCS#7 padding not enabled\n"));
#endif
		}
	} else {
		/* Stream ciphers modes should be able to
		 * deal with unaligned last bytes: CTR, OFB, ...
		 */
		uint8_t *dst      = aistate->ai_input_dst;
		uint32_t numbytes = aistate->ai_bytes_left;

		DEBUG_ASSERT(aistate->ai_bytes_left == aistate->ai_unaligned_bytes);

		if (aistate->ai_bytes_left > 0U) {
			uint32_t outlen = aistate->ai_out_len;

			DEBUG_ASSERT(aistate->ai_out_len >= aistate->ai_bytes_left);

			/*
			 * AES stream mode ciphers with 1..15 bytes
			 *
			 * If done: this is the last AES operation for encrypt dofinal()
			 */
			if (BOOL_IS_TRUE(aistate->ai_stream_cipher)) {
				if (aistate->ai_bytes_left < SE_AES_BLOCK_LENGTH) {
					/* The last_block was zero padded above
					 * to 16 bytes;
					 * so pass that as full block to engine
					 * code and set destination to internal
					 * buffer to avoid overshooting dst
					 * (by padded bytes).
					 *
					 * Then copy the correct amount of ciphered
					 * partial block to dst.
					 *
					 * Note that src is also aes_context->buf here,
					 * so decipher in place.
					 */
					dst = aes_context->buf;
					numbytes = SE_AES_BLOCK_LENGTH;
					outlen = numbytes;
				}
			}

			/*
			 * If done: this is the last AES operation for decrypt dofinal()
			 */
			ret = process_last_aes_data_block(aes_context, aistate,
							  last_block, dst,
							  outlen, numbytes);
			CCC_ERROR_CHECK(ret);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Handle the last incomplete AES block ([0..15] bytes, inclusive) for the AES context
 *
 * Deal with pkcs#7 padding when required by cipher mode.
 *
 * @param aes_context AES process context, defines the state for the operation
 * @param aistate state of current process handler call
 * @param data_param Final bytes for the AES call
 * @param unpad_last_full_block Last deciphered block written by AES engine
 *   when it needs unpadding, NULL otherwise.
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t handle_aes_last_block(struct se_aes_context *aes_context,
				      struct aes_istate *aistate,
				      const uint8_t *data_param,
				      const uint8_t *unpad_last_full_block)
{
	status_t ret = NO_ERROR;

	uint8_t last_aes_block[ SE_AES_BLOCK_LENGTH ] = { 0x00U };
	uint8_t *last_block = NULL;
	const uint8_t *data = data_param;

	LTRACEF("entry\n");

	/* Must have less than one full AES block data here */
	if (aistate->ai_bytes_left >= SE_AES_BLOCK_LENGTH) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Only unaligned bytes or padding can be handled after this
	 * point.
	 *
	 * Note: It's possible that no bytes have been handled at all
	 *       and bytes_left is zero. But only when either
	 *       aistate->ai_add_padding or using a stream cipher mode at
	 *       this point.
	 */
	if (aistate->ai_bytes_left > 0U) {
		se_util_mem_move(last_aes_block, data, aistate->ai_bytes_left);
	}

	/* Do not access this anymore (it may be aes_context->buf or
	 * something else)
	 */
	data = NULL;
	(void)data;

	/* Now aes_context->buf is free, so use it for operations on last
	 * aes block because SE engine can not access the stack
	 * variables with DMA in all elwironments.
	 */
	se_util_mem_move(aes_context->buf, last_aes_block, SE_AES_BLOCK_LENGTH);

	last_block = aes_context->buf;

	/* All original full AES data blocks are handled at this
	 * point. Length errors handled by caller; handle last block
	 * padding here if padding.
	 *
	 * Data for last block is already or will be in last_block.
	 */
	if (BOOL_IS_TRUE(aes_context->ec.is_encrypt)) {

		/* Ciphering */
		ret = aes_last_block_cipher(aes_context, aistate, last_block);
		CCC_ERROR_CHECK(ret);
	} else {
		/* Deciphering.
		 *
		 * Data never grows when doing AES decipher; but it
		 * may shrink.
		 */
		ret = aes_last_block_decipher(aes_context, aistate, last_block,
					      unpad_last_full_block);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t update_write_state(struct se_aes_context *aes_context,
				   struct aes_istate *aistate,
				   uint32_t consumed)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Next AES (if needed at all) will deal only with the last block */
	aes_context->ec.is_last	    = true;
	CCC_SAFE_ADD_U32(aes_context->dst_total_written, aes_context->dst_total_written,
			 consumed);
	if (aistate->ai_bytes_left < consumed) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	aistate->ai_bytes_left -= consumed;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t do_aes_final(struct se_aes_context *aes_context,
			     struct aes_istate *aistate,
			     const uint8_t *data_arg, uint32_t full_blocks,
			     bool is_last_op)
{
	status_t ret = NO_ERROR;
	const uint8_t *data = data_arg;
	const uint8_t *unpad_last_full_block = NULL;

	LTRACEF("entry\n");

	/* Pass all full AES blocks through the cipher
	 */
	if (full_blocks > 0U) {
		uint32_t consumed = full_blocks * SE_AES_BLOCK_LENGTH;
		aes_context->ec.is_last = is_last_op;
		ret = do_aes_and_check(aes_context, data, aistate->ai_input_dst,
				       aistate->ai_out_len, consumed, consumed);
		CCC_ERROR_CHECK(ret, LTRACEF("AES crypto failed: %d\n", ret));

		ret = update_write_state(aes_context, aistate, consumed);
		CCC_ERROR_CHECK(ret);

		if (!BOOL_IS_TRUE(aes_context->ec.is_encrypt) &&
		    TE_AES_MODE_PADDING(aes_context->ec.algorithm)) {
			/* Pass the last deciphered data block to last block handler
			 * in case it needs to be unpadded.
			 */
			unpad_last_full_block =
				&aistate->ai_input_dst[consumed - SE_AES_BLOCK_LENGTH];
		}

		aistate->ai_input_dst   = &aistate->ai_input_dst[consumed];

		if ((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U) {
			if (aistate->ai_out_len < consumed) {
				CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
			}
			aistate->ai_out_len -= consumed;
		}

		if (aistate->ai_bytes_left != 0U) {
			data = &data[consumed];
		} else {
			data = NULL; /* No fresh client to pass through the cipher.
				      * Implied by aistate->ai_bytes_left == 0U, but
				      * also mark this with NULL data to avoid coverity
				      * reports.
				      */
		}
	}

	ret = handle_aes_last_block(aes_context, aistate, data,
				    unpad_last_full_block);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_aes_final_generic(struct se_aes_context *aes_context,
					 struct aes_istate *aistate,
					 const uint8_t *data)
{
	status_t ret = NO_ERROR;
	bool is_last_op = true;
	uint32_t full_blocks = aistate->ai_bytes_left / SE_AES_BLOCK_LENGTH;

	aistate->ai_unaligned_bytes = aistate->ai_bytes_left % SE_AES_BLOCK_LENGTH;
	aistate->ai_add_padding = false;

	/* Check alignment, flag padding modes, etc */
	ret = check_aes_alignment(aes_context, aistate);
	CCC_ERROR_CHECK(ret);

	/* Set false if need to do more than one AES operations after
	 * this point of enc/dec dofinal()
	 */
	if (BOOL_IS_TRUE(aistate->ai_add_padding) || (aistate->ai_unaligned_bytes != 0U)) {
		is_last_op = false;
	}

	ret = do_aes_final(aes_context, aistate, data,
			   full_blocks, is_last_op);
	CCC_ERROR_CHECK(ret);

#if CCC_WITH_AES_GCM
	if (TE_AES_GCM_MODE(aes_context->ec.algorithm)) {
		ret = ae_gcm_finalize_tag(aes_context);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("AES-GCM failed to finalize TAG: %d\n", ret));
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}


/**
 * @brief Handle last chunk of data for the AES aes_context.
 *
 * AES dofinal() eventually enters to handle the last data chunk.
 *
 * @param aes_context AES process context, defines the state for the operation
 * @param aistate state of current process handler call
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t handle_aes_final(struct se_aes_context *aes_context,
				 struct aes_istate *aistate)
{
	status_t ret	   = NO_ERROR;
	bool	 mode_done = false;

	/* Context buffer is either empty or all input data
	 *  to handle are in aes_context->buf at this point.
	 */
	const uint8_t *data = NULL;

	LTRACEF("entry\n");

	switch(aes_context->ec.algorithm) {
#if CCC_WITH_AES_GCM
	case TE_ALG_AES_GCM:
#endif
	case TE_ALG_AES_CTR:
	case TE_ALG_AES_OFB:
		aistate->ai_stream_cipher = true;
		break;

	default:
		/* not streaming modes */
		break;
	}

	if (aes_context->used > 0U) {
		if (aistate->ai_bytes_left > 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		data = &aes_context->buf[0];
		aistate->ai_bytes_left = aes_context->used;
	} else {
		/* Note: bytes_left may also be ZERO here.
		 * src may also be NULL in case nothing is passed in
		 *  (i.e. in that case bytes_left is also zero).
		 */
		data = &aistate->ai_input_src[0];
	}

#if HAVE_AES_XTS
	if (aes_context->ec.algorithm == TE_ALG_AES_XTS) {
		uint32_t bytes_left = aistate->ai_bytes_left;

		/* XTS must pass all bytes through the engine in one
		 * operation. XTS does not pad anything and it can be
		 * unaligned (as a special case it can be bit aligned,
		 * does not even need to be byte aligned).
		 */
		ret = do_aes_and_check(aes_context, data, aistate->ai_input_dst,
				       aistate->ai_out_len, bytes_left,
				       bytes_left);
		CCC_ERROR_CHECK(ret, LTRACEF("AES crypto failed: %d\n", ret));

		aes_context->dst_total_written += bytes_left;
		aistate->ai_input_dst   = &aistate->ai_input_dst[bytes_left];
		aistate->ai_out_len    -= bytes_left;
		data = &data[bytes_left];
		aistate->ai_bytes_left = 0U;
		mode_done = true;
	}
#endif /* HAVE_AES_XTS */

	if (!BOOL_IS_TRUE(mode_done)) {
		ret = handle_aes_final_generic(aes_context, aistate, data);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_aes_update_full_blocks(struct se_aes_context *aes_context,
					      struct aes_istate *aistate,
					      uint32_t full_blocks)
{
	status_t ret = NO_ERROR;
	uint32_t consumed = 0U;

	LTRACEF("entry\n");

	CCC_SAFE_MULT_U32(consumed, full_blocks, SE_AES_BLOCK_LENGTH);

	ret = do_aes_and_check(aes_context, aistate->ai_input_src,
			       aistate->ai_input_dst,
			       aistate->ai_out_len, consumed, consumed);
	CCC_ERROR_CHECK(ret, LTRACEF("AES crypto failed: %d\n", ret));

	aistate->ai_input_src = &aistate->ai_input_src[consumed];
	aistate->ai_input_dst = &aistate->ai_input_dst[consumed];

	if ((aistate->ai_out_len < consumed) ||
	    (aistate->ai_bytes_left < consumed)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	aistate->ai_out_len  -= consumed;
	CCC_SAFE_ADD_U32(aes_context->dst_total_written,
			 aes_context->dst_total_written, consumed);
	aistate->ai_bytes_left -= consumed;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Handle intermediate chunk of data for the AES context.
 *
 * AES update() enters here when there is more data to handle.
 *
 * @param aes_context AES process context, defines the state for the operation
 * @param aistate state of current process handler call
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t handle_aes_update(struct se_aes_context *aes_context,
				  struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;
	uint32_t full_blocks = 0U;
	uint32_t consumed    = 0U;

	LTRACEF("entry\n");

	if (aistate->ai_bytes_left == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	full_blocks = aistate->ai_bytes_left / SE_AES_BLOCK_LENGTH;

	/* For an update the aes_context buffer was flushed (to AES) by
	 * caller if it was not empty and here aistate->ai_bytes_left >
	 * 0 => aes_context->buf can not contain any data here.
	 */
	if (aes_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Hold on to the last AES block if the remaining data is
	 * an exact multiple of 16 bytes (only if the algo needs to use
	 * the input block or the resulting AES processed block).
	 *
	 * This is needed because we do not know if the following
	 * dofinal() call will provide any data at all and we
	 * need to e.g. pad/unpad the data.
	 *
	 * Removing padding on decipher is delaying since it is not
	 * known if dofinal() provides any data to handle padding. If not,
	 * on decipher padding must be removed from the last update() data
	 * block.
	 */
	if ((full_blocks > 0U) && ((aistate->ai_bytes_left % SE_AES_BLOCK_LENGTH) == 0U)) {
		if (TE_AES_MODE_PADDING(aes_context->ec.algorithm)) {
			full_blocks--;
		}
	}

	/* Handle full_blocks context gather buffer multiples immediately for
	 * an update()
	 */
	if (full_blocks > 0U) {
		ret = handle_aes_update_full_blocks(aes_context, aistate, full_blocks);
		CCC_ERROR_CHECK(ret);
	}

	/* We have 1..16 bytes left in aistate->ai_bytes_left here
	 *
	 * Handle partial gather buffers on later calls (on next
	 * update or dofinal) Store the remaining partial block of
	 * data to the (now) empty context buffer
	 */
	consumed = aistate->ai_bytes_left;
	se_util_mem_move(&aes_context->buf[0U], aistate->ai_input_src, consumed);
	aes_context->used += consumed;
	aistate->ai_input_src = &aistate->ai_input_src[consumed];
	aistate->ai_bytes_left = 0U;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

// XXX TODO: Implement aes_process_nc_regions() to handle non-contiguous memory
// XXX Kernel code must provide contiguous buffers, this can not lwrrently handle non-contig yet...

/* handle AES crypto data processing ops (update,dofinal)
 *
 * Two cipher directions: encrypt && decrypt => this affects padding etc.
 *
 * If the block cipher mode is not streaming or padding =>
 * total data size must be AES block size aligned but only when dofinal
 * gets eventually called. Intermediate calls (i.e. update calls)
 * do not need to be aligned in any way, even when using unpadded block
 * cipher modes (ECB_NOPAD, CBC_NOPAD).
 *
 */
static status_t aes_process_check_args(
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

	if ((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U) {
		uint32_t aes_output_size = input_params->input_size;

		if (BOOL_IS_TRUE(aes_context->ec.is_encrypt) &&
		    TE_AES_MODE_PADDING(aes_context->ec.algorithm)) {
			uint32_t remain = input_params->input_size % SE_AES_BLOCK_LENGTH;

			aes_output_size += (SE_AES_BLOCK_LENGTH - remain);
		}

		if (aes_output_size > input_params->output_size) {
			CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
					     LTRACEF("AES[0x%x] output buffer %u too short for %u bytes of input\n",
						     aes_context->ec.algorithm,
						     input_params->output_size,
						     input_params->input_size));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_set_data_to_context_buf(struct se_aes_context *aes_context,
					    struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;
	uint32_t consumed  = 0U;
	uint32_t total_bytes = 0U;
	const uint32_t buf_size = CCC_DMA_MEM_AES_BUFFER_SIZE;

	LTRACEF("entry\n");

	CCC_SAFE_ADD_U32(total_bytes, aes_context->used, aistate->ai_bytes_left);
	if (total_bytes <= buf_size) {
		consumed = aistate->ai_bytes_left;
	} else {
		consumed = buf_size - aes_context->used;
	}

	if (consumed > 0U) {
		se_util_mem_move(&aes_context->buf[aes_context->used],
				 aistate->ai_input_src, consumed);
		if (aistate->ai_bytes_left < consumed) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
		CCC_SAFE_ADD_U32(aes_context->used, aes_context->used, consumed);
		aistate->ai_bytes_left -= consumed;
		aistate->ai_input_src   = &aistate->ai_input_src[consumed];
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_aes_context_buf(struct se_aes_context *aes_context,
				       struct aes_istate *aistate, bool is_last)
{
	status_t ret = NO_ERROR;
	const uint32_t buf_size = CCC_DMA_MEM_AES_BUFFER_SIZE;

	LTRACEF("entry\n");

	ret = aes_set_data_to_context_buf(aes_context, aistate);
	CCC_ERROR_CHECK(ret);

	/* Have more bytes and the aes_context buf is full? =>
	 * pass aes_context buf to AES engine.
	 */
	if ((aistate->ai_bytes_left > 0U) && (aes_context->used == buf_size)) {
		aes_context->ec.is_last = false;

		if (!IS_DMA_ALIGNED(aistate->ai_input_dst) ||
		    !IS_DMA_ALIGNED(aistate->ai_out_len)) {
			/*
			 * AES engine writes to DST which need to be CACHE_LINE aligned
			 * If not, the cache management may cause issues if CPU manages
			 * values part of such unaligned cache lines while the AES engine
			 * operation is in progress => handle small unaligned data chunks
			 * (< CACHE_LINE bytes) here, larger data chunks MUST BE aligned
			 * by caller OR the CPU must not handle any data in such
			 * unaligned changelines while SE AES is in progress.
			 */
			ret = do_aes_and_check(aes_context, aes_context->buf,
					       aes_context->buf,
					       aistate->ai_out_len,
					       aes_context->used,
					       aes_context->used);
			CCC_ERROR_CHECK(ret,
					LTRACEF("AES crypto failed: %d\n",
						ret));

			se_util_mem_move(aistate->ai_input_dst,
					 aes_context->buf,
					 aes_context->used);
		} else {
			ret = do_aes_and_check(aes_context, aes_context->buf,
					       aistate->ai_input_dst,
					       aistate->ai_out_len,
					       aes_context->used,
					       aes_context->used);
			CCC_ERROR_CHECK(ret,
					LTRACEF("AES crypto failed: %d\n",
						ret));
		}
		aes_context->ec.is_last = is_last;

		/* AES intermediate block aligned ops write
		 * out as many bytes as was in the input
		 */
		CCC_SAFE_ADD_U32(aes_context->dst_total_written,
				 aes_context->dst_total_written, aes_context->used);

		/* Added this many bytes to output buffer */
		if (aistate->ai_out_len < aes_context->used) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
		aistate->ai_input_dst = &aistate->ai_input_dst[aes_context->used];
		aistate->ai_out_len  -= aes_context->used;

		aes_context->used = 0U;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Data handler for AES modes (excluding unaligned AES-CTS mode data)
 *
 * All AES modes (except unaligned AES-CTS) pass input data via this
 * function.  AES-CTS (NIST-CBC-CS2) uses this call as well when the
 * last buffer is 16 byte aligned (reduced to normal AES-CBC with no
 * block swapping for last blocks).
 *
 * This handles the unaligned data gathering into context->buf until a
 * full AES block is collected; a full block is then processed. No
 * data gathering is done unless mandated due to unaligned
 * intermediate data chunks.
 *
 * On dofinal() call the handle_aes_final function is called to
 * complete the AES operation and and on update() calls if there are
 * more bytes than fit the gather buffer a call to the
 * handle_aes_update function to process the excess intermediate
 * bytes.
 *
 * For performance reasons 16 byte (aes block size) aligned buffers
 * are recommended for all clients.
 *
 * The data buffers passed to this call must be contiguous. This must
 * be taken care of by the caller. In normal AES cases the input size
 * is equivalent to the output size and both input and output are
 * processed via SE DMA that does not support linked gather buffers
 * (anymore). So the data must be contiguous and the output data must
 * fit in the contiguous output buffer.
 *
 * @param context AES process context, defines the state for the operation
 * @param aistate state of current process handler call
 *
 * @return NO_ERROR on success, error value otherwise
 */
static status_t aes_process_input_normal(struct se_aes_context *aes_context,
					 struct aes_istate *aistate)
{
	status_t ret         = NO_ERROR;
	/* This is always at least two AES blocks. It is also a
	 * multiple of 16 with all cache line lengths, so can use all
	 * of aes_context->buf to buffer AES data when required.
	 */
	const uint32_t buf_size = CCC_DMA_MEM_AES_BUFFER_SIZE;
	bool is_last = aes_context->ec.is_last;

	LTRACEF("entry\n");

	/* Key derivations do not use partial buffer processing
	 */
	if (((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U) &&
	    (aistate->ai_bytes_left > 0U) &&
	    ((aes_context->used > 0U) || (aistate->ai_bytes_left < buf_size))) {

		/* Have more bytes and the aes_context buf is full? =>
		 * pass aes_context buf to AES engine.
		 */
		ret = handle_aes_context_buf(aes_context, aistate, is_last);
		CCC_ERROR_CHECK(ret);
	}

	if (BOOL_IS_TRUE(is_last)) {
		ret = handle_aes_final(aes_context, aistate);
	} else {
		if (aistate->ai_bytes_left > 0U) {
			ret = handle_aes_update(aes_context, aistate);
		}
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_process_algo(struct se_aes_context *aes_context,
				 struct aes_istate *aistate)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U) {
		// XXX TODO: consider bytes_left + aes_context->used?
		if (aistate->ai_out_len < aistate->ai_bytes_left) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     LTRACEF("AES output len %u < input len %u\n",
						     aistate->ai_out_len, aistate->ai_bytes_left));
		}
	}

	ret = aes_process_input_normal(aes_context, aistate);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* The code below will DMA ALIGN short AES DESTINATION buffers
 * (by using data aligned aes_context->buf as tmp output buffer)
 * but it does not (at least lwrrently) assist handling unaligned
 * (addr,size) buffers larger than CACHE_LINE size.
 *
 * The callers of this need to arrange the output buffers to be
 * DMA aligned both by address and size for larger buffers.
 */
status_t se_aes_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret         = NO_ERROR;
	uint32_t pre_written = 0U;

	struct aes_istate aistate = { .ai_bytes_left = 0U };

	LTRACEF("entry\n");

	ret = aes_process_check_args(input_params, aes_context);
	CCC_ERROR_CHECK(ret);

#if HAVE_SE_ASYNC_AES
	if ((AES_ASYNC_MAGIC == aes_context->async_context.magic) &&
	    BOOL_IS_TRUE(aes_context->async_context.started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Synchronous AES operation started while async operation is running\n"));
	}
#endif

	aistate.ai_input_src  = input_params->src;
	aistate.ai_input_dst  = input_params->dst;
	aistate.ai_out_len    = input_params->output_size;
	aistate.ai_bytes_left = input_params->input_size;

	pre_written = aes_context->dst_total_written;

	ret = aes_process_algo(aes_context, &aistate);
	CCC_ERROR_CHECK(ret);

	DEBUG_ASSERT(aistate.ai_bytes_left == 0U);

	if (aes_context->dst_total_written < pre_written) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
	/* Number of bytes written in this call */
	input_params->output_size = aes_context->dst_total_written - pre_written;

	if (aistate.ai_strip_pad_bytes > 0U) {
		if (input_params->output_size < aistate.ai_strip_pad_bytes) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("Error in algo 0x%x decipher pad handling: can't strip padding %u\n",
						     aes_context->ec.algorithm, aistate.ai_strip_pad_bytes));
		}

		/* Adjust the number of bytes placed to DST in this call
		 * (data shrinks when padding removed)
		 *
		 * TODO: it is not exactly right to write the last buffer to
		 * CLIENT DST before the padding has been removed. It may be
		 * fine internally but it would be better to not do that...
		 *
		 * Lwrrently the output buffer must be the same size as the input data
		 * (which IMHO is a valid requirement anyway because the caller
		 *  does not necessarily know the amount of padding removed)
		 */
		input_params->output_size  -= aistate.ai_strip_pad_bytes;
		aes_context->dst_total_written -= aistate.ai_strip_pad_bytes;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_AES

status_t se_aes_process_async_check(const struct se_aes_context *aes_context, bool *done_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == aes_context) || (NULL == done_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*done_p = false;

	if (!BOOL_IS_TRUE(aes_context->async_context.started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Async AES has not been started\n"));
	}

	ret = CCC_ENGINE_SWITCH_AES_ASYNC_CHECK(aes_context->ec.engine,
					     &aes_context->async_context, done_p);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_process_async_data_param_check(
	const struct se_data_params *input_params,
	const struct se_aes_context *aes_context, uint32_t bytes_left)
{
	status_t ret = NO_ERROR;
	uint32_t max_output_bytes = bytes_left;

	LTRACEF("entry\n");

	if ((NULL == input_params->src) || (NULL == input_params->dst)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Async AES invalid src/dst\n"));
	}

	if (0U == bytes_left) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async AES does not support NULL data input\n"));
	}

	if (bytes_left > MAX_AES_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	if (!BOOL_IS_TRUE(aes_context->ec.is_last) ||
	    TE_AES_BLOCK_CIPHER_MODE(aes_context->ec.algorithm)) {
		if ((bytes_left % AES_BLOCK_SIZE) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Async AES input buffer size %u not AES block aligned\n",
							       bytes_left));
		}
	} else {
		/* Only dofinal() ASYNC AES calls for stream cipher modes
		 * (e.g. AES-CTR) enter here.
		 *
		 * Caller must align dst size to full AES block
		 * size multiple as engine DMA writes full blocks
		 * only.
		 *
		 * This means that ASYNC AES OUTPUT BUFFERS must
		 * always be 16 byte multiple in size.
		 *
		 * INPUT BUFFER size must be >= "16 byte multiple
		 * aligned data length", even if the input data length
		 * itself for the stream cipher mode would not be 16
		 * byte multiple!
		 *
		 * E.g. if the last "input data" chunk length is only
		 * 1 byte for stream cipher mode dofinal, the BUFFER
		 * SIZE holding that data must be aligned to 16
		 * bytes. So in this case it must be 16 byte buffer to
		 * hold 1 byte of data.
		 *
		 * I.e. the BUFFER holding the input data must be
		 * readable to the 16 byte aligned boundary. Otherwise
		 * DMA engine will read past end of buffer, since the
		 * HW reads/writes only AES block size multiples!!!
		 *
		 * Input buffer size can not be checked for this case,
		 * as CCC does not know what the actual buffer size
		 * is.  It knows what the data size is in that buffer.
		 * => caller needs to make sure the input buffer size
		 * is aligned to 16 in all cases.
		 *
		 * These requirements apply to ASYNC AES stream
		 * cipher modes.
		 *
		 * The SYNC mode AES deals with these issues by using
		 * internal buffers for last AES block for such
		 * special cases, but the ASYNC mode requires the
		 * CLIENT to quarantee that the input/output/etc is as
		 * required by HW.
		 *
		 * ----
		 *
		 * Size ALIGN to 16 byte boundary for DST buffer size
		 */
		max_output_bytes = ((bytes_left + 15U) & ~15U);
	}

	/* Since we are not padding with an ASYNC call => size does not normally expand
	 * on encryption.
	 */
	if (input_params->output_size < max_output_bytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Async AES(0x%x) output buffer too short (%u < %u)\n",
						       aes_context->ec.algorithm,
						       input_params->output_size,
						       max_output_bytes));
	}

	/* SRC does not need to be DMA aligned.
	 * Check that the first chunk DST output buffer is. Later DST buffers
	 * need to be AES block size aligned.
	 */
	if (BOOL_IS_TRUE(aes_context->ec.is_first)) {
		if (! IS_DMA_ALIGNED(input_params->dst)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Async AES dst iaddress not CACHE line aligned: %p\n",
							       input_params->dst));
		}
	} else {
		if (0U != ((uintptr_t)input_params->dst & (uintptr_t)(SE_AES_BLOCK_LENGTH - 1U))) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Async AES dst address not block size aligned: %p\n",
							       input_params->dst));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_process_async_input_start(
	const struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;
	bool     is_contiguous = false;

	LTRACEF("entry\n");

	ret = aes_process_check_args(input_params, aes_context);
	CCC_ERROR_CHECK(ret);

	LTRACEF("AES INPUT: src=%p, input_size=%u, dst=%p, output_size=%u\n",
		input_params->src, input_params->input_size,
		input_params->dst, input_params->output_size);

	bytes_left = input_params->input_size;

	/* Async AES data param validity checks */
	ret = aes_process_async_data_param_check(input_params, aes_context, bytes_left);
	CCC_ERROR_CHECK(ret);

	ret = se_is_contiguous_memory_region(input_params->src, bytes_left, &is_contiguous);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Async AES data contiguous check failed: %d\n", ret));

	if (!BOOL_IS_TRUE(is_contiguous)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async AES does not support non-contiguous data (length %u)\n",
					     bytes_left));
	}

	is_contiguous = false;
	ret = se_is_contiguous_memory_region(input_params->dst, input_params->output_size, &is_contiguous);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Async AES output contiguous check failed: %d\n", ret));

	if (!BOOL_IS_TRUE(is_contiguous)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async AES does not support non-contiguous output buffer (length %u)\n",
					     input_params->output_size));
	}

	LTRACEF("Async AES starting\n");

	CCC_OBJECT_ASSIGN(aes_context->async_context.input, *input_params);
	aes_context->async_context.econtext = &aes_context->ec;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Handle async AES data processing ops (update,dofinal)
 *
 *** XXXX FIXME: this may not be wise, should create a tmp_input in caller?
 * input_params can be NULL if finishing an async operation (it is not used
 * in that case).
 */
status_t se_aes_process_async_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (aes_context->ec.domain != TE_CRYPTO_DOMAIN_KERNEL) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Async AES unsupported data domain: %u\n",
					     aes_context->ec.domain));
	}

	if (BOOL_IS_TRUE(aes_context->async_start)) {
		ret = aes_process_async_input_start(input_params, aes_context);
		CCC_ERROR_CHECK(ret);

		HW_MUTEX_TAKE_ENGINE(aes_context->ec.engine);

		aes_context->async_context.magic = AES_ASYNC_MAGIC;
		ret = CCC_ENGINE_SWITCH_AES_ASYNC_START(aes_context->ec.engine,
							 &aes_context->async_context);

		/* Engine is left locked; client is responsible for releasing it */
	} else {
		/* Engine is unlocked here */

		ret = CCC_ENGINE_SWITCH_AES_ASYNC_FINISH(aes_context->ec.engine, &aes_context->async_context);
		aes_context->async_context.magic = 0U;

		HW_MUTEX_RELEASE_ENGINE(aes_context->ec.engine);

		LTRACEF("Async AES finished: %d\n", ret);
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_AES */
#endif /* HAVE_SE_AES */
