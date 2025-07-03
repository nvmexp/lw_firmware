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

#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3

#include <crypto_md.h>
#include <crypto_md_proc.h>
#include <crypto_process_call.h>

#if HAVE_SW_WHIRLPOOL
#include <whirlpool/crypto_whirlpool.h>
#endif

#if HAVE_SW_SHA3
#include <sha3/crypto_sha3.h>
#endif

#if HAVE_MD5
#include <md5/crypto_md5_proc.h>
#endif

#if HAVE_HW_SHA3 && (HAVE_SE_SHA == 0)
#error "HAVE_HW_SHA3 requires HAVE_SE_SHA"
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define SE_MD_MAGIC	0x4d444d47U

enum digest_type_e {
	DIGEST_TYPE_NONE = 0,
	DIGEST_TYPE_SHA  = 1, /* HW SHA3/SHAKE use this type as well */
	DIGEST_TYPE_MD5  = 2,
	DIGEST_TYPE_WHIRLPOOL = 3,
#if HAVE_SW_SHA3
	DIGEST_TYPE_SW_SHA3 = 4,
#endif
};

/* Digest / HW state */
struct se_md_state_s {
	union { /* First field in the object (alignment is smaller) */
#if HAVE_SE_SHA
		struct se_sha_context sha_context; /**< HW context: SHA-2 and SHA-3/SHAKE HW version */
#endif
#if HAVE_MD5
		struct soft_md5_context md5_context; /**< SW context: md5 */
#endif
#if HAVE_SW_WHIRLPOOL
		struct soft_whirlpool_context whirlpool_context; /**< SW context: WHIRLPOOL */
#endif
#if HAVE_SW_SHA3
		struct soft_sha3_context sha3_context; /**< SW context: SHA-3/SHAKE SW version */
#endif
	};
	enum digest_type_e md_type;	/**< Union selector field */
	uint32_t md_magic;		/**< Object magic number */
};

#if HAVE_CONTEXT_MEMORY
/* Class digest memory fixed sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s md_mem_specs = {
	.cm_lo_key_size = 0U, /* Digests do not use separate key spaces */

#if CCC_WITH_CONTEXT_DMA_MEMORY
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_SHA_RESERVATION_SIZE),
#endif

	/* Generic part that holds all, including above
	 * if DMA not enabled. Supports SHA3.
	 */
	.cm_lo_size = (CMEM_DESCRIPTOR_SIZE + 576U +
		       (CMEM_DESCRIPTOR_SIZE +
			CCC_DMA_MEM_SHA_RESERVATION_SIZE)),
};
#endif /* HAVE_CONTEXT_MEMORY */

static status_t md_check_sanity(const crypto_context_t *ctx,
				const te_args_data_t *args,
				const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_md_state_s *s = NULL;

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_DIGEST) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == run_state) || (NULL == run_state->rs.s_object)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (RUN_STATE_SELECT_MD != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for MD\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_md;
	if (s->md_magic != SE_MD_MAGIC) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	return ret;
}

/* note that the SW algos do not use the cmem argument since
 * they lwrrently do not need to access external memory objects outside the
 * existing contexts.
 */
static status_t md_type_static_init(const crypto_context_t *ctx,
				    te_args_init_t *args,
				    struct se_md_state_s *s,
				    enum digest_type_e md_type,
				    struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	const struct engine_s *engine = NULL;

	LTRACEF("entry\n");

	switch(md_type) {
#if HAVE_MD5
	case DIGEST_TYPE_MD5:
		ret = md5_context_static_init(ctx->ctx_domain, args->engine_hint,
					      ctx->ctx_algo, &s->md5_context);
		engine = s->md5_context.ec.engine;
		break;
#endif /* HAVE_MD5 */

#if HAVE_SW_WHIRLPOOL
	case DIGEST_TYPE_WHIRLPOOL:
		ret = whirlpool_context_static_init(ctx->ctx_domain, args->engine_hint,
						    ctx->ctx_algo, &s->whirlpool_context);
		engine = s->whirlpool_context.ec.engine;
		break;
#endif /* HAVE_SW_WHIRLPOOL */

#if HAVE_SW_SHA3
	case DIGEST_TYPE_SW_SHA3:
		ret = sha3_context_static_init(ctx->ctx_domain, args->engine_hint,
					       ctx->ctx_algo, &s->sha3_context,
					       cmem);
		engine = s->sha3_context.ec.engine;
		break;
#endif /* HAVE_SW_SHA3 */

	case DIGEST_TYPE_SHA:
		ret = sha_context_static_init(ctx->ctx_domain,
					      args->engine_hint,
					      ctx->ctx_algo,
					      &s->sha_context,
					      cmem);

		engine = s->sha_context.ec.engine;
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	s->md_type = md_type;
	args->engine_hint = engine->e_id;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_HW_SHA3
/* Setup additional SHAKE params if provided by caller. If not, use the defaults
 * set in sha_engine_context_static_init_cm()
 */
static status_t shake_econtext_setup(struct se_engine_sha_context *sha_econtext,
				     struct shake_init_args *shake_init)
{
	status_t ret = NO_ERROR;
	uint32_t bits = 0U;

	LTRACEF("entry\n");

	bits = shake_init->shake_xof_bitlen;

	if (0U != bits) {
		if (bits > MAX_SHAKE_XOF_BITLEN) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("SHAKE XOF bit length invalid: %u\n", bits));
		}

		/* CCC implementation restriction. It's possible to support this when
		 * SHA_DST_MEM==1 => not yet done.
		 */
		if (bits > MAX_SHAKE_XOF_BITLEN_IN_REGISTER) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_IMPLEMENTED,
					     LTRACEF("SHAKE longer than %u bits is not yet supported\n",
						     MAX_SHAKE_XOF_BITLEN_IN_REGISTER));
		}

		if ((bits % 8U) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("SHAKE HW supports only byte multiple XOF output\n"));
		}

#if 0
		// XXX If this not needed anymore remove once shake support completed.

		/* Number of word size elements that contain the SHAKE result
		 * There may be extra bits at the end that need to be dropped.
		 */
		uint32_t words = 0U;
		words = (bits / BITS_IN_WORD);
		if ((bits % BITS_IN_WORD) != 0U) {
			words++;
		}
#endif

		sha_econtext->shake_xof_bitlen = bits;
		sha_econtext->hash_size = (bits / 8U);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HW_SHA3 */

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
/* If a separate DMA buffer is allocated for md context field buf,
 * release it.
 */
static void release_md_context_dma_buf(struct se_md_state_s *s_md)
{
#if HAVE_SE_SHA /* This is the only digest type with DMA buffer assigned */
	if ((NULL != s_md) && (DIGEST_TYPE_SHA == s_md->md_type)) {
		sha_context_dma_buf_release(&s_md->sha_context);
	}
#endif
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

static status_t md_per_algo_setting(const crypto_context_t *ctx,
				    te_args_init_t *args,
				    struct se_md_state_s *s,
				    struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch(ctx->ctx_algo) {
#if HAVE_MD5
	case TE_ALG_MD5:
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_MD5, cmem);
		break;
#endif /* HAVE_MD5 */

#if HAVE_SW_WHIRLPOOL
	case TE_ALG_WHIRLPOOL:
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_WHIRLPOOL, cmem);
		break;
#endif /* HAVE_SW_WHIRLPOOL */

#if HAVE_SW_SHA3
	case TE_ALG_SHA3_224:
	case TE_ALG_SHA3_256:
	case TE_ALG_SHA3_384:
	case TE_ALG_SHA3_512:
		/* XXXX FIXME: Support SW SHAKEs */
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_SW_SHA3, cmem);
		break;
#endif /* HAVE_SW_SHA3 */

#if HAVE_HW_SHA3
	case TE_ALG_SHAKE128:
	case TE_ALG_SHAKE256:
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_SHA, cmem);
		if (NO_ERROR != ret) {
			break;
		}

		ret = shake_econtext_setup(&s->sha_context.ec, &args->shake);
		break;

	case TE_ALG_SHA3_224:
	case TE_ALG_SHA3_256:
	case TE_ALG_SHA3_384:
	case TE_ALG_SHA3_512:
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_SHA, cmem);
		break;
#endif /* HAVE_HW_SHA3 */

#if HAVE_SE_SHA
	case TE_ALG_SHA1:
	case TE_ALG_SHA224:
	case TE_ALG_SHA256:
	case TE_ALG_SHA384:
#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
	case TE_ALG_SHA512_256:
#endif
	case TE_ALG_SHA512:
		ret = md_type_static_init(ctx, args, s, DIGEST_TYPE_SHA, cmem);
		break;
#endif /* HAVE_SE_SHA */

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_md_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");
	MEASURE_MEMORY_START("Generic digest");

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
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &md_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("cipher init: state already set up\n"));
	}

	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_md_state_s,
				 struct se_md_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = md_per_algo_setting(ctx, args, s, cmem);
	CCC_ERROR_CHECK(ret);

	s->md_magic = SE_MD_MAGIC;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_MD;
	ctx->ctx_run_state.rs.s_md = s;

	s = NULL;

fail:
	if (NULL != s) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		release_md_context_dma_buf(s);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_update(const crypto_context_t *ctx, struct run_state_s *run_state,
			     te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_md_state_s *s = NULL;

	LTRACEF("entry\n");

	ret = md_check_sanity(ctx, args, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_md;

	LTRACEF("SRC: %p, src_size %u\n",
		args->src, args->src_size);

	if ((NULL != args->src) && (args->src_size > 0U)) {
		struct se_data_params input;

		input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
		input.input_size = args->src_size;
		input.dst = NULL; /* digest update uses internal intermediate buffers */
		input.output_size = 0U;

		switch (s->md_type) {
#if HAVE_SE_SHA
		case DIGEST_TYPE_SHA:
			ret = PROCESS_SHA(&input, &s->sha_context);
			break;
#endif /* HAVE_SE_SHA */

#if HAVE_MD5
		case DIGEST_TYPE_MD5:
			ret = PROCESS_SW_MD5(&input, &s->md5_context);
			break;
#endif /* HAVE_MD5 */

#if HAVE_SW_WHIRLPOOL
		case DIGEST_TYPE_WHIRLPOOL:
			ret = PROCESS_SW_WHIRLPOOL(&input, &s->whirlpool_context);
			break;
#endif /* HAVE_SW_WHIRLPOOL */

#if HAVE_SW_SHA3
		case DIGEST_TYPE_SW_SHA3:
			ret = PROCESS_SW_SHA3(&input, &s->sha3_context);
			break;
#endif /* HAVE_SW_SHA3 */

		default:
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Domain 0x%x digest 0x%x failed: %d\n",
						  ctx->ctx_domain,
						  ctx->ctx_algo, ret));
	}

	/* MD update never writes anything to client buffer */
	args->dst_size = 0U;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_dofinal(const crypto_context_t *ctx,
			      struct run_state_s *run_state,
			      te_args_data_t *args,
			      te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_md_state_s *s = NULL;
	struct se_data_params input;
	uint32_t digest_len = 0U;

	(void)init_args;

	LTRACEF("entry\n");

	ret = md_check_sanity(ctx, args, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_md;

	LTRACEF("DOMAIN: 0x%x, DST: %p, dst_size %u\n",
		ctx->ctx_domain, args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if (IS_SHAKE(ctx->ctx_algo)) {
		/* For SHAKEs this is not constant */
		digest_len = s->sha_context.ec.hash_size;
	} else {
		ret = get_message_digest_size(ctx->ctx_algo, &digest_len);
		CCC_ERROR_CHECK(ret);
	}

	if ((NULL == args->dst) || (args->dst_size < digest_len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Digest does not fit in output buffer\n"));
	}

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size;

	switch (s->md_type) {
#if HAVE_SE_SHA
	case DIGEST_TYPE_SHA:
		s->sha_context.ec.is_last = true;

		ret = PROCESS_SHA(&input, &s->sha_context);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("SHA", s->sha_context.ec.perf_usec);
#endif
		break;
#endif /* HAVE_SE_SHA */

#if HAVE_MD5
	case DIGEST_TYPE_MD5:
		s->md5_context.ec.is_last = true;
		ret = PROCESS_SW_MD5(&input, &s->md5_context);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("MD5", s->md5_context.ec.perf_usec);
#endif
		break;
#endif /* HAVE_MD5 */

#if HAVE_SW_WHIRLPOOL
	case DIGEST_TYPE_WHIRLPOOL:
		s->whirlpool_context.ec.is_last = true;
		ret = PROCESS_SW_WHIRLPOOL(&input, &s->whirlpool_context);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("WHIRLPOOL", s->whirlpool_context.ec.perf_usec);
#endif
		break;
#endif /* HAVE_SW_WHIRLPOOL */

#if HAVE_SW_SHA3
	case DIGEST_TYPE_SW_SHA3:
		s->sha3_context.ec.is_last = true;
		ret = PROCESS_SW_SHA3(&input, &s->sha3_context);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("SHA3", s->sha3_context.ec.perf_usec);
#endif
		break;
#endif /* HAVE_SW_SHA3 */

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("digest 0x%x failed: %d\n",
					  ctx->ctx_algo, ret));

	args->dst_size = input.output_size;

fail:
	LTRACEF("exit: %d (digest size %u)\n", ret, args->dst_size);
	return ret;
}

status_t se_crypto_md_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_DIGEST) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to DIGEST reset with %u class object\n",
						   ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_MD != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for MD\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
			/* If a separate DMA buffer is allocated for md context field buf,
			 * release it.
			 */
			release_md_context_dma_buf(ctx->ctx_run_state.rs.s_md);
#endif
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U, sizeof_u32(struct se_md_state_s));
		}

		CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
				  CMTAG_API_STATE,
				  ctx->ctx_run_state.rs.s_object);
	}
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

fail:
	MEASURE_MEMORY_STOP;

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_SHA && HAVE_SE_SHA

static status_t sha_async_arg_check(const crypto_context_t *ctx,
				    const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_md_state_s *s = NULL;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == run_state)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_MD != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for MD\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_md;
	if ((NULL == s) || (s->md_magic != SE_MD_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (s->md_type != DIGEST_TYPE_SHA) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("Asynchronous digests only supported by HW SHA engine\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_async_start_operation(const crypto_context_t *ctx,
					  const te_args_data_t *args,
					  struct se_md_state_s *s,
					  uint32_t digest_len,
					  bool is_dofinal)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry:\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(is_dofinal)) {
		LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);

		if ((NULL == args->dst) || (args->dst_size < digest_len)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		input.output_size = args->dst_size;
	} else {
		LTRACEF("DST: ignored in digest update (using internal buffer)\n");

		input.dst = NULL; /* digest update uses internal intermediate buffers */
		input.output_size = 0U;
	}

	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	s->sha_context.ec.is_last = is_dofinal;

	ret = PROCESS_SHA_ASYNC(&input, &s->sha_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Async SHA 0x%x digest %s start failed: %d\n",
					  ctx->ctx_algo,
					  (BOOL_IS_TRUE(is_dofinal) ? "dofinal" : "update"),
					  ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_async_check_state(const crypto_context_t *ctx,
					const struct run_state_s *run_state,
					te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	const struct se_md_state_s *s = NULL;
	bool is_idle = false;

	LTRACEF("entry\n");

	ret = sha_async_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_md;

	if (NULL == init_args) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	init_args->async_state = ASYNC_CHECK_ENGINE_NONE;

	ret = PROCESS_SHA_ASYNC_POLL(&s->sha_context, &is_idle);
	CCC_ERROR_CHECK(ret,
			LTRACEF("SHA engine idle check failed\n"));

	if (BOOL_IS_TRUE(is_idle)) {
		init_args->async_state = ASYNC_CHECK_ENGINE_IDLE;
	} else {
		init_args->async_state = ASYNC_CHECK_ENGINE_BUSY;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_md_async_handle(const crypto_context_t *ctx,
					  const struct run_state_s *run_state,
					  te_args_data_t *args,
					  const te_args_init_t *init_args,
					  bool start_operation,
					  bool is_dofinal)
{
	status_t ret = NO_ERROR;
	struct se_md_state_s *s = NULL;
	uint32_t digest_len = 0U;

	(void)init_args;

	LTRACEF("entry\n");

	ret = sha_async_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_md;

	s->sha_context.async_start = start_operation;

	if (IS_SHAKE(ctx->ctx_algo)) {
		/* For SHAKEs result size is not constant */
		digest_len = s->sha_context.ec.hash_size;
	} else {
		ret = get_message_digest_size(ctx->ctx_algo, &digest_len);
		CCC_ERROR_CHECK(ret);
	}

	if (BOOL_IS_TRUE(start_operation)) {
		/* Start the async SHA operation.
		 */
		ret = sha_async_start_operation(ctx, args, s,
				digest_len, is_dofinal);
		CCC_ERROR_CHECK(ret);

	} else {
		/* Complete the async SHA started above
		 *
		 * This never passes any additional input.
		 */
		s->sha_context.ec.is_last = is_dofinal;

		ret = PROCESS_SHA_ASYNC(NULL, &s->sha_context);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Async SHA 0x%x digest update finish failed: %d\n",
						  ctx->ctx_algo, ret));

		if (BOOL_IS_TRUE(is_dofinal)) {
			/* The dst_size is verified in START to be long enough
			 */
			args->dst_size = digest_len;
		} else {
			args->dst_size = 0U;
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_async_update(const crypto_context_t *ctx,
				   struct run_state_s *run_state,
				   te_args_data_t *args,
				   bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = se_crypto_md_async_handle(ctx, run_state,
					args, NULL,
					start_operation,
					false);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_md_async_dofinal(const crypto_context_t *ctx,
				    struct run_state_s *run_state,
				    te_args_data_t *args,
				    te_args_init_t *init_args,
				    bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = se_crypto_md_async_handle(ctx, run_state,
					args, init_args,
					start_operation,
					true);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_SHA && HAVE_SE_SHA */

#endif /* #if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3 */
