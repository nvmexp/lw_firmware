/*
 * Copyright (c) 2015-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* All SE engine HW related code should be in this file.
 * If not => move it here and move other code out of here (TODO).
 */

/* NOTE about key slots.  As a convention we should assume that one
 * out of the 4 rsa keyslots is reserved for secure code (e.g. this
 * driver) and this reg shyould then be set TZ access only ( which is
 * a sticky to reset setting). Otherwise the SE driver can not do any
 * RSA operations with the SE PKA0 unit.
 *
 * As a convention (as dislwssed and suggested by me ;-) I will take
 * the last RSA key slot (slot 3) and will make it TZ only at some
 * point of time before releasing this driver...
 *
 * Same thing for AES key slots => I need at least one keyslot for
 * this code, but I do not know which number I can start to use =>
 * more dislwssions required. Now using AES key slot 2 without
 * agreements of any kind for testing (but hey, it seems to
 * work!).
 *
 * What ever slot is reserved for this it will most likely be
 * the only keyslot this driver can use to write keys. Existing key
 * use will need some authorization mechanism (as dislwssed the
 * dynloader adds simple manifest authorizations) but theese are not
 * yet merged. Lwrrently any client can use any keyslot (which is not
 * really that fine, even though all clients are secure).
 *
 * In case I do the keystore using ciphered keys => then it might be
 * required to allocate one more AES keyslot for this purpose.
 */

/* XXX TODO set "TZ use only" to any keyslot we set in this code before setting it...
 */

/**
 * @file   tegra_se_sha.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2015
 *
 * @brief  SHA engine engine layer code T18X/T19X and T23X
 */

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#include <include/tegra_se.h>
#include <tegra_se_gen.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#include <ccc_safe_arith.h>

#if !HAVE_SE_SHA
#if HAVE_HMAC_SHA
#error "HMAC-SHA requires HAVE_SE_SHA (SHA engine)"
#endif
#endif

#if HAVE_SE_SHA

/**** SHA digest operations ****/

/**
 * @brief return the max chunk size the SHA engine can handle in one call.
 *
 * Largest data that can be processed in an intermediate operation for
 * HW digest engine (SHA, SHA3, etc) TASK.
 *
 * The intermediate chunks must be multiple of the algorithm internal
 * block size or the engine can not save context after the SE TASK and
 * release the engine to continue later.
 *
 * Returns the largest umber of bytes < 16 MB that is multiple of the
 * algorithm internal block size (block size defined in the algorithm
 * specification and in the IAS).
 *
 * The HW engine only provides intermediate results to save when
 * the input data size is a multiple of SHA algorithm block size.
 *
 * @param econtext Get the block_size for the SHA algo from here
 * @param max_chunk_size_p Ref contains the max chunk size if no error.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_engine_get_sha_max_chunk_size(const struct se_engine_sha_context *econtext,
					  uint32_t *max_chunk_size_p)
{
	status_t ret = NO_ERROR;
	uint32_t max_chunk_size = 0U;

	if ((NULL == econtext) || (NULL == max_chunk_size_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (econtext->block_size) {
	case SHA_BLOCK_SIZE_64:
		max_chunk_size = MAX_SHA_ENGINE_CHUNK_SIZE_BSIZE64;
		break;
	case SHA_BLOCK_SIZE_128:
		max_chunk_size = MAX_SHA_ENGINE_CHUNK_SIZE_BSIZE128;
		break;

#if HAVE_HW_SHA3
	case 72U:
	case 168U:
	case 144U:
		max_chunk_size = 0xFFFFC0U;	/* 16777152 */
		break;
	case 104U:
		max_chunk_size = 0xFFFFD8U;	/* 16777176 */
		break;
	case 136U:
		max_chunk_size = 0xFFFF88U;	/* 16777096 */
		break;
#endif /* HAVE_HW_SHA3 */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*max_chunk_size_p = max_chunk_size;

fail:
	return ret;
}

/**
 * @brief argument checker for the SHA process call
 *
 * @param ac context containing this CCC call state
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t engine_sha_process_arg_check(const async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t max_chunk_size = 0U;

	LTRACEF("entry\n");

	if ((NULL == ac) || (NULL == ac->econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (0U == ac->econtext->hash_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("SHA digest size not set\n"));
	}

	if (SHA_ASYNC_MAGIC != ac->magic) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("SHA AC magic: 0x%x", ac->magic));
	}

	ret = se_engine_get_sha_max_chunk_size(ac->econtext, &max_chunk_size);
	CCC_ERROR_CHECK(ret);

	/* Engine can only handle up to 2^24 bytes in one call
	 * but needs to be sha block size multiples.
	 */
	if (ac->input.input_size > max_chunk_size) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("SHA input chunk size too big: 0x%x\n",
					     ac->input.input_size));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if SE_NULL_SHA_DIGEST_FIXED_RESULTS
/**
 * @brief T18X SHA engine can not callwlate SHA digest results of zero
 * length input data.
 *
 * So use the fixed standard values from this function when
 * SE_NULL_SHA_DIGEST_FIXED_RESULTS is nonzero.
 *
 * @param econtext sha engine context for the operation
 * @param hash_result result will get copied here
 * @param rbad FI state variable, set to NO_ERROR on success.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t fixed_sha_empty_digest(const struct se_engine_sha_context *econtext,
				       uint8_t *hash_result,
				       status_t *rbad)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;
	const uint8_t *rvec = NULL;

	const uint8_t null_sha1[]   =
		{ 0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,
		  0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,
		  0xaf,0xd8,0x07,0x09 };
	const uint8_t null_sha224[] =
		{ 0xd1,0x4a,0x02,0x8c,0x2a,0x3a,0x2b,0xc9,
		  0x47,0x61,0x02,0xbb,0x28,0x82,0x34,0xc4,
		  0x15,0xa2,0xb0,0x1f,0x82,0x8e,0xa6,0x2a,
		  0xc5,0xb3,0xe4,0x2f };
	const uint8_t null_sha256[] =
		{ 0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,
		  0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
		  0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,
		  0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55 };
	const uint8_t null_sha384[] =
		{ 0x38,0xb0,0x60,0xa7,0x51,0xac,0x96,0x38,
		  0x4c,0xd9,0x32,0x7e,0xb1,0xb1,0xe3,0x6a,
		  0x21,0xfd,0xb7,0x11,0x14,0xbe,0x07,0x43,
		  0x4c,0x0c,0xc7,0xbf,0x63,0xf6,0xe1,0xda,
		  0x27,0x4e,0xde,0xbf,0xe7,0x6f,0x65,0xfb,
		  0xd5,0x1a,0xd2,0xf1,0x48,0x98,0xb9,0x5b };
	const uint8_t null_sha512[] =
		{ 0xcf,0x83,0xe1,0x35,0x7e,0xef,0xb8,0xbd,
		  0xf1,0x54,0x28,0x50,0xd6,0x6d,0x80,0x07,
		  0xd6,0x20,0xe4,0x05,0x0b,0x57,0x15,0xdc,
		  0x83,0xf4,0xa9,0x21,0xd3,0x6c,0xe9,0xce,
		  0x47,0xd0,0xd1,0x3c,0x5d,0x85,0xf2,0xb0,
		  0xff,0x83,0x18,0xd2,0x87,0x7e,0xec,0x2f,
		  0x63,0xb9,0x31,0xbd,0x47,0x41,0x7a,0x81,
		  0xa5,0x38,0x32,0x7a,0xf9,0x27,0xda,0x3e };

#if HAVE_NIST_TRUNCATED_SHA2
	const uint8_t null_sha512_224[] = {
		0x6e, 0xd0, 0xdd, 0x02, 0x80, 0x6f, 0xa8, 0x9e,
		0x25, 0xde, 0x06, 0x0c, 0x19, 0xd3, 0xac, 0x86,
		0xca, 0xbb, 0x87, 0xd6, 0xa0, 0xdd, 0xd0, 0x5c,
		0x33, 0x3b, 0x84, 0xf4,
	};

	const uint8_t null_sha512_256[] = {
		0xc6, 0x72, 0xb8, 0xd1, 0xef, 0x56, 0xed, 0x28,
		0xab, 0x87, 0xc3, 0x62, 0x2c, 0x51, 0x14, 0x06,
		0x9b, 0xdd, 0x3a, 0xd7, 0xb8, 0xf9, 0x73, 0x74,
		0x98, 0xd0, 0xc0, 0x1e, 0xce, 0xf0, 0x96, 0x7a,
	};
#endif /* HAVE_NIST_TRUNCATED_SHA2 */

	LTRACEF("entry\n");

	*rbad = ERR_BAD_STATE;

	if ((NULL == econtext) || (NULL == hash_result)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	hlen = econtext->hash_size;

	switch (econtext->hash_algorithm) {
	case TE_ALG_SHA1:
		rvec = null_sha1;
		break;
	case TE_ALG_SHA224:
		rvec = null_sha224;
		break;
	case TE_ALG_SHA256:
		rvec = null_sha256;
		break;
	case TE_ALG_SHA384:
		rvec = null_sha384;
		break;
	case TE_ALG_SHA512:
		rvec = null_sha512;
		break;
#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
		rvec = null_sha512_224;
		hlen = 28U;
		break;
	case TE_ALG_SHA512_256:
		rvec = null_sha512_256;
		hlen = 32U;
		break;
#endif
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*rbad = NO_ERROR;
	se_util_mem_move(&hash_result[0], &rvec[0], hlen);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Does CCC have pre-configured values for zero length input for this algorithm?
 *
 * Some HW versions do not know how to handle digest of
 * zero length data. For those, use SW fixed results; as
 * specified in the config file.
 *
 * Only SHA-1 and SHA-2 related empty digests pre-configured.
 *
 * @param algo algorithm to check for fixed zero length input response.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static bool known_empty_digest(te_crypto_algo_t algo)
{
	bool call_ok = IS_SHA2(algo);

	LTRACEF("entry\n");

#if HAVE_NIST_TRUNCATED_SHA2
	call_ok = IS_NIST_TRUNCATED_DIGEST(algo) || call_ok;
#endif

#if HAVE_SHA1
	call_ok = (algo == TE_ALG_SHA1) || call_ok;
#endif
	LTRACEF("exit\n");
	return call_ok;
}
#endif /* SE_NULL_SHA_DIGEST_FIXED_RESULTS */

/* On dofinal copy the result from context to the destination result if so requested.
 * The intermediate result is stored in the econtext->tmphash buffer.
 */
static status_t sha_get_result_locked(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	/* This is correct for SHA-2 and SHA_3, but not for truncated digests
	 */
	uint32_t result_size = ac->econtext->hash_size;

	LTRACEF("entry\n");

	if (NULL != ac->input.dst) {
		if (ac->input.dst != ac->econtext->tmphash) {
#if HAVE_NIST_TRUNCATED_SHA2
			/* If truncated digest => truncate result */
			switch (ac->econtext->hash_algorithm) {
			case TE_ALG_SHA512_224:
				result_size = (224U / 8U);
				break;
			case TE_ALG_SHA512_256:
				result_size = (256U / 8U);
				break;
			default:
				/* Here result_size is ac->econtext->hash_size */
				(void)result_size;
				break;
			}
#endif /* HAVE_NIST_TRUNCATED_SHA2 */

			if (ac->input.output_size < result_size) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						     LTRACEF("DST too small for sha result (%u < %u)\n",
							     ac->input.output_size,
							     result_size));
			}
			se_util_mem_move(ac->input.dst, ac->econtext->tmphash,
					 result_size);
		}
		ac->input.output_size = result_size;
	}

	LOG_HEXBUF("SHA RESULT", ac->econtext->tmphash, result_size);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_read_engine_result_locked(const async_sha_ctx_t *ac,
					      status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	bool fixed_value = false;

	LTRACEF("entry\n");

	*rbad_p = ERR_BAD_STATE;

#if SE_NULL_SHA_DIGEST_FIXED_RESULTS
	if (BOOL_IS_TRUE(ac->econtext->is_last) &&
	    BOOL_IS_TRUE(ac->econtext->is_first) &&
	    (0U == ac->econtext->byte_count) &&
	    known_empty_digest(ac->econtext->hash_algorithm)) {
		ret = fixed_sha_empty_digest(ac->econtext,
					     &ac->econtext->tmphash[0],
					     rbad_p);
		CCC_ERROR_CHECK(ret);
		CCC_ERROR_CHECK(*rbad_p);
		fixed_value = true;
	}
#endif /* SE_NULL_SHA_DIGEST_FIXED_RESULTS */

	if (!BOOL_IS_TRUE(fixed_value)) {
		ret = se_wait_engine_free(ac->econtext->engine);
		CCC_ERROR_CHECK(ret);

		*rbad_p = NO_ERROR;

#if HAVE_HW_SHA3
		/* IMPLEMENTATION RESTRICTION CHECK */

		/*
		 * FIXME:
		 * Current CCC code does not support longer output, since the result is
		 * copied to tmphash below => it would not fit there.
		 * SHAKE output could be longer (arbitrary long) so, preferrably, copy
		 * final result directly to client output buffer...
		 */
		if (IS_SHAKE(ac->econtext->hash_algorithm)) {
			if (ac->econtext->hash_size > (MAX_SHAKE_XOF_BITLEN_IN_REGISTER / 8U)) {
				CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
						     LTRACEF("SHAKE output does not fit in output registers: %u\n",
							     ac->econtext->hash_size));
			}
		}
#endif /* HAVE_HW_SHA3 */

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != ac->gen_op_perf_usec) {
			show_se_gen_op_entry_perf("SHA", get_elapsed_usec(ac->gen_op_perf_usec));
		}
#endif

		SHA_ERR_CHECK(ac->econtext->engine, "SHA DONE");

		/* Read the intermediate/final HASH_RESULT and store it to memory
		 * (to econtext->tmphash).
		 *
		 * This is allowed when
		 * - this is the last SHA operation and this is the final result
		 * - the amount of data now passed is a multiple of the digest block size
		 *
		 * The callers must make sure one of these conditions is held if the
		 * sha operation has more data.
		 */
#if SHA_DST_MEMORY
		/* old HW:
		 * Intermediate results are always in HASH_REGS even if the
		 * final result is written to DST=MEM. Callee handles this.
		 *
		 * New HW:
		 * Intermediate results are in HASH_REGS unless SHA_DST_MEMORY is set.
		 * For MEM output the intermediate and final results are in memory buffer.
		 * Intermediate values get word swapped, but no word swapping is done
		 * for the final result (word swapping required only for SHA-384 and
		 * SHA-512, since they internally operate on 64 bit ints in little endian).
		 *
		 * Callee handles this.
		 */
		se_sha_engine_process_read_result_locked(ac->econtext,
							 ac->aligned_result,
							 ac->swap_result_words);
#else
		se_sha_engine_process_read_result_locked(ac->econtext,
							 NULL,
							 ac->swap_result_words);
#endif
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_SHA
/**
 * @brief Non-blocking check if ASYNC SHA engine used for operation is idle
 *
 * @param ac Async SHA call context
 * @param is_idle_p
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t engine_sha_locked_async_done(const async_sha_ctx_t *ac, bool *is_idle_p)
{
	status_t ret = NO_ERROR;
	bool use_hw = true;

	LTRACEF("entry\n");

	ret = engine_sha_process_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (NULL == is_idle_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Async SHA operation has not been started\n"));
	}

#if SE_NULL_SHA_DIGEST_FIXED_RESULTS
	if (BOOL_IS_TRUE(ac->econtext->is_last) &&
	    BOOL_IS_TRUE(ac->econtext->is_first) &&
	    (0U == ac->econtext->byte_count) &&
	    known_empty_digest(ac->econtext->hash_algorithm)) {
		*is_idle_p = true;
		use_hw = false;
		LTRACEF("Async SHA poll => idle with NULL input\n");
	}
#endif

	if (BOOL_IS_TRUE(use_hw)) {
		ret = se_async_check_engine_free(ac->econtext->engine, is_idle_p);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_SHA */

#if SHA_DST_MEMORY
static void sha_engine_read_mem_result_locked(struct se_engine_sha_context *econtext,
					      uint8_t *aligned_memory_result,
					      uint32_t result_size,
					      bool swap_result_words)
{
	uint32_t *hash32 = &econtext->tmphash32[0];
	uint8_t *p = aligned_memory_result;

	SE_CACHE_ILWALIDATE((VIRT_ADDR_T)p, DMA_ALIGN_SIZE(result_size));

	if (BOOL_IS_TRUE(econtext->is_last)) {
		se_util_mem_move(econtext->tmphash, p, result_size);
	} else {
		uint32_t inx = 0U;

		for (inx = 0U; inx < (result_size / BYTES_IN_WORD); inx++) {
			uint32_t save_index = inx;

			/* Don't bother to swap intermediate result bytes to big endian.
			 * They only get written back to hash regs as little endian.
			 */
			uint32_t val = se_util_buf_to_u32(p, false);

			if (BOOL_IS_TRUE(swap_result_words)) {
				if ((inx & 0x1U) != 0U) {
					save_index--;
				} else {
					save_index++;
				}
			}

			p = &p[BYTES_IN_WORD];
			hash32[save_index] = val;
		}
	}

	se_util_mem_set(p, 0U, result_size);
}
#endif /* SHA_DST_MEMORY */

/* Actually the intermediate context word size is:
 *  5 => SHA-1
 *  8 => SHA-224, SHA-256
 * 16 => SHA-384, SHA-512 and SHA-512 truncated.
 *
 * 50 => SHA3 (incl. SHAKEs)
 *
 * But HW recommends saving, restoring and clearing 16 words for SHA1/SHA2 and 50 words
 * for SHA3 (keccak) intermediate and final results. FINAL result lengths are defined by
 * algorithm result size, not by this.
 *
 * FIXME: Implementation restricts SHAKE output to max 1600 bits
 * (length of 50 registers) even when SHA_DST_MEMORY==1 writes SHAKE
 * final (long) result to memory. HW restricts the SHA_DST_MEMORY==0
 * case to 50 words: that can not be fixed by SW.
 */
void se_sha_engine_process_read_result_locked(struct se_engine_sha_context *econtext,
					      uint8_t *aligned_memory_result,
					      bool swap_result_words)
{
	uint32_t result_size = econtext->hash_size;
	uint32_t context_wsize = (SHA_CONTEXT_STATE_BITS / BITS_IN_WORD);
	uint32_t inx = 0U;
	bool endian_swap_hash_regs = true;

	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	(void)aligned_memory_result;

#if HAVE_HW_SHA3
	if (IS_SHA3(econtext->hash_algorithm) || IS_SHAKE(econtext->hash_algorithm)) {
		context_wsize = (KECCAK_CONTEXT_STATE_BITS / BITS_IN_WORD);
		endian_swap_hash_regs = false;
	}
#endif

	if (!BOOL_IS_TRUE(econtext->is_last)) {
		/* Intermediate operations need to save the context words
		 */
		result_size = (context_wsize * BYTES_IN_WORD);
	}

	CCC_LOOP_BEGIN {
		uint32_t *hash32 = &econtext->tmphash32[0];

#if SHA_DST_MEMORY
		/* If there is a memory buffer, read the intermediate and final results from there,
		 * T23X does not store intermediates in HASH_REGS at all. Earlier SEs saved them in both.
		 */
		if (NULL != aligned_memory_result) {
			sha_engine_read_mem_result_locked(econtext,
							  aligned_memory_result,
							  result_size,
							  swap_result_words);
			LOG_HEXBUF("==> SHA MEM result",
				   econtext->tmphash, result_size);

			CCC_LOOP_STOP;
		}
#endif /* SHA_DST_MEMORY */

		for (inx = 0U; inx < (result_size / BYTES_IN_WORD); inx++) {
			uint32_t save_index = inx;
			uint32_t val = tegra_engine_get_reg(econtext->engine,
							    SE0_SHA_HASH_RESULT_0 + (BYTES_IN_WORD*inx));
			if (BOOL_IS_TRUE(endian_swap_hash_regs)) {
				/* SHA-1 and SHA-2 HW implementation registers
				 * are in little endian byte order,
				 * so these words get byte swapped.
				 */
				val = se_util_swap_word_endian(val);

				/* This is never done unless word is also
				 * swapped above.
				 */
				if (BOOL_IS_TRUE(swap_result_words)) {
					if ((inx & 0x1U) != 0U) {
						save_index--;
					} else {
						save_index++;
					}
				}
			}

			hash32[save_index] = val;
		}
		LOG_HEXBUF("normal SHA interm/final", econtext->tmphash, result_size);

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

#if CLEAR_HW_RESULT
	LTRACEF("Clearing SHA context %u words\n", context_wsize);
	for (inx = 0U; inx < context_wsize; inx++) {
		tegra_engine_set_reg(econtext->engine, SE0_SHA_HASH_RESULT_0 + (BYTES_IN_WORD*inx), 0U);
	}
#endif

	LTRACEF("exit\n");
}

/**
 * @brief Write intermediate results back to HASH_REGs when continuing
 *  sha digest operations.
 *
 * @param econtext sha engine context to continue operation with.
 * @param value32 word aligned value to write to HASH_REGs
 * @param num_words Number of words in the written value
 * @param swap_bytes Swap bytes in each written word
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t se_write_sha_value_regs(const struct se_engine_sha_context *econtext,
					const uint32_t *value32, uint32_t num_words,
					bool swap_bytes)
{
	status_t ret = NO_ERROR;

	/* Load intermediate SHA digest result to SHA:HASH_RESULT(0..15) to
	 * continue the SHA callwlation and tell the SHA engine
	 * to use it.
	 */
	uint32_t inx = 0U;
	uint32_t val;

	LTRACEF("entry\n");

	for (; inx < num_words; inx++) {
		uint32_t value_index = inx;

		if (BOOL_IS_TRUE(swap_bytes)) {
			/* HW generated SHA continuation values in value32 need
			 * to be byte swapped (swap 32 bit word bytes) and
			 * write back to engine.
			 */
			val = se_util_swap_word_endian(value32[value_index]);
		} else {
			/* Pre-computed digest init values must not be swapped
			 * here.
			 */
			val = value32[value_index];
		}

		tegra_engine_set_reg(econtext->engine,
				     SE0_SHA_HASH_RESULT_0 + (BYTES_IN_WORD * inx),
				     val);
	}

	FI_BARRIER(u32, inx);
	if (num_words != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_NIST_TRUNCATED_SHA2
/**
 * @brief Setup engine result register to callwlate NIST truncated SHA digests
 * SHA-512/256 and SHA-512/224.
 *
 * See http lwlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
 *  for SHA512/t truncated digest predefined IV values (approved algorithms).
 *
 * The 64 bit NIST table big endian values will be written as two
 * 32 bit word values to the HW.
 *
 * These values could be callwlated from a SHA-512 truncated H0 base table but
 * no reason as long as we support only the two approved truncated digests
 * SHA-512/224 and SHA-512/256 => use the values from the NIST spec.
 *
 * NOTE: The 64 bit integers are stored in LITTLE ENDIAN format in memory.
 * When casting the table entries to 32 bit integers they must be written
 * to the HW intermediate register like this
 *
 * Table64[] => {
 *   0x0001020304050607ULL,
 * }
 *
 * Table entry represented in memory as two
 * successive LE words:
 *  07060504
 *  03020100
 *
 * Write to register in that byte order (do not swap bytes):
 *
 * write_word(0x04050607)
 * write_word(0x00010203)
 *
 * @param econtext sha engine context to callwlate the truncated digest with.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t se_setup_nist_truncated_digest(const se_engine_sha_context_t *econtext)
{
	status_t ret = NO_ERROR;
	const uint32_t *H0 = NULL;
	uint32_t words = 0U;

	LTRACEF("entry\n");

	/* 64 bit values colwerted into a pair of
	 * 32 bit BE entries. Words swapped.
	 */
	const uint32_t H0_512_224_BE[] = {
		0x19544da2U, 0x8c3d37c8U,
		0x89dcd4d6U, 0x73e19966U,
		0x32ff9c82U, 0x1dfab7aeU,
		0x582f9fcfU, 0x679dd514U,
		0x7bd44da8U, 0x0f6d2b69U,
		0x04c48942U, 0x77e36f73U,
		0x6a1d36c8U, 0x3f9d85a8U,
		0x91d692a1U, 0x1112e6adU,
	};

	const uint32_t H0_512_256_BE[] = {
		0xfc2bf72lw, 0x22312194U,
		0xc84c64c2U, 0x9f555fa3U,
		0x6f53b151U, 0x2393b86bU,
		0x5940eabdU, 0x96387719U,
		0xa88effe3U, 0x96283ee2U,
		0x53863992U, 0xbe5e1e25U,
		0x2c85b8aaU, 0x2b0199flw,
		0x81c52ca2U, 0x0eb72ddlw,
	};

	switch (econtext->hash_algorithm) {
	case TE_ALG_SHA512_224:
		H0 = H0_512_224_BE;
		words = sizeof_u32(H0_512_224_BE) / BYTES_IN_WORD;
		break;

	case TE_ALG_SHA512_256:
		H0 = H0_512_256_BE;
		words = sizeof_u32(H0_512_256_BE) / BYTES_IN_WORD;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/**
	 * Write the selected 64 bit values as two 32 bit words
	 * and don't swap the bytes in the words before
	 * writing them to the hash register.
	 *
	 * Use the value as SHA digest initialization value
	 * for the split digests since the HW does not know
	 * these init vectors internally.
	 */
	ret = se_write_sha_value_regs(econtext, H0, words, CFALSE);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_NIST_TRUNCATED_SHA2 */

status_t se_sha_engine_set_msg_sizes(const struct se_data_params *input_params,
				     const struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;

	uint64_t size_left_bits = 0U;
	uint32_t size_left_bits0 = 0U;
	uint32_t size_left_bits1 = 0U;

	uint32_t size_left = input_params->input_size;

	LTRACEF("entry\n");

	CCC_SAFE_MULT_U64(size_left_bits, (uint64_t)size_left, 8UL);

	/* SE HW expects the SE0_SHA_MSG_LEFT value to be larger
	 * than the IN_ADDR_HI field SZ if it this is not the
	 * last sha call in the split operation digest sequence
	 *
	 * So, make MSG_LEFT one byte larger. The caller must take
	 * care of all boundary conditions (e.g. like the dofinal has
	 * no data, passing in only block size data, etc).
	 *
	 * NOTE: If is_first and dofinal() contain no data => do
	 *       SHA(NULL). In the new units the engine has been modified
	 *       to callwlate these values correctly. In the old
	 *       devices (e.g. T186) the code returns
	 *       the values from hard coded SHA-1/SHA-2 tables in
	 *	 fixed_sha_empty_digest()
	 */
	if (!BOOL_IS_TRUE(econtext->is_last)) {
		CCC_SAFE_ADD_U64(size_left_bits, size_left_bits, 8UL);
	}

	size_left_bits0 = se_util_lsw_u64(size_left_bits);
	size_left_bits1 = se_util_msw_u64(size_left_bits);

	/* Set up total message length (SE_SHA_MSG_LENGTH is specified in bits).
	 *
	 * Zero out MSG_LENGTH2-3 and MSG_LEFT2-3. This makes the maximum size
	 * digested by the CCC <= 4gbits (536870912 bytes).
	 *
	 * See SE IAS, topic: "Special Notes for use cases where the
	 * Total Message Length is not available in the beginning"
	 *
	 * Make sure SHA_MSG_LENGTH is never zero, since new HW supports
	 * callwlating NULL SHA digests with the engine. To avoid incorrectly
	 * treating the message as zero length, set the intermediate
	 * SHA_MSG_LENGTH == SHA_MSG_LEFT. The last SHA_MSG_LENGTH must still
	 * be set to the number of bits handled by the digest.
	 */
	if (BOOL_IS_TRUE(econtext->is_last)) {

		// XXX FIXME => set byte_count in handlers !!!

		uint64_t input_size_bits = (uint64_t)(econtext->byte_count) * 8U;
		uint32_t input_size_bits0 = se_util_lsw_u64(input_size_bits);
		uint32_t input_size_bits1 = se_util_msw_u64(input_size_bits);

		tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_0, input_size_bits0);
		tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_1, input_size_bits1);
	} else {
		tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_0, size_left_bits0);
		tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_1, size_left_bits1);
	}

	/* Set the high order words zero in both cases.
	 */
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_2, 0U);
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LENGTH_3, 0U);

	/* Continuing with another update requires
	 * size_left be bigger than byte size; this is now taken care of above.
	 */
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LEFT_0, size_left_bits0);
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LEFT_1, size_left_bits1);
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LEFT_2, 0U);
	tegra_engine_set_reg(econtext->engine, SE0_SHA_MSG_LEFT_3, 0U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if SHA_DST_MEMORY
/*
 * SHA engine usually writes to HASH_REG, not to memory.
 *
 * If output buffer is provided it MUST BE DMA aligned (both address and length)
 */
static status_t write_sha_engine_out_regs(
		const struct se_data_params *input_params,
		const struct se_engine_sha_context *econtext,
		uint32_t md_data_size,
		PHYS_ADDR_T paddr_md_data_addr)
{
	status_t ret = NO_ERROR;
	uint32_t md_out_size = 0U;

	PHYS_ADDR_T paddr_out_addr = PADDR_NULL;

	(void)md_data_size;
	(void)paddr_md_data_addr;

	LTRACEF("entry\n");

	if ((NULL != input_params->dst) && (0U != input_params->output_size)) {
		uint32_t out_hi = 0U;

		md_out_size = input_params->output_size;

		paddr_out_addr = DOMAIN_VADDR_TO_PADDR(TE_CRYPTO_DOMAIN_KERNEL,
						       input_params->dst);

		SE_PHYS_RANGE_CHECK(paddr_out_addr, md_out_size);
		SE_CACHE_ILWALIDATE((VIRT_ADDR_T)input_params->dst, DMA_ALIGN_SIZE(md_out_size));

		if (!IS_40_BIT_PHYSADDR(paddr_out_addr)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("In address out of SE 40 bit range\n"));
		}

		/* Write the OUT_ADDR (LOWBITS) */
		tegra_engine_set_reg(econtext->engine, SE0_SHA_OUT_ADDR_0,
				     se_util_lsw_u64((uint64_t)paddr_out_addr));

		out_hi = LW_FLD_SET_DRF_NUM(SE0, SHA_OUT_ADDR_HI, MSB,
					    SE_GET_32_UPPER_PHYSADDR_BITS(paddr_out_addr),
					    out_hi);
		out_hi = LW_FLD_SET_DRF_NUM(SE0, SHA_OUT_ADDR_HI, SZ, md_out_size,
					    out_hi);

		/* Write the OUT_ADDR_HI (HIBITS | SIZE) */
		tegra_engine_set_reg(econtext->engine, SE0_SHA_OUT_ADDR_HI_0, out_hi);
	}

#if HAVE_SE_APERTURE
	ret = se_set_aperture(paddr_md_data_addr, md_data_size,
			      paddr_out_addr, md_out_size);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* SHA_DST_MEMORY */

status_t se_write_sha_engine_inout_regs(const struct se_data_params *input_params,
					const struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	uint32_t md_data_size = 0U;
	PHYS_ADDR_T paddr_md_data_addr = PADDR_NULL;

	LTRACEF("entry\n");

	md_data_size = input_params->input_size;

	LTRACEF("SHA input input_params->src %p md_data_size (%u) (dumped below)\n",
		input_params->src, md_data_size);

	if ((NULL != input_params->src) && (md_data_size > 0U)) {
		LOG_HEXBUF("SHA engine input", input_params->src, md_data_size);

		paddr_md_data_addr = DOMAIN_VADDR_TO_PADDR(econtext->domain,
							   input_params->src);
		SE_PHYS_RANGE_CHECK(paddr_md_data_addr, md_data_size);

		SE_CACHE_FLUSH((VIRT_ADDR_T)input_params->src, DMA_ALIGN_SIZE(md_data_size));

		if (!IS_40_BIT_PHYSADDR(paddr_md_data_addr)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("In address out of SE 40 bit range\n"));
		}
	} else {
		md_data_size = 0U;
	}

	/* Program input address and HI register. */
	tegra_engine_set_reg(econtext->engine, SE0_SHA_IN_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_md_data_addr));

	/* Program input message buffer size (24 bits) into SE0_SHA_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 */
	se_config_reg = 0U;

	se_config_reg = LW_FLD_SET_DRF_NUM(SE0, SHA_IN_ADDR_HI, SZ,
					   md_data_size, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0, SHA_IN_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr_md_data_addr),
					   se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_SHA_IN_ADDR_HI_0, se_config_reg);

#if SHA_DST_MEMORY
	/* SHA engine usually writes to HASH_REG, not to memory!
	 *
	 * If output buffer is provided it MUST BE DMA aligned (both address and length)
	 */
	ret = write_sha_engine_out_regs(input_params, econtext, md_data_size,
					paddr_md_data_addr);
	CCC_ERROR_CHECK(ret);
#else

#if HAVE_SE_APERTURE
	/* HASH_REGS do not need output aperture */
	ret = se_set_aperture(paddr_md_data_addr, md_data_size,
			      PADDR_NULL, 0U);
	CCC_ERROR_CHECK(ret);
#endif
#endif /* SHA_DST_MEMORY */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_task_config_init(const struct se_engine_sha_context *econtext,
				     uint32_t *sha_task_config_p)
{
	status_t ret = NO_ERROR;
	uint32_t sha_task_config = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == sha_task_config_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	sha_task_config = LW_DRF_DEF(SE0, SHA_TASK_CONFIG, HW_INIT_HASH, ENABLE);

#if HAVE_NIST_TRUNCATED_SHA2
	if (IS_NIST_TRUNCATED_DIGEST(econtext->hash_algorithm)) {
		ret = se_setup_nist_truncated_digest(econtext);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to set up NIST truncated digest values\n"));

		sha_task_config = LW_DRF_DEF(SE0, SHA_TASK_CONFIG, HW_INIT_HASH, DISABLE);
	}
#endif

	*sha_task_config_p = sha_task_config;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_set_sha_task_config(const struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t sha_task_config_reg = 0U;
	bool endian_swap = CTRUE;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(econtext->is_first)) {
		ret = sha_task_config_init(econtext, &sha_task_config_reg);
		CCC_ERROR_CHECK(ret);
	} else {
		/* This is always a digest context previously saved, it gets
		 * restored here.
		 */
		uint32_t context_wsize = (SHA_CONTEXT_STATE_BITS / BITS_IN_WORD);

#if HAVE_HW_SHA3
		if (IS_SHA3(econtext->hash_algorithm) || IS_SHAKE(econtext->hash_algorithm)) {
			/* SHA3/SHAKE result regs are in big endian in all cases
			 */
			context_wsize = (KECCAK_CONTEXT_STATE_BITS / BITS_IN_WORD);
			endian_swap = CFALSE;
		}
#endif

		/* Even with DST=MEM the intermediate SHA engine results
		 * are in SHA_RESULT regs. These intermediate values must not be
		 * WORD swapped when continuing the operation from intermediate
		 * result (intermediate results were not word swapped on context save).
		 *
		 * SHA3/SHAKE results are in big endian and must not be swapped.
		 */
		ret = se_write_sha_value_regs(econtext,
					      (const uint32_t *)&econtext->tmphash32[0],
					      context_wsize,
					      endian_swap);
		CCC_ERROR_CHECK(ret);

		sha_task_config_reg = LW_DRF_DEF(SE0, SHA_TASK_CONFIG, HW_INIT_HASH, DISABLE);
	}

	tegra_engine_set_reg(econtext->engine, SE0_SHA_TASK_CONFIG_0, sha_task_config_reg);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_HW_SHA3
static status_t shake_set_hash_length_reg(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t xof_bits = ac->econtext->shake_xof_bitlen;

	LTRACEF("entry\n");

	if (0U == xof_bits) {
		/* Context init sets defaults, modified by caller init
		 * if provided. Zero is not valid here.
		 */
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("SHAKE XOF length not set\n"));
	}

#if SHA_DST_MEMORY
	if (xof_bits > MAX_SHAKE_XOF_BITLEN) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("SHAKE XOF %u bits larger than HW supports\n",
					     xof_bits));
	}
#else
	if (xof_bits > MAX_SHAKE_XOF_BITLEN_IN_REGISTER) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("SHAKE XOF %u bits do not fit in HASH_REG\n",
					     xof_bits));
	}
#endif
	/* Write SHAKE XOF bit length to SE0_SHA_HASH_LENGTH_0 */
	tegra_engine_set_reg(ac->econtext->engine, SE0_SHA_HASH_LENGTH_0, xof_bits);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HW_SHA3 */

static status_t start_sha_engine_setup(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t se_config_reg = 0U;
	uint32_t sha_pkt_mode = 0U;

	LTRACEF("entry\n");

	CCC_OBJECT_ASSIGN(input, ac->input);

	/* SE0 SHA unit requires SHA-384 and SHA-512 final result
	 * words to be swapped. This is because the SHA-384 and
	 * SHA-512 algos operate on 64 bit data instead of 32 bit data
	 * like the smaller digests.  When the 64 bit LE result is
	 * read in 32 bit words from the SHA result regs they get
	 * "swapped" and need to be swapped back again to correct BE
	 * word order.
	 */
	ac->swap_result_words = false;

	/* Callwlate output size */
	switch (ac->econtext->hash_algorithm) {
#if HAVE_SHA1
	case TE_ALG_SHA1:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA1;
		break;
#endif
#if HAVE_SHA224
	case TE_ALG_SHA224:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA224;
		break;
#endif
	case TE_ALG_SHA256:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA256;
		break;
	case TE_ALG_SHA384:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA384;
#if SHA_DST_MEMORY
		/* Only swap intermediate words for writing them back to HASH_REGS
		 * when continuing. Reverse of the HASH_REG case.
		 */
		ac->swap_result_words = ! ac->econtext->is_last;
#else
		ac->swap_result_words = ac->econtext->is_last;
#endif
		break;
#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
	case TE_ALG_SHA512_256:
#endif
	case TE_ALG_SHA512:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA512;
#if SHA_DST_MEMORY
		/* Only swap intermediate words for writing them back to HASH_REGS
		 * when continuing. Reverse of the HASH_REG case.
		 */
		ac->swap_result_words = ! ac->econtext->is_last;
#else
		ac->swap_result_words = ac->econtext->is_last;
#endif
		break;
#if HAVE_HW_SHA3
		/* XXX FIXME: Do these need any word swapping when result
		 * is in a register????
		 */
	case TE_ALG_SHA3_224:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA3_224;
		break;
	case TE_ALG_SHA3_256:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA3_256;
		break;
	case TE_ALG_SHA3_384:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA3_384;
		break;
	case TE_ALG_SHA3_512:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHA3_512;
		break;
	case TE_ALG_SHAKE128:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHAKE128;
		break;
	case TE_ALG_SHAKE256:
		sha_pkt_mode = SE_MODE_PKT_SHAMODE_SHAKE256;
		break;
#endif /* HAVE_HW_SHA3 */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0, SHA_CONFIG, ENC_MODE, sha_pkt_mode, se_config_reg);

#if SHA_DST_MEMORY
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DST, MEMORY,
					   se_config_reg);
#else
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DST, HASH_REG,
					   se_config_reg);
#endif
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DEC_ALG, NOP,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, SHA,
					   se_config_reg);

	/* Write to SE0_SHA_CONFIG register. */
	tegra_engine_set_reg(ac->econtext->engine, SE0_SHA_CONFIG_0, se_config_reg);

#if HAVE_HW_SHA3
	if (IS_SHAKE(ac->econtext->hash_algorithm)) {
		ret = shake_set_hash_length_reg(ac);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_HW_SHA3 */

	/* SHA engine result are written to HASH_REGs, not
	 * to caller memory buffer.
	 */
	input.dst = NULL;

#if SHA_DST_MEMORY
	/* Except in this case (host1x, etc, reasons).
	 *
	 * ac->aligned_result is a DMA aligned output buffer
	 * where SE SHA DMA can safely write to.
	 */
	input.dst = ac->aligned_result;
	input.output_size = MAX_MD_ALIGNED_SIZE;
#endif /* SHA_DST_MEMORY */

	ret = se_write_sha_engine_inout_regs(&input, ac->econtext);
	CCC_ERROR_CHECK(ret);

	ret = se_set_sha_task_config(ac->econtext);
	CCC_ERROR_CHECK(ret);

	ret = se_sha_engine_set_msg_sizes(&ac->input, ac->econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief start the SHA HW operation after setting the SHA engine
 * operation registers (directly or calling other functions).
 *
 * Program SHA engine to get a digest value and start the engine.
 *
 * @param ac async SHA call context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t se_start_sha_engine(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = start_sha_engine_setup(ac);
	CCC_ERROR_CHECK(ret);

	/* Just leave the SHA digest intermediate/final results to HASH_REG.
	 * It gets copied out below (and may need fixing anyway)
	 *
	 * I do not think this is a real perf issue in TZ operations...
	 */
	SHA_ERR_CHECK(ac->econtext->engine, "SHA START");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	ac->gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif
	/*
	 * Issue START command and true for last chunk.
	 */
	ret = tegra_start_se0_operation_locked(ac->econtext->engine, 0U);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Deal with operations done before starting the SHA HW engine,
 * then start the SHA engine and return.
 *
 * Note that synchronous SHA calls are just doing like this:
 * - construct an async this call context
 * - start async SHA
 * - finish async SHA
 *
 * This means that sync calls are started like async calls that are immediately
 * polled with a blocking finalize call until the SHA completes.
 *
 * @param ac this SHA call context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t sha_process_locked_async_start(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	bool use_hw = true;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_sha_process_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					 CCC_ERROR_MESSAGE("SHA operation context already started\n"));
	}

	SHA_ERR_CHECK(ac->econtext->engine, "Before SHA setup");

#if SHA_DST_MEMORY
	if (NULL != ac->aligned_result) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("DMA buffer already allocated in SHA start\n"));
	}

	ac->aligned_result = CMTAG_MEM_GET_BUFFER(ac->econtext->cmem,
						  CMTAG_ALIGNED_DMA_BUFFER,
						  CCC_DMA_MEM_ALIGN,
						  DMA_ALIGN_SIZE(MAX_MD_ALIGNED_SIZE));
	if (NULL == ac->aligned_result) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}
#endif

	if (BOOL_IS_TRUE(ac->econtext->is_first)) {
		ac->econtext->byte_count = 0U;
	}

	CCC_SAFE_ADD_U32(ac->econtext->byte_count, ac->econtext->byte_count, \
			 ac->input.input_size);

#if SE_NULL_SHA_DIGEST_FIXED_RESULTS
	if (BOOL_IS_TRUE(ac->econtext->is_last) &&
	    (0U == ac->econtext->byte_count) &&
	    known_empty_digest(ac->econtext->hash_algorithm)) {
		use_hw = false;
	}
#endif

	if (BOOL_IS_TRUE(use_hw)) {
		ret = se_start_sha_engine(ac);
		CCC_ERROR_CHECK(ret);
	}

	ac->started = true;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Do a blocking poll waiting for the SHA engine to complete.
 *
 * Note that synchronous SHA calls are just doing like this:
 * - construct an async this call context
 * - start async SHA
 * - finish async SHA
 *
 * This means that sync calls are started like async calls that are immediately
 * polled with a blocking finalize call until the SHA completes.
 *
 * @param ac this SHA call context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t sha_process_locked_async_finish(async_sha_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

#if HAVE_SE_ASYNC_SHA
	ret = engine_sha_process_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Async SHA not started\n"));
	}

	/* This can be called only once */
	ac->started = false;
#endif

	ret = sha_read_engine_result_locked(ac, &rbad);
	CCC_ERROR_CHECK(ret);
	CCC_ERROR_CHECK(rbad);

	ac->econtext->is_first = false;

	/* On dofinal copy the result from context to the result if so requested.
	 * It is also stored in the econtext->tmphash array.
	 *
	 * On intermediate digests, nothing is output.
	 */
	if (BOOL_IS_TRUE(ac->econtext->is_last)) {
		ret = sha_get_result_locked(ac);
		CCC_ERROR_CHECK(ret);
	}

	FI_BARRIER(status, rbad);
fail:
#if SHA_DST_MEMORY
	if (NULL != ac->aligned_result) {
		se_util_mem_set(ac->aligned_result, 0U, MAX_MD_ALIGNED_SIZE);
		CMTAG_MEM_RELEASE(ac->econtext->cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  ac->aligned_result);
	}
#endif
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if ASYNC_FUNCTION_EXPOSE

status_t engine_sha_locked_async_start(async_sha_ctx_t *ac)
{
	return sha_process_locked_async_start(ac);
}

status_t engine_sha_locked_async_finish(async_sha_ctx_t *ac)
{
	return sha_process_locked_async_finish(ac);
}

#if HAVE_LONG_ASYNC_NAMES
/* Backwards compatibility only */
status_t engine_sha_process_locked_async_done(const async_sha_ctx_t *ac, bool *is_idle_p)
{
	return engine_sha_locked_async_done(ac, is_idle_p);
}

status_t engine_sha_process_block_locked_async_start(async_sha_ctx_t *ac)
{
	return engine_sha_locked_async_start(ac);
}

status_t engine_sha_process_block_locked_async_finish(async_sha_ctx_t *ac)
{
	return engine_sha_locked_async_finish(ac);
}
#endif /* HAVE_LONG_ASYNC_NAMES */

#endif /* ASYNC_FUNCTION_EXPOSE */

/**
 * @brief Do a synchronous SHA engine call.
 *
 * This SE TASK completes in this call (but the SHA may still not be
 * finalized, depending on if the call was a update() or dofinal()
 * call. SHA digests, like everything else in CCC crypto API must be
 * completed by a dofinal() call.
 *
 * This call creates an async SHA object and then starts a async SHA operation
 * and when that returns it immediately does a blocking poll to wait for the
 * SHA operation to complete.
 *
 * @param input_params Caller in/out data parameters.
 * @param econtext sha engine context used for the SHA operation.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t engine_sha_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;
	async_sha_ctx_t ac = { .magic = 0U };

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_OBJECT_ASSIGN(ac.input, *input_params);

	ac.econtext = econtext;
	ac.magic = SHA_ASYNC_MAGIC;

	ret = sha_process_locked_async_start(&ac);
	CCC_ERROR_CHECK(ret);

	ret = sha_process_locked_async_finish(&ac);
	CCC_ERROR_CHECK(ret);

fail:
	se_util_mem_set((uint8_t *)&ac, 0U, sizeof_u32(ac));

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
/**
 * @brief Unused function, not compiled in.
 */
void engine_sha_close_locked(void)
{
	DEBUG_ASSERT_HW_MUTEX;
	return;
}
#endif

#endif /* HAVE_SE_SHA */
