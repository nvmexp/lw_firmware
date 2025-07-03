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

/* CMAC-AES-*, HMAC-SHA-* and HMAC-MD5 support.
 *
 * => supports MD5, SHA1 and SHA2 digests for HMACs and AES block cipher for CMAC.
 *
 * If other digests are added (e.g. sha-3) code requires
 * some changes to call those different digests and the mac_state_t object
 * then needs a new digest contexts for those algorithms.
 */

#include <crypto_system_config.h>

#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5

#include <crypto_mac.h>
#include <crypto_mac_proc.h>
#include <crypto_process_call.h>

#if HAVE_CMAC_AES
#include <crypto_cipher_proc.h>
#endif
#if HAVE_HMAC_SHA
#include <crypto_md_proc.h>
#endif

#if HAVE_HMAC_MD5
#include <md5/crypto_md5_proc.h>
#include <md5/crypto_mac_md5.h>
#endif

#include <crypto_mac_intern.h>

#ifndef HAVE_HW_HMAC_SHA256_KEY_COMPRESS
#define HAVE_HW_HMAC_SHA256_KEY_COMPRESS 0
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif


#if HAVE_CONTEXT_MEMORY
/* Class mac memory fixed minimal sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s mac_mem_specs = {
#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* SHA dma size is larger than AES dma size, use that */
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE + 512U),
#endif
	/* Minimum size for context memory
	 * CMAC-AES = 1600 bytes
	 * HMAC_SHAXXX reserves more space.
	 */
	.cm_lo_size = ((CMEM_DESCRIPTOR_SIZE + 822U) +
		       (CMEM_DESCRIPTOR_SIZE + 512U) +
		       CMEM_KEY_MEMORY_STATIC_SIZE),
};
#endif /* HAVE_CONTEXT_MEMORY */

#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16U
#endif

/* Magic values for the SHA context object */
#define SE_MAC_MAGIC	0x534d4143U

/*
 * Juki => XXX openssl command for CMAC-AES-128 to verify results...
 * echo -n "data" | openssl dgst -mac cmac -macopt cipher:aes-128-cbc -macopt hexkey:00000000000000000000000000000000 -sha1
 *
 * And HMAC-SHA1
 * echo -n "data" | openssl dgst -mac hmac -macopt hexkey:00000000000000000000000000000000 -sha1
 */

#if HAVE_CMAC_DST_KEYTABLE
static status_t cmac_setup_key_derivation(te_args_init_t *args,
					  struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;
	uint32_t key_size = 0U;
	bool upper_quad = false;

	if (args->mac.mac_dst_keyslot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CMAC dst keyslot %u invalid\n",
					     args->mac.mac_dst_keyslot));
	}

	if ((args->mac.mac_flags & INIT_FLAG_MAC_KEY_SIZE_MASK) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CMAC dst key length not specified\n"));
	}

	upper_quad = ((args->mac.mac_flags & INIT_FLAG_MAC_DST_UPPER_QUAD) != 0U);

	s->cmac.context_aes.ec.aes_flags |= AES_FLAGS_DST_KEYSLOT;

	key_size = (args->mac.mac_flags & INIT_FLAG_MAC_KEY_SIZE_MASK);

	switch (key_size) {
	case INIT_FLAG_MAC_DST_KEY128:
		s->cmac.context_aes.ec.aes_flags |= AES_FLAGS_DERIVE_KEY_128;
		if (BOOL_IS_TRUE(upper_quad)) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			LTRACEF("Request to write upper quad with 128 bit key\n");
			break;
		}
		break;
	case INIT_FLAG_MAC_DST_KEY192:
		s->cmac.context_aes.ec.aes_flags |= AES_FLAGS_DERIVE_KEY_192;
		break;
	case INIT_FLAG_MAC_DST_KEY256:
		s->cmac.context_aes.ec.aes_flags |= AES_FLAGS_DERIVE_KEY_256;
		break;
	default:
		LTRACEF("CMAC-AES derivation dst key length undefined\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(upper_quad)) {
		/* Request to write key bits 128..255 */
		s->cmac.context_aes.ec.aes_flags |= AES_FLAGS_DERIVE_KEY_HIGH;
	}

	LTRACEF("CMAC destination keyslot %u (quad: %s)\n",
		args->mac.mac_dst_keyslot, (upper_quad ? "upper" : "lower"));

	/* CMAC-AES derivation target keyslot
	 */
	s->cmac.context_aes.ec.se_ks.ks_target_keyslot = args->mac.mac_dst_keyslot;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_DST_KEYTABLE */

static status_t se_crypto_mac_init_check_context(const crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("MAC init: state already set up\n"));
	}

	if ((TE_ALG_MODE_MAC != ctx->ctx_alg_mode) &&
	    (TE_ALG_MODE_MAC_VERIFY != ctx->ctx_alg_mode)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("MAC algo 0x%x mode %u unsupported\n",
						       ctx->ctx_algo, ctx->ctx_alg_mode));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CMAC_AES
static status_t se_crypto_cmac_aes_init(const crypto_context_t *ctx,
					struct context_mem_s *cmem,
					te_args_init_t *args,
					struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->cmac.context_aes,
				      cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("CMAC aes context static init failed: %d\n", ret));

	/* Setup  space for the CMAC subkey and CMAC intermediate/final value is
	 * callwlated to the phash buffer in aes context. This overlaps
	 * the iv_stash buffer used for other aes modes so it does not take up space.
	 *
	 * The engine layer uses the phash but it does not need the subkeys; the subkeys
	 * are used by the process layer.
	 */
	s->cmac.context_aes.ec.is_hash    = true;
	s->cmac.context_aes.ec.is_encrypt = false;
	s->cmac.context_aes.cmac.pk1	  = s->cmac.pk1;
	s->cmac.context_aes.cmac.pk2	  = s->cmac.pk2;

	/* Return the id of the selected engine to caller (information only) */
	args->engine_hint = s->cmac.context_aes.ec.engine->e_id;

#if HAVE_CMAC_DST_KEYTABLE
	if ((args->mac.mac_flags & INIT_FLAG_MAC_DST_KEYSLOT) != 0U) {
		ret = cmac_setup_key_derivation(args, s);
		CCC_ERROR_CHECK(ret);
	} else {
		if ((args->mac.mac_flags & INIT_FLAG_MAC_KEY_SIZE_MASK) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("CMAC dst key length flags set without destination flag\n"));
		}
	}
#endif /* HAVE_CMAC_DST_KEYTABLE  */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t call_aes_cmac_process_context(struct se_data_params *input, struct se_aes_context *actx)
{
	status_t ret = NO_ERROR;

	ret = PROCESS_AES_CMAC(input, actx);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("CMAC AES failed: %d\n", ret));
fail:
	return ret;
}
#endif /* HAVE_CMAC_AES */

#if HAVE_HMAC_SHA
static status_t se_crypto_hmac_sha_init(const crypto_context_t *ctx,
					struct context_mem_s *cmem,
					te_args_init_t *args,
					struct se_mac_state_s *s,
					te_crypto_algo_t digest_algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Properties of the SHA digest used for the HMAC */
	ret = sha_context_static_init(ctx->ctx_domain, args->engine_hint,
				      digest_algorithm, &s->hmac.context_sha,
				      cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("HMAC sha context static init failed: %d\n", ret));

	s->hmac.ipad_ok = false;
	args->engine_hint = s->hmac.context_sha.ec.engine->e_id;

	/* Set the context algorithm for non-digest HMAC-SHA operation
	 */
	s->hmac.context_sha.ec.algorithm = ctx->ctx_algo;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t call_sha_hmac_process_context(struct se_data_params *input, struct se_sha_context *sctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = PROCESS_SHA(input, sctx);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("HMAC SHA failed: %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HMAC_SHA */

static status_t crypto_mac_init(crypto_context_t *ctx,
				te_args_init_t *args,
				struct se_mac_state_s *s,
				struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	(void)args;

	switch(ctx->ctx_algo) {
#if HAVE_HMAC_MD5
	case TE_ALG_HMAC_MD5:
		s->mac_type = MAC_TYPE_HMAC_MD5;
		ret = se_crypto_hmac_md5_init(ctx, cmem, args, s, TE_ALG_MD5);
		break;
#endif

#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES:		/* CMAC-AES, key length 128, 196, 256 */
		s->mac_type = MAC_TYPE_CMAC_AES;
		// CMAC uses RFC specified IV value (zeroes) set in CALLOC
		ret = se_crypto_cmac_aes_init(ctx, cmem, args, s);
		break;
#endif

#if HAVE_HMAC_SHA
#if HAVE_SHA1
	case TE_ALG_HMAC_SHA1:		/* HMAC-SHA-1 */
		s->mac_type = MAC_TYPE_HMAC_SHA1;
		ret = se_crypto_hmac_sha_init(ctx, cmem, args, s, TE_ALG_SHA1);
		break;
#endif
#if HAVE_SHA224
	case TE_ALG_HMAC_SHA224:	/* HMAC-SHA-224 */
		s->mac_type = MAC_TYPE_HMAC_SHA2;
		ret = se_crypto_hmac_sha_init(ctx, cmem, args, s, TE_ALG_SHA224);
		break;
#endif
	case TE_ALG_HMAC_SHA256:	/* HMAC-SHA-256 */
		s->mac_type = MAC_TYPE_HMAC_SHA2;
		ret = se_crypto_hmac_sha_init(ctx, cmem, args, s, TE_ALG_SHA256);
		break;

	case TE_ALG_HMAC_SHA384:	/* HMAC-SHA-384 */
		s->mac_type = MAC_TYPE_HMAC_SHA2;
		ret = se_crypto_hmac_sha_init(ctx, cmem, args, s, TE_ALG_SHA384);
		break;

	case TE_ALG_HMAC_SHA512:	/* HMAC-SHA-512 */
		s->mac_type = MAC_TYPE_HMAC_SHA2;
		ret = se_crypto_hmac_sha_init(ctx, cmem, args, s, TE_ALG_SHA512);
		break;
#endif /* HAVE_HMAC_SHA */

#if HAVE_GMAC_AES
		/* GMAC-AES handled in crypto_ae.c(se_crypto_ae_init),
		 * it is a combined MAC && AES CTR mode (AEAD mode)
		 */
	case TE_ALG_GMAC_AES:
		LTRACEF("GMAC entered MAC handler, needs to be in AE\n");
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("MAC algorithm 0x%x unsupported\n", ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	s->mac_magic = SE_MAC_MAGIC;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_MAC;
	ctx->ctx_run_state.rs.s_mac = s;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
/* If a separate DMA buffer is allocated for mac context field buf,
 * release it.
 */
static void release_mac_context_dma_buf(struct se_mac_state_s *s_mac)
{
	if (NULL != s_mac) {
		switch (s_mac->mac_type) {
#if HAVE_CMAC_AES
		case MAC_TYPE_CMAC_AES:
			aes_context_dma_buf_release(&s_mac->cmac.context_aes);
			break;
#endif

#if HAVE_HMAC_SHA
		case MAC_TYPE_HMAC_SHA1:
		case MAC_TYPE_HMAC_SHA2:
		case MAC_TYPE_HW_HMAC_SHA2:
			sha_context_dma_buf_release(&s_mac->hmac.context_sha);
			break;
#endif
		default:
			/* these mac contexts not using
			 * separate dma buffers
			 */
			break;
		}
	}
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

status_t se_crypto_mac_init(crypto_context_t *ctx,
			    te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_mac_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("Generic MAC");

	ret = se_crypto_mac_init_check_context(ctx);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &mac_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_mac_state_s,
				 struct se_mac_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = crypto_mac_init(ctx, args, s, cmem);
	CCC_ERROR_CHECK(ret);

	s = NULL;
fail:
	if (NULL != s) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		release_mac_context_dma_buf(s);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

#if HAVE_SW_HMAC_SHA || HAVE_HMAC_MD5

#if HAVE_SW_HMAC_SHA
static status_t hmac_handle_ipad_intern_sha(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(s->hmac.context_sha.ec.is_first)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (s->hmac.context_sha.used != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	se_util_mem_set(s->hmac.context_sha.buf, HMAC_IPAD_VALUE,
			s->hmac.context_sha.ec.block_size);

	for (inx = 0U; inx < s->hmac.hmac_key_size; inx++) {
		s->hmac.context_sha.buf[inx] ^= BYTE_VALUE(s->hmac.hmac_key[inx]);
	}

	s->hmac.context_sha.used = s->hmac.context_sha.ec.block_size;

fail:
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

static status_t hmac_sha_opad_calc(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	struct se_engine_sha_context *sha_econtext = NULL;
	uint32_t inx;

	LTRACEF("entry\n");

	// SHA
	if (!BOOL_IS_TRUE(s->hmac.context_sha.ec.is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Nothing can remain in the context buffer after the digest completes */
	if (s->hmac.context_sha.used != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Copy the current SHA digest value to the OPAD sha digest
	 * input below.
	 *
	 * HMAC = hash(opad || hash(ipad || message))
	 *
	 * The "INNER_DIGEST" value (hash(ipad || message) is now in
	 *  s->hmac.context_sha.ec.tmphash
	 */

	/* Use the contiguous s->hmac.context_sha.buf, it is now free.
	 *  opad = 0x5C....0x5C XOR key
	 */
	se_util_mem_set(s->hmac.context_sha.buf, HMAC_OPAD_VALUE,
			s->hmac.context_sha.ec.block_size);
	for (inx = 0U; inx < s->hmac.hmac_key_size; inx++) {
		s->hmac.context_sha.buf[inx] =
		    BYTE_VALUE((uint32_t)s->hmac.context_sha.buf[inx] ^ s->hmac.hmac_key[inx]);
	}

	/* Just mark this as "in use" */
	s->hmac.context_sha.used = s->hmac.context_sha.ec.block_size;

	sha_econtext = CMTAG_MEM_GET_OBJECT(s->hmac.context_sha.ec.cmem,
					    CMTAG_ECONTEXT,
					    0U,
					    struct se_engine_sha_context,
					    struct se_engine_sha_context *);
	if (NULL == sha_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Digest the key value with the same digest
	 * as the HMAC is done with.
	 *
	 * Set the CMEM ref to sha engine context from HMAC-SHA context
	 */
	ret = sha_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY,
						s->hmac.context_sha.ec.hash_algorithm,
						sha_econtext,
						s->hmac.context_sha.ec.cmem);
	CCC_ERROR_CHECK(ret);

	input.src         = s->hmac.context_sha.buf;
	input.input_size  = sha_econtext->block_size;
	input.dst         = NULL;
	input.output_size = 0U;

	DUMP_DATA_PARAMS("opad HMAC", 0x1U, &input);

	/* Shortlwt directly to the SE HW function,
	 * this does not need the gather buffer services because the first
	 * digest data (opad) is exactly block size long and the second
	 * digest (inner digest result) is_last !
	 */
	HW_MUTEX_TAKE_ENGINE(sha_econtext->engine);

	ret = CCC_ENGINE_SWITCH_SHA(sha_econtext->engine, &input, sha_econtext);
	if (NO_ERROR == ret) {
		/* Move the INNER_DIGEST to the contiguous context buffer
		 * and feed it to SHA. The output of the ECONTEXT SHA digest
		 * is written to the CONTEXT tmphash (not to ECONTEXT).
		 *
		 * This is also the HMAC result.
		 */
		se_util_mem_move(s->hmac.context_sha.buf,
				 s->hmac.context_sha.ec.tmphash,
				 s->hmac.context_sha.ec.hash_size);

		/* Just mark this as "in use" */
		s->hmac.context_sha.used = s->hmac.context_sha.ec.hash_size;

		input.src         = s->hmac.context_sha.buf;
		input.input_size  = s->hmac.context_sha.ec.hash_size;
		input.dst         = s->hmac.context_sha.ec.tmphash;
		input.output_size = sizeof_u32(s->hmac.context_sha.ec.tmphash);

		sha_econtext->is_last = true;

		ret = CCC_ENGINE_SWITCH_SHA(sha_econtext->engine,
					    &input, sha_econtext);
		/* ret : value checked below after MUTEX release */

		/* Feel free again */
		s->hmac.context_sha.used = 0U;

		DUMP_DATA_PARAMS("Result HMAC", 0x3U, &input);
	}

	HW_MUTEX_RELEASE_ENGINE(sha_econtext->engine);

	CCC_ERROR_CHECK(ret);
fail:
	if (NULL != sha_econtext) {
		CMTAG_MEM_RELEASE(s->hmac.context_sha.ec.cmem,
				  CMTAG_ECONTEXT,
				  sha_econtext);
	}

	LTRACEF("exit\n");
	return ret;
}
#endif /* HAVE_SW_HMAC_SHA */

status_t hmac_handle_ipad(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == s) || (NULL == s->hmac.hmac_key)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {

		switch (s->mac_type) {
#if HAVE_SW_HMAC_SHA
		case MAC_TYPE_HMAC_SHA1:
		case MAC_TYPE_HMAC_SHA2:
			ret = hmac_handle_ipad_intern_sha(s);
			break;
#endif /* HAVE_SW_HMAC_SHA */

#if HAVE_HMAC_MD5
		case MAC_TYPE_HMAC_MD5:
			ret = hmac_handle_ipad_intern_md5(s);
			break;
#endif /* HAVE_HMAC_MD5 */

		default:
			CCC_ERROR_MESSAGE("HMAC type %u not supported\n", s->mac_type);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
		CCC_ERROR_CHECK(ret);

		s->hmac.ipad_ok = true;
	}

fail:
	LTRACEF("exit\n");
	return ret;
}

status_t hmac_handle_opad(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry: %p\n", s);

	if ((NULL == s) || (NULL == s->hmac.hmac_key)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	switch (s->mac_type) {
#if HAVE_SW_HMAC_SHA
	case MAC_TYPE_HMAC_SHA1:
	case MAC_TYPE_HMAC_SHA2:
		ret = hmac_sha_opad_calc(s);
		break;
#endif /* HAVE_SW_HMAC_SHA */

#if HAVE_HMAC_MD5
	case MAC_TYPE_HMAC_MD5:
		ret = hmac_md5_opad_calc(s);
		break;
#endif /* HAVE_HMAC_MD5 */

	default:
		CCC_ERROR_MESSAGE("HMAC type %u not supported\n", s->mac_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SW_HMAC_SHA || HAVE_HMAC_MD5 */

static status_t mac_update_arg_check(const crypto_context_t *ctx,
				     const struct run_state_s *run_state)
{
	status_t ret = NO_ERROR;
	const struct se_mac_state_s *s = NULL;

	if (RUN_STATE_SELECT_MAC != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for MAC\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_mac;
	if ((NULL == s) || (s->mac_magic != SE_MAC_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("MAC magic invalid\n"));
	}

	if (ctx->ctx_class != TE_CLASS_MAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to do MAC update with %u class object\n",
						       ctx->ctx_class));
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("MAC key not defined\n"));
	}
fail:
	return ret;
}

status_t se_crypto_mac_update(const crypto_context_t *ctx, struct run_state_s *run_state,
			      te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_mac_state_s *s = NULL;
	struct se_data_params input;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = mac_update_arg_check(ctx, run_state);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_mac;

	LTRACEF("MAC DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("MAC SRC: %p, src_size %u\n", args->src, args->src_size);

	// XXX for NULL or zero sized src update could just return, not trap.
	if (NULL == args->src) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("MAC src/dst invalid\n"));
	}

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = NULL;
	input.output_size = 0U;

	switch (s->mac_type) {
#if HAVE_SW_HMAC_SHA
	case MAC_TYPE_HMAC_SHA1:
	case MAC_TYPE_HMAC_SHA2:
		if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {
			ret = hmac_handle_ipad(s);
			if (NO_ERROR != ret) {
				break;
			}
		}

		ret = call_sha_hmac_process_context(&input, &s->hmac.context_sha);
		break;
#endif

#if HAVE_HW_HMAC_SHA
	case MAC_TYPE_HW_HMAC_SHA2:
		ret = call_sha_hmac_process_context(&input, &s->hmac.context_sha);
		break;
#endif

#if HAVE_HMAC_MD5
	case MAC_TYPE_HMAC_MD5:
		if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {
			ret = hmac_handle_ipad(s);
			if (NO_ERROR != ret) {
				break;
			}
		}

		ret = call_md5_hmac_process_context(&input, &s->hmac.context_md5);
		break;
#endif

#if HAVE_CMAC_AES
	case MAC_TYPE_CMAC_AES:
		// Can SE HW handle zero length digests?
		ret = call_aes_cmac_process_context(&input, &s->cmac.context_aes);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("MAC type %u not supported\n", s->mac_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	// MAC update never writes anything to client output buffer
	args->dst_size = 0U;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mac_dofinal_check_args_rest2(
	const crypto_context_t *ctx,
	const te_args_data_t *args,
	const struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;

	(void)s;

	LTRACEF("entry\n");

	if (TE_ALG_MODE_MAC_VERIFY == ctx->ctx_alg_mode) {
		if ((NULL == args->src_mac) || (0U == args->src_mac_size)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	} else {
#if HAVE_CMAC_AES
		/* CMAC-AES dst may be keyslot and this can be NULL; dst checked by caller
		 */
		if ((TE_ALG_CMAC_AES != ctx->ctx_algo) ||
		    ((s->cmac.context_aes.ec.aes_flags & AES_FLAGS_DST_KEYSLOT) == 0U)) {
			if (NULL == args->dst) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						     CCC_ERROR_MESSAGE("MAC dst invalid\n"));
			}
		}
#else
		if (NULL == args->dst) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("MAC dst invalid\n"));
		}
#endif /* HAVE_CMAC_AES */
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t crypto_mac_dofinal_check_args_rest(
	const crypto_context_t *ctx,
	const te_args_data_t *args,
	const struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (ctx->ctx_class != TE_CLASS_MAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to do MAC dofinal with %u class object\n",
						       ctx->ctx_class));
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("MAC key not defined\n"));
	}

	if ((NULL == args->src) && (args->src_size > 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Mac src invalid (NULL but size nonzero)\n"));
	}

	ret = mac_dofinal_check_args_rest2(ctx, args, s);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_mac_dofinal_check_args(
	const crypto_context_t *ctx,
	const struct run_state_s *run_state,
	const te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	const struct se_mac_state_s *s = NULL;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_MAC != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for MAC\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_mac;
	if ((NULL == s) || (s->mac_magic != SE_MAC_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("MAC magic invalid\n"));
	}

	/* Call separate function to check arguments in argument check function,
	 * because CCM was > 10.
	 */
	ret = crypto_mac_dofinal_check_args_rest(ctx, args, s);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Automotive requested MAC signature verifications,
 * so return ERR_SIGNATURE_ILWALID on mac mismatch
 */
static status_t compare_mac_signature(const uint8_t *result,
				      uint32_t result_size,
				      const te_args_data_t *args)
{
	status_t ret = ERR_BAD_STATE;

	/* XXX Already checked... */
	if (NULL == args->src_mac) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (result_size != args->src_mac_size) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	if (!BOOL_IS_TRUE(se_util_vec_match(result, args->src_mac,
					    result_size, CFALSE, &ret))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_HMAC_SHA || HAVE_HMAC_MD5
status_t handle_hmac_result(const crypto_context_t *ctx,
			    uint8_t *result,
			    uint32_t result_size,
			    te_args_data_t *args,
			    status_t *rbad_p)
{
	status_t ret = ERR_BAD_STATE;

	if (TE_ALG_MODE_MAC_VERIFY == ctx->ctx_alg_mode) {
		ret = compare_mac_signature(result, result_size, args);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = copy_data_to_domain_address(ctx->ctx_domain, args->dst,
						  result, result_size);
		CCC_ERROR_CHECK(ret);
		args->dst_size = result_size;
	}

	*rbad_p = NO_ERROR;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HMAC_SHA || HAVE_HMAC_MD5 */

#if HAVE_SW_HMAC_SHA
static status_t mac_hmac_sha_dofinal(const crypto_context_t *ctx,
				     struct run_state_s *run_state,
				     te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	struct se_mac_state_s *s = NULL;
	struct se_data_params input;
	uint8_t *result = NULL; /* Some target subsystem functions do not like const pointers */
	uint32_t result_size = 0U;

	LTRACEF("entry\n");

	s = run_state->rs.s_mac;
	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.output_size = args->dst_size;

	if (input.output_size < s->hmac.context_sha.ec.hash_size) {
		CCC_ERROR_MESSAGE("HMAC SHA result does not fit in %u bytes\n",
			      input.output_size);
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {
		ret = hmac_handle_ipad(s);
		CCC_ERROR_CHECK(ret);
	}

	LTRACEF("Mac algo 0x%x, Digest s->hmac.context_sha.hash_algorithm: 0x%x\n",
		ctx->ctx_algo, s->hmac.context_sha.ec.hash_algorithm);

	/* Do not copy the HMAC intermediate digest to client buffer
	 * (neither to TA user space buffer nor Trusty client buffer)
	 *
	 * If this is called from a TA, the operation domain is TA and it will call
	 * ta_se_sha_process_input(), which calls se_sha_process_input()
	 * which will place the SHA digest to tmphash *always*, also in the case
	 * where dst == NULL. When that returns the ta_se_sha_process_input()
	 * does not copy the sha result to TA space, because the dst is NULL
	 * Notice that we can not set the dst to s->hmac.context_sha.ec.tmphash
	 * because the code would trat that as a TA address soace buffer and
	 * trap it (because it is not a TA address).
	 *
	 * So, just leave this to NULL.
	 */
	input.dst = NULL;

	/* Finalize the digest value callwlation to ec.tmphash buffer,
	 * hmac_handle_opad() uses the hash value.
	 */
	s->hmac.context_sha.ec.is_last = true;

	ret = call_sha_hmac_process_context(&input, &s->hmac.context_sha);
	CCC_ERROR_CHECK(ret);

	/* This finds the digest value from the context and overwrites
	 *  the same context value with the final result.
	 */
	ret = hmac_handle_opad(s);
	CCC_ERROR_CHECK(ret);

	/* Copy the HMAC final digest value from context_sha.ec.tmphash to client buffer
	 */
	result = &s->hmac.context_sha.ec.tmphash[0];
	result_size = s->hmac.context_sha.ec.hash_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("HMAC-SHA", s->hmac.context_sha.ec.perf_usec);
#endif

	ret = handle_hmac_result(ctx, result, result_size, args, &rbad);
	CCC_ERROR_CHECK(ret);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = SE_ERROR(ERR_FAULT);
	}
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}
#endif /* HAVE_SW_HMAC_SHA */

#if HAVE_HW_HMAC_SHA
static status_t mac_hw_hmac_sha_dofinal(const crypto_context_t *ctx,
					struct run_state_s *run_state,
					te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	struct se_mac_state_s *s = NULL;
	struct se_data_params input;
	uint8_t *result = NULL; /* Some target subsystem functions do not like const pointers */
	uint32_t result_size = 0U;

	LTRACEF("entry\n");

	s = run_state->rs.s_mac;
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	if (NULL == args->dst) {
		/* NULL only with CMAC-AES derivation to keyslot
		 * args->dst_size is verified 0 below as well (special case).
		 */
		input.dst = NULL;
	} else {
		LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	}
	input.input_size = args->src_size;
	input.output_size = args->dst_size;

	if (input.output_size < s->hmac.context_sha.ec.hash_size) {
		CCC_ERROR_MESSAGE("HMAC SHA result does not fit in %u bytes\n",
				  input.output_size);
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("HMAC HW algo 0x%x, Digest s->hmac.context_sha.hash_algorithm: 0x%x\n",
		s->hmac.context_sha.ec.algorithm, s->hmac.context_sha.ec.hash_algorithm);

	s->hmac.context_sha.ec.is_last = true;

	/* Do not copy the HMAC intermediate digest to client buffer
	 * (neither to TA user space buffer nor Trusty client buffer)
	 *
	 * If this is called from a TA, the operation domain is TA and it will call
	 * ta_se_sha_process_input(), which calls se_sha_process_input()
	 * which will place the SHA digest to tmphash *always*, also in the case
	 * where dst == NULL. When that returns the ta_se_sha_process_input()
	 * does not copy the sha result to TA space, because the dst is NULL
	 * Notice that we can not set the dst to s->hmac.context_sha.ec.tmphash
	 * because the code would trat that as a TA address soace buffer and
	 * trap it (because it is not a TA address).
	 *
	 * So, just leave this to NULL.
	 */
	input.dst = NULL;

	ret = call_sha_hmac_process_context(&input, &s->hmac.context_sha);
	CCC_ERROR_CHECK(ret);

	/* Copy the HMAC final digest value from context_sha.ec.tmphash to client buffer
	 */
	result = &s->hmac.context_sha.ec.tmphash[0];
	result_size = s->hmac.context_sha.ec.hash_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("HW-HMAC-SHA2", s->hmac.context_sha.ec.perf_usec);
#endif

	ret = handle_hmac_result(ctx, result, result_size, args, &rbad);
	CCC_ERROR_CHECK(ret);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = SE_ERROR(ERR_FAULT);
	}
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}
#endif /* HAVE_HW_HMAC_SHA */

#if HAVE_CMAC_AES
static status_t mac_cmac_aes_dofinal(const crypto_context_t *ctx,
				     struct run_state_s *run_state,
				     te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_mac_state_s *s = NULL;
	struct se_data_params input;
	uint8_t *mac_dmabuf = NULL;

	LTRACEF("entry\n");

	(void)ctx;

	s = run_state->rs.s_mac;
	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	input.output_size = args->dst_size;

	if (NULL == args->dst) {
		/* NULL only with CMAC-AES derivation to keyslot
		 * args->dst_size is verified 0 below as well (special case).
		 */
		input.dst = NULL;
	} else {
		if (TE_ALG_MODE_MAC_VERIFY == ctx->ctx_alg_mode) {
			mac_dmabuf = CMTAG_MEM_GET_BUFFER(s->cmac.context_aes.ec.cmem,
							  CMTAG_ALIGNED_DMA_BUFFER,
							  CCC_DMA_MEM_ALIGN,
							  DMA_ALIGN_SIZE(AES_BLOCK_SIZE));
			if (NULL == mac_dmabuf) {
				CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
			}

			input.dst = mac_dmabuf;
			input.output_size = AES_BLOCK_SIZE;
		} else {
			LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
			input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		}
	}

	if ((s->cmac.context_aes.ec.aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
		/* CMAC verify can not target a keyslot
		 */
		if (TE_ALG_MODE_MAC_VERIFY == ctx->ctx_alg_mode) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		/* dst must be NULL when destination is keyslot
		 */
		if (NULL != input.dst) {
			CCC_ERROR_MESSAGE("CMAC dst keyslot: dst\n");
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		if (0U != input.output_size) {
			CCC_ERROR_MESSAGE("CMAC dst keyslot: len\n");
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	} else {
		if (NULL == input.dst) {
			CCC_ERROR_MESSAGE("MAC dst invalid\n");
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		if (input.output_size < AES_BLOCK_SIZE) {
			CCC_ERROR_MESSAGE("CMAC AES result does not fit in %u bytes\n",
					  input.output_size);
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

	s->cmac.context_aes.ec.is_last = true;

	ret = call_aes_cmac_process_context(&input, &s->cmac.context_aes);
	CCC_ERROR_CHECK(ret);

	/* CMAC final value copied in the call above.
	 * If call succeeds 16 bytes are written. Target depends
	 * on call arguments: either memory or optionally to keytable (in T19X)
	 *
	 * In the latter case dst is NULL but 16 bytes get written
	 * to keytable.
	 */
	if ((s->cmac.context_aes.ec.aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
		args->dst_size = input.output_size;
	} else {
		if (TE_ALG_MODE_MAC_VERIFY == ctx->ctx_alg_mode) {
			ret = compare_mac_signature(mac_dmabuf, AES_BLOCK_SIZE, args);
			CCC_ERROR_CHECK(ret);
		} else {
			args->dst_size = AES_BLOCK_SIZE;
		}
	}
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("CMAC-AES", s->cmac.context_aes.ec.perf_usec);
#endif

fail:
	if (NULL != mac_dmabuf) {
		CMTAG_MEM_RELEASE(s->cmac.context_aes.ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  mac_dmabuf);
	}
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES */

status_t se_crypto_mac_dofinal(const crypto_context_t *ctx,
			       struct run_state_s *run_state,
			       te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_mac_state_s *s = NULL;

	LTRACEF("entry\n");

	(void)init_args; // exists for passing back final state in some ops

	ret = se_crypto_mac_dofinal_check_args(ctx, run_state, args);
	CCC_ERROR_CHECK(ret);

	s = run_state->rs.s_mac;

	switch (s->mac_type) {
#if HAVE_SW_HMAC_SHA
	case MAC_TYPE_HMAC_SHA1:
	case MAC_TYPE_HMAC_SHA2:
		ret = mac_hmac_sha_dofinal(ctx, run_state, args);
		break;
#endif /* HAVE_SW_HMAC_SHA */

#if HAVE_HW_HMAC_SHA
	case MAC_TYPE_HW_HMAC_SHA2:
		ret = mac_hw_hmac_sha_dofinal(ctx, run_state, args);
		break;
#endif /* HAVE_HW_HMAC_SHA */

#if HAVE_HMAC_MD5
	case MAC_TYPE_HMAC_MD5:
		ret = mac_hmac_md5_dofinal(ctx, run_state, args);
		break;
#endif /* HAVE_HMAC_MD5 */

#if HAVE_CMAC_AES
	case MAC_TYPE_CMAC_AES:
		ret = mac_cmac_aes_dofinal(ctx, run_state, args);
		break;
#endif /* HAVE_CMAC_AES */

	default:
		CCC_ERROR_MESSAGE("MAC type %u not supported\n", s->mac_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

status_t se_crypto_mac_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_MAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to MAC reset with %u class object\n",
						   ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_MAC != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for MAC\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
			/* If a separate DMA buffer is allocated for md context field buf,
			 * release it.
			 */
			release_mac_context_dma_buf(ctx->ctx_run_state.rs.s_mac);
#endif
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U, sizeof_u32(struct se_mac_state_s));
		}
		CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
				  CMTAG_API_STATE,
				  ctx->ctx_run_state.rs.s_object);
	}
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

fail:
	MEASURE_MEMORY_STOP;

	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

#if HAVE_CMAC_AES
static status_t cmac_set_key(te_crypto_key_t *key, const te_args_key_data_t *kargs,
			     struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (kargs->k_key_type != KEY_TYPE_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("CMAC: only AES keys accepted\n"));
	}

	ret = setup_aes_key(aes_context, key, kargs);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup CMAC-AES key\n"));

	ret = cmac_process_derive_subkeys(aes_context);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES */

#if HAVE_HMAC_SHA

#if HAVE_SW_HMAC_SHA || HAVE_HW_HMAC_SHA256_KEY_COMPRESS
/*
 * Copy the long HMAC key to a temporary DMA accessible buffer,
 * digest it and release the erased copy.
 *
 * Note: This temporary key handling buffer is not allocated from the
 * KEY MEMORY as it needs to be accessed by DMA.
 */
static status_t digest_long_hmac_sha_key(struct se_sha_context *sha_context,
					 te_crypto_domain_t	cdomain,
					 te_crypto_key_t       *key,
					 const te_args_key_data_t *kargs)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint8_t *kbuf = NULL;
	uint32_t klen = kargs->k_byte_size;

	LTRACEF("entry\n");

	key->k_byte_size = 0U;

#if (HAVE_SW_HMAC_SHA == 0) && HAVE_HW_HMAC_SHA256_KEY_COMPRESS
	if (TE_ALG_SHA256 != sha_context->ec.hash_algorithm) {
		/* Only SHA256 compresses a very long key into 256 bit
		 * key which matches one of the HW supported key
		 * lengths: 128, 192 or 256 bits.
		 *
		 * So a very long key compressed by SHA256 will be
		 * used as HMAC-SHA256 key of 256 bits by HMAC-SHA2
		 * HW; others digests do not produce supported lengths
		 * and are rejected here.
		 */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("HW HMAC key compression not supported for this digest\ņ"));
	}
#endif /* (HAVE_SW_HMAC_SHA == 0) && HAVE_HW_HMAC_SHA256_KEY_COMPRESS */

	kbuf = CMTAG_MEM_GET_BUFFER(sha_context->ec.cmem,
				    CMTAG_ALIGNED_DMA_BUFFER,
				    CCC_DMA_MEM_ALIGN,
				    klen);
	if (NULL == kbuf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Copy the client buffer long key from kargs
	 * to the DMA accessible kbuf requested above.
	 */
	se_util_mem_move(kbuf, kargs->k_hmac_key, klen);

	input.src         = kbuf;
	input.input_size  = klen;
	input.dst         = key->k_hmac_key;
	input.output_size = sizeof_u32(key->k_hmac_key);

	// XXX CHECK if possible to use input sha context for this sha as well???
	ret = sha_digest(sha_context->ec.cmem, cdomain, &input,
			 sha_context->ec.hash_algorithm);
	CCC_ERROR_CHECK(ret);

	key->k_byte_size = input.output_size;
fail:
	if (NULL != kbuf) {
		/* Erase kbuf copy */
		se_util_mem_set(kbuf, 0U, klen);
		CMTAG_MEM_RELEASE(sha_context->ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  kbuf);
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SW_HMAC_SHA || HAVE_HW_HMAC_SHA256_KEY_COMPRESS */

/* When the hmac key is longer than SHA block size it gets compressed
 *  with the digest algorithm.
 *
 * The smallest block size for SHA digests is 64 bytes, so it must be
 * longer than that to get DMA accessed.
 */
static status_t hmac_sha_set_key(struct se_mac_state_s *s,
				 te_crypto_key_t *key,
				 const te_args_key_data_t *kargs,
				 struct se_sha_context *sha_context,
				 te_crypto_domain_t   cdomain)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (kargs->k_key_type != KEY_TYPE_HMAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("HMAC: only supports HMAC keys\n"));
	}

	/* NOTE about HMAC HW CONTEXT keyslots =>
	 * Need to leave the key to keyslot during this HW context operation
	 *  and CLEAR IT AT THE END. Same for AES-GCM and other engine operations
	 *  for which the intermediate keyslot context can not be saved by SW.
	 */
	key->k_keyslot = kargs->k_keyslot;	/* Used only by HMAC HW */
	key->k_flags   = kargs->k_flags;

	/* NO-OP for hybrid mode HMAC-SHA
	 */
	if ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) {
		sha_context->ec.sha_flags |= SHA_FLAGS_LEAVE_KEY_TO_KEYSLOT;
	}

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		key->k_byte_size = kargs->k_byte_size;

		/* Engine code must not write the key */
		sha_context->ec.sha_flags |= SHA_FLAGS_USE_PRESET_KEY;

		LTRACEF("HMAC key size %u, preset to kslot %u\n",
			key->k_byte_size, key->k_keyslot);
	} else {
		if (kargs->k_byte_size > sha_context->ec.block_size) {
#if HAVE_SW_HMAC_SHA || HAVE_HW_HMAC_SHA256_KEY_COMPRESS
			ret = digest_long_hmac_sha_key(sha_context, cdomain,
						       key, kargs);
			CCC_ERROR_CHECK(ret);
#else
			(void)cdomain;
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("HW HMAC key compression not supported\ņ"));
#endif /* HAVE_SW_HMAC_SHA || HAVE_HW_HMAC_SHA256_KEY_COMPRESS */
		} else {
			/* Use the supplied value directly as HMAC key */
			se_util_mem_move(key->k_hmac_key, kargs->k_hmac_key, kargs->k_byte_size);
			key->k_byte_size = kargs->k_byte_size;
		}

		/* ipad and opad callwlators access the hmac key from here.
		 * Engine context does not need access to the key.
		 */
		s->hmac.hmac_key      = key->k_hmac_key;
		s->hmac.hmac_key_size = key->k_byte_size;

		LTRACEF("HMAC key size %u\n", key->k_byte_size);
		LOG_HEXBUF("HMAC key", key->k_hmac_key, key->k_byte_size);
	}

	key->k_key_type  = kargs->k_key_type;

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		/* If a key already is in keyslot, do not force it to be written there
		 * in this operation.
		 */
		key->k_flags &= ~KEY_FLAG_FORCE_KEYSLOT;
		LTRACEF("Clearing FORCE_KEYSLOT when USE_KEYSLOT flag is set\n");
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_HW_HMAC_SHA

static status_t hmac_handler_type_uses_hw(struct se_mac_state_s *s,
					  const te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;
	struct se_key_slot *ks = &s->hmac.context_sha.ec.se_ks;

	/* Swap the MAC_TYPE_HMAC_SHA2 into MAC_TYPE_HW_HMAC_SHA2
	 */
	s->mac_type = MAC_TYPE_HW_HMAC_SHA2;
	s->hmac.ipad_ok = true;	/* Callwlated by HW into keyslot context */

	/* Switch engine calls to use the SHA engine in HMAC mode and
	 * restore HMAC-HW algorithm to mac algorithm.
	 */
	s->hmac.context_sha.ec.is_hw_hmac = true;

	LTRACEF("Selecting HMAC-SHA2 HW engine (kslot %u)\n",
		key->k_keyslot);

	/* Cert-C overflow check on multiply below */
	if ((16U != key->k_byte_size) &&
	    (24U != key->k_byte_size) &&
	    (32U != key->k_byte_size)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ks->ks_slot = key->k_keyslot;
	ks->ks_bitsize = (key->k_byte_size * 8U);
	ks->ks_keydata = NULL;

	if ((key->k_flags & KEY_FLAG_FORCE_KEYSLOT) != 0U) {
		// XXXX FIXME: Verify what the manifest is going to be!!!
		ks->ks_keydata = s->hmac.hmac_key;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t hmac_handler_by_keyflags_intern(struct se_mac_state_s *s,
						te_crypto_key_t *key,
						bool hw_support)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

#if HAVE_SW_HMAC_SHA == 0
	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		/* Only HW HMAC is supported, key must be in
		 * a keyslot.
		 */
		key->k_flags |= KEY_FLAG_FORCE_KEYSLOT;
	}
#endif

	if (((key->k_flags & KEY_FLAG_FORCE_KEYSLOT) != 0U) ||
	    ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U)) {

		if (!BOOL_IS_TRUE(hw_support)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
		}

		/* Select HW or SW/Hybrid operation */
		ret = hmac_handler_type_uses_hw(s, key);
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t hmac_handler_type_select(struct se_mac_state_s *s,
					 te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;
	bool hw_support = false;
	bool hmac_sha2 = MAC_TYPE_HMAC_SHA2 == s->mac_type;

	LTRACEF("entry\n");

	/* Keyslot must be valid (AES keyslots == HMAC keyslots) */
	hw_support = (key->k_keyslot < SE_AES_MAX_KEYSLOTS);

	/* Only HMAC-SHA2 algos supported by HW */
	hw_support = hmac_sha2 && hw_support;

	// XXXXX FIXME: CHECK IF CCC needs the k_byte_size to be set even if using an existing key!!!

	if (((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) == 0U)) {
		/* Only 128/16B, 192/24B and 256/32B
		 * bit HMAC-SHA2 keys supported by HW
		 *
		 * But this value is irrelevant if the key is already in keyslot.
		 * Then the KAC keyslot manifest specifies the key size, not this.
		 */
		hw_support = ((16U == key->k_byte_size) ||
			      (24U == key->k_byte_size) ||
			      (32U == key->k_byte_size)) && hw_support;
	} else {
		if (BOOL_IS_TRUE(hw_support)) {
			s->hmac.hmac_key = NULL;
			LTRACEF("hmac_key set NULL, HW uses existing keyslot value\n");
		}
	}

#if HAVE_SW_HMAC_SHA == 0
	if (!BOOL_IS_TRUE(hw_support)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Only HW supported key sizes enabled for HMAC-SHA2\n"));
	}
#endif /* HAVE_SW_HMAC_SHA == 0 */

	/* If HW use is ok so and writing a key or using an existing
	 * key => set select the MAC_TYPE_HW_HMAC_SHA2 type to use
	 * T23X HW for HMAC-SHA2.
	 */
	ret = hmac_handler_by_keyflags_intern(s, key, hw_support);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HW_HMAC_SHA */

static status_t hmac_handle_key(struct se_mac_state_s *s,
				te_crypto_key_t *key,
				const te_args_key_data_t *kdata,
				te_crypto_domain_t cdomain)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = hmac_sha_set_key(s, key, kdata, &s->hmac.context_sha, cdomain);
	CCC_ERROR_CHECK(ret);

#if HAVE_HW_HMAC_SHA
	/* Select SW/HW hybrid with SHA engine (default) or HMAC-SHA2 HW engine
	 * If SW HMAC is not supported, trap as error.
	 */
	ret = hmac_handler_type_select(s, key);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HMAC_SHA */

static status_t se_crypto_mac_set_key_check_args(const crypto_context_t *ctx,
						 const te_crypto_key_t *key,
						 const te_args_key_data_t *key_args)
{

	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_MAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to set a MAC key with class %u object\n",
						   ctx->ctx_class));
	}

	if (RUN_STATE_SELECT_MAC != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for MAC\n",
					     ctx->ctx_run_state.rs_select));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_mac_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
			       const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_mac_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	ret = se_crypto_mac_set_key_check_args(ctx, key, key_args);
	CCC_ERROR_CHECK(ret);

	s = ctx->ctx_run_state.rs.s_mac;
	if ((NULL == s) || (s->mac_magic != SE_MAC_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	kdata = GET_DOMAIN_ADDR(ctx->ctx_domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (s->mac_type) {
#if HAVE_CMAC_AES
	case MAC_TYPE_CMAC_AES:
		ret = cmac_set_key(key, kdata, &s->cmac.context_aes);
		break;
#endif

#if HAVE_HMAC_MD5
	case MAC_TYPE_HMAC_MD5:
		ret = hmac_md5_set_key(s, key, kdata, &s->hmac.context_md5,
				       ctx->ctx_domain);
		break;
#endif

#if HAVE_HMAC_SHA
	/* At this point mac_type not yet set to HW HMAC */
	case MAC_TYPE_HMAC_SHA1:
	case MAC_TYPE_HMAC_SHA2:
		ret = hmac_handle_key(s, key, kdata, ctx->ctx_domain);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("Unsupported MAC type %u\n",
			      s->mac_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5 */
