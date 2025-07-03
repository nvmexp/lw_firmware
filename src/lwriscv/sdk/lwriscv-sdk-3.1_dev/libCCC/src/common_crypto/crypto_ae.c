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

/* Support for authenticating encryption (AEAD) modes
 * (e.g. AES-CCM and AES-GCM)
 */
#include <crypto_system_config.h>

#if HAVE_AES_AEAD

#include <crypto_cipher_proc.h>
#include <crypto_ae.h>

#if CCC_WITH_AES_GCM
#include <aes-gcm/crypto_ops_gcm.h>
#endif

#if HAVE_AES_CCM
#include <aes-ccm/crypto_ops_ccm.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CONTEXT_MEMORY

#if HAVE_AES_CCM
#define WORK_CMEM_CCM_WBUF_SIZE CCC_AES_CCM_WORK_BUFFER_SIZE
#else
#define WORK_CMEM_CCM_WBUF_SIZE 0U
#endif

/* Class digest memory fixed sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s ae_mem_specs = {
	/* Check CCM/GCM, this is max for normal AES.
	 * Reduced by KEY and DMA size if enabled,
	 * and if so this becomes 256 bytes.
	 *
	 * CCM adds a work buffer to DMA mem size.
	 * Added one aes engine context to genmem.
	 *
	 * If you place normal engine contexts to other memories than
	 * genmem then you need to provide more than "low size" memory
	 * with those (in general, not just in AE).
	 *
	 * cm_lo_size = 1600 bytes
	 */
	.cm_lo_size = (CMEM_KEY_MEMORY_STATIC_SIZE +
		       (CMEM_DESCRIPTOR_SIZE + CCC_DMA_MEM_AES_BUFFER_SIZE) +
		       WORK_CMEM_CCM_WBUF_SIZE +
		       (CMEM_DESCRIPTOR_SIZE + 384U)),

#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* If separate DMA memory used, this is min size for that.
	 */
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   WORK_CMEM_CCM_WBUF_SIZE +
			   CCC_DMA_MEM_AES_BUFFER_SIZE),
#endif
};
#endif /* HAVE_CONTEXT_MEMORY */

#define SE_AE_MAGIC	0x00004145U

#ifndef CCC_AES_GCM_DECIPHER_COMPARE_TAG
#define CCC_AES_GCM_DECIPHER_COMPARE_TAG CCC_WITH_AES_GCM
#endif

struct se_ae_state_s {
	/* HW state */

	/* Let this be the first field in the object; (alignment requirements)
	 * Because the AE_CONTEXT.BUF (16 bytes) is the first entry, the object needs to be
	 * aligned to 16 byte boundary to guarantee that the BUF field does not cross page
	 * boundary. No other fields need to be contiguous in this object.
	 *
	 * This, like the other context BUF fields need to be contiguous because
	 * they are accessed by the SE engine using physical addresses.
	 */
	struct se_aes_context ae_context;
	uint32_t ae_magic;

	union {
#if CCC_WITH_AES_GCM
		/* XXX Auth tag len lwrrently 16 */
		struct {
			uint32_t counter[ SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
			uint8_t auth_tag[ SE_AES_BLOCK_LENGTH ];
			uint8_t tag[ SE_AES_BLOCK_LENGTH ];
			uint32_t auth_tag_len;
		} ae_gcm;
#endif /* CCC_WITH_AES_GCM */

#if HAVE_AES_CCM
		struct {
			uint32_t counter[ SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
			uint8_t nonce[ SE_AES_BLOCK_LENGTH ]; /* From [7..13] inclusive bytes */
		} ae_ccm;
#endif
	};
};

#if CCC_WITH_AES_GCM
static status_t ae_gcm_state_init(const crypto_context_t *ctx,
				  const te_args_init_t *args,
				  struct se_ae_state_s *s)
{
	status_t ret = NO_ERROR;
	uint32_t nonce_len = 0U;

	LTRACEF("entry\n");

	if (ctx->ctx_algo == TE_ALG_GMAC_AES) {
		s->ae_context.ec.is_hash = true;
	} else {
		s->ae_context.ec.is_encrypt = (ctx->ctx_alg_mode == TE_ALG_MODE_ENCRYPT);
	}

	/* Space for objects */
	s->ae_context.ec.gcm.counter = &s->ae_gcm.counter[0]; /* 4 word counter */
	s->ae_context.ec.gcm.tag     = &s->ae_gcm.tag[0]; /* callwlated tag (init: 0)*/

	/* Deal with the possible AAD input data first */
	s->ae_context.ec.aes_flags |= AES_FLAGS_AE_AAD_DATA;

	/* HW AES-GCM internally uses the keyslot context not accessible
	 * to SW from context init to context reset. The keyslot must not
	 * be erased or reused for something else before context reset.
	 */
	s->ae_context.ec.aes_flags |= AES_FLAGS_HW_CONTEXT;

	/* GCM init time AAD data object; this enables a
	 * single AES-GCM call doing both AAD+DATA operations
	 *
	 * It is also possible to pass multiple AAD data chunks by
	 * calling CCC AES-GCM api so that there is input data
	 * but the output buffer is <NULL, 0U>. Once the FIRST
	 * call that contains non-null output buffer is received,
	 * that and the subsequent calls to the same context
	 * are treated like CT/PT, not like AAD. After that point
	 * passing in <NULL, 0U> destination buffer will be trapped
	 * as error.
	 */
	s->ae_context.gcm.aad = args->aes.gcm_mode.aad;

	/* Initially AAD_LEN field is length of AAD data passed in
	 * INIT primitive but it switches role during context
	 * processing.
	 *
	 * Later it contains the cumulative number of processed AAD
	 * bytes for DOFINAL.
	 */
	s->ae_context.ec.gcm.aad_len = args->aes.gcm_mode.aad_len;

	/* Keep AAD settings consistent
	 */
	if (NULL == s->ae_context.gcm.aad) {
		s->ae_context.ec.gcm.aad_len = 0U;
	}

	if (0U == s->ae_context.ec.gcm.aad_len) {
		s->ae_context.gcm.aad = NULL;
	}

	/* Copy the AES-GCM IV value.
	 *
	 * The HW restricts support of NONCE length to 12 bytes (96 bits)
	 * with AES-GCM/GMAC.
	 *
	 * This means the counter length is exactly 4 bytes/32 bits.
	 *
	 * The specification defines (but does not recommend to support)
	 * nonce values which are not 96 bits long.
	 *
	 * Adding support for those is not possible in general sense
	 * with this HW implementation since other than 96 bit nonces require
	 * callwlating the GHASH value with the key in the keyslot.
	 *
	 * Because HW does not allow SW access to the GHASH function, which
	 * uses the hash key derived from the key value in the keyslot
	 * SW can not derive the IV value required for the non 96 bit nonces.
	 * (In general the AES-GCM key is in keyslot with manifest purpose
	 *  set to GCM which does not allow SW to callwlate the HASH KEY
	 *  used for GHASH).
	 */
	nonce_len = args->aes.gcm_mode.nonce_len;
	if (0U == nonce_len) {
		nonce_len = AES_GCM_NONCE_LENGTH;
		LTRACEF("AES-GCM defaults to 12 byte (96 bit) nonce value\n");
	}

	/* NIST also recommends that implementations only support
	 * 12 byte (96 bit) nonce values, so this is fine.
	 */
	if (AES_GCM_NONCE_LENGTH != nonce_len) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("AES-GCM supports only 12 byte (96 bit) nonce value\n"));
	}

	/* Setup initial J0 value in AES GCM big endian counter:
	 * J0 = <NONCE || 00000001>
	 *
	 * Other than 12 byte nonces require additional callwlations!!!
	 */
	se_util_mem_move((uint8_t *)&s->ae_gcm.counter[0],
			 &args->aes.gcm_mode.nonce[0],
			 AES_GCM_NONCE_LENGTH);

	/* Init 4 byte J0 counter value to big endian 1.
	 *
	 * HW will derive the initial hash
	 * value H internally for H = AES(K; zerovector)
	 */
	s->ae_gcm.counter[3] = se_util_swap_word_endian(AES_GCM_J0_COUNTER_VALUE);

	/* Caller specified TAG bytes
	 *
	 * Lower layers use 16 byte tags.
	 * Final comparison and export: this many bytes.
	 */
	s->ae_gcm.auth_tag_len = args->aes.gcm_mode.tag_len;

	if (0U == s->ae_gcm.auth_tag_len) {
		s->ae_gcm.auth_tag_len = SE_AES_BLOCK_LENGTH;
		LTRACEF("AES-GCM defaults to 16 byte (128 bit) tag value\n");
	}

	/* 64 and 32 bit tags can be "trusted" only in special cases when
	 * amount of messages per key is very limited.
	 *
	 * Please see NIST publication NIST 800-38D Appendix C.
	 *
	 * Using minimum acceptable TAG length AES_GCM_MIN_TAG_LENGTH
	 * bytes for now in CCC. Redefine smaller only after security
	 * analysis.
	 *
	 * By default, CCC requires 9 byte (72 bit) nonce values
	 * or longer (max 16 bytes, 128 bits). [1 byte increment].
	 */
	if ((s->ae_gcm.auth_tag_len > SE_AES_BLOCK_LENGTH) ||
	    (s->ae_gcm.auth_tag_len < AES_GCM_MIN_TAG_LENGTH)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("AES-GCM tag length %u bits not supported\n",
					     (s->ae_gcm.auth_tag_len * 8U)));
	}

	/* Copy the max AES-GCM 128 bit TAG space from caller buffer.
	 * (The value is used only with decipher operation).
	 *
	 * auth_tag_len is the number of bytes in tag.
	 *
	 * On encryption the callwlated TAG is copied back to
	 * ae_gcm.tag when AES-GCM completes.
	 *
	 * On decryption the TAG is checked to match the value copied
	 * from ae_gcm.tag.
	 *
	 * [XXX Lwrrently CCC copies auth_tag_len bytes of TAG back also on decipher]
	 *
	 * Note: This implementation passes the deciphered data out
	 * before the tag is verified. There is no HW support for more
	 * elaborate actions.
	 *
	 * For example it would be possible to re-encrypt the
	 * decrypted output with some random key and then pass that
	 * re-encrypted CT(rand) out to caller. Then, after a
	 * successful TAG validation the rand key would be passed to
	 * caller who could then decipher the CT(rand) data to gain
	 * access to original plaintext.
	 *
	 * In case TAG validation would instead FAIL, the rand key
	 * would be erased making CT(rand) useless for any purpose.
	 *
	 * If the above is REQUIRED then it could be done but
	 * effecively this would be killing performance of AES-GCM.
	 *
	 * If this is needed => please add a requirement.
	 */
	se_util_mem_move(s->ae_gcm.auth_tag, args->aes.gcm_mode.tag,
			 s->ae_gcm.auth_tag_len);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_AES_GCM */

#if HAVE_AES_CCM
static status_t ae_ccm_state_init(crypto_context_t *ctx,
				  const te_args_init_t *args,
				  struct se_ae_state_s *s)
{
	status_t ret = NO_ERROR;

	uint32_t tag_len   = args->aes.ccm_mode.tag_len;
	uint32_t nonce_len = args->aes.ccm_mode.nonce_len;
	uint32_t aad_len   = args->aes.ccm_mode.aad_len;
	uint8_t *work_buf  = NULL;

	/* Deal with caller AAD
	 */
	if (aad_len > 0U) {
		if (NULL == args->aes.ccm_mode.aad) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AAD NULL, length %u\n", aad_len));
		}

		// XXXX CLIENT BUFFER ADDRESS
		s->ae_context.ccm.aad = args->aes.ccm_mode.aad;
	} else {
		s->ae_context.ccm.aad = NULL;
	}

	s->ae_context.ccm.aad_len = aad_len;

	/* Caller does not provide a plaintext tag to CCM => the tag is part of CT:
	 * CCM tag is appended to ciphertext: tag_len last bytes of ciphertext
	 * are the tag (both cipher and decipher cases).
	 *
	 * AES-CCM tag length in bytes is always even.
	 */
	if (!AES_CCM_TAG_LEN_VALID(tag_len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid tag length value %u\n", tag_len));
	}

	s->ae_context.ec.ccm.tag_len = tag_len;

	/* Deal with caller nonce, check specified size limits.
	 */
	if (!AES_CCM_NONCE_LEN_VALID(nonce_len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid nonce length %u\n", nonce_len));
	}

	s->ae_context.ec.ccm.nonce_len = nonce_len;
	se_util_mem_move(&s->ae_ccm.nonce[0], &args->aes.ccm_mode.nonce[0], nonce_len);
	s->ae_context.ccm.nonce = &s->ae_ccm.nonce[0];

	/* CCM needs a work buffer. Partilwlarly decipher is specified not to
	 * return cleartext to caller on TAG mismatch. The simplest mechanism
	 * to do this is to decipher to a local buffer which needs to be big
	 * enough to hold the deciphered data.
	 *
	 * TODO: This is the first approach, could implement a different
	 * one later (e.g. generate one-time pads and XOR output with those
	 * and reveal the one time pad stream to caller only after TAG verification
	 * succeeds). Or something else. Let's see ;-)
	 */
	work_buf = CMTAG_MEM_GET_BUFFER(context_memory_get(&ctx->ctx_mem),
					CMTAG_ALIGNED_DMA_BUFFER,
					DMA_ALIGN_SIZE(SE_AES_BLOCK_LENGTH),
					DMA_ALIGN_SIZE(CCC_AES_CCM_WORK_BUFFER_SIZE));
	if (NULL == work_buf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	s->ae_context.ec.ccm.wbuf = work_buf;
	s->ae_context.ec.ccm.wbuf_size = DMA_ALIGN_SIZE(CCC_AES_CCM_WORK_BUFFER_SIZE);

	/* Word aligned CTR mode counter value buffer */
	s->ae_context.ec.ccm.counter = &s->ae_ccm.counter[0];

	/* Cipher/decipher flag */
	s->ae_context.ec.is_encrypt = (ctx->ctx_alg_mode == TE_ALG_MODE_ENCRYPT);
fail:
	return ret;
}
#endif /* HAVE_AES_CCM */

static status_t ae_init_check_args(const crypto_context_t *ctx,
					     const te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("AE init: state already set up\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ae_check_algo_supported(const te_crypto_algo_t ctx_algo,
					const te_crypto_alg_mode_t ctx_alg_mode)
{
	status_t ret = NO_ERROR;
	bool supported = false;

	LTRACEF("entry\n");

	switch (ctx_alg_mode) {
	case TE_ALG_MODE_ENCRYPT:
	case TE_ALG_MODE_DECRYPT:
#if HAVE_AES_CCM
		if (TE_ALG_AES_CCM == ctx_algo) {
			supported = true;
		}
#endif
#if HAVE_AES_GCM
		if (TE_ALG_AES_GCM == ctx_algo) {
			supported = true;
		}
#endif
		break;

	case TE_ALG_MODE_MAC:
#if HAVE_GMAC_AES
		if (TE_ALG_GMAC_AES == ctx_algo) {
			supported = true;
		}
#endif
		break;

	default:
		CCC_ERROR_MESSAGE("AE algo 0x%x mode %u unsupported\n",
				  ctx_algo, ctx_alg_mode);
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

static status_t ae_state_init(crypto_context_t *ctx,
			      const te_args_init_t *args,
			      struct se_ae_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch(ctx->ctx_algo) {
#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
		ret = ae_gcm_state_init(ctx, args, s);
		break;
#endif
#if HAVE_GMAC_AES
	case TE_ALG_GMAC_AES:
		ret = ae_gcm_state_init(ctx, args, s);
		break;
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
		ret = ae_ccm_state_init(ctx, args, s);
		break;
#endif /* HAVE_AES_CCM */

	default:
		CCC_ERROR_MESSAGE("AE algorithm 0x%x unsupported\n",
				  ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
static void release_ae_context_dma_buf(struct se_ae_state_s *s)
{
	if (NULL != s) {
		aes_context_dma_buf_release(&s->ae_context);
	}
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

status_t se_crypto_ae_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_ae_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("Authenticating Encryption (AE)");

	ret = ae_init_check_args(ctx, args);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &ae_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	/* Moved memory allocation to before algorithm check to improve
	 * function-call coverage for integration test, the function-call
	 * RELEASE_CONTIGUOUS_MEMORY() after fail label gets then exelwted
	 * on algorithm error.
	 *
	 * First (aes block size or CACHE_LINE which ever is larger)
	 * bytes in this buffer must not cross a page boundary
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 DMA_ALIGN_SIZE(SE_AES_BLOCK_LENGTH),
				 struct se_ae_state_s,
				 struct se_ae_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	ret = ae_check_algo_supported(ctx->ctx_algo, ctx->ctx_alg_mode);
	CCC_ERROR_CHECK(ret);

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->ae_context,
				      cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES AE context static init failed %d\n",
					  ret));

	ret = ae_state_init(ctx, args, s);
	CCC_ERROR_CHECK(ret);

	/*
	 * Treat the AES AE mode counters as "big endian"
	 * and do big_endian increment by default.
	 *
	 * TODO: Add some option to specify LE if required...
	 * (see AES_CTR mode options as one way to do it)
	 */
	s->ae_context.ec.aes_flags |= AES_FLAGS_CTR_MODE_BIG_ENDIAN;
	s->ae_magic = SE_AE_MAGIC;

	/* return a selected engine in the hint */
	args->engine_hint = s->ae_context.ec.engine->e_id;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_AE;
	ctx->ctx_run_state.rs.s_ae = s;

	s = NULL;

fail:
	if (NULL != s) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		release_ae_context_dma_buf(s);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ae_arg_check(const crypto_context_t *ctx,
			     const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_ae_state_s *s = NULL;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_AE != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Invalid selector 0x%x for AE\n",
						       run_state->rs_select));
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AE key not defined\n"));
	}

	s = run_state->rs.s_ae;
	if ((NULL == s) || (s->ae_magic != SE_AE_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AE magic invalid\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_AES_AEAD_UPDATE
status_t se_crypto_ae_update(const crypto_context_t *ctx, struct run_state_s *run_state,
			     te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_ae_state_s *s = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	ret = ae_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

#if HAVE_AES_CCM
	if (TE_ALG_AES_CCM == ctx->ctx_algo) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-CCM does not support update calls\n"));
	}
#endif

	s = run_state->rs.s_ae;

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	if (NULL == args->dst) {
		input.dst = NULL;
		input.output_size = 0U;
	} else {
		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		input.output_size = args->dst_size; // length of DST buffer
	}

	ret = call_aes_process_handler(&input, &s->ae_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AE op 0x%x failed: 0x%x\n",
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
#endif /* HAVE_AES_AEAD_UPDATE */

/* Process tag, e.g. copy back/compare the AE mode Auth Tag depending on algorithm.
 *
 * Some AE modes copy the 16 byte final tag to caller, some do it elsewhere
 * (e.g. CCM appends it to end of CT, by convention.
 *  But GCM does not do that, the TAG is copied to init arguments tag field;
 *  also by convention).
 *
 * E.g. CCM code handles tag internally, not processed here in any way.
 */
static status_t ae_dofinal_process_tag(const crypto_context_t *ctx,
				       const struct se_ae_state_s *s,
				       te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;

	(void)s;
	(void)init_args;

#if CCC_AES_GCM_DECIPHER_COMPARE_TAG
	if (TE_ALG_AES_GCM == s->ae_context.ec.algorithm) {
		/* GMAC does not compare tags, CCC only generates tags for it.
		 * (GMAC caller must compare tag).
		 */
		if (!BOOL_IS_TRUE(s->ae_context.ec.is_encrypt)) {
			if (!BOOL_IS_TRUE(se_util_vec_match(&init_args->aes.gcm_mode.tag[0],
							    &s->ae_context.ec.gcm.tag[0],
							    s->ae_gcm.auth_tag_len,
							    CFALSE,
							    &ret))) {
				CCC_ERROR_WITH_ECODE(ERR_NOT_VALID, LTRACEF("AES-GCM tag value mismatch\n"));
			}
			CCC_ERROR_CHECK(ret);
		}
	}
#endif /* CCC_AES_GCM_DECIPHER_COMPARE_TAG */

	switch (ctx->ctx_algo) {
#if CCC_WITH_AES_GCM
	case TE_ALG_AES_GCM:
	case TE_ALG_GMAC_AES:
		/* Copy back the AE mode Auth Tag in big endian byte order.
		 */
		se_util_mem_move(&init_args->aes.gcm_mode.tag[0],
				 &s->ae_context.ec.gcm.tag[0],
				 s->ae_gcm.auth_tag_len);
#if SE_RD_DEBUG
		LTRACEF("GCM %u byte tag (AAD %u, TEXT %u bytes)\n",
			s->ae_gcm.auth_tag_len,
			s->ae_context.ec.gcm.aad_len,
			s->ae_context.dst_total_written);
		LOG_HEXBUF("GCM tag value",
			   &s->ae_context.ec.gcm.tag[0],
			   s->ae_gcm.auth_tag_len);
#endif
		break;
#endif /* CCC_WITH_AES_GCM */

#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
		/* AES-CCM does not pass cleartext tags around,
		 * it is processed and appended to CT data.
		 */
		break;
#endif

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	return ret;
}


status_t se_crypto_ae_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
			      te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_ae_state_s *s = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	ret = ae_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_ae;

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	if (NULL == args->dst) {
		input.dst = NULL;
		input.output_size = 0U;
	} else {
		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		input.output_size = args->dst_size; // DST length
	}

	s->ae_context.ec.is_last = true;

	ret = call_aes_process_handler(&input, &s->ae_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AE op 0x%x failed: 0x%x\n",
					  ctx->ctx_algo, ret));

	/* Amount of bytes written to DST in this op. (this is
	 * usually not the same as bytes passed to this update, due to
	 * buffering and block aligning).
	 */
	args->dst_size = input.output_size;
	LTRACEF("DST bytes written: %u\n", args->dst_size);

	/* Postprocess AE TAG for the algorithm
	 */
	ret = ae_dofinal_process_tag(ctx, s, init_args);
	CCC_ERROR_CHECK(ret);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("AES AE", s->ae_context.ec.perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_AES_CCM
static void ae_reset_ccm_wbuf(struct se_ae_state_s *s)
{
	if (NULL != s) {
		uint8_t *wbuf = s->ae_context.ec.ccm.wbuf;
		uint32_t wbuf_size = s->ae_context.ec.ccm.wbuf_size;

		if (NULL != wbuf) {
			if (wbuf_size <= DMA_ALIGN_SIZE(CCC_AES_CCM_WORK_BUFFER_SIZE)) {
				se_util_mem_set(wbuf, 0U, wbuf_size);
			}

			CMTAG_MEM_RELEASE(s->ae_context.ec.cmem,
					  CMTAG_ALIGNED_BUFFER,
					  wbuf);

			s->ae_context.ec.ccm.wbuf = NULL;
			s->ae_context.ec.ccm.wbuf_size = 0U;
		}
	}
}
#endif /* HAVE_AES_CCM */

status_t se_crypto_ae_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NULL context\n"));
	}

	if (ctx->ctx_class != TE_CLASS_AE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to AE reset with %u class object\n",
						       ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_AE != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for AE\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if HAVE_AES_CCM
			if (TE_ALG_AES_CCM == ctx->ctx_algo) {
				ae_reset_ccm_wbuf(ctx->ctx_run_state.rs.s_ae);
			}
#endif /* HAVE_AES_CCM */

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
			release_ae_context_dma_buf(ctx->ctx_run_state.rs.s_ae);
#endif
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U,
					sizeof_u32(struct se_ae_state_s));
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

static status_t ae_mode_setup(const struct se_ae_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NULL AE state\n"));
	}

	switch (s->ae_context.ec.algorithm) {
#if CCC_WITH_AES_GCM
	case TE_ALG_AES_GCM:
	case TE_ALG_GMAC_AES:
		// XXXX HW AES-GCM does not need special setups.
		break;
#endif

#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
		/* AES-CCM is setup at the dofinal call; it is a single-shot operation
		 * and needs the input data length which is not available
		 * before dofinal() call.
		 */
		break;
#endif

	default:
		LTRACEF("Unsupported AE algorithm 0x%x\n",
			s->ae_context.ec.algorithm);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ae_set_key_check_args(
		const crypto_context_t *ctx,
		const te_crypto_key_t *key,
		const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_AE != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for AE\n",
					     ctx->ctx_run_state.rs_select));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_ae_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
			      const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_ae_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	ret = ae_set_key_check_args(ctx, key, key_args);
	CCC_ERROR_CHECK(ret);

	s = ctx->ctx_run_state.rs.s_ae;
	if ((NULL == s) || (s->ae_magic != SE_AE_MAGIC)) {
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
				     CCC_ERROR_MESSAGE("AE only supports AES ciphers and keys\n"));
	}

	ret = setup_aes_key(&s->ae_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AES key\n"));

	ret = ae_mode_setup(s);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AE mode\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_AEAD */
