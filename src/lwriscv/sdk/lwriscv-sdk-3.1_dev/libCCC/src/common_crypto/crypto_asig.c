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

/* Support for asymmetric ciphers with SE engine (RSA cipher)
 */
#include <crypto_system_config.h>

#if CCC_WITH_RSA || CCC_WITH_ECC || CCC_WITH_XMSS

#include <crypto_asig.h>
#include <crypto_asig_proc.h>
#include <crypto_process_call.h>

#if CCC_WITH_ECC
/* EC context setup functions */
#include <crypto_ec.h>
#endif

#if CCC_WITH_XMSS
/* XMSS context setup functions */
#include <xmss/crypto_xmss.h>
#endif

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
static const struct cm_layout_s asig_mem_specs = {
#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif
#if CCC_WITH_CONTEXT_DMA_MEMORY
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_SHA_RESERVATION_SIZE),
#endif
	.cm_lo_size = (2368U +
		       CMEM_KEY_MEMORY_STATIC_SIZE +
		       (CMEM_DESCRIPTOR_SIZE +
			CCC_DMA_MEM_SHA_RESERVATION_SIZE)),
};

#if CCC_WITH_XMSS
/* XMSS uses memory in different way so use a different
 * spec for that.
 *
 * For now XMSS does not use a key object for holding root_seed, there
 * is not enough memory for that now in 4096 byte CMEM object. If
 * required either must make the XMSS work buffer smaller or split the
 * key object into e.g. "small_key" object and current key object.
 * This would require generic key handling code changes and is not
 * implemented at this point. So store root_seed to context.
 *
 * This XMSS version does not use HW SHA engine, so do not spec DMA
 * memory for those, also reducing CMEM usage.
 */

#if CCC_WITH_XMSS_MINIMAL_CMEM == 0
#error "Verify XMSS memory consumption to be less than 4096 bytes"
#endif

static const struct cm_layout_s xmss_asig_mem_specs = {
	.cm_lo_key_size = 0U,
	.cm_lo_dma_size = 0U,

	/* Actual now 2856+632+64. For 64 byte aligned
	 * chunks use the ceiling values; with some slack
	 * for small adjustments.
	 */
	.cm_lo_size = (2880U + 640U + CMEM_DESCRIPTOR_SIZE),
};
#endif /* CCC_WITH_XMSS */
#endif /* HAVE_CONTEXT_MEMORY */

#if !defined(HAVE_DEFAULT_RSA_PSS_VERIFY_DYNAMIC_SALT)
/* Unless defined otherwise: use dynamic salt length in RSA PSS verify
 * It is possible to also set this on in RSA init flags
 */
#define HAVE_DEFAULT_RSA_PSS_VERIFY_DYNAMIC_SALT 1
#endif

#define SE_ASIG_MAGIC 0x41534947U

enum sig_selector_e {
	SIG_TYPE_UNKNOWN = 0,
	SIG_TYPE_RSA,
	SIG_TYPE_EC,
	SIG_TYPE_XMSS,
};

struct se_asig_state_s {
	/* Keep this union the first field in the object */
	union {
#if CCC_WITH_RSA
		/* HW state */
		struct se_rsa_context rsa_as_context;
#endif
#if CCC_WITH_ECC
		struct se_ec_context ec_as_context;
#endif
#if CCC_WITH_XMSS
		struct se_xmss_context xmss_as_context;
#endif
	};
	enum sig_selector_e sig_type;
	uint32_t sig_magic;
};

#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519 || CCC_WITH_XMSS

static status_t asig_init_arg_check(const crypto_context_t *ctx,
				    const te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("asig init: state already set up\n"));
	}

fail:
	return ret;
}

/* exported signature functions */

static status_t asig_set_signature_type(te_crypto_algo_t ctx_algo,
					enum sig_selector_e *stype_p)
{
	status_t ret = NO_ERROR;
	enum sig_selector_e stype = SIG_TYPE_UNKNOWN;

	LTRACEF("entry\n");

	if (NULL == stype_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*stype_p = stype;

	switch(ctx_algo) {
#if CCC_WITH_RSA
#if HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN
	case TE_ALG_RSASSA_PKCS1_V1_5_MD5:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA1:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA224:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:
		stype = SIG_TYPE_RSA;
		break;
#endif

	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
		stype = SIG_TYPE_RSA;
		break;
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA
	case TE_ALG_ECDSA:
		stype = SIG_TYPE_EC;
		break;
#endif

#if CCC_WITH_ED25519
	case TE_ALG_ED25519CTX:
	case TE_ALG_ED25519PH:
	case TE_ALG_ED25519:
		stype = SIG_TYPE_EC;
		break;
#endif

#if CCC_WITH_XMSS
	case TE_ALG_XMSS_SHA2_20_256:
		stype = SIG_TYPE_XMSS;
		break;
#endif

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*stype_p = stype;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_RSA
static status_t asig_rsa_context_init(const crypto_context_t *ctx,
				      struct context_mem_s *cmem,
				      te_args_init_t *args,
				      struct se_asig_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = rsa_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->rsa_as_context,
				      cmem);
	CCC_ERROR_CHECK(ret);

	/* Is the input to the RSA signature function a digest or the actual data?
	 */
	if ((args->rsa.flags & INIT_FLAG_RSA_DIGEST_INPUT) != 0U) {
		s->rsa_as_context.ec.rsa_flags |= RSA_FLAGS_DIGEST_INPUT_FIRST;
	}

	/* FIXME: Salt flag only used for RSA-PSS, maybe should not set for #1v1.5 ... */
#if HAVE_DEFAULT_RSA_PSS_VERIFY_DYNAMIC_SALT
	LTRACEF("RSA verify/sign => dynamic salt length (default)\n");
	s->rsa_as_context.ec.rsa_flags |= RSA_FLAGS_DYNAMIC_SALT;
#else
	if ((args->rsa.flags & INIT_FLAG_RSA_SALT_DYNAMIC_LEN) != 0U) {
		LTRACEF("RSA verify => dynamic salt length\n");
		s->rsa_as_context.ec.rsa_flags |= RSA_FLAGS_DYNAMIC_SALT;
	}
#endif /* HAVE_DEFAULT_RSA_PSS_VERIFY_DYNAMIC_SALT */

	/* By default RSA data is in BIG ENDIAN order. Set a flag if LE.
	 */
	if ((args->rsa.flags & INIT_FLAG_RSA_DATA_LITTLE_ENDIAN) == 0U) {
		LTRACEF("RSA data treated as big endian\n");
		s->rsa_as_context.ec.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_DATA;
	}

	/* Verify "deciphers with public key"
	 * Signing "encrypts with private key"
	 */
	if (ctx->ctx_alg_mode == TE_ALG_MODE_SIGN) {
		s->rsa_as_context.ec.rsa_flags |= RSA_FLAGS_USE_PRIVATE_KEY;
		s->rsa_as_context.is_encrypt = true;
	}

	s->rsa_as_context.mode = ctx->ctx_alg_mode;
	args->engine_hint = s->rsa_as_context.ec.engine->e_id;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
#if CCC_WITH_ED25519
static status_t ed25519_domain_context_init(struct se_ec_context *ec_context,
					    const struct dom_context_s *dom_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ec_context->dom_context.length = 0U;

	if (dom_context->length != 0U) {
		if (dom_context->length > MAX_ECC_DOM_CONTEXT_SIZE) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Optional ECC domain context too long: %u\n",
						     dom_context->length));
		}
		ec_context->dom_context.length = dom_context->length;

		se_util_mem_move(&ec_context->dom_context.value[0],
				 &dom_context->value[0],
				 ec_context->dom_context.length);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif

static status_t asig_ec_context_init(const crypto_context_t *ctx,
				     struct context_mem_s *cmem,
				     te_args_init_t *args,
				     struct se_asig_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = ec_context_static_init(ctx->ctx_domain, args->engine_hint,
				     ctx->ctx_algo, args->ec.lwrve_id,
				     &s->ec_as_context, cmem);
	CCC_ERROR_CHECK(ret);

#if CCC_WITH_ED25519
	if ((TE_ALG_ED25519PH == ctx->ctx_algo) || (TE_ALG_ED25519CTX == ctx->ctx_algo)) {
		ret = ed25519_domain_context_init(&s->ec_as_context,
						  &args->ec.dom_context);
		CCC_ERROR_CHECK(ret);
	} else {
		s->ec_as_context.dom_context.length = 0U;
	}
#endif

	/* Is the input to the ECDSA signature function a digest or the actual data?
	 *
	 * If digest is requested, digest algorithm is in args->ec.sig_digest_algo
	 * since it is not encoded in the ECDSA algorithm id (as it is in RSA sigs).
	 *
	 * For ED25519 the algorithm selects this (RFC-8032):
	 * TE_ALG_ED25519    => signing a message
	 * TE_ALG_ED25519PH  => signing a message digest (PH = pre-hash)
	 * TE_ALG_ED25519CTX => signing a message, with optional context octets
	 *
	 * Such algos do not use this machanism.
	 */
	if ((args->ec.flags & INIT_FLAG_EC_DIGEST_INPUT) != 0U) {
		uint32_t digest_len = 0U;

		ret = get_message_digest_size(args->ec.sig_digest_algo, &digest_len);
		CCC_ERROR_CHECK(ret);

		s->ec_as_context.md_algorithm = args->ec.sig_digest_algo;
		s->ec_as_context.ec.ec_flags |= CCC_EC_FLAGS_DIGEST_INPUT_FIRST;
	}

	/* ECDSA signature format is OpenSSL ASN.1 DER blob
	 * instead of te_ec_sig_t object
	 */
	if ((args->ec.flags & INIT_FLAG_EC_ASN1_SIGNATURE) != 0U) {
		s->ec_as_context.ec.ec_flags |= CCC_EC_FLAGS_ASN1_SIGNATURE;
	}

	/* By default EC data is in BIG ENDIAN order. Set a flag if LE.
	 */
	if ((args->ec.flags & INIT_FLAG_EC_DATA_LITTLE_ENDIAN) == 0U) {
		LTRACEF("EC data treated as big endian\n");
		s->ec_as_context.ec.ec_flags |= CCC_EC_FLAGS_BIG_ENDIAN_DATA;
	}

#if HAVE_LWPKA11_SUPPORT
	/* Effective in ECDSA sign operations; ignored by others.
	 */
	if ((args->ec.flags & INIT_FLAG_EC_KEYSTORE) == 0U) {
		s->ec_as_context.ec.ec_flags |= CCC_EC_FLAGS_KEYSTORE;
	}
#endif

	/* Verify validates sig with public key
	 * Signing uses private key
	 */
	if (ctx->ctx_alg_mode == TE_ALG_MODE_SIGN) {
		s->ec_as_context.ec.ec_flags |= CCC_EC_FLAGS_USE_PRIVATE_KEY;
	}
	s->ec_as_context.mode = ctx->ctx_alg_mode;
	args->engine_hint = s->ec_as_context.ec.engine->e_id;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if CCC_WITH_XMSS
static status_t asig_xmss_context_init(const crypto_context_t *ctx,
				       struct context_mem_s *cmem,
				       te_args_init_t *args,
				       struct se_asig_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = xmss_context_static_init(ctx->ctx_domain, args->engine_hint,
				       ctx->ctx_algo, &s->xmss_as_context, cmem);
	CCC_ERROR_CHECK(ret);

	/* XMSS is always signing a message, there is no pre-hash version for this algorithm.
	 */
	if ((args->ec.flags & INIT_FLAG_EC_DIGEST_INPUT) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("XMSS signatures are for messages, not digests\n"));
	}

	/* By XMSS signature data is in BIG ENDIAN order.
	 */
	if ((args->ec.flags & INIT_FLAG_EC_DATA_LITTLE_ENDIAN) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Little endian signatures not supported\n"));
	}

	LTRACEF("XMSS data treated as big endian\n");
	s->xmss_as_context.ec.xmss_flags |= CCC_EC_FLAGS_BIG_ENDIAN_DATA;

	/* Verify validates sig with public key
	 * Signing uses private key
	 */
	if (ctx->ctx_alg_mode == TE_ALG_MODE_SIGN) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_IMPLEMENTED,
				     LTRACEF("XMSS signing is not implemented\n"));
	}
	s->xmss_as_context.mode = ctx->ctx_alg_mode;

	/* There is no XMSS engine in current HW versions.
	 * The SHA-256 could be callwlated by HW, but that is not yet supported by
	 * XMSS verify code (pure SW now).
	 */
	args->engine_hint = CCC_ENGINE_SOFT;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_XMSS */

static status_t asig_context_init(crypto_context_t *ctx,
				  struct context_mem_s *cmem,
				  te_args_init_t *args,
				  struct se_asig_state_s *s,
				  enum sig_selector_e stype)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (stype) {
#if CCC_WITH_RSA
	case SIG_TYPE_RSA:
		ret = asig_rsa_context_init(ctx, cmem, args, s);
		break;
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
	case SIG_TYPE_EC:
		ret = asig_ec_context_init(ctx, cmem, args, s);
		break;
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if CCC_WITH_XMSS
	case SIG_TYPE_XMSS:
		ret = asig_xmss_context_init(ctx, cmem, args, s);
		break;
#endif /* CCC_WITH_XMSS */

	default:
		CCC_ERROR_MESSAGE("asymmetric signature algorithm 0x%x unsupported\n",
				  ctx->ctx_algo);
		LTRACEF("SIG_TYPE %u unsupported\n", stype);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t do_asig_init(crypto_context_t *ctx,
			     te_args_init_t *args,
			     enum sig_selector_e stype,
			     struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	struct se_asig_state_s *s = NULL;

	LTRACEF("entry\n");

	/* Moved memory allocation to before algorithm check to improve
	 * function-call coverage for integration test, the function-call
	 * RELEASE_CONTIGUOUS_MEMORY() after fail label gets then exelwted
	 * on algorithm error.
	 *
	 * SE RSA engine requires contiguous memory, PKA1 RSA and PKA1
	 * EC do not. Now the API state object always contains a
	 * contiguous context buffer now, independent of the alignment
	 * value (either separate buffer is allocated from DMA memory
	 * or the API_STATE object is completely contiguous).
	 *
	 * Note: The above was for PKA0, which is no longer used.
	 * But even so, the RSA sigs use SHA engine which will read data
	 * with DMA and this sha buffer needs to be contiguous.
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_asig_state_s,
				 struct se_asig_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

#if CCC_WITH_XMSS_MINIMAL_CMEM
	if (TE_ALG_XMSS_SHA2_20_256 != ctx->ctx_algo) {
		ret = context_get_key_mem(ctx);
		CCC_ERROR_CHECK(ret);
	}
#else
	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);
#endif

	if ((TE_ALG_MODE_SIGN != ctx->ctx_alg_mode) &&
	    (TE_ALG_MODE_VERIFY != ctx->ctx_alg_mode)) {
		CCC_ERROR_MESSAGE("asig algo 0x%x mode %u unsupported\n",
				  ctx->ctx_algo, ctx->ctx_alg_mode);
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	ret = asig_context_init(ctx, cmem, args, s, stype);
	CCC_ERROR_CHECK(ret);

	s->sig_type        = stype;
	s->sig_magic       = SE_ASIG_MAGIC;

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_ASIG;
	ctx->ctx_run_state.rs.s_asig = s;

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

/* exported signature functions */

status_t se_crypto_asig_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	enum sig_selector_e stype = SIG_TYPE_UNKNOWN;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");
	MEASURE_MEMORY_START("Generic signature");

	ret = asig_init_arg_check(ctx, args);
	CCC_ERROR_CHECK(ret);

	ret = asig_set_signature_type(ctx->ctx_algo, &stype);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	{
		const struct cm_layout_s *amem_spec = &asig_mem_specs;

		/* CMEM may be NULL or checked cmem setup after this call.
		 *
		 * If NULL, CCC uses subsystem memory functions for this context.
		 *
		 * If not NULL, CCC manages the CMEM object internally and does not use
		 * subsystem memory functions for this context.
		 *
		 * Could optimize mem usage: RSA uses much more memory than EC =>
		 * could use different mem_specs based on stype.
		 */
#if CCC_WITH_XMSS
		/* XMSS memory usage pattern is different, so use
		 * different specs. The normal asig memory spec would need
		 * more than 4096 bytes and fail with SHA3 also enabled.
		 */
		if (TE_ALG_XMSS_SHA2_20_256 == ctx->ctx_algo) {
			amem_spec = &xmss_asig_mem_specs;
		}
#endif
		ret = context_memory_get_init(&ctx->ctx_mem, &cmem, amem_spec);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_CONTEXT_MEMORY */

	ret = do_asig_init(ctx, args, stype, cmem);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t setup_crypto_asig_dofinal_arg_check(const crypto_context_t *ctx,
						    const struct run_state_s *run_state,
						    const te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	const struct se_asig_state_s *s = NULL;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_ASIG != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for ASIG\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_asig;
	if ((NULL == s) || (s->sig_magic != SE_ASIG_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
static status_t asig_type_check_ec_args(const te_args_data_t *args,
					const struct se_asig_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == s->ec_as_context.ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve undefined\n"));
	}

	/* ED25519 signs messages (not digests) => empty message is allowed */
	if (TE_LWRVE_ED25519 != s->ec_as_context.ec.ec_lwrve->id) {
		if (NULL == args->src) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("null source address\n"));
		}
	}

	if (0U == args->src_signature_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("No signature\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

static status_t crypto_asig_type_check(const te_args_data_t *args,
				       const struct se_asig_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (s->sig_type) {
#if CCC_WITH_RSA
	case SIG_TYPE_RSA:
		if (NULL == args->src) {
			LTRACEF("null source address\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;
#endif

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
	case SIG_TYPE_EC:
		ret = asig_type_check_ec_args(args, s);
		break;
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519*/

#if CCC_WITH_XMSS
	case SIG_TYPE_XMSS:
		/* XXXXX FIXME: I do not know yet is XMSS allows signing empty messages.
		 * If it does, remove this error check!
		 */
		if (NULL == args->src) {
			LTRACEF("null source address\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;
#endif

	default:
		LTRACEF("SIG_TYPE %u unsupported\n", s->sig_type);
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_RSA
static status_t asig_rsa_dofinal(const crypto_context_t *ctx,
				 struct se_asig_state_s *s,
				 te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	// either src or src_digest (in the same field value)
	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_SIGN:
		/* RSA signature must fit in buffer
		 */
		if (args->dst_size < s->rsa_as_context.ec.rsa_size) {
			LTRACEF("Buffer too short for RSA signature\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		input.output_size = args->dst_size;

		ret = PROCESS_RSA_SIGN(&input, &s->rsa_as_context);
		if (NO_ERROR != ret) {
			LTRACEF("RSA sign operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret);
			break;
		}

		args->dst_size = input.output_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("RSA sign", s->rsa_as_context.ec.perf_usec);
#endif
		break;

	case TE_ALG_MODE_VERIFY:
		/* Signature length must match rsa size.
		 */
		if (args->src_signature_size != s->rsa_as_context.ec.rsa_size) {
			LTRACEF("Invalid signature length %u for RSA size %u\n",
				args->src_signature_size, s->rsa_as_context.ec.rsa_size);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		input.src_signature = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src_signature);
		input.src_signature_size = args->src_signature_size;

		ret = PROCESS_RSA_VERIFY(&input, &s->rsa_as_context);
		if (NO_ERROR != ret) {
			LTRACEF("RSA verify operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret);
			break;
		}

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("RSA verify", s->rsa_as_context.ec.perf_usec);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
static status_t asig_ec_dofinal(const crypto_context_t *ctx,
				struct se_asig_state_s *s,
				te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_SIGN:
		input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
		input.output_size = args->dst_size;

		ret = PROCESS_EC_SIGN(&input, &s->ec_as_context);
		if (NO_ERROR != ret) {
			LTRACEF("ECC sign operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret);
			break;
		}

		args->dst_size = input.output_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("ECC sign", s->ec_as_context.ec.perf_usec);
#endif
		break;

	case TE_ALG_MODE_VERIFY:
		input.src_signature = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src_signature);
		input.src_signature_size = args->src_signature_size;

		ret = PROCESS_EC_VERIFY(&input, &s->ec_as_context);
		if (NO_ERROR != ret) {
			LTRACEF("ECC verify operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret);
			break;
		}

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("ECC verify", s->ec_as_context.ec.perf_usec);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if CCC_WITH_XMSS
static status_t asig_xmss_dofinal(const crypto_context_t *ctx,
				  struct se_asig_state_s *s,
				  te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input; /* all fields set */

	LTRACEF("entry\n");

	/* either src or src_digest (in the same field value) */
	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_SIGN:
		ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
		LTRACEF("XMSS signing not implemented\n");
		break;

	case TE_ALG_MODE_VERIFY:
		if (args->src_signature_size != XMSS_SHA2_20_256_SIGLEN) {
			LTRACEF("Invalid signature length %u for XMSS_SHA2_20_256 (%u)\n",
				args->src_signature_size, XMSS_SHA2_20_256_SIGLEN);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		input.src_signature = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src_signature);
		input.src_signature_size = args->src_signature_size;

		ret = PROCESS_XMSS_VERIFY(&input, &s->xmss_as_context);
		if (NO_ERROR != ret) {
			LTRACEF("XMSS verify operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret);
			break;
		}

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("XMSS verify", s->xmss_as_context.ec.perf_usec);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_XMSS */

status_t se_crypto_asig_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_asig_state_s *s = NULL;

	(void)init_args;

	LTRACEF("entry\n");

	ret = setup_crypto_asig_dofinal_arg_check(ctx, run_state, args);
	CCC_ERROR_CHECK(ret);

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Asymmetric key not defined\n"));
	}

	s = run_state->rs.s_asig;

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	ret = crypto_asig_type_check(args, s);
	CCC_ERROR_CHECK(ret);


#if CCC_WITH_RSA
	if (SIG_TYPE_RSA == s->sig_type) {
		ret = asig_rsa_dofinal(ctx, s, args);
		CCC_ERROR_CHECK(ret);
	}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
	if (SIG_TYPE_EC == s->sig_type) {
		ret = asig_ec_dofinal(ctx, s, args);
		CCC_ERROR_CHECK(ret);
	}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if CCC_WITH_XMSS
	if (SIG_TYPE_XMSS == s->sig_type) {
		ret = asig_xmss_dofinal(ctx, s, args);
		CCC_ERROR_CHECK(ret);
	}
#endif /* CCC_WITH_XMSS */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_asig_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_ASYMMETRIC_SIGNATURE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to signature class reset with %u class object\n",
						   ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_ASIG != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for ASIG\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if CCC_WITH_ECDSA || CCC_WITH_ED25519
			struct se_asig_state_s *s = ctx->ctx_run_state.rs.s_asig;

			if (s->sig_magic == SE_ASIG_MAGIC) {
				/* EC PKA1 engine object may need cleanup
				 * TODO => make more generic
				 *
				 * (could add a release function to
				 * all engines and just call them in a
				 * more generic way, if not NULL)
				 */
				if (SIG_TYPE_EC == s->sig_type) {
					if (s->ec_as_context.ec.engine->e_id == CCC_ENGINE_PKA) {
						PKA_DATA_EC_RELEASE(&s->ec_as_context.ec);
					}
				}
			}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U, sizeof_u32(struct se_asig_state_s));
			CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
					  CMTAG_API_STATE,
					  ctx->ctx_run_state.rs.s_object);
		}
	}
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

fail:
	MEASURE_MEMORY_STOP;

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519 */

#if CCC_WITH_RSA
static status_t asig_rsa_key(struct se_asig_state_s *s,
			     const te_args_key_data_t *kdata,
			     te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((kdata->k_key_type != KEY_TYPE_RSA_PUBLIC) &&
	    (kdata->k_key_type != KEY_TYPE_RSA_PRIVATE)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Only supports RSA public and private keys\n"));
	}

	ret = se_setup_rsa_key(&s->rsa_as_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to setup RSA key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
static status_t asig_ec_key(struct se_asig_state_s *s,
			    const te_args_key_data_t *kdata,
			    te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((kdata->k_key_type != KEY_TYPE_EC_PUBLIC) &&
	    (kdata->k_key_type != KEY_TYPE_EC_PRIVATE)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Only supports EC public and private keys\n"));
	}

	ret = se_setup_ec_key(&s->ec_as_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to setup EC key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if CCC_WITH_XMSS
static status_t asig_xmss_key(struct se_asig_state_s *s,
			      const te_args_key_data_t *kdata,
			      te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (KEY_TYPE_XMSS_PRIVATE == kdata->k_key_type) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	/* KEY may be NULL for this algorithm due to CMEM size issues
	 */
	if (KEY_TYPE_XMSS_PUBLIC != kdata->k_key_type) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Only supports XMSS public and private keys\n"));
	}

	/* When CCC_WITH_XMSS_MINIMAL_CMEM set the key object is NULL here.
	 * In that case root_seed is stored in engine context.
	 *
	 * When not set root_seed is stored to key object
	 * and a reference to it is stored in engine context.
	 */
	ret = se_setup_xmss_key(&s->xmss_as_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to setup XMSS key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_XMSS */

static status_t setup_asig_key(struct se_asig_state_s *s,
			       const te_args_key_data_t *kdata,
			       te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (s->sig_type) {
#if CCC_WITH_RSA
	case SIG_TYPE_RSA:
		ret = asig_rsa_key(s, kdata, key);
		break;
#endif

#if CCC_WITH_ECDSA || CCC_WITH_ED25519
	case SIG_TYPE_EC:
		ret = asig_ec_key(s, kdata, key);
		break;
#endif

#if CCC_WITH_XMSS
	case SIG_TYPE_XMSS:
		ret = asig_xmss_key(s, kdata, key);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_asig_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_asig_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if CCC_WITH_XMSS_MINIMAL_CMEM
	/* key can be NULL for XMSS here with CMEM.
	 */
	if ((TE_ALG_XMSS_SHA2_20_256 != ctx->ctx_algo) && (NULL == key)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
#else
	if (NULL == key) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
#endif

	if (RUN_STATE_SELECT_ASIG != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for ASIG\n",
					     ctx->ctx_run_state.rs_select));
	}

	s = ctx->ctx_run_state.rs.s_asig;
	if ((NULL == s) || (s->sig_magic != SE_ASIG_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	kdata = GET_DOMAIN_ADDR(ctx->ctx_domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = setup_asig_key(s, kdata, key);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA || CCC_WITH_ECC || CCC_WITH_XMSS */
