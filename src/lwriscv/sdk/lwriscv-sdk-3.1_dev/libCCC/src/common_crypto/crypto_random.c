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

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API

#include <crypto_random.h>
#include <crypto_random_proc.h>
#include <crypto_process_call.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CONTEXT_MEMORY
/* Class digest memory fixed sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s random_mem_specs = {
	.cm_lo_size = 128U, /* Minimum size for context memory */
	.cm_lo_key_size = 0U,
	.cm_lo_dma_size = 0U,
};
#endif /* HAVE_CONTEXT_MEMORY */

#define SE_RNG_MAGIC	0x52414e44U

enum rng_type_id {
	RNG_TYPE_NONE = 0,
	RNG_TYPE_DRNG = 1,
#if CCC_ENABLE_TRNG_API
	RNG_TYPE_TRNG = 2,
#endif
};

struct se_rng_state_s {
	struct se_rng_context rng_context;	// HW context
	enum rng_type_id rng_type;
	uint32_t rng_magic;
};

static status_t crypto_random_get_rng_type(te_crypto_algo_t algo,
					   enum rng_type_id *rng_type_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == rng_type_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
#if CCC_ENABLE_TRNG_API
	case TE_ALG_RANDOM_TRNG:
		*rng_type_p = RNG_TYPE_TRNG;
		break;
#endif

	case TE_ALG_RANDOM_DRNG:
		*rng_type_p = RNG_TYPE_DRNG;
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_random_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_rng_state_s *s = NULL;
	enum rng_type_id rng_type = RNG_TYPE_NONE;
	const uint32_t ptr_bsize = sizeof_u32(uint8_t *);
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("RANDOM NUMBER GENERATOR");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &random_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("rng init: state already set up\n"));
	}

	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 ptr_bsize,
				 struct se_rng_state_s,
				 struct se_rng_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = crypto_random_get_rng_type(ctx->ctx_algo, &rng_type);
	CCC_ERROR_CHECK(ret);

	/* There is no cmem reference in rng engine context.
	 * note: cmem setting => better if no need to allocate anything in
	 * rnd calls after API state.
	 */
	ret = rng_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->rng_context);
	CCC_ERROR_CHECK(ret);

	args->engine_hint = s->rng_context.ec.engine->e_id;

	s->rng_type = rng_type;
	s->rng_magic = SE_RNG_MAGIC;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_RNG;
	ctx->ctx_run_state.rs.s_rng = s;

	s = NULL;

fail:
	if (NULL != s) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t crypto_random_check_args(const crypto_context_t *ctx,
					 const struct run_state_s *run_state,
					 const te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	const struct se_rng_state_s *s = NULL;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_RNG != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for RNG\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_rng;
	if ((NULL == s) || (SE_RNG_MAGIC != s->rng_magic)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_random_dofinal(const crypto_context_t *ctx,
				  struct run_state_s *run_state,
				  te_args_data_t *args,
				  te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_rng_state_s *s = NULL;
	struct se_data_params input;

	(void)init_args;

	LTRACEF("entry\n");

	ret = crypto_random_check_args(ctx, run_state, args);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_rng;

	LTRACEF("DOMAIN: 0x%x, DST: %p, dst_size %u\n",
		ctx->ctx_domain, args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if ((NULL == args->dst) || (0U == args->dst_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* SRC is ignored in RNG dofinal() */
	input.src = NULL;
	input.input_size = 0U;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size;

	switch (s->rng_type) {
#if CCC_ENABLE_TRNG_API
	case RNG_TYPE_TRNG:
#endif
	case RNG_TYPE_DRNG:
		ret = PROCESS_RNG(&input, &s->rng_context);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("RANDOM", s->rng_context.ec.perf_usec);
#endif
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret,
			LTRACEF("RNG 0x%x failed: %d\n",
				ctx->ctx_algo, ret));

	args->dst_size = input.output_size;

fail:
	LTRACEF("exit, ret: %d (RNG size %u)\n", ret, args->dst_size);
	return ret;
}

status_t se_crypto_random_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_RANDOM) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to RNG reset with %u class object\n",
						       ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_RNG != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for RNG\n",
				ctx->ctx_run_state.rs_select);
		} else {
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U,
					sizeof_u32(struct se_rng_state_s));
		}
		CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
				  CMTAG_API_STATE,
				  ctx->ctx_run_state.rs.s_object);
	}
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

fail:
	MEASURE_MEMORY_STOP;

	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_DRNG || CCC_ENABLE_TRNG_API */
