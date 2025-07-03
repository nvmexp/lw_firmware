/*
 * Copyright (c) 2018-2019, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#include <crypto_system_config.h>

#if HAVE_SW_WHIRLPOOL

#include <crypto_md.h>
#include <crypto_whirlpool.h>

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t whirlpool_engine_context_static_init(te_crypto_domain_t domain,
					      engine_id_t engine_id,
					      te_crypto_algo_t algo,
					      struct se_engine_whirlpool_context *wp_econtext)
{
	status_t ret = NO_ERROR;

	(void)algo;

	ret = tegra_select_engine(&wp_econtext->engine, CCC_ENGINE_CLASS_WHIRLPOOL, engine_id);
	if (NO_ERROR != ret) {
		ERROR_MESSAGE("WHIRLPOOL engine selection failed: %u\n", ret);
		goto fail;
	}

	wp_econtext->is_first	= true;
	wp_econtext->is_last	= false;
	wp_econtext->hash_size	= 64U;
	wp_econtext->block_size = 64U;
	wp_econtext->domain	= domain;

	rhash_whirlpool_init(&wp_econtext->wp_ctx);

#if TEGRA_MEASURE_PERFORMANCE
	wp_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	return ret;
}

/* Does not memset zero => do it yourself */
status_t whirlpool_context_static_init(te_crypto_domain_t domain,
				       engine_id_t engine_id, te_crypto_algo_t algo,
				       struct soft_whirlpool_context *wp_context)
{
	status_t ret = NO_ERROR;

	if ((algo != TE_ALG_WHIRLPOOL) || (NULL == wp_context)) {
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		goto fail;
	}

	switch (domain) {
	case TE_CRYPTO_DOMAIN_KERNEL:
		wp_context->process = soft_whirlpool_process_input;
		break;
#if HAVE_USER_MODE
	case TE_CRYPTO_DOMAIN_TA:
		wp_context->process = ta_soft_whirlpool_process_input;
		break;
#endif
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		goto fail;
	}

	ret = whirlpool_engine_context_static_init(domain, engine_id, algo, &wp_context->ec);
	if (NO_ERROR != ret) {
		ERROR_MESSAGE("WHIRLPOOL engine context setup failed: %u\n", ret);
		goto fail;
	}

	wp_context->byte_count = 0U;
fail:
	return ret;
}

/* handle WHIRLPOOL digest data processing ops (update,dofinal)
 *
 * This gets also called from TA code (user space API in Trusty).
 */
status_t soft_whirlpool_process_input(
	struct se_data_params *input_params,
	struct soft_whirlpool_context *context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;

	LTRACEF("entry\n");

	if ((NULL == context) || (NULL == input_params)) {
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		goto fail;
	}

	bytes_left = input_params->input_size;

	LTRACEF("DIGEST INPUT: sizeof(uintptr_t)=%u, src=%p, input_size=%u, hash_addr dst=%p\n",
		sizeof_u32(uintptr_t), input_params->src, input_params->input_size,
		input_params->dst);

	if ((NULL == input_params->src) && (input_params->input_size > 0U)) {
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		goto fail;
	}

	if (!BOOL_IS_TRUE(context->ec.is_last)) {
		input_params->output_size = 0U;
	}

	if (BOOL_IS_TRUE(context->ec.is_last)) {
		if (input_params->output_size < context->ec.hash_size) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			goto fail;
		}

		if (bytes_left > 0U) {
			rhash_whirlpool_update(&context->ec.wp_ctx,
					       input_params->src, (size_t)bytes_left);
			context->ec.is_first = false;
			context->byte_count += bytes_left;
			bytes_left = 0U;
		}

		rhash_whirlpool_final(&context->ec.wp_ctx, context->ec.tmphash);

#if SE_RD_DEBUG
		LTRACEF("WHIRLPOOL result %u bytes\n", context->ec.hash_size);
		LOG_HEXBUF("WHIRLPOOL result", context->ec.tmphash, context->ec.hash_size);
#endif

		input_params->output_size = 0U;
		if (NULL != input_params->dst) {
			if (context->ec.tmphash != input_params->dst) {
				(void)memmove(input_params->dst, context->ec.tmphash,
					      context->ec.hash_size);
			}
			input_params->output_size = context->ec.hash_size;
		}
	} else {
		if (bytes_left > 0U) {
			rhash_whirlpool_update(&context->ec.wp_ctx,
					       input_params->src,
					       (size_t)bytes_left);
			context->ec.is_first = false;
			context->byte_count += bytes_left;
			bytes_left = 0U;
		}
	}

	DEBUG_ASSERT(bytes_left == 0U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#endif /* HAVE_SW_WHIRLPOOL */
