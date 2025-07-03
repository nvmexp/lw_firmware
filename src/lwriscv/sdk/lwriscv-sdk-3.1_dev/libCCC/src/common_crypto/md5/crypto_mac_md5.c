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

/* HMAC_MD5 support used by crypto_mac.c
 */
#if HAVE_HMAC_MD5

#include <crypto_process_call.h>

#include <crypto_mac_intern.h>
#include <md5/crypto_md5_proc.h>
#include <md5/crypto_mac_md5.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t se_crypto_hmac_md5_init(const crypto_context_t *ctx,
				 struct context_mem_s *cmem,
				 te_args_init_t *args,
				 struct se_mac_state_s *s,
				 te_crypto_algo_t digest_algorithm)
{
	status_t ret = NO_ERROR;

	(void)cmem; /* method does not need runtime memory */

	LTRACEF("entry\n");

	/* Properties of the digest used for the HMAC */
	ret = md5_context_static_init(ctx->ctx_domain, args->engine_hint,
				      digest_algorithm, &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("HMAC md5 context static init failed: %d\n", ret));

	s->hmac.ipad_ok = false;
	args->engine_hint = s->hmac.context_md5.ec.engine->e_id;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t hmac_handle_ipad_intern_md5(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	struct se_data_params input;
	uint8_t md5_ipad[64U];

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(s->hmac.context_md5.ec.is_first)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (s->hmac.context_md5.ec.block_size != sizeof_u32(md5_ipad)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	se_util_mem_set(md5_ipad, HMAC_IPAD_VALUE, sizeof_u32(md5_ipad));

	for (inx = 0U; inx < s->hmac.hmac_key_size; inx++) {
		md5_ipad[inx] ^= s->hmac.hmac_key[inx];
	}

	input.src = md5_ipad;
	input.input_size = sizeof_u32(md5_ipad);

	/* intermediate MD5 does not output anything */
	input.dst = NULL;
	input.output_size = 0U;

	LOG_HEXBUF("HMAC-MD5 ipad input =>", md5_ipad, 64U);

	// XXX check if this is done by caller...
	ret = soft_md5_process_input(&input, &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret, LTRACEF("HMAC-MD5 ipad failed: %u\n", ret));

fail:
	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

status_t hmac_md5_opad_calc(struct se_mac_state_s *s)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint8_t md5_opad[64U];
	uint8_t inner_digest[16U];
	uint32_t inx;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(s->hmac.context_md5.ec.is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Save the inner_digest = hash(i_key_pad || message)
	 * MD5 engine context reused below
	 */
	se_util_mem_move(inner_digest, s->hmac.context_md5.ec.tmphash,
			 sizeof_u32(inner_digest));

	se_util_mem_set(md5_opad, HMAC_OPAD_VALUE, sizeof_u32(md5_opad));
	for (inx = 0U; inx < s->hmac.hmac_key_size; inx++) {
		md5_opad[inx] ^= s->hmac.hmac_key[inx];
	}

	input.src	 = md5_opad;
	input.input_size = sizeof_u32(md5_opad);

	/* intermediate MD5 does not output anything */
	input.dst	  = NULL;
	input.output_size = 0U;

	ret = md5_context_static_init(TE_CRYPTO_DOMAIN_KERNEL,
				      ENGINE_ANY,
				      TE_ALG_MD5,
				      &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("HMAC-MD5 opad input =>", md5_opad, 64U);

	/* result = hash((outer key pad) + (inner digest))
	 */
	ret = soft_md5_process_input(&input, &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret,
			LTRACEF("HMAC-MD5 opad failed: %u\n", ret));

	s->hmac.context_md5.ec.is_last = true;

	LOG_HEXBUF("HMAC-MD5 inner digest =>", inner_digest, 16U);

	input.src	  = inner_digest;
	input.input_size  = sizeof_u32(inner_digest);
	input.dst	  = s->hmac.context_md5.ec.tmphash;
	input.output_size = sizeof_u32(s->hmac.context_md5.ec.tmphash);

	ret = soft_md5_process_input(&input, &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret,
			LTRACEF("HMAC-MD5 result failed: %u\n", ret));

	LOG_HEXBUF("HMAC-MD5 result =>", s->hmac.context_md5.ec.tmphash, 16U);

fail:
	LTRACEF("exit\n");
	return ret;
}

status_t call_md5_hmac_process_context(struct se_data_params *input, struct soft_md5_context *m5ctx)
{
	status_t ret = NO_ERROR;

	ret = PROCESS_SW_MD5(input, m5ctx);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("HMAC MD5 failed: %d\n", ret));
fail:
	return ret;
}

status_t mac_hmac_md5_dofinal(const crypto_context_t *ctx,
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

	if (input.output_size < s->hmac.context_md5.ec.hash_size) {
		CCC_ERROR_MESSAGE("HMAC MD5 result does not fit in %u bytes\n",
				  input.output_size);
		CCC_ERROR_CHECK(ret);
	}

	if (!BOOL_IS_TRUE(s->hmac.ipad_ok)) {
		ret = hmac_handle_ipad(s);
		CCC_ERROR_CHECK(ret);
	}

	s->hmac.context_md5.ec.is_last = true;

	/* Do not copy the HMAC intermediate digest to client buffer
	 * (neither to TA user space buffer nor Trusty client buffer)
	 *
	 * See above; this needs to be NULL.
	 */
	input.dst = NULL;

	ret = call_md5_hmac_process_context(&input, &s->hmac.context_md5);
	CCC_ERROR_CHECK(ret);

	/* This finds the digest value from the context and overwrites
	 *  the same context value with the final result.
	 */
	ret = hmac_handle_opad(s);
	CCC_ERROR_CHECK(ret);

	/* Copy the HMAC final digest value to client buffer
	 */
	result = &s->hmac.context_md5.ec.tmphash[0];
	result_size = s->hmac.context_md5.ec.hash_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("HMAC-MD5", s->hmac.context_md5.ec.perf_usec);
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

status_t hmac_md5_set_key(struct se_mac_state_s *s,
			  te_crypto_key_t *key,
			  const te_args_key_data_t *kargs,
			  struct soft_md5_context *md5_context,
			  te_crypto_domain_t   cdomain)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (kargs->k_key_type != KEY_TYPE_HMAC) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("HMAC-MD5: only supports HMAC keys\n"));
	}

	if (kargs->k_byte_size > md5_context->ec.block_size) {
		struct soft_md5_context md5_ctx;
		struct se_data_params input;

		/* Digest the (long) key value with MD5
		 */
		se_util_mem_set((uint8_t *)&md5_ctx, 0U, sizeof_u32(md5_ctx));
		ret = md5_context_static_init(cdomain,
					      ENGINE_ANY, TE_ALG_MD5,
					      &md5_ctx);
		CCC_ERROR_CHECK(ret);

		md5_ctx.ec.is_last = true;

		input.src         = kargs->k_hmac_key;
		input.input_size  = kargs->k_byte_size;
		input.dst         = key->k_hmac_key;
		input.output_size = sizeof_u32(key->k_hmac_key);

		ret = CORE_PROCESS_SW_MD5(&input, &md5_ctx);
		CCC_ERROR_CHECK(ret);

		key->k_byte_size = md5_ctx.ec.hash_size;

		// Don't leave generated key in stack (in tmphash)
		se_util_mem_set((uint8_t *)&md5_ctx, 0U, sizeof_u32(md5_ctx));
	} else {
		/* Use the supplied value directly as HMAC key (it's short enough) */
		se_util_mem_move(key->k_hmac_key, kargs->k_hmac_key, kargs->k_byte_size);
		key->k_byte_size = kargs->k_byte_size;
	}

	key->k_key_type  = kargs->k_key_type;
	key->k_flags     = kargs->k_flags;

	/* ipad and opad callwlators access the hmac key from here
	 * Engine context does not need access to the key.
	 */
	s->hmac.hmac_key      = key->k_hmac_key;
	s->hmac.hmac_key_size = key->k_byte_size;

#if SE_RD_DEBUG
	LTRACEF("HMAC-MD5 key value set: (size %u)\n", key->k_byte_size);
	LOG_HEXBUF("HMAC-MD5 key", key->k_hmac_key, key->k_byte_size);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_HMAC_MD5 */
