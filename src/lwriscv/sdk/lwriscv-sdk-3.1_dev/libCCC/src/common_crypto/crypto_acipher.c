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

#if 0
// openssl rsautl -raw -pubin -inkey public_key.pem -encrypt -hexdump -in /tmp/ff-1.bin

// print RSA pub key
//  openssl rsa -text -pubin -in public_key.pem -modulus
// Note: The code can not read these pubkeys!!! It can only handle
// RSA public key DER format which is different (simpler) than this.
/*
 * Public-Key: (512 bit)
 * Modulus:
 *    00:e1:2c:c5:7e:85:8b:b6:e6:e2:98:62:da:fe:2f:
 *    fd:b6:73:5e:dc:63:8b:57:6d:82:bc:88:53:e2:ed:
 *    da:d3:47:f4:79:d4:69:94:7b:95:7c:c7:a0:45:1f:
 *    da:96:10:95:33:bf:e3:e4:c8:10:d2:34:72:2f:2b:
 *    ee:f2:19:f6:8b
 * Exponent: 65537 (0x10001)
 * Modulus=E12CC57E858BB6E6E29862DAFE2FFDB6735EDC638B576D82BC8853E2EDDAD347F479D469947B957CC7A0451FDA96109533BFE3E4C810D234722F2BEEF219F68B
 */
#endif

/* Support for asymmetric ciphers with SE engine (RSA cipher)
 */

#include <crypto_system_config.h>

#if HAVE_RSA_CIPHER

#include <crypto_acipher.h>
#include <crypto_asig.h>
#include <crypto_process_call.h>

#include <crypto_asig_proc.h>

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
static const struct cm_layout_s acipher_mem_specs = {
#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif
#if CCC_WITH_CONTEXT_DMA_MEMORY
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_SHA_RESERVATION_SIZE),
#endif
	.cm_lo_size = (2048U +
		       CMEM_KEY_MEMORY_STATIC_SIZE +
		       (CMEM_DESCRIPTOR_SIZE +
			CCC_DMA_MEM_SHA_RESERVATION_SIZE)),
};

#endif /* HAVE_CONTEXT_MEMORY */

#define SE_ACIPHER_MAGIC 0x41435048U

struct se_acipher_state_s {
	/* HW state */
	/* First field in the object (alignment is smaller) */
	struct se_rsa_context acip_context;

	uint32_t acip_magic;
#if HAVE_OAEP_LABEL
	uint8_t oaep_label_digest[ MAX_DIGEST_LEN ];
#endif
};

static status_t se_crypto_acipher_init_check_args(const crypto_context_t *ctx,
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
				     CCC_ERROR_MESSAGE("asymm cipher init: state already set up\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_acipher_check_algorithm(te_crypto_algo_t ctx_algo,
						  bool *use_oaep_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == use_oaep_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(ctx_algo) {
#if HAVE_PLAIN_DH
	case TE_ALG_DH:
#endif
	case TE_ALG_MODEXP:
	case TE_ALG_RSAES_PKCS1_V1_5:
		break;
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
		*use_oaep_p = true;
		break;

	default:
		CCC_ERROR_MESSAGE("asymm cipher algorithm 0x%x unsupported\n",
				  ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_acipher_algo_mode_init(crypto_context_t *ctx,
						 struct se_acipher_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	// This only supports public key encryption and private key decryption.
	//
	// XXX    (because signing is not handled by this code, only direct cipher/decipher
	// XXX     are handled here, and the only kind of encryption done by
	// XXX     a private key is "signing" (encrypting anything else with a private key
	// XXX     makes no sense))
	//
	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_ENCRYPT:
		/*
		 * m == cleartext message (sizeof rsa modulus)
		 * n == modulus
		 * e == pubkey exponent
		 *
		 * bigInt(m).pow(e).mod(n)
		 */
		s->acip_context.is_encrypt  = true;
		s->acip_context.ec.rsa_flags &= ~RSA_FLAGS_USE_PRIVATE_KEY;
		break;

#if HAVE_PLAIN_DH
	case TE_ALG_MODE_DERIVE:
		/* DH is modular exponentiation
		 * (== RSA decipher with a with a private key)
		 */
		if (TE_ALG_DH != ctx->ctx_algo) {
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}

		/*
		 * mEnc == ciphered message (sizeof rsa modulus)
		 * n == modulus
		 * d == private exponent
		 *
		 * mEnc.pow(d).mod(n)
		 */
		s->acip_context.is_encrypt  = false;
		s->acip_context.ec.rsa_flags |= RSA_FLAGS_USE_PRIVATE_KEY;
		break;
#endif

	case TE_ALG_MODE_DECRYPT:
		/*
		 * mEnc == ciphered message (sizeof rsa modulus)
		 * n == modulus
		 * d == private exponent
		 *
		 * mEnc.pow(d).mod(n)
		 */
		s->acip_context.is_encrypt  = false;
		s->acip_context.ec.rsa_flags |= RSA_FLAGS_USE_PRIVATE_KEY;
		break;

	default:
		CCC_ERROR_MESSAGE("asymm cipher algo 0x%x mode %u unsupported\n",
			      ctx->ctx_algo, ctx->ctx_alg_mode);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_acipher_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_acipher_state_s *s = NULL;
	bool use_oaep = false;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("RSA cipher");

	ret = se_crypto_acipher_init_check_args(ctx, args);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &acipher_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	/* Moved memory allocation to before algorithm check to improve
	 * function-call coverage for integration test, the function-call
	 * RELEASE_CONTIGUOUS_MEMORY() after fail label gets then exelwted
	 * on algorithm error.
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_acipher_state_s,
				 struct se_acipher_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	ret = se_crypto_acipher_check_algorithm(ctx->ctx_algo, &use_oaep);
	CCC_ERROR_CHECK(ret);

	ret = rsa_context_static_init(ctx->ctx_domain,
			args->engine_hint, ctx->ctx_algo,
			&s->acip_context, cmem);
	CCC_ERROR_CHECK(ret);

	ret = se_crypto_acipher_algo_mode_init(ctx, s);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(use_oaep)) {
#if HAVE_OAEP_LABEL
		if (args->rsa.oaep_label_length > MAX_OAEP_LABEL_SIZE) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		if (args->rsa.oaep_label_length > 0U) {
			struct se_data_params input;

			input.src	  = args->rsa.oaep_label;
			input.input_size  = args->rsa.oaep_label_length;
			input.dst	  = s->oaep_label_digest;
			input.output_size = sizeof_u32(s->oaep_label_digest);

			ret = sha_digest(cmem, ctx->ctx_domain, &input,
					 s->acip_context.md_algorithm);
			CCC_ERROR_CHECK(ret,
					LTRACEF("Failed to callwlate OAEP label digest\n"));

			s->acip_context.ec.oaep_label_digest = s->oaep_label_digest;
		}
#endif
	}

	// By default RSA data is in BIG ENDIAN order. Set a flag if LE.
	//
	if ((args->rsa.flags & INIT_FLAG_RSA_DATA_LITTLE_ENDIAN) == 0U) {
		LTRACEF("RSA data treated as big endian\n");
		s->acip_context.ec.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_DATA;
	}

	/* PSS sLen only used when handling signatures */
	s->acip_context.sig_slen = 0U;

	// return a selected engine in the hint
	args->engine_hint = s->acip_context.ec.engine->e_id;

	s->acip_magic = SE_ACIPHER_MAGIC;
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_ACIP;
	ctx->ctx_run_state.rs.s_acipher = s;

	s = NULL;

fail:
	if (NULL != s) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit, err: %d\n", ret);
	return ret;
}

status_t se_crypto_acipher_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				   te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_acipher_state_s *s = NULL;
	struct se_data_params input;

	(void)init_args;

	LTRACEF("entry\n");

	if (RUN_STATE_SELECT_ACIP != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for ACIP\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_acipher;
	if ((NULL == s) || (s->acip_magic != SE_ACIPHER_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Asymmetric cipher magic invalid\n"));
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Asymmetric cipher key not defined\n"));
	}

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if (NULL == args->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Asymmetric cipher dst invalid\n"));
	}

	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size;

	ret = PROCESS_RSA_CRYPTO(&input, &s->acip_context);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("asymm cipher 0x%x mode 0x%x failed: 0x%x\n",
				      ctx->ctx_algo, ctx->ctx_alg_mode, ret));

	args->dst_size = input.output_size;
	LTRACEF("DST bytes written: %u\n", args->dst_size);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("Asymmetric RSA cipher", s->acip_context.ec.perf_usec);
#endif

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}

status_t se_crypto_acipher_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_ASYMMETRIC_CIPHER) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to ACIPHER reset with %u class object\n",
						       ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_ACIP != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for acipher\n",
				ctx->ctx_run_state.rs_select);
		} else {
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U,
					sizeof_u32(struct se_acipher_state_s));
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

static status_t se_crypto_acipher_set_key_check_args(
			const crypto_context_t *ctx,
			const te_crypto_key_t *key,
			const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_ACIP != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for ACIP\n",
					     ctx->ctx_run_state.rs_select));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_crypto_acipher_set_key_check_key_type(
			const te_crypto_algo_t algo,
			const te_key_type_t k_key_type)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (algo) {
#if HAVE_PLAIN_DH
	case TE_ALG_DH:
		if (k_key_type != KEY_TYPE_DH) {
			CCC_ERROR_MESSAGE("DH requires a modulus and private key exponent\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
		break;
#endif

	default:
		if ((k_key_type != KEY_TYPE_RSA_PUBLIC) &&
		    (k_key_type != KEY_TYPE_RSA_PRIVATE)) {
			CCC_ERROR_MESSAGE("Only supports RSA public and private keys\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_acipher_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				   const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_acipher_state_s *s = NULL;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	ret = se_crypto_acipher_set_key_check_args(ctx, key, key_args);
	CCC_ERROR_CHECK(ret);

	s = ctx->ctx_run_state.rs.s_acipher;
	if ((NULL == s) || (s->acip_magic != SE_ACIPHER_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	kdata = GET_DOMAIN_ADDR(ctx->ctx_domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = se_crypto_acipher_set_key_check_key_type(ctx->ctx_algo,
							kdata->k_key_type);
	CCC_ERROR_CHECK(ret);

	ret = se_setup_rsa_key(&s->acip_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup RSA key\n"));

fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_CIPHER */
