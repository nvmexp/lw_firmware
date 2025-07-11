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
 * @file   crypto_process_random.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Processing layer for random number generation.
 */

#include <crypto_system_config.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG

#include <crypto_cipher_proc.h>
#include <crypto_random_proc.h>

static status_t se_rng_engine_call(const struct se_rng_context *rng_context,
				   uint8_t *dest, uint32_t outlen)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	if (NULL == rng_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.src = NULL;
	input.input_size = 0U;
	input.dst = dest;
	input.output_size = outlen;

	DEBUG_ASSERT_PHYS_DMA_OK(dest, outlen);

	HW_MUTEX_TAKE_ENGINE(rng_context->ec.engine);
	ret = CCC_ENGINE_SWITCH_RNG(rng_context->ec.engine, &input, &rng_context->ec);
	HW_MUTEX_RELEASE_ENGINE(rng_context->ec.engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_SE_RANDOM && HAVE_NC_REGIONS

/* The se_rng_engine_call() called function handles all kind of
 * buffer sizes and alignments in the implementation (by using an
 * internal cache if required to avoid CACHE_LINE issues) => AES
 * output buffer alignment requirements are not enforced in this call.
 */
static status_t rng_process_nc_regions(struct se_rng_context *rng_context,
				       uint8_t *dst_param, uint32_t remain_param)
{
	status_t ret = NO_ERROR;
	uint32_t remain = remain_param;
	uint8_t *dst = dst_param;

	LTRACEF("entry\n");

	if ((NULL == rng_context) || (NULL == dst) || (0U == remain)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if SE_RD_DEBUG
	LTRACEF("RNG processing: requested %u bytes\n", remain);
#endif

	while (remain > 0U) {
		uint32_t clength = 0U;

		/* Find the amount of bytes (up to REMAIN bytes) that
		 * can be accessed (in this case: written) with a phys
		 * mem DMA before a non-contiguous page boundary.
		 *
		 * I.e. in a "contiguous phys memory region" from KVA.
		 */
		ret = ccc_nc_region_get_length(rng_context->ec.domain, dst, remain, &clength);
		if (NO_ERROR != ret) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Failed to get contiguous region length: %d\n",
						     ret));
		}

		if (remain <= clength) {
			ret = se_rng_engine_call(rng_context, dst, remain);
			CCC_ERROR_CHECK(ret,
					LTRACEF("RNG failed: %d\n",
						ret));
			remain = 0U;
		} else {
			ret = se_rng_engine_call(rng_context, dst, clength);
			CCC_ERROR_CHECK(ret,
					LTRACEF("RNG failed: %d\n",
						ret));
			remain -= clength;
			dst     = &dst[clength];
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCCC_WITH_SE_RANDOM && HAVE_NC_REGIONS */

/* handle RNG data generation op (dofinal)
 */
status_t se_random_process(
	struct se_data_params *input_params,
	struct se_rng_context *rng_context)
{
	status_t ret = NO_ERROR;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	if ((NULL == rng_context) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("RNG REQUEST: dst=%p output_size=%u\n",
		input_params->dst, input_params->output_size);

	if (0U == input_params->output_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_LOOP_BEGIN {
#if CCC_WITH_SE_RANDOM
#if HAVE_NC_REGIONS
		if (rng_context->ec.engine->e_dev->cd_id == SE_CDEV_SE0) {
			/* Only DRNG generated by AES engines use DMA output
			 * If non-contiguous pages can be in dst =>
			 * possibly generate in chunks.
			 */
			ret = rng_process_nc_regions(rng_context,
						     input_params->dst,
						     input_params->output_size);
			CCC_LOOP_STOP;
		}
#endif /* HAVE_NC_REGIONS */
#endif /* CCC_WITH_SE_RANDOM */

		/* The other DRNG/TRNG units results set with CPU to
		 * memory buffers no need to be contiguous in phys
		 * mem.
		 */
		ret = se_rng_engine_call(rng_context,
					 input_params->dst,
					 input_params->output_size);
		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(NEED_SE_CRYPTO_GENERATE_RANDOM)
/* Shorthand for generating DRGN random numbers
 * for the kernel context requests when destination buffer is contiguous
 * and SE DMA writable.
 *
 * Note that this function must not be called when ANY CCC related
 * MUTEX is held.
 *
 * Note: Use the macro CCC_GENERATE_RANDOM(buf, len) for GP random numbers
 * if caller does not hold any CCC mutexes. If it does => do not use that macro, need
 * to handle each possible call separately. Should always be safe to use RNG1
 * if that is defined for the system.
 */
status_t se_crypto_kernel_generate_random(uint8_t *buf, uint32_t buflen)
{
	status_t ret = NO_ERROR;

	struct se_data_params input;
	struct se_engine_rng_context econtext = { .engine = NULL };

	LTRACEF("entry (%u bytes)\n", buflen);

	input.src = NULL;
	input.input_size = 0U;
	input.dst = buf;
	input.output_size = buflen;

	/* There is no context memory ref in the DRNG context */
	ret = rng_engine_context_static_init(TE_CRYPTO_DOMAIN_KERNEL, ENGINE_ANY, TE_ALG_RANDOM_DRNG, &econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RNG engine context setup failed: %u\n",
				      ret));

	HW_MUTEX_TAKE_ENGINE(econtext.engine);
	ret = CCC_ENGINE_SWITCH_RNG(econtext.engine, &input, &econtext);
	HW_MUTEX_RELEASE_ENGINE(econtext.engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* defined(NEED_SE_CRYPTO_GENERATE_RANDOM) */
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */
