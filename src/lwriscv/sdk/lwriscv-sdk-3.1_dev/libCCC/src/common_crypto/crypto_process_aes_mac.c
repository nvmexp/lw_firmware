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
 * @file   crypto_process_aes_mac.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Processing layer for AES based MAC operations
 */

#include <crypto_system_config.h>
#include <crypto_cipher_proc.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CMAC_AES

#define AES_CMAC_CONST_RB 0x87U

static status_t cmac_process_subkey_check_args(
			const struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == aes_context) || (NULL == aes_context->cmac.pk1) ||
	    (NULL == aes_context->cmac.pk2)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("CMAC-AES subkey objects undefined\n"));
	}

	if (aes_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AES aes_context buffer not free for subkey generation\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_construct_pk1(uint8_t *pk1, uint8_t *zero_L)
{
	status_t ret = NO_ERROR;
	uint32_t most_significant_bit = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	for (i = 0U; i < SE_AES_BLOCK_LENGTH; i++) {
		pk1[i] = zero_L[i]; /* PK1 = L */
		zero_L[i] = 0U;	/* Clear the intermediate value */
	}

	FI_BARRIER(u32, i);

	if (SE_AES_BLOCK_LENGTH != i) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/*
	 * L := AES-{128,192,256}(K, const_Zero);
	 * if MSB(L) is equal to 0,
	 * then K1 := L << 1;
	 * else K1 := (L << 1) XOR const_Rb;
	 */
	most_significant_bit = (uint32_t)pk1[0] & 0x80U;
	ret = se_util_left_shift_one_bit(pk1, SE_AES_BLOCK_LENGTH);
	CCC_ERROR_CHECK(ret);

	if (0U != most_significant_bit) {
		pk1[SE_AES_BLOCK_LENGTH - 1U] ^= AES_CMAC_CONST_RB;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_construct_pk2(uint8_t *pk2, uint8_t *pk1)
{
	status_t ret = NO_ERROR;
	uint32_t most_significant_bit = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	/*
	 * if MSB(K1) is equal to 0
	 * then K2 := K1 << 1;
	 * else K2 := (K1 << 1) XOR const_Rb;
	 */
	most_significant_bit = (uint32_t)pk1[0] & 0x80U;

	for (i = 0U; i < SE_AES_BLOCK_LENGTH; i++) {
		pk2[i] = pk1[i];
	}

	FI_BARRIER(u32, i);

	if (SE_AES_BLOCK_LENGTH != i) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ret = se_util_left_shift_one_bit(pk2, SE_AES_BLOCK_LENGTH);
	CCC_ERROR_CHECK(ret);

	if (0U != most_significant_bit) {
		pk2[SE_AES_BLOCK_LENGTH - 1U] ^= AES_CMAC_CONST_RB;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* CMAC subkey generation. This is step1 in CMAC Hash
 * Called by the crypto_mac.c level.
 */
status_t cmac_process_derive_subkeys(struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint8_t *pk1 = NULL;
	uint8_t *pk2 = NULL;
	uint8_t *zero_L = NULL;

	LTRACEF("entry\n");

	ret = cmac_process_subkey_check_args(aes_context);
	CCC_ERROR_CHECK(ret);

	aes_context->used = SE_AES_BLOCK_LENGTH;

	/* This is DMA aligned => encrypt in place, not directly to pk1 */
	zero_L = &aes_context->buf[0];
	se_util_mem_set(zero_L, 0U, SE_AES_BLOCK_LENGTH);

	ret = aes_ecb_encrypt_block(&aes_context->ec, zero_L, zero_L);
	if (NO_ERROR != ret) {
		uint32_t inx = 0x0U;
		for (inx = 0U; inx < SE_AES_BLOCK_LENGTH; inx++) {
			/* Clear the intermediate value */
			zero_L[inx] = 0U;
		}
	}
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to construct CMAC value L\n"));

	pk1 = aes_context->cmac.pk1;

	/* This call clears zero_L in all cases so it does
	 * not need to be cleared separately.
	 */
	ret = cmac_process_construct_pk1(pk1, zero_L);

	zero_L = NULL;
	aes_context->used = 0U;

	/* NOTE: Do not clobber ret after the AES call above (before this point)
	 */
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to construct CMAC value PK1: %d\n", ret));

	pk2 = aes_context->cmac.pk2;

	/*
	 * PK1 == L at this point (zero_L was cleared, now NULL).
	 */
#if SE_RD_DEBUG
	LOG_HEXBUF("CMAC subkey intermediate: L[16]", pk1, SE_AES_BLOCK_LENGTH);
#endif

	ret = cmac_process_construct_pk2(pk2, pk1);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LOG_HEXBUF("CMAC subkey K1 =>", aes_context->cmac.pk1, SE_AES_BLOCK_LENGTH);
	LOG_HEXBUF("CMAC subkey K2 =>", aes_context->cmac.pk2, SE_AES_BLOCK_LENGTH);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Push the contiguous data shunk through the engine for
 * CMAC.
 *
 * The engine has a size limitation of 16 MB per chunk, so
 * this function auto-splits the data into suitable chunks.
 */
static status_t se_cmac_engine(const engine_t *engine,
			       struct se_data_params *input,
			       struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	struct se_data_params pinput;
	uint32_t bytes_left = input->input_size;
	uint32_t processed = 0U;
	bool is_last = econtext->is_last;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	econtext->is_last = false;

	HW_MUTEX_TAKE_ENGINE(engine);

	CCC_LOOP_BEGIN {
		uint32_t bcount = 0U;

		pinput.src = &input->src[processed];
		pinput.dst = NULL;
		pinput.output_size = 0U;

		if (bytes_left <= MAX_AES_ENGINE_CHUNK_SIZE) {
			bcount = bytes_left;
			econtext->is_last = is_last;

			if (BOOL_IS_TRUE(is_last)) {
				pinput.dst         = input->dst;
				pinput.output_size = input->output_size;
			}
		} else {
			bcount = MAX_AES_ENGINE_CHUNK_SIZE;
		}

		pinput.input_size = bcount;

		ret = CCC_ENGINE_SWITCH_CMAC(engine, &pinput, econtext);
		CCC_ERROR_END_LOOP(ret);

		CCC_SAFE_ADD_U32(processed, processed, bcount);
		bytes_left -= bcount;

	        if (0U == bytes_left) {
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

	input->output_size = pinput.output_size;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_cmac_engine_call(struct se_aes_context *aes_context,
				    const uint8_t *in, uint8_t *out,
				    uint32_t outlen, uint32_t bytes)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.src = in;
	input.input_size = bytes;
	input.dst = out;
	input.output_size = outlen;

	DEBUG_ASSERT_PHYS_DMA_OK(in, bytes);

	ret = se_cmac_engine(aes_context->ec.engine, &input, &aes_context->ec);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_NC_REGIONS

static status_t cmac_process_nc_check_args(
	const struct se_aes_context *aes_context,
	const uint8_t *kva, uint32_t remain)
{
	status_t ret = NO_ERROR;

	if ((NULL == aes_context) || (NULL == kva) || (0U == remain)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (aes_context->used != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	return ret;
}

/* Complexity reduction helper */
static status_t cmac_process_nc_regions_intern(struct se_aes_context *aes_context,
					       uint32_t *remain_p,
					       uint32_t clength,
					       const uint8_t *kva_param,
					       uint32_t *kva_offset_p,
					       uint8_t *dst,
					       uint32_t block_size)
{
	status_t ret = NO_ERROR;
	uint32_t remain = *remain_p;
	uint32_t kva_offset = 0U;
	const uint8_t *kva = kva_param;

	LTRACEF("entry\n");

	if ((remain > 0U) && (clength > 0U)) {
		if ((remain <= clength) && BOOL_IS_TRUE(aes_context->ec.is_last)) {
			/* pass the final bytes for this cmac (this completes dofinal) */
			ret = se_cmac_engine_call(aes_context, kva, dst, 0U, remain);
			CCC_ERROR_CHECK(ret,
					LTRACEF("CMAC mac failed: %d\n",
						ret));
			kva_offset += remain;
			remain = 0U;
		} else {
			/* This does not handle last bytes of dofinal => ec.is_last must not be
			 * set true for the digest and partial bytes get copied to aes_context buf.
			 */
			uint32_t blocks = clength / block_size;
			uint32_t nbytes = blocks * block_size;

			if (blocks > 0U) {
				/* pass full blocks before non-contig page boundary */
				ret = se_cmac_engine_call(aes_context, kva, dst, 0U, nbytes);
				CCC_ERROR_CHECK(ret,
						LTRACEF("CMAC mac failed: %d\n",
							ret));

				/* Update, may get used below */
				kva = &kva[nbytes];
				kva_offset += nbytes;
				remain -= nbytes;
			}

			/* Move the partial cmac block before page boundary to aes_context->buf
			 * for later processing.
			 */
			nbytes = clength % block_size;
			if (nbytes > 0U) {
				se_util_mem_move(aes_context->buf, kva, nbytes);
				remain -= nbytes;

				kva_offset += nbytes;

				aes_context->used = nbytes;
			}
		}
	}

	*remain_p = remain;

	/* Caller advances its kva to &kva[kva_offset] (using caller kva)
	 */
	*kva_offset_p = kva_offset;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Complexity reduction helper */
static status_t cmac_process_nc_regions_cbuf(struct se_aes_context *aes_context,
					     uint32_t block_size,
					     uint8_t *dst)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((aes_context->used == block_size) || BOOL_IS_TRUE(aes_context->ec.is_last)) {
		ret = se_cmac_engine_call(aes_context,
					  aes_context->buf,
					  dst,
					  0U,
					  aes_context->used);
		CCC_ERROR_CHECK(ret,
				LTRACEF("CMAC mac failed: %d\n", ret));
		aes_context->used = 0U;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Note: aes_context->ec.is_last is likely never true when calling
 * this because CMAC last block handling happens in the caller.
 */
static status_t cmac_process_nc_regions(struct se_aes_context *aes_context,
					const uint8_t *kva_param, uint32_t remain_param)
{
	status_t ret	    = NO_ERROR;
	uint32_t block_size = 0U;
	const uint8_t *kva  = kva_param;
	uint8_t *dst        = NULL;	/* always use internal buffer with this */
	uint32_t remain     = remain_param;

	LTRACEF("entry\n");

	ret = cmac_process_nc_check_args(aes_context, kva, remain);
	CCC_ERROR_CHECK(ret);

	block_size = AES_BLOCK_SIZE;

#if SE_RD_DEBUG && LOCAL_TRACE	// R&D... TODO: remove
	{
		uint32_t full_blocks = remain / block_size;
		uint32_t stray_bytes = remain % block_size;

		LTRACEF("CMAC processing: %u blocks and %u stray bytes\n",
			full_blocks, stray_bytes);

		if ((stray_bytes > 0U) && !BOOL_IS_TRUE(aes_context->ec.is_last)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Data not block aligned for intermediate CMAC (stray %u)\n",
						     stray_bytes));
		}
	}
#endif

	while (remain > 0U) {
		uint32_t clength = 0U;

		/* Find the amount of bytes (up to REMAIN bytes) that
		 * can be accessed with a phys mem DMA before a
		 * non-contiguous page boundary.
		 *
		 * I.e. in one "contiguous memory region".
		 */
		ret = ccc_nc_region_get_length(aes_context->ec.domain, kva, remain, &clength);
		if (NO_ERROR != ret) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Failed to get contiguous region length: %d\n", ret));
		}

		if ((aes_context->used > 0U) || (clength < block_size)) {
			uint32_t fill_size = block_size - aes_context->used;


			/* clength may not be long enough to complete
			 * the CMAC block.
			 */
			if (clength < fill_size) {
				fill_size = clength;
			}

			se_util_mem_move(&aes_context->buf[aes_context->used], kva, fill_size);
			remain  -= fill_size;
			clength -= fill_size;
			kva      = &kva[fill_size];
			aes_context->used += fill_size;

			if (aes_context->used < block_size) {

				DEBUG_ASSERT(0U == clength);

				if (remain > 0U) {
					continue;
				}
			}

			/* if aes_context->buf is full OR this is the
			 * dofinal call and we have no more data =>
			 * mac last partial block.
			 */
			ret = cmac_process_nc_regions_cbuf(aes_context, block_size, dst);
			CCC_ERROR_CHECK(ret);
		}

		/* At this point: aes_context->buf is empty; clength may still be nonzero (chunk has more data)
		 */
		{
			uint32_t kva_offset = 0U;
			ret = cmac_process_nc_regions_intern(aes_context, &remain,
							     clength, kva, &kva_offset,
							     dst, block_size);
			CCC_ERROR_CHECK(ret);

			kva = &kva[kva_offset];
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_NC_REGIONS */

#if HAVE_CMAC_DST_KEYTABLE

static status_t aes_cbc_block_to_keyslot(
	struct se_engine_aes_context *econtext,
	const uint8_t *aes_in)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	struct se_engine_aes_context *aes_ectx = NULL;
	uint32_t dst_key_size =
		(econtext->aes_flags &
		 (AES_FLAGS_DERIVE_KEY_128 | AES_FLAGS_DERIVE_KEY_192 | AES_FLAGS_DERIVE_KEY_256));

	LTRACEF("entry\n");

	if (econtext->se_ks.ks_target_keyslot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CMAC-AES dst keyslot out of range: %u\n",
					     econtext->se_ks.ks_target_keyslot));
	}

	if (NULL == aes_in) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	DEBUG_ASSERT_PHYS_DMA_OK(aes_in, SE_AES_BLOCK_LENGTH);

	aes_ectx = CMTAG_MEM_GET_OBJECT(econtext->cmem,
					CMTAG_ECONTEXT,
					0U,
					struct se_engine_aes_context,
					struct se_engine_aes_context *);
	if (NULL == aes_ectx) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Use CBC mode to write the keyslot word quads.
	 */
	/* Set the CMEM ref to aes engine context from caller engine context */
	ret = aes_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD,
						aes_ectx, econtext->cmem);
	CCC_ERROR_CHECK(ret);

	LTRACEF("Use same keyslot for final AES as used for CMAC block processing\n");
	CCC_OBJECT_ASSIGN(aes_ectx->se_ks, econtext->se_ks);

	/* Allow the caller to use existing keys from keyslots.
	 */
	if ((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* This only works if the original key can be used for AES-CBC
		 * as well, no matter what the original operation is.
		 *
		 * E.g. with CMAC key is used for both CMAC and AES-CBC.
		 */
		LTRACEF("Use existing key in AES keyslot for algo implied AES-CBB operation\n");

		/* This is used for "implicit AES operations", so do
		 * not erase the keyslot after this
		 */
		aes_ectx->aes_flags =
			(aes_ectx->aes_flags | AES_FLAGS_USE_PRESET_KEY | AES_FLAGS_LEAVE_KEY_TO_KEYSLOT);
	}

	aes_ectx->is_last    = true;
	aes_ectx->is_encrypt = true;

	/* Flag that AES engine should write result there
	 *
	 * Flag this so that callee knows CMAC-AES is deriving key,
	 * not plain AES-CBC-UNPAD.
	 */
	aes_ectx->aes_flags =
		(aes_ectx->aes_flags | AES_FLAGS_DST_KEYSLOT | dst_key_size | AES_FLAGS_CMAC_DERIVE);

	/* Caller decides which quad the CBC writes to
	 */
	aes_ectx->aes_flags |= (econtext->aes_flags & AES_FLAGS_DERIVE_KEY_HIGH);
	LTRACEF("XXXX CMAC deriving quad: %u\n",
		(((econtext->aes_flags & AES_FLAGS_DERIVE_KEY_HIGH) == 0U) ? 0U : 1U));

	LTRACEF("XXXX CMAC targetting keyslot %u\n", aes_ectx->se_ks.ks_target_keyslot);

	/* Write specified quad of keyslot (128 bits)
	 */
	input.src	  = aes_in;
	input.input_size  = SE_AES_BLOCK_LENGTH;

	// XXX a special case: <NULL, 16> is ok as keyslot is target
	input.dst	  = NULL;
	input.output_size = SE_AES_BLOCK_LENGTH;

	/* Copy CMAC phash to iv_stash for AES-CBC.
	 */
	se_util_mem_move32(&aes_ectx->iv_stash[0], &econtext->aes_mac.phash[0],
			   (SE_AES_BLOCK_LENGTH / BYTES_IN_WORD));

	HW_MUTEX_TAKE_ENGINE(aes_ectx->engine);

	ret = CCC_ENGINE_SWITCH_AES(aes_ectx->engine, &input, aes_ectx);

	HW_MUTEX_RELEASE_ENGINE(aes_ectx->engine);

	CCC_ERROR_CHECK(ret);
fail:
	if (NULL != aes_ectx) {
		struct context_mem_s *cmem = aes_ectx->cmem;
		se_util_mem_set((uint8_t *)aes_ectx, 0U,
				sizeof_u32(struct se_engine_aes_context));
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ECONTEXT,
				  aes_ectx);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_DST_KEYTABLE */

static status_t cmac_process_final_full_blocks(struct se_aes_context *aes_context,
					       uint32_t *bytes_left_p,
					       const uint8_t **input_src_p)
{
	status_t ret = NO_ERROR;
	uint32_t full_blocks = 0U;
	uint32_t consumed = 0U;
	uint32_t bytes_left = 0U;
	const uint8_t *input_src = NULL;

	LTRACEF("entry\n");

	if ((NULL == bytes_left_p) || (NULL == input_src_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input_src = *input_src_p;
	bytes_left = *bytes_left_p;
	full_blocks = bytes_left / SE_AES_BLOCK_LENGTH;

	/* Can not push all bytes to AES yet, hold on to [1..16] bytes.
	 * Note: bytes_left can also be zero here in one case (see below).
	 */
	if ((full_blocks > 0U) && ((bytes_left % SE_AES_BLOCK_LENGTH) == 0U)) {
		full_blocks--;
	}

	if (full_blocks > 0U) {
		consumed = full_blocks * SE_AES_BLOCK_LENGTH;
		aes_context->ec.is_last = false;
#if HAVE_NC_REGIONS
		ret = cmac_process_nc_regions(aes_context, input_src, consumed);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("CMAC-AES mac failed: %d\n", ret));
#else
		ret = se_cmac_engine_call(aes_context, input_src, NULL,
					  0U, consumed);
		CCC_ERROR_CHECK(ret,
				LTRACEF("CMAC mac failed: %d\n",
					ret));
#endif
		aes_context->ec.is_last = true;
		CCC_SAFE_ADD_U32(aes_context->byte_count, aes_context->byte_count,
				 consumed);
		CCC_SAFE_SUB_U32(bytes_left, bytes_left, consumed);
		*input_src_p = &input_src[consumed];
		*bytes_left_p = bytes_left;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_set_subkey(struct se_aes_context *aes_context,
					   uint8_t *data,
					   const uint8_t **subkey_p,
					   uint32_t *bytes_left_p)
{
	status_t ret = NO_ERROR;
	const uint8_t *subkey = NULL;
	uint32_t bytes_left = 0U;

	LTRACEF("entry\n");

	if ((NULL == subkey_p) || (NULL == bytes_left_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	bytes_left = *bytes_left_p;

	if ((bytes_left == 0U) || ((bytes_left % SE_AES_BLOCK_LENGTH) != 0U)) {

		/* If there is no data or the data size is not aligned to 16 bytes
		 * it means that the last block must be first padded to 16 byte
		 * boundary and then the padded block is XORed with aes_context->cmac.pk2.
		 *
		 * This is the RFC-4493 case "(b) otherwise" in Figure 2.1.
		 */
		uint32_t pad_bytes = 0U;

		DEBUG_ASSERT(bytes_left <= 16U);	// Must have less or equal to one block
		DEBUG_ASSERT(data == aes_context->buf);

		subkey = aes_context->cmac.pk2;

		pad_bytes = SE_AES_BLOCK_LENGTH - (bytes_left % SE_AES_BLOCK_LENGTH);

		/* Zeropad the AES block starting at &data[bytes_left] with 1..16 bytes
		 *  to the next AES block boundary.
		 */
		se_util_mem_set(&data[bytes_left], 0U, pad_bytes);

		/* The first pad byte must be set to 0x80 */
		data[SE_AES_BLOCK_LENGTH - pad_bytes] = 0x80U;

		/* SRC data is now zero padded to AES block boundary and
		 *  it grew by pad_bytes.
		 */
		CCC_SAFE_ADD_U32(bytes_left, bytes_left, pad_bytes);

	} else {
		/* RFC4493 Figure 2.1 case "(a) positive multiple block length"
		 * XOR last (aes) block with aes_context->cmac.pk1.
		 *
		 * Simple case, just set the subkey.
		 */
		subkey = aes_context->cmac.pk1;
	}

	*subkey_p = subkey;
	*bytes_left_p = bytes_left;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_xor_last_block(uint8_t *data,
					    const uint8_t *subkey)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if ((NULL == data) || (NULL == subkey)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	for (inx = 0U; inx < SE_AES_BLOCK_LENGTH; inx++) {
		uint32_t sk = subkey[inx];
		data[inx] = BYTE_VALUE(data[inx] ^ sk);
	}

	FI_BARRIER(u32, inx);

	if (SE_AES_BLOCK_LENGTH != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_do_cmac_last(struct se_data_params *input_params,
					  struct se_aes_context *aes_context,
					  uint8_t *input_dst,
					  uint8_t *data,
					  uint32_t bytes_left)
{
	status_t ret = NO_ERROR;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
#if HAVE_CMAC_DST_KEYTABLE
		if ((aes_context->ec.aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
			ret = aes_cbc_block_to_keyslot(&aes_context->ec, data);
			CCC_ERROR_END_LOOP(ret,
					   LTRACEF("CMAC-AES block to keyslot failed: %d", ret));

			se_util_mem_set(data, 0U, SE_AES_BLOCK_LENGTH);
			CCC_LOOP_STOP;
		}
#endif /* HAVE_CMAC_DST_KEYTABLE */

		/* Push all the remaining input (now always AES block aligned)
		 * through encryption (mac).
		 *
		 * CMAC result will be placed to input_dst.
		 */
		ret = se_cmac_engine_call(aes_context, data, input_dst,
					  input_params->output_size,
					  bytes_left);
		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("CMAC mac failed: %d\n",
					   ret));
		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	/* All data has now been handled and the final result is ready */
	CCC_SAFE_ADD_U32(aes_context->byte_count, aes_context->byte_count, bytes_left);
	aes_context->used = 0U;

	input_params->output_size = SE_AES_BLOCK_LENGTH;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_input_last(struct se_data_params *input_params,
					struct se_aes_context *aes_context,
					const uint8_t *input_src_param,
					uint8_t *input_dst,
					uint32_t bytes_left_param)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = bytes_left_param;

	/* aes_context buffer is either empty or all input data
	 *  to handle are in aes_context->buf at this point.
	 */
	uint8_t *data = NULL;
	const uint8_t *subkey = NULL;
	const uint8_t *input_src = input_src_param;

	LTRACEF("entry\n");

	if (aes_context->used > 0U) {
		if (bytes_left > 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
		data = &aes_context->buf[0];
		bytes_left = aes_context->used;
	} else {
		ret = cmac_process_final_full_blocks(aes_context, &bytes_left,
						     &input_src);
		CCC_ERROR_CHECK(ret);

		/* At most one AES block of data remains unhandled here.
		 *
		 * Note: But bytes_left may also be zero here => only in the
		 *  case when neither update() nor dofinal() has passed any data
		 *  to the CMAC, i.e. this is CMAC of nul input.
		 */
		if (bytes_left > 0U) {
			se_util_mem_move(&aes_context->buf[0], input_src, bytes_left);
			data = &aes_context->buf[0];
			/* input_src processed */
		}
	}

	if (bytes_left == 0U) {
		DEBUG_ASSERT(NULL == data);
		data = &aes_context->buf[0];
	}

	/* Any remaining data is now in aes_context->buf so it can
	 *  be freely modified. Rememeber that aes_context->buf
	 *  size is aligned to AES blocks so the CMAC PK2 case
	 *  with padding always fits there and PK1 case does not
	 *  grow the data.
	 *
	 * It can contain [0..buf_len] bytes (inclusive) here.
	 *
	 * Do the last block padding according to RFC4493.
	 */
	ret = cmac_process_set_subkey(aes_context, data, &subkey, &bytes_left);
	CCC_ERROR_CHECK(ret);

	DEBUG_ASSERT(SE_AES_BLOCK_LENGTH == bytes_left);
	DEBUG_ASSERT(data == aes_context->buf);

#if SE_RD_DEBUG
	LOG_HEXBUF("CMAC XOR data (below) with PK", &data[0], bytes_left);
	LOG_HEXBUF("CMAC XOR data with PK (below), subkey", subkey, 16U);
#endif

	/* XOR last block with the proper subkey before encrypting
	 */
	ret = cmac_process_xor_last_block(data, subkey);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LOG_HEXBUF("CMAC XOR result", &data[0U], 16U);
#endif

	ret = cmac_process_do_cmac_last(input_params, aes_context,
					input_dst, data, bytes_left);
	CCC_ERROR_CHECK(ret);

	/* bytes_left could be set 0 */
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_input_update(struct se_aes_context *aes_context,
					  const uint8_t *input_src_param,
					  uint32_t bytes_left_param,
					  uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = bytes_left_param;
	/* update() enters here only if there is more data */
	uint32_t full_blocks = 0U;
	uint32_t consumed = 0U;
	const uint8_t *input_src = input_src_param;

	LTRACEF("entry\n");

	/* CERT INT33-C avoid to devided by zero */
	if (0U == buf_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	full_blocks = bytes_left / buf_size;

	/* For an update the context buffer must have been filled first =>
	 * here deal with the bytes that did not fit there.
	 * It is not possible to have data in both context block and in client input
	 * here, the (full) context buffer was handled above and used == 0
	 */
	if (aes_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Be careful to hold at least one source byte so we
	 * can do CMAC subkey XORin for the last block.
	 *
	 * This is important because we do not know if the dofinal
	 * (i.e. call with is_last set to true) contains any data or not.
	 * We do not know that until we see the dofinal() arguments.
	 *
	 * Hence, if the data happens to be aligned to the buf_size =>
	 * keep enough data to fill the aes_context->buf, if not aligned
	 * => just keep the unaligned number of bytes, i.e. [1..(buf_len - 1)]
	 * (inclusive).
	 */
	if ((full_blocks > 0U) && ((bytes_left % buf_size) == 0U)) {
		full_blocks--;
	}

	// Deal with the next N full blocks of input
	if (full_blocks > 0U) {
		consumed = full_blocks * buf_size;
#if HAVE_NC_REGIONS
		ret = cmac_process_nc_regions(aes_context, input_src, consumed);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("CMAC-AES mac failed: %d\n", ret));
#else
		ret = se_cmac_engine_call(aes_context, input_src, NULL,
					  0U, consumed);
		CCC_ERROR_CHECK(ret,
				LTRACEF("CMAC mac failed: %d\n",
					ret));
#endif
		CCC_SAFE_ADD_U32(aes_context->byte_count, aes_context->byte_count, consumed);
		input_src = &input_src[consumed];
		CCC_SAFE_SUB_U32(bytes_left, bytes_left, consumed);
	}

	// Save the rest of bytes (i.e. [1..buf_size]) to aes_context->buf
	//
	if (bytes_left > 0U) {
		consumed = bytes_left;

		se_util_mem_move(&aes_context->buf[aes_context->used],
				 input_src, consumed);

		aes_context->used += consumed;
		/* input_src processed */
	}

	/* bytes_left could be set 0 */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_input_check_args(
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

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_update_input(struct se_aes_context *aes_context,
				  uint32_t *bytes_left_p,
				  const uint8_t **input_src_p,
				  uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	uint32_t consumed = 0U;
	uint32_t bytes_left = 0U;
	uint32_t total_bytes= 0U;
	const uint8_t *input_src = NULL;

	LTRACEF("entry\n");

	if ((NULL == bytes_left_p) || (NULL == input_src_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	bytes_left = *bytes_left_p;
	input_src = *input_src_p;

	CCC_SAFE_ADD_U32(total_bytes, aes_context->used, bytes_left);
	if (total_bytes <= buf_size) {
		consumed = bytes_left;
	} else {
		CCC_SAFE_SUB_U32(consumed, buf_size, aes_context->used);
	}

	if (consumed > 0U) {
		se_util_mem_move(&aes_context->buf[aes_context->used],
				 input_src, consumed);
		CCC_SAFE_ADD_U32(aes_context->used, aes_context->used, consumed);
		CCC_SAFE_SUB_U32(bytes_left, bytes_left, consumed);
		*input_src_p = &input_src[consumed];
		*bytes_left_p = bytes_left;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_input(struct se_aes_context *aes_context,
				   uint32_t *bytes_left_p,
				   const uint8_t **input_src_p,
				   uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	bool is_last = aes_context->ec.is_last;

	LTRACEF("entry\n");

	ret = cmac_update_input(aes_context, bytes_left_p, input_src_p, buf_size);
	CCC_ERROR_CHECK(ret);

	/* Must not flush this buffer until we have more data available.
	 *
	 * This is because the last CMAC block needs to be XORed with a subkey
	 * before it is encrypted and we do not know if there is any
	 * more data passed in (unless this call is_last).
	 */
	if (*bytes_left_p > 0U) {
		if (aes_context->used != buf_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("CMAC context buf not full when flushing\n"));
		}
		aes_context->ec.is_last = false;
		ret = se_cmac_engine_call(aes_context, aes_context->buf, NULL,
				  0U, aes_context->used);
		CCC_ERROR_CHECK(ret,
				LTRACEF("CMAC mac failed: %d\n",
					ret));
		aes_context->ec.is_last = is_last;
		CCC_SAFE_ADD_U32(aes_context->byte_count, aes_context->byte_count,
				 aes_context->used);
		aes_context->used = 0U;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* handle AES CMAC mac data processing ops (update,dofinal)
 */
status_t se_cmac_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;
	uint32_t buf_size = 0U;

	const uint8_t *input_src = NULL;
	uint8_t *input_dst = NULL;

	LTRACEF("entry\n");

	ret = cmac_process_input_check_args(input_params, aes_context);
	CCC_ERROR_CHECK(ret);

	input_src = input_params->src;
	input_dst = input_params->dst;

	bytes_left = input_params->input_size;

	/* CMAC code handling the last block lwrrently expects that
	 * the gather buffer is 16 bytes long when it injects the K1
	 * or K2 subkey value to the final result.
	 */
	buf_size = SE_AES_BLOCK_LENGTH;

	if ((bytes_left > 0U) &&
	    ((aes_context->used > 0U) || (bytes_left < buf_size))) {
		ret = cmac_process_input(aes_context, &bytes_left,
					 &input_src, buf_size);
		CCC_ERROR_CHECK(ret);
	}

	if (BOOL_IS_TRUE(aes_context->ec.is_last)) {
		ret = cmac_process_input_last(input_params,
					      aes_context,
					      input_src,
					      input_dst,
					      bytes_left);
	} else {
		if (bytes_left > 0U) {
			ret = cmac_process_input_update(aes_context,
							input_src,
							bytes_left,
							buf_size);
		}
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES */
