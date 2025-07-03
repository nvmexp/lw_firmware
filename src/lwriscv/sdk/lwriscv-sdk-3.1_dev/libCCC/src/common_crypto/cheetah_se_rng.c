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
 * driver) and this reg should then be set TZ access only ( which is
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

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#ifndef SE0_AES0_SPARE_0
/* This file only exists in T194 and newer; e.g. this define
 * has been moved to the private version.
 */
#include <arse0_private.h>
#endif

#include <crypto_ops.h>
#include <tegra_se_gen.h>
#include <tegra_se_aes.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if !HAVE_SE_AES
#if HAVE_SE_RANDOM
#error "SE DRNG requires HAVE_SE_AES (AES engine)"
#endif
#endif /* HAVE_SE_AES */

#if HAVE_SE_RANDOM

/* Allow code to create DRNG_STATIC_BUFFER_SIZE random bytes as a cache
 * of "fresh random bytes" which are delivered to clients that do not
 * provide CACHE_LINE aligned buffers (aligned buffers do not use this
 * feature).
 *
 * If this is defined 0 an intermedate buffer is still used, but no
 * bytes are cached; so each call with non-aligned buffer will always
 * use HW to generate bytes.
 *
 * Requests with CACHE_LINE aligned buffers will also always use HW to
 * generate bytes directly to the provided buffer.
 */
#ifndef HAVE_ALLOW_CACHED_RANDOM
#define HAVE_ALLOW_CACHED_RANDOM 1
#endif

/* Allow system config to optionally override the intermediate DRNG
 * buffer size used when non-cache line aligned buffer is passed as
 * DRNG target.
 *
 * If HAVE_ALLOW_CACHED_RANDOM is zero => using a large
 * DRNG_STATIC_BUFFER_SIZE is not very useful (unless generating huge
 * amounts of random numbers).
 *
 * The DRNF buffer buffer and DRNG_RBUF_SIZE will always be cache line
 * aligned to minimize SE phys DMA write issues with caches.
 */
#ifndef DRNG_STATIC_BUFFER_SIZE

#if HAVE_ALLOW_CACHED_RANDOM
#define DRNG_STATIC_BUFFER_SIZE	64U
#else
/* Assuming smaller is better here; override if you want
 * bigger intermediate buffers
 *
 * CACHE_LINE is e.g. 64 bytes in ARM-V8 and 32 bytes in ARM-R5
 */
#define DRNG_STATIC_BUFFER_SIZE	(CACHE_LINE)
#endif /* HAVE_ALLOW_CACHED_RANDOM */

#endif /* DRNG_STATIC_BUFFER_SIZE */

/* This size must be cache line aligned, written by DMA engine.
 * Min size is CACHE_LINE bytes.
 */
#define DRNG_RBUF_SIZE DMA_ALIGN_SIZE(DRNG_STATIC_BUFFER_SIZE)

/* Use one of the SE AES0 engine to generate DRNG random numbers
 */
static status_t se_config_drng(const struct se_engine_rng_context *econtext,
			       uint32_t bytes)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	/* CONFIG register */

	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, RNG,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST, MEMORY,
					   se_config_reg);

	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
					   SE_MODE_PKT_AESMODE_KEY256,
					   se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	/* CRYPTO_CONFIG register */

	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   XOR_POS, BYPASS, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
					   RANDOM, se_config_reg);

#if HAVE_SOC_FEATURE_KAC == 0
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   CORE_SEL, ENCRYPT,
					   se_config_reg);

	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   HASH_ENB, DISABLE, se_config_reg);
#endif /* HAVE_SOC_FEATURE_KAC == 0 */

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

	/* RNG_CONFIG field range [1..0] for MODE(FORCE_RESEED) and range [3..2] SRC(ENTROPY)
	 * are set to the same register value
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, RNG_CONFIG,
					   SRC, ENTROPY, se_config_reg);

	/* Using the FORCE_RESEED to always re-sample the ring-oscillators.
	 */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, RNG_CONFIG,
					   MODE, FORCE_RESEED, se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_RNG_CONFIG_0, se_config_reg);

	/* Reseed only once per call (reseed interval set to #bytes in request) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_RNG_RESEED_INTERVAL_0, bytes);

	/* Set the ring oscillators as entropy source, but do not lock
	 * it for now. (Considering security: best option would be to
	 * lock this!!!!)
	 *
	 * RO_ENTROPY_SUBSAMPLE default value is 0 => HW already using
	 * 1/8 of the samples; this is not altered here.
	 *
	 * XXX Maybe should not assume that default are still valid and just
	 * XXX  set everything => TODO
	 */
	se_config_reg = 0U;

	/* Enable RO's as entropy source with default subsample rate */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, RNG_SRC_CONFIG,
					   RO_ENTROPY_SUBSAMPLE, DEFAULT, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, RNG_SRC_CONFIG,
					   RO_ENTROPY_SOURCE, ENABLE, se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_RNG_SRC_CONFIG_0, se_config_reg);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_aligned_random_arg_check(uint32_t bytes, const uint8_t *vbuf)
{
	status_t ret = NO_ERROR;

	if ((NULL == vbuf) || (0U == bytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (bytes > (16U * 1024U * 1024U)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	/* Both the number of bytes and the buffer address need to be CACHE_LINE
	 * aligned for this function.
	 */
	if (!(IS_DMA_ALIGNED(&vbuf[0]) && IS_DMA_ALIGNED(bytes))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

/*TODO => GENRND upper limit is 16 MB per task => check that!!!
 */
static status_t get_aligned_random_bytes(const struct se_engine_rng_context *econtext,
					 uint32_t bytes, uint8_t *vbuf)
{
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr = PADDR_NULL;
	uint32_t se_config_reg = 0U;
	uint32_t num_blocks = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = get_aligned_random_arg_check(bytes, vbuf);
	CCC_ERROR_CHECK(ret);

	DEBUG_ASSERT((CACHE_LINE % SE_AES_BLOCK_LENGTH) == 0U);

	num_blocks = bytes / SE_AES_BLOCK_LENGTH;

	ret = se_config_drng(econtext, bytes);
	CCC_ERROR_CHECK(ret,
			    CCC_ERROR_MESSAGE("Failed to config SE RNG: %d\n",
					      ret));

	paddr = DOMAIN_VADDR_TO_PADDR(econtext->domain, vbuf);
	SE_PHYS_RANGE_CHECK(paddr, bytes);

	/* Just flush for the DST here; it will get ilwalidated only once after the AES
	 * operation completes. This makes a difference in a write-through
	 * cache system.
	 */
	SE_CACHE_FLUSH((VIRT_ADDR_T)vbuf, DMA_ALIGN_SIZE(bytes));

	/* Program output address (low 32 bits) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_0, (uint32_t)paddr);

	/* Program output address (hi bits) */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, bytes,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr),
		se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

	// FIXME: Not sure how this needs to be in T23X, IAS text has changed.
	// TODO:  VERIFY IAS!!!
	//
#if 1
	/* Last block is num_blocks - 1 */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_LAST_BLOCK_0,
			     num_blocks - 1U);
#else
	// XXXX TODO: verify if T23X needs N instead of N-1 here or indeed "bitlength"
	// as stated in IAS...???
	//
	// IAS text is ambiquous.... => FIX IT!!
#error "Verify semantics of LAST_BLOCK register for random"
#endif

#if HAVE_SE_APERTURE
	ret = se_set_aperture(PADDR_NULL, 0U, paddr, bytes);
	CCC_ERROR_CHECK(ret);
#endif

	AES_ERR_CHECK(econtext->engine, "DRNG START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		/**
		 * Issue START command in SE_OPERATION.OP
		 */
		ret = tegra_start_se0_operation_locked(econtext->engine, 0U);
		CCC_ERROR_CHECK(ret);

		ret = se_wait_engine_free(econtext->engine);
		CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != gen_op_perf_usec) {
			show_se_gen_op_entry_perf("DRNG", get_elapsed_usec(gen_op_perf_usec));
		}
#endif
	}

	AES_ERR_CHECK(econtext->engine, "DRNG DONE");

	SE_CACHE_ILWALIDATE((VIRT_ADDR_T)vbuf, bytes);

#if SE_RD_DEBUG
	if (bytes >= 16U) {
		uint32_t zeros = 0U;
		uint32_t c_zeros = 0U;
		uint32_t i = 0U;

		/* Trivial check for DRNG output quality */
		for(i = 0U; i < bytes; i++) {
			if (0U == vbuf[i]) {
				zeros++;
				c_zeros++;
			} else {
				c_zeros = 0U;
			}

#define C_ZERO_LIMIT 6U
			if (c_zeros > C_ZERO_LIMIT) {
				LOG_HEXBUF("RND failed data (c_zero)", vbuf, bytes);
				CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
							 CCC_ERROR_MESSAGE("RND data has more than %u conselwtive zero bytes (position: %u)\n",
									   C_ZERO_LIMIT, i));
			}
		}

		if (zeros > (bytes / 2U)) {
			/* poor quality entropy is bad, better fail it */
			LOG_HEXBUF("RND failed data (zerocount)", vbuf, bytes);
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
						 CCC_ERROR_MESSAGE("simple random quality validation failed: %u zeros in %u bytes\n",
								   zeros, bytes));
		}
	}
#endif /* SE_RD_DEBUG */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_rng0_generate_check_args(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == input_params->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (0U == input_params->output_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rng0_generate_unaligned(
	const struct se_engine_rng_context *econtext,
	uint8_t *vaddr_param,
	uint32_t bytes_param)
{
	status_t ret = NO_ERROR;
	uint32_t bytes = bytes_param;
	uint8_t *vaddr = vaddr_param;

	/* Persistent buffer for handling rand byte requests which are
	 * not CACHE_LINE aligned. Do not allocate this anymore when
	 * supporting context memories because in that case persistent
	 * mempool buffers are not available nor used for anything =>
	 * use static memory instead.
	 *
	 * RNG0 is no longer used so this code is not compiled in production.
	 * So this is for completeness only and does not really matter
	 * if this is statically allocated.
	 *
	 * This is only used for such an unaligned request.
	 *
	 * DRNG_RBUF_SIZE is always dma aligned.
	 */
	static DMA_ALIGN_DATA uint8_t rbuf[ DRNG_RBUF_SIZE ];

	/* the RBUF is filled with random bytes and if HAVE_ALLOW_CACHED_RANDOM
	 * is nonzero the leftover bytes are used for randomness for the subsequent
	 * RND calls (used as one-time pads). When the buffer runs out, it is refilled
	 * with randomness.
	 *
	 * If HAVE_ALLOW_CACHED_RANDOM is zero => the leftover bytes are thrown out
	 * without being used. Each call with unaligned buffer generates
	 * new randomness with SE/HW.
	 */
#if HAVE_ALLOW_CACHED_RANDOM
	static uint32_t rbuf_index = DRNG_RBUF_SIZE;
#else
	uint32_t rbuf_index = DRNG_RBUF_SIZE;
#endif

	LTRACEF("entry\n");

	do {
		uint32_t available = 0U;

		if (rbuf_index >= DRNG_RBUF_SIZE) {
			ret = get_aligned_random_bytes(econtext,
						       DRNG_RBUF_SIZE,
						       rbuf);
			if (NO_ERROR != ret) {
				continue;
			}
			rbuf_index = 0U;
		}

		available = DRNG_RBUF_SIZE - rbuf_index;

		if (bytes <= available) {
			se_util_mem_move(vaddr, &rbuf[rbuf_index], bytes);
			se_util_mem_set(&rbuf[rbuf_index], 0U, bytes);
			rbuf_index += bytes;
			break;
		}

		se_util_mem_move(vaddr, &rbuf[rbuf_index], available);
		rbuf_index = DRNG_RBUF_SIZE;

		bytes -= available;
		vaddr  = &vaddr[available];
	} while(NO_ERROR == ret);

	CCC_ERROR_CHECK(ret,
			    CCC_ERROR_MESSAGE("Failed to generate SE RNG: %d (unaligned)\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_rng0_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t bytes = 0U;
	uint8_t *vaddr = NULL;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_rng0_generate_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	bytes = input_params->output_size;
	vaddr = input_params->dst;

	if (!(IS_DMA_ALIGNED(vaddr) && IS_DMA_ALIGNED(bytes))) {
		ret = se_rng0_generate_unaligned(econtext, vaddr, bytes);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = get_aligned_random_bytes(econtext, bytes, vaddr);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to generate SE RNG: %d (aligned)\n", ret));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_rng0_close_locked(void)
{
	// Nothing lwrrently
}
#endif
#endif /* HAVE_SE_RANDOM */
