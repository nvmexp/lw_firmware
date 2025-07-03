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

/* Support for symmetric block ciphers with SE engine (AES-*)
 *
 * Other symmetric cipher algorithms can be supported in SW (fully or partially)
 * by adding calls from here.
 */
#include <crypto_system_config.h>

#if HAVE_SE_AES

#include <crypto_cipher.h>
#include <crypto_cipher_proc.h>
#include <crypto_process_call.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define SE_CIPHER_MAGIC	0x43504852U

#if HAVE_CONTEXT_MEMORY
/* Class cipher memory fixed sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s cipher_mem_specs = {
	/* This is max for normal AES.
	 * Reduced by KEY and DMA size if enabled,
	 * and if so this becomes 256 bytes.
	 *
	 * cm_lo_size = 1472 bytes
	 */
	.cm_lo_size = (CMEM_KEY_MEMORY_STATIC_SIZE +
		       (CMEM_DESCRIPTOR_SIZE + CCC_DMA_MEM_AES_BUFFER_SIZE) +
		       (CMEM_DESCRIPTOR_SIZE + 256U)),

#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* If separate DMA memory used, this is min size for that.
	 */
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_AES_BUFFER_SIZE),
#endif
};
#endif /* HAVE_CONTEXT_MEMORY */

/* Only AES supported by the HW */

struct se_cipher_state_s {
	/* HW state */

	/* Let this be the first field in the object; (alignment requirements)
	 * Because the CIP_CONTEXT.BUF (16 bytes) is the first entry, the object needs to be
	 * aligned to 16 byte boundary to guarantee that the BUF field does not cross page
	 * boundary. No other fields need to be contiguous in this object.
	 *
	 * This, like the other context BUF fields need to be contiguous because
	 * they are accessed by the SE engine using physical addresses.
	 */
	struct se_aes_context cip_context;

	uint32_t cip_magic;
};

static status_t cipher_init_arg_check(const crypto_context_t *ctx, const te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("cipher init: state already set up\n"));
	}

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_ENCRYPT:
	case TE_ALG_MODE_DECRYPT:
		break;
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("cipher algo 0x%x mode %u unsupported\n",
					  ctx->ctx_algo, ctx->ctx_alg_mode));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
static void release_cipher_context_dma_buf(struct se_cipher_state_s *s_cipher)
{
	if (NULL != s_cipher) {
		aes_context_dma_buf_release(&s_cipher->cip_context);
	}
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

// XXX TODO => could init more stuff here and less in CTX allocate.
// So the "reset" context can actually be initialized to ANY crypto op instead of just an
// identical operation.
//
status_t se_crypto_cipher_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_cipher_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("Generic AES");

	ret = cipher_init_arg_check(ctx, args);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &cipher_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	/* First (aes block size or CACHE_LINE which ever is larger)
	 * bytes in this buffer must not cross a page boundary
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 DMA_ALIGN_SIZE(SE_AES_BLOCK_LENGTH),
				 struct se_cipher_state_s,
				 struct se_cipher_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->cip_context, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES context static init failed %d\n", ret));

	ret = aes_init_econtext(&s->cip_context.ec, args);
	CCC_ERROR_CHECK(ret);

	s->cip_context.ec.is_encrypt = (ctx->ctx_alg_mode == TE_ALG_MODE_ENCRYPT);

	if (TE_AES_MODE_USES_IV(ctx->ctx_algo)) {
		se_util_mem_move((uint8_t *)s->cip_context.ec.iv_stash, args->aes.ci_iv, AES_IV_SIZE);


		/* With this flag the AES keyslot (or other location) already
		 * contains the INITIAL IV value for the first AES cipher operation.
		 *
		 * In case the operation is continued the continuation
		 * IV is handled normally.
		 *
		 * This setting triggers an error later when a key setup is called:
		 * the pre-set IV can only be used in case also the key
		 * is preset into the keyslot; this is indicated by
		 * KEY_FLAG_USE_KEYSLOT_KEY set as well, but that
		 * flag is in the key object which is not yet known here.
		 */
		if ((args->aes.flags & INIT_FLAG_CIPHER_USE_PRESET_IV) != 0U) {
#if HAVE_AES_IV_PRESET
			s->cip_context.ec.aes_flags |= AES_FLAGS_USE_KEYSLOT_IV;
#else
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     CCC_ERROR_MESSAGE("AES IV preset not supported\n"));
#endif /* HAVE_AES_IV_PRESET */
		}
	}

	args->engine_hint = s->cip_context.ec.engine->e_id;

	s->cip_magic = SE_CIPHER_MAGIC;
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_CIP;
	ctx->ctx_run_state.rs.s_cipher = s;

	s = NULL;

fail:
	if (NULL != s) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		release_cipher_context_dma_buf(s);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_capi_arg_check(const crypto_context_t *ctx,
				   const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_cipher_state_s *s = NULL;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_CIP != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for ACIP\n",
					     run_state->rs_select));
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Cipher key not defined\n"));
	}

	s = run_state->rs.s_cipher;
	if ((NULL == s) || (s->cip_magic != SE_CIPHER_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_update(const crypto_context_t *ctx, struct run_state_s *run_state,
				 te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_cipher_state_s *s = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	ret = aes_capi_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_cipher;

	if (NULL == args) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if (NULL == args->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Cipher dst invalid\n"));
	}

#if HAVE_AES_XTS
	/* AES-XTS data must be provided in a single dofinal() call.
	 *
	 * CCC could collect data from update calls to internal buffer
	 * until the XTS data block is complete, but this data block could
	 * be arbitrary long (probably at least several kilobytes long).
	 *
	 * Because the AES1 engine XTS implementation does not allow
	 * the engine state to be saved and restored (e.g. the tweak value
	 * is callwlated once per engine START) => CCC lwrrently also
	 * requires that the XTS data is provided in a single DOFINAL()
	 * call.
	 */
	if (TE_ALG_AES_XTS == s->cip_context.ec.algorithm) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("AES-XTS does not support update calls (HW restriction)\n"));
	}
#endif /* HAVE_AES_XTS */

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size; // length of DST buffer

	ret = call_aes_process_handler(&input, &s->cip_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("cipher op 0x%x failed: 0x%x\n",
					  ctx->ctx_algo, ret));

	/* Return amount of bytes written to DST in this call.
	 * Usually not the same as bytes passed to this update, due to
	 * buffering and cipher block aligning.
	 */
	args->dst_size = input.output_size;
	LTRACEF("DST bytes written (this op): %u\n", args->dst_size);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cipher_export_ctr_iv(const crypto_context_t *ctx,
				     const struct se_cipher_state_s *s,
				     te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	LTRACEF("enter\n");

	if ((NULL == ctx) || (NULL == s)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != init_args) {
		if (TE_ALG_AES_CTR == ctx->ctx_algo) {
#if HAVE_AES_CTR == 0
			/* Algorithm not supported */
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#else
			/* Copy back the updated counter.
			 * Only BE counters supported.
			 */
			se_util_mem_move(init_args->aes.ctr_mode.ci_counter,
					 (const uint8_t *)&s->cip_context.ec.ctr.counter[0],
					 sizeof_u32(s->cip_context.ec.ctr.counter));
#if SE_RD_DEBUG
			LTRACEF("AES CTR mode final counter value, inc %u, aes handled %u bytes\n",
				s->cip_context.ec.ctr.increment,
				s->cip_context.dst_total_written);
			LOG_HEXBUF("AES CTR final ctr value",
				   s->cip_context.ec.ctr.counter, SE_AES_BLOCK_LENGTH);
#endif
#endif /* HAVE_AES_CTR */
		} else {
			if (TE_AES_MODE_USES_IV(ctx->ctx_algo)) {
				/* Copy back the updated (final) IV value */
				se_util_mem_move(init_args->aes.ci_iv,
						 (const uint8_t *)&s->cip_context.ec.iv_stash[0],
						 AES_IV_SIZE);
#if SE_RD_DEBUG
				LOG_HEXBUF("AES final IV value", s->cip_context.ec.iv_stash,
					   AES_IV_SIZE);
#endif
			}
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				  te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_cipher_state_s *s = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	ret = aes_capi_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_cipher;

	if (NULL == args) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if (NULL == args->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Cipher dst invalid\n"));
	}

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size; // DST length

	s->cip_context.ec.is_last = true;

	ret = call_aes_process_handler(&input, &s->cip_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("cipher op 0x%x failed: 0x%x\n",
				ctx->ctx_algo, ret));

	/* Amount of bytes written to DST in this op.  (this is
	 * not always the same as bytes passed to this update, due to
	 * buffering and block aligning).
	 */
	args->dst_size = input.output_size;
	LTRACEF("DST bytes written: %u\n", args->dst_size);

	ret = cipher_export_ctr_iv(ctx, s, init_args);
	CCC_ERROR_CHECK(ret);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("AES", s->cip_context.ec.perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NULL context\n"));
	}

	if (ctx->ctx_class != TE_CLASS_CIPHER) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to CIPHER reset with %u class object\n",
						   ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_CIP != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for CIP\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
			/* A separate DMA buffer is allocated for AES context field buf,
			 * release it.
			 */
			release_cipher_context_dma_buf(ctx->ctx_run_state.rs.s_cipher);
#endif
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U,
					sizeof_u32(struct se_cipher_state_s));
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

status_t se_crypto_cipher_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				  const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_cipher_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_CIP != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for CIP\n",
					     ctx->ctx_run_state.rs_select));
	}

	s = ctx->ctx_run_state.rs.s_cipher;
	if ((NULL == s) || (s->cip_magic != SE_CIPHER_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	// No longer copies the key before passing it to setup_aes_key()
	// This may be considered kind of an issue, but the clients are all secure!
	//
	kdata = GET_DOMAIN_ADDR(ctx->ctx_domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (kdata->k_key_type != KEY_TYPE_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Only supports AES ciphers and keys\n"));
	}

	ret = setup_aes_key(&s->cip_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AES key\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_AES

static status_t aes_async_arg_check(const struct se_cipher_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)s;

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_async_check_state(const crypto_context_t *ctx,
					    const struct run_state_s *run_state,
					    te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	const struct se_cipher_state_s *s = NULL;
	bool is_idle = false;

	LTRACEF("entry\n");

	ret = aes_capi_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_cipher;

	ret = aes_async_arg_check(s);
	CCC_ERROR_CHECK(ret);

	if (NULL == init_args) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	init_args->async_state = ASYNC_CHECK_ENGINE_NONE;

	ret = PROCESS_AES_ASYNC_POLL(&s->cip_context, &is_idle);
	CCC_ERROR_CHECK(ret,
			LTRACEF("AES engine idle check failed\n"));

	if (BOOL_IS_TRUE(is_idle)) {
		init_args->async_state = ASYNC_CHECK_ENGINE_IDLE;
	} else {
		init_args->async_state = ASYNC_CHECK_ENGINE_BUSY;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_cipher_async_handle_start(const crypto_context_t *ctx,
						    struct se_cipher_state_s *s,
						    te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	(void)ctx;

	LTRACEF("entry\n");

	if (NULL == args) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);
	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size;

	ret = PROCESS_AES_ASYNC(&input, &s->cip_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Async AES 0x%x digest %s start failed: %d\n",
					  ctx->ctx_algo,
					  (BOOL_IS_TRUE(s->cip_context.ec.is_last) ? "dofinal" : "update"),
					  ret));

	// XXXX FIXME: This should be set in FINISH!!!!
	//
	// TODO: ASYNC AES does not handle padding (lwrrently)
	// and input_size is verified in the process_async call
	// to be multiple of 16.... So the FINISH will eventually
	// set INPUT_SIZE as output size if it succeeds.
	//
	// XXXX FIXME: May need to add a field to keep track of written bytes in async
	// XXXX        and set this in FINISH...
	//
	args->dst_size = input.input_size;
	LTRACEF("DST bytes written: %u\n", args->dst_size);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_cipher_async_handle_finish(const crypto_context_t *ctx,
						     struct se_cipher_state_s *s,
						     te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Complete the async AES started above
	 *
	 * This never passes any additional input.
	 */
	ret = PROCESS_AES_ASYNC(NULL, &s->cip_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Async AES 0x%x update finish failed: %d\n",
					  ctx->ctx_algo, ret));

	/* Return amount of bytes written to DST in this call.
	 * Usually not the same as bytes passed to this update, due to
	 * buffering and cipher block aligning.
	 */
	if (BOOL_IS_TRUE(s->cip_context.ec.is_last)) {
		ret = cipher_export_ctr_iv(ctx, s, init_args);
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_cipher_async_handle(const crypto_context_t *ctx,
					      const struct run_state_s *run_state,
					      te_args_data_t *args,
					      te_args_init_t *init_args,
					      bool start_operation,
					      bool is_dofinal)
{
	status_t ret = NO_ERROR;
	struct se_cipher_state_s *s = NULL;

	LTRACEF("entry\n");

	ret = aes_capi_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_cipher;

	ret = aes_async_arg_check(s);
	CCC_ERROR_CHECK(ret);

#if HAVE_AES_XTS
	if (!BOOL_IS_TRUE(is_dofinal)) {
		/* AES-XTS data must be provided in a single dofinal() call.
		 *
		 * CCC could collect data from update calls to internal buffer
		 * until the XTS data block is complete, but this data block could
		 * be arbitrary long (probably at least several kilobytes long).
		 *
		 * Because the AES1 engine XTS implementation does not allow
		 * the engine state to be saved and restored (e.g. the tweak value
		 * is callwlated once per engine START) => CCC lwrrently also
		 * requires that the XTS data is provided in a single DOFINAL()
		 * call.
		 */
		if (TE_ALG_AES_XTS == s->cip_context.ec.algorithm) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     CCC_ERROR_MESSAGE("AES-XTS does not support update calls (HW restriction)\n"));
		}
	}
#endif /* HAVE_AES_XTS */

	s->cip_context.async_start = start_operation;
	s->cip_context.ec.is_last = is_dofinal;

	if (BOOL_IS_TRUE(start_operation)) {
		ret = se_crypto_cipher_async_handle_start(ctx, s, args);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = se_crypto_cipher_async_handle_finish(ctx, s, init_args);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_async_update(const crypto_context_t *ctx,
				       struct run_state_s *run_state,
				       te_args_data_t *args,
				       bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = se_crypto_cipher_async_handle(ctx, run_state,
					    args, NULL,
					    start_operation,
					    false);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_cipher_async_dofinal(const crypto_context_t *ctx,
					struct run_state_s *run_state,
					te_args_data_t *args,
					te_args_init_t *init_args,
					bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = se_crypto_cipher_async_handle(ctx, run_state,
					    args, init_args,
					    start_operation,
					    true);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_AES */

#endif /* HAVE_SE_AES */
