/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* Support for SE key slot operations via the CCC API
 */
#include <crypto_system_config.h>

#if HAVE_SE_KAC_KEYOP

#include <crypto_kwuw.h>

#include <crypto_kwuw_proc.h>
#include <crypto_cipher_proc.h>
#include <crypto_process_call.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CONTEXT_MEMORY
/* Class digest memory fixed sizes for context memory.
 *
 * These are ABSOLUTE MINIMUM requirements for systems
 * which are completely OUT OF MEMORY. Please pass
 * a full page of context memory if possible.
 *
 * Object alignment: 64 bytes
 * Field alignment: 64 bytes
 */
static const struct cm_layout_s kwuw_mem_specs = {
#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* If separate DMA memory used, this is min size for that.
	 */
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_AES_BUFFER_SIZE),
#endif

	/* Generic actually uses plus the above in case they are not
	 * enabled.
	 */
	.cm_lo_size = (320U +
		       CMEM_KEY_MEMORY_STATIC_SIZE +
		       (CMEM_DESCRIPTOR_SIZE +
			CCC_DMA_MEM_AES_BUFFER_SIZE)),
};
#endif /* HAVE_CONTEXT_MEMORY */

#define SE_KWUW_MAGIC	0x4b575557U

struct se_kwuw_state_s {
	/* HW state */

	/* Let this be the first field in the object; (alignment requirements)
	 * Because the KOP_CONTEXT.BUF (16 bytes) is the first entry, the object needs to be
	 * aligned to 16 byte boundary to guarantee that the BUF field does not cross page
	 * boundary. No other fields need to be contiguous in this object.
	 *
	 * This, like the other context BUF fields need to be contiguous because
	 * they are accessed by the SE engine using physical addresses.
	 */
	struct se_aes_context kop_context;

	uint32_t kop_magic;
	uint32_t kop_counter[ SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
	uint8_t kop_auth_tag[ SE_AES_BLOCK_LENGTH ];

	/* Used by KUW.
	 * Contains the 256 bit wrapped key extracted from KW blob.
	 */
	uint8_t kop_kuw_wkey[ KAC_KEY_WRAP_DST_WKEY_MAX_BYTE_LEN ];

	uint32_t kop_wrap_index;
};

/* Special case for mapping TE_ALG_KEYOP_KWUW based on the mode:
 * the algorithm is set to TE_ALG_KEYOP_KW (mode encrypt) or
 * TE_ALG_KEYOP_KUW (mode decrypt).
 *
 * Mode validity verified by caller.
 */
static te_crypto_algo_t kwuw_algo_map(te_crypto_algo_t algo_arg,
				      te_crypto_alg_mode_t mode)
{
	te_crypto_algo_t algo = algo_arg;

	if (TE_ALG_KEYOP_KWUW == algo) {
		if (TE_ALG_MODE_ENCRYPT == mode) {
			algo = TE_ALG_KEYOP_KW;
			LTRACEF("KWUW switched to KW in encrypt\n");
		} else {
			algo = TE_ALG_KEYOP_KUW;
			LTRACEF("KWUW switched to KUW in decrypt\n");
		}
	}
	return algo;
}

static status_t se_crypto_kwuw_init_check_args(const crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;
	bool supported = false;

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("KWUW init: state already set up\n"));
	}

	/* KW uses ENCRYPT and KUW uses DECRYPT modes; KWUW ok with
	 * both.
	 */
	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_ENCRYPT:
		supported = ((TE_ALG_KEYOP_KW == ctx->ctx_algo) ||
			     (TE_ALG_KEYOP_KWUW == ctx->ctx_algo));
		break;

	case TE_ALG_MODE_DECRYPT:
		supported = ((TE_ALG_KEYOP_KUW == ctx->ctx_algo) ||
			     (TE_ALG_KEYOP_KWUW == ctx->ctx_algo));
		break;

	default:
		CCC_ERROR_MESSAGE("KeyOp algo 0x%x mode %u unsupported\n",
			      ctx->ctx_algo, ctx->ctx_alg_mode);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(supported)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kwuw_init(crypto_context_t *ctx,
			  const te_args_init_t *args,
			  struct se_kwuw_state_s *s)
{
	status_t ret = NO_ERROR;

	/* Init the 4 byte AES-GCM Counter0 value to 1.
	 * Copied as memory buffer, swap endian for GCM engine.
	 *
	 * This is Counter0 value in GCM spec. Since HW is used
	 * for complete GCM operation, Counter0 (0x1) is used,
	 * not Counter1 to init to counter registers.
	 */
	uint32_t Counter0 = se_util_swap_word_endian(0x1U);

	switch(ctx->ctx_algo) {
	case TE_ALG_KEYOP_KWUW:
	case TE_ALG_KEYOP_KW:
	case TE_ALG_KEYOP_KUW:
		/* Wrap key index (KW => keyslot to wrap out, KUW <= keyslot to unwrap to)
		 */
		s->kop_wrap_index = args->kwuw.wrap_index;

		s->kop_context.ec.is_encrypt = (ctx->ctx_alg_mode == TE_ALG_MODE_ENCRYPT);

		/* Setup the KW/KUW crypto op (AES-GCM) counter value
		 * from the client provided IV (96 bit IV used).
		 *
		 * HW only supports 96 bit IV values (leaves 4 bytes
		 * for the counter)
		 */
		se_util_mem_move((uint8_t *)&s->kop_counter[0U],
				 args->kwuw.iv,
				 KW_GCM_IV_FIXED_LENGTH);

		s->kop_counter[3U] = Counter0;

		LOG_HEXBUF("KWUW initial counter", s->kop_counter, SE_AES_BLOCK_LENGTH);

		s->kop_context.ec.kwuw.counter = &s->kop_counter[0];
		s->kop_context.ec.kwuw.tag     = &s->kop_auth_tag[0];
		/* Using aes context(buf) for KUW status word output buffer,
		 * if written out by SE DMA.
		 */
		s->kop_context.ec.kwuw.wbuf    = &s->kop_context.buf[0];

		if (!BOOL_IS_TRUE(s->kop_context.ec.is_encrypt)) {
			/* Either DECRYPT/KUW or DECRYPT/KWUW */
			s->kop_context.ec.kwuw.wkey = &s->kop_kuw_wkey[0];
		}
		break;

	default:
		CCC_ERROR_MESSAGE("KWUW algorithm 0x%x unsupported\n",
			      ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/*
	 * Treat the AES-GCM KWUW mode counters as "word big endian
	 * order" and do big_endian increment (each word value stored
	 * in memory in little endian).
	 *
	 * (IV[12] | NONCE[12]) || Counter0[4]
	 *
	 */
	s->kop_context.ec.aes_flags |= AES_FLAGS_CTR_MODE_BIG_ENDIAN;
	s->kop_magic = SE_KWUW_MAGIC;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_KWUW;
	ctx->ctx_run_state.rs.s_kwuw = s;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_kwuw_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_kwuw_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("KWUW");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = se_crypto_kwuw_init_check_args(ctx);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &kwuw_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	/* KW/KUW is using the aes context buffer (it is DMA aligned and contiguous)
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_kwuw_state_s,
				 struct se_kwuw_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->kop_context,
				      cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("KWUW aes context static init failed %d\n",
					  ret));

	ret = kwuw_init(ctx, args, s);
	CCC_ERROR_CHECK(ret);

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

static status_t se_crypto_kwuw_dofinal_check_args(const crypto_context_t *ctx,
						  const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_kwuw_state_s *s = NULL;

	LTRACEF("entry\n");

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("KWUW key not defined\n"));
	}

	if (RUN_STATE_SELECT_KWUW != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for KWUW\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_kwuw;
	if ((NULL == s) || (s->kop_magic != SE_KWUW_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("KWUW magic invalid\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Does not copy the TAG from the KW blob to the caller gcm_tag.
 *
 * Please treat KW as an abstract operation that returns an opaque object,
 * it just happens to be GCM for now and the blob format is dolwmented in IAS,
 * but the blob should not be interpreted by the client.
 */
status_t se_crypto_kwuw_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_kwuw_state_s *s = NULL;
	struct se_data_params input = { .output_size = 0U, .dst = NULL, };
	const uint8_t *wblob = NULL;

	(void)init_args;

	LTRACEF("entry\n");

	ret = se_crypto_kwuw_dofinal_check_args(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_kwuw;

	/* Additional AAD information input in args->src for both KW and KUW.
	 */
	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	switch (kwuw_algo_map(ctx->ctx_algo, ctx->ctx_alg_mode)) {
	case TE_ALG_KEYOP_KW:
		/* This is destination for KW (where to write wrapped blob) and source for KUW (wrapped blob)
		 */
		if (NULL != args->dst) {
			input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
			input.output_size = args->dst_size;
		}

		if (input.output_size < KAC_KEY_WRAP_BLOB_LEN) {
			LTRACEF("Key wrap destination buffer too small\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		/* Key wrap output size is by HW specification constant size.
		 * This value is not modified by callee.
		 */
		input.output_size = KAC_KEY_WRAP_BLOB_LEN;
		break;

	case TE_ALG_KEYOP_KUW:
		/* KUW does not output any data (DST NULL).
		 * input.src is optional AAD.
		 */
		if (NULL == args->src_kwrap) {
			LTRACEF("No wrapped key provided\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		wblob = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src_kwrap);
		ret = se_kwuw_parse_wrap_blob(&s->kop_context.ec, wblob, args->src_kwrap_size);
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("SRC: %p, src_size %u\n", input.src, input.input_size);
	LTRACEF("DST: %p, dst_size %u\n", input.dst, input.output_size);

	s->kop_context.ec.is_last = true;

	ret = PROCESS_AES_KWUW(&input, &s->kop_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("KWUW op 0x%x (mode: =x0%x) failed: 0x%x\n",
				      ctx->ctx_algo,
				      ctx->ctx_alg_mode,
				      ret));

	/* Amount of bytes written to DST by this op.
	 * 0 for KUW, KAC_KEY_WRAP_BLOB_LEN in KW.
	 */
	args->dst_size = input.output_size;
	LTRACEF("DST byte count: %u\n", args->dst_size);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("AES KWUW", s->kop_context.ec.perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_kwuw_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NULL context\n"));
	}

	if (ctx->ctx_class != TE_CLASS_KW) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to KWUW reset with %u class object\n",
						       ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_KWUW != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for KWUW\n",
				ctx->ctx_run_state.rs_select);
		} else {
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U,
					sizeof_u32(struct se_kwuw_state_s));
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

status_t se_crypto_kwuw_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_kwuw_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_KWUW != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for KWUW\n",
					     ctx->ctx_run_state.rs_select));
	}

	s = ctx->ctx_run_state.rs.s_kwuw;
	if ((NULL == s) || (s->kop_magic != SE_KWUW_MAGIC)) {
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
				     CCC_ERROR_MESSAGE("KWUW only supports AES ciphers and keys\n"));
	}

	ret = setup_aes_key(&s->kop_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AES key\n"));

	/* Set KWUW op wrap/unwrap target key index to the key object.
	 *
	 * ks_slot is KEY1 (GCM key)
	 * ks_wrap_index is KEY2 or KEY3 (key to wrap out with KW or to unwrap to with KUW)
	 */
	s->kop_context.ec.se_ks.ks_wrap_index = s->kop_wrap_index;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KEYOP */
