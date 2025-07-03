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

/**
 * @file   crypto_context.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * @brief  Top level entry points to common crypto (CCC) operations.
 *
 * All normal CCC calls operate upon the top level crypto context with
 * functions in this file.
 *
 * The context is first set up for some algorithm in a specified
 * operating mode. After that the context operations
 * (init/setkey/update/dofinal/reset) can be performed with the
 * context. After initializing the context it can be used for the
 * specified operation until it completes or an error oclwrs.  After
 * completion or error the context must be reset before it can be used
 * for a new operation.
 *
 * All supported algorithms using the crypto context in CCC use the
 * same object types and calling sequence. The operating context is
 * held in the crypto context and client subsystem settings and data are
 * passed in with the arg parameters.
 *
 * When CCC is used in library mode (i.e. not via system calls from
 * some other address space), it is possible to bypass the context
 * level completely and directly call e.g. the engine level handlers,
 * but these are special cases bypassing the CCC crypto API and
 * requires special considerations.
 *
 * For USER_MODE calls e.g. via syscalls from TA address space =>
 * So for normal use cases the CCC API will always operate on the
 * crypto context object with the main handler functions in this file:
 *
 * crypto_context_setup() => initializes a crypto context
 *
 * crypto_handle_operation() => perform all CCC API operations
 *
 * crypto_context_reset() => reset a crypto context
 *
 * For LIBRARY calls e.g. via syscalls from TA address space =>
 * (handler is the same function, only setup/reset changes).
 *
 * When CCC is in library mode there are two helper functions (which
 * call the above main handlers) that may be used instead for
 * colwenience (the handler function is the same):
 *
 * crypto_kernel_context_setup() => library mode context initialize
 *
 * crypto_handle_operation() => perform all CCC API operations
 *
 * crypto_kernel_context_reset() => library mode context reset
 */
#include <crypto_system_config.h>

#include <crypto_md.h>
#include <crypto_mac.h>
#include <crypto_cipher.h>
#include <crypto_acipher.h>
#include <crypto_asig.h>
#include <crypto_random.h>

#if HAVE_AES_AEAD
#include <crypto_ae.h>
#endif

#if HAVE_SE_KAC_KEYOP
#include <crypto_kwuw.h>
#endif

#if CCC_WITH_ECC
#include <crypto_derive.h>
#endif

#if TEGRA_MEASURE_PERFORMANCE
#include <crypto_measure.h>
#endif

#include <crypto_classcall.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#include <ccc_safe_arith.h>

/**************************/

#ifndef HAVE_USE_STATIC_BUFFER_IN_CMEM
/* Defaulting this to 1 will cause CCC to declare the internal
 * static 4096 byte buffer for implicit use case by default.
 *
 * If you do not want that buffer to be declared and used,
 * set this zero in your config file. Then your subsystem needs to
 * pass in CMEM buffers as dislwssed in the comment below.
 */
#define HAVE_USE_STATIC_BUFFER_IN_CMEM 1
#endif

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE
/* This contains the implicit buffers refs for CMEM requests in
 * special elwironments (single threaded, only one algorithm
 * running). In such case CCC can use an internal buffer from which
 * CCC CMEM reserves memory objects from, instead of passing such
 * buffers in each context setup call via the context (the latter is a
 * generic mechanism which must be used if multiple algorithms run at
 * the same time).
 *
 * For the simpler ("invisible") method of using CMEM in CCC crypto
 * API calls =>
 *
 * When HAVE_USE_STATIC_BUFFER_IN_CMEM is nonzero, CCC will internally
 * declare a static 4096 byte buffer and pass it to CMEM using the call
 * below for the GENERIC memory type. If it is zero, no such buffer exists
 * and subsystem needs to set such buffer with the call below to use
 * implicit context memory.
 *
 * To set up the implicit memory (or GENERIC, DMA and KEY) memories for
 * this static case, the subsystem must call the initialization
 * function:
 *
 * status_t cmem_set_context_implicit_buffer(enum cmem_type_e cmtype,
 *					     void *buf,
 *					     uint32_t blen);
 *
 * for each memory type it uses from the static case.  When buffers
 * are not in use, the caller may alter these buffers at runtime by
 * calling the above routine again.  When in use, ERR_BAD_STATE is
 * returned.
 *
 * DMA and KEY memories will get used if set and enabled. If trying to
 * set then when not enabled in compile times => ERR_NOT_SUPPORTED is
 * returned.
 *
 * If DMA or KEY memory is not set when supported, then GENERIC memory
 * is used for such requests as well.
 *
 * Each memory must be linear, at least 64 byte aligned memory.  4096
 * byte buffer is big enough for all requests (combined) from GENERIC
 * memory.
 *
 * If DMA and/or KEY memories are supported, then GENERIC memory can be
 * made smaller. How much smaller depends on the use case.  Minimal
 * memory approximations are in each API layer handler file for the
 * specific algorithm class for now.  It is recommended to use 4096
 * bytes for generic memory.  If key memory is used, GENERIC memory can
 * be (4096-1088) = 3008 bytes.
 *
 * If DMA memory is used, then that should be at least 640 bytes long.
 * In case DMA memory is used, GENERIC memory should be at least
 * (4096 - 576) = 3520 bytes (or larger).
 *
 * KEY memory must be at least 1088 bytes long.
 *
 * If both KEY and DMA memories set, then GENERIC memory must be
 * (4096 - 576 - 1088) = 2432 bytes long.
 *
 * NOTE: All above sizes are now APPROXIMATIONS => please use larger
 * 64 byte aligned linear buffers, if possible. If buffer set too
 * small for the algorithms used, an error is reported
 * (ERR_NO_MEMORY).
 */
static struct implicit_cmem_s implicit_cmem;

#ifndef LOG_CCC_CMEM
#define LOG_CCC_CMEM LTRACEF
#endif

#endif /* HAVE_IMPLICIT_CONTEXT_MEMORY_USE */

/* CLASS functions which are not supported get ignored.
 */
#define CFUN_NOSUP_NONE   0x0000U /**< This call supports all functions */
#define CFUN_NOSUP_UPDATE 0x0001U /**< This call does not support UPDATE */
#define CFUN_NOSUP_SETKEY 0x0004U /**< This call does not support SETKEY */
#define CFUN_NOSUP_ASYNC  0x0008U /**< This call does not support ASYNC
				   * calls Even when support exists, the
				   * async functions may not be compiled
				   * if not enabled; the caller needs to
				   * handle this.
				   */

/* Table mapping generic operation class to crypto functions
 * for that class.
 *
 * Table only scanned by context_class_setup_functions() so could be
 * moved local there.
 */
static const struct crypto_class_funs_s crypto_class_funs[] = {
#if HAVE_SE_AES
	{
		.cfun_class	    = TE_CLASS_CIPHER,
		.cfun_nosup = CFUN_NOSUP_NONE,
	},
#endif /* HAVE_SE_AES */

#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	{
		.cfun_class	    = TE_CLASS_MAC,
		.cfun_nosup = CFUN_NOSUP_ASYNC,
	},
#endif /* HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5 */

#if HAVE_AES_AEAD
	/* AEAD modes like CCM, GCM and GMAC */
	{
		.cfun_class	    = TE_CLASS_AE,
#if HAVE_AES_AEAD_UPDATE
		.cfun_nosup = CFUN_NOSUP_ASYNC,
#else
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_ASYNC,
#endif
	},
#endif /* HAVE_AES_AEAD */

#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	{
		.cfun_class	    = TE_CLASS_DIGEST,
		.cfun_nosup = CFUN_NOSUP_SETKEY,
	},
#endif /* HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3 */

#if HAVE_RSA_CIPHER
	{
		// update is not defined for asymmetric ciphers (RSA, RSA_OAEP, RSA_PKCS1-V1_5)
		.cfun_class	    = TE_CLASS_ASYMMETRIC_CIPHER,
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_ASYNC,
	},
#endif /* HAVE_RSA_CIPHER */

#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519
	{
		// update is not defined for signature validators (RSA-PSS, ECDSA)
		.cfun_class	    = TE_CLASS_ASYMMETRIC_SIGNATURE,
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_ASYNC,
	},
#endif /* CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519 */

#if HAVE_CCC_KDF
	{
		// update is not defined for key derivation (ECDH or KDF)
		.cfun_class	    = TE_CLASS_KEY_DERIVATION,
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_ASYNC,
	},
#endif /* HAVE_CCC_KDF */

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API
	{
		.cfun_class	    = TE_CLASS_RANDOM,
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_SETKEY | CFUN_NOSUP_ASYNC,
	},
#endif /* CCC_WITH_DRNG || CCC_ENABLE_TRNG_API */

#if HAVE_SE_KAC_KEYOP
	{
		.cfun_class	    = TE_CLASS_KW,
		.cfun_nosup = CFUN_NOSUP_UPDATE | CFUN_NOSUP_ASYNC,
	}
#endif /* HAVE_SE_KAC_KEYOP */
};

static bool mode_is_cipher(te_crypto_alg_mode_t mode)
{
	return ((TE_ALG_MODE_ENCRYPT == mode) || (TE_ALG_MODE_DECRYPT == mode));
}

static bool mode_is_asymmetric_signature(te_crypto_alg_mode_t mode)
{
	return ((TE_ALG_MODE_SIGN == mode) || (TE_ALG_MODE_VERIFY == mode));
}

#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
static bool mode_is_mac(te_crypto_alg_mode_t mode)
{
	return ((TE_ALG_MODE_MAC == mode) || (TE_ALG_MODE_MAC_VERIFY == mode));
}
#endif

/* Lookup context functions for the given class; if functions found set the
 * context class as well.
 */
static status_t context_class_setup_functions(crypto_context_t *ctx,
					      te_crypto_class_t cclass)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	const uint32_t abytes = sizeof_u32(crypto_class_funs);

	for (; inx < (abytes / sizeof_u32(struct crypto_class_funs_s)); inx++) {
		if (crypto_class_funs[inx].cfun_class == cclass) {
			ctx->ctx_funs  = &crypto_class_funs[inx];
			ctx->ctx_class = cclass;
			break;
		}
	}

	if (NULL == ctx->ctx_funs) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

fail:
	return ret;
}

static bool algo_handler_aes_helper(te_crypto_alg_mode_t mode, te_crypto_class_t *cclass_p)
{
	bool mode_ok = false;
	te_crypto_class_t cclass;

	do {
#if HAVE_SE_AES_KDF
		if (TE_ALG_MODE_DERIVE == mode) {
			cclass  = TE_CLASS_KEY_DERIVATION;
			mode_ok = true;
			break;
		}
#endif /* HAVE_SE_AES_KDF */

		cclass  = TE_CLASS_CIPHER;
		mode_ok = mode_is_cipher(mode);
	} while(CFALSE);

	*cclass_p = cclass;
	return mode_ok;
}

#if HAVE_CMAC_AES
static bool algo_handler_cmac_aes_helper(te_crypto_alg_mode_t mode, te_crypto_class_t *cclass_p)
{
	bool mode_ok = false;
	te_crypto_class_t cclass;

	if (TE_ALG_MODE_DERIVE == mode) {
		cclass  = TE_CLASS_KEY_DERIVATION;
#if HAVE_SE_CMAC_KDF
		mode_ok = true;
#endif
	} else {
		cclass   = TE_CLASS_MAC;
		mode_ok = mode_is_mac(mode);
	}

	*cclass_p = cclass;
	return mode_ok;
}
#endif /* HAVE_CMAC_AES */

static status_t crypto_context_setup_algo_handlers(struct crypto_context_s *ctx,
						   te_crypto_alg_mode_t mode,
						   te_crypto_algo_t algo)
{
	status_t ret = NO_ERROR;
	te_crypto_class_t cclass;
	bool mode_ok = false;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (TE_HANDLE_FLAG_RESET != ctx->ctx_handle_state) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* All supported algorithms handled in this switch */
	switch (algo) {
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
		mode_ok = algo_handler_aes_helper(mode, &cclass);
		break;

#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_CIPHER;
		break;
#endif
#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_CIPHER;
		break;
#endif
#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_CIPHER;
		break;
#endif

#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_AE;
		break;
#endif

#if HAVE_GMAC_AES
	case TE_ALG_GMAC_AES:
		mode_ok = (TE_ALG_MODE_MAC == mode);
		cclass   = TE_CLASS_AE;
		break;
#endif

#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_AE;
		break;
#endif /* HAVE_AES_CCM */

#if HAVE_RSA_PKCS1V15_CIPHER
	case TE_ALG_RSAES_PKCS1_V1_5:
#endif
#if HAVE_RSA_OAEP
#if HAVE_SHA1
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
#endif
#if HAVE_SHA224
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
#endif
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
#endif /* HAVE_RSA_OAEP */
	case TE_ALG_MODEXP:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_ASYMMETRIC_CIPHER;
		break;

#if HAVE_MD5
	case TE_ALG_MD5: /* implemented in SW */
#endif
#if HAVE_SW_WHIRLPOOL
	case TE_ALG_WHIRLPOOL: /* implemented in SW */
#endif
#if HAVE_SHA1
	case TE_ALG_SHA1:
#endif
#if HAVE_SHA224
	case TE_ALG_SHA224:
#endif
	case TE_ALG_SHA256:
	case TE_ALG_SHA384:
	case TE_ALG_SHA512:

#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
	case TE_ALG_SHA512_256:
#endif

#if CCC_WITH_SHA3
	case TE_ALG_SHA3_224: /* implemented in SW or HW, depending on the SOC */
	case TE_ALG_SHA3_256:
	case TE_ALG_SHA3_384:
	case TE_ALG_SHA3_512:
	case TE_ALG_SHAKE128:
	case TE_ALG_SHAKE256:
#endif
		mode_ok = (TE_ALG_MODE_DIGEST == mode);
		cclass   = TE_CLASS_DIGEST;
		break;

#if 0
	case TE_ALG_RSASSA_PKCS1_V1_5_MD5:		// not yet implemented
#endif
#if CCC_WITH_XMSS
	case TE_ALG_XMSS_SHA2_20_256:
#endif
#if HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN
#if HAVE_SHA1
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA1:
#endif
#if HAVE_SHA224
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA224:
#endif
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
	case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:
#endif

#if HAVE_SHA1
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
#endif
#if HAVE_SHA224
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
#endif
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
		mode_ok = mode_is_asymmetric_signature(mode);
		cclass   = TE_CLASS_ASYMMETRIC_SIGNATURE;
		break;

#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES:
		mode_ok = algo_handler_cmac_aes_helper(mode, &cclass);
		break;
#endif

#if HAVE_HMAC_MD5
	case TE_ALG_HMAC_MD5:
		mode_ok = mode_is_mac(mode);
		cclass   = TE_CLASS_MAC;
		break;
#endif

#if HAVE_HMAC_SHA
#if HAVE_SHA1
	case TE_ALG_HMAC_SHA1:
#endif
#if HAVE_SHA224
	case TE_ALG_HMAC_SHA224:
#endif
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
		mode_ok = mode_is_mac(mode);
		cclass   = TE_CLASS_MAC;
		break;
#endif

#if CCC_ENABLE_TRNG_API
	case TE_ALG_RANDOM_TRNG:
		mode_ok = (TE_ALG_MODE_RANDOM == mode);
		cclass   = TE_CLASS_RANDOM;
		break;
#endif

#if CCC_WITH_DRNG
	case TE_ALG_RANDOM_DRNG:
		mode_ok = (TE_ALG_MODE_RANDOM == mode);
		cclass   = TE_CLASS_RANDOM;
		break;
#endif

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
		mode_ok = (TE_ALG_MODE_DERIVE == mode);
		cclass   = TE_CLASS_KEY_DERIVATION;
		break;
#endif

#if CCC_WITH_ECC
#if CCC_WITH_X25519
	case TE_ALG_X25519:
#endif
	case TE_ALG_ECDH:
		mode_ok = (TE_ALG_MODE_DERIVE == mode);
		cclass   = TE_CLASS_KEY_DERIVATION;
		break;

#if CCC_WITH_ED25519
	case TE_ALG_ED25519:
	case TE_ALG_ED25519PH:
	case TE_ALG_ED25519CTX:
#endif
	case TE_ALG_ECDSA: // XXX TODO: Consider specifying digest in the alg operation code. Or somewhere!
		mode_ok = mode_is_asymmetric_signature(mode);
		cclass   = TE_CLASS_ASYMMETRIC_SIGNATURE;
		break;
#endif

#if HAVE_PLAIN_DH
	case TE_ALG_DH:
		/* DH is a asymmetric modular exponentiation operation (like RSA cipher),
		 * but mode derives a secret.
		 */
		mode_ok = (TE_ALG_MODE_DERIVE == mode);
		cclass   = TE_CLASS_ASYMMETRIC_CIPHER;
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case TE_ALG_KEYOP_KWUW:
	case TE_ALG_KEYOP_KW:
	case TE_ALG_KEYOP_KUW:
		mode_ok = mode_is_cipher(mode);
		cclass   = TE_CLASS_KW;
		break;
#endif

		// HW does not support, SW does not exist yet...
	case TE_ALG_AES_CTS:	// in TE_CLASS_CIPHER
		/* FALLTHROUGH */

		// PKA1 supports the EC && modular primitives, plain DH not yet implemented
	default:
		CCC_ERROR_MESSAGE("Algorithm 0x%x (mode %u) not supported\n",
			      algo, mode);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(mode_ok)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Algorithm 0x%x context mode %u is invalid\n",
						   algo, mode));
	}

	ret = context_class_setup_functions(ctx, cclass);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Crypto operations for class %u not defined\n", cclass));

	ctx->ctx_algo     = algo;
	ctx->ctx_alg_mode = mode;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_USER_MODE == 0
static
#endif
status_t crypto_context_setup(struct crypto_context_s *ctx, uint32_t client_id,
			      te_crypto_alg_mode_t mode, te_crypto_algo_t algo,
			      te_crypto_domain_t domain)
{
	status_t ret = NO_ERROR;

	/* up counter for unique handles (SW mutex locked on increment) */
	static uint32_t context_handle_id;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ctx->ctx_handle_state = TE_HANDLE_FLAG_RESET;
	ctx->ctx_app_index = client_id;

	/* This field is only set here for the crypto context.
	 *
	 * If the call is made to CCC in library mode, i.e. from
	 * same address space (e.g. via call to crypto_kernel_context_setup())
	 * this is set to TE_CRYPTO_DOMAIN_KERNEL.
	 *
	 * If the call is received e.g. via a syscall from some user space task
	 * this will be set to te_crypto_domain_t by the caller.
	 */
	ctx->ctx_domain = domain;
	ctx->ctx_magic = CRYPTO_CONTEXT_MAGIX;

#if HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM
	LTRACEF("Algorithm and mode set by INIT primitive operation\n");
	(void)mode;
	(void)algo;
#else
	ret = crypto_context_setup_algo_handlers(ctx, mode, algo);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM */

	/* If the SW runs parallel => lock this to keep the value unique. */
	SW_MUTEX_TAKE;
	ret = CCC_ADD_U32(context_handle_id, context_handle_id, 1U);
	ctx->ctx_handle = context_handle_id;
	SW_MUTEX_RELEASE;

	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t crypto_kernel_context_setup(struct crypto_context_s *ctx, uint32_t kernel_id,
				     te_crypto_alg_mode_t mode, te_crypto_algo_t algo)
{
	status_t ret = NO_ERROR;
	uint32_t kid = kernel_id;

	LTRACEF("entry\n");

	kid = 0xFEED0000U + (kid & 0xFFFFU);

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set((uint8_t *)ctx, 0U, sizeof_u32(crypto_context_t));

	ret = crypto_context_setup(ctx, kid, mode, algo, TE_CRYPTO_DOMAIN_KERNEL);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static void release_key_memory(struct crypto_context_s *ctx)
{
	if ((NULL != ctx) && (NULL != ctx->ctx_key)) {
		uint32_t msize = sizeof_u32(te_crypto_key_t);
		se_util_mem_set((uint8_t *)ctx->ctx_key, 0U, msize);

		CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
				  CMTAG_KOBJECT,
				  ctx->ctx_key);
	}
}

#if HAVE_USER_MODE == 0
static
#endif
void crypto_context_reset(struct crypto_context_s *ctx)
{
	LTRACEF("entry\n");

#if TEGRA_MEASURE_MCP
	measure_mcp_results(true);
#endif

	if (NULL != ctx) {
		bool do_creset = false;

		if (NULL != ctx->ctx_key) {
			release_key_memory(ctx);
		}

		do_creset = (NULL != ctx->ctx_funs);

		if (BOOL_IS_TRUE(do_creset)) {
			status_t ret = CONTEXT_CALL_CLASS_RESET(ctx);
			if (NO_ERROR != ret) {
				LTRACEF("context free returned error %d\n", ret);
				/* FALLTHROUGH */
			}
		}

		/* The cfun_reset() call should set this NULL
		 */
		if (NULL != ctx->ctx_run_state.rs.s_object) {
			LTRACEF("Context run state not released => forced free!!!\n");

			CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
					  CMTAG_API_STATE,
					  ctx->ctx_run_state.rs.s_object);

			ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;
		}

		ctx->ctx_handle_state = TE_HANDLE_FLAG_RESET;

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE
		if (implicit_cmem.cmem_owner_ctx_handle == ctx->ctx_handle) {
			implicit_cmem.cmem_owner_ctx_handle = 0U;

			LOG_CCC_CMEM("==== CMEM page unset from ctx_handle %u\n", ctx->ctx_handle);

			if (NULL != ctx->ctx_mem.cmem_buf) {
				if (implicit_cmem.imp.cmem_buf != ctx->ctx_mem.cmem_buf) {
					LOG_ERROR("**XX CMEM page for context, but incorrect object %p\n",
						  ctx->ctx_mem.cmem_buf);
				}
			}

			ctx->ctx_mem.cmem_buf = NULL;
			ctx->ctx_mem.cmem_size = 0U;

#if CCC_WITH_CONTEXT_KEY_MEMORY
			ctx->ctx_mem.cmem_key_buf = NULL;
			ctx->ctx_mem.cmem_key_size = 0U;
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
			ctx->ctx_mem.cmem_dma_buf = NULL;
			ctx->ctx_mem.cmem_dma_size = 0U;
#endif
		}
#endif /* HAVE_IMPLICIT_CONTEXT_MEMORY_USE */
	}

	LTRACEF("exit\n");
}

void crypto_kernel_context_reset(struct crypto_context_s *ctx)
{
	LTRACEF("entry\n");
	crypto_context_reset(ctx);
	LTRACEF("exit\n");
}

#if HAVE_USER_MODE
/* Release resources in one crypto context.
 *
 * This is not called for TE_CRYPTO_DOMAIN_KERNEL objects.
 *
 * Only used for Trusty TA domain contexts lwrrently.
 *
 * FIXME: Rework these before Trusty merge: call
 * trusty function to deal with the list handling,
 * it does not belong here!!!
 */
void crypto_context_release(struct crypto_context_s **ctx_p)
{
	if ((NULL != ctx_p) && (NULL != *ctx_p)) {
		crypto_context_t *ctx = *ctx_p;

		LTRACEF("entry: ctx %p\n", ctx);

		if (ctx->ctx_domain != TE_CRYPTO_DOMAIN_KERNEL) {
			if (CRYPTO_CONTEXT_MAGIX == ctx->ctx_magic) {
				if ((NULL != ctx->ctx_run_state.rs.s_object) ||
				    (ctx->ctx_handle_state != TE_HANDLE_FLAG_RESET)) {
					crypto_context_reset(ctx);
				}

				/* Subsystem macro must clean up
				 * state and release any memory
				 * allocated for this object.
				 */
				CCC_CONTEXT_RELEASE(ctx);

				*ctx_p = NULL;
			}
		}

		LTRACEF("exit\n");
	}
}

static status_t handle_op_release(crypto_context_t **ctx_p)
{
	status_t ret = ERR_BAD_STATE;

	LTRACEF("entry\n");

	if ((NULL == ctx_p) || (NULL == *ctx_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((*ctx_p)->ctx_domain == TE_CRYPTO_DOMAIN_KERNEL) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Releasing kernel context?\n"));
	}

	crypto_context_release(ctx_p);
	ret = NO_ERROR;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_USER_MODE */

/* crypto_handle_operation() helpers */

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE

#if HAVE_USE_STATIC_BUFFER_IN_CMEM

#ifndef CCC_CMEM_IMPLICIT_BUFFER_SIZE
#define CCC_CMEM_IMPLICIT_BUFFER_SIZE 4096U
#endif

static status_t use_static_cmem_buffer(void)
{
	status_t ret = NO_ERROR;

	/* This will not be correct for all subsystems.
	 *
	 * Those systems can now set the implicit buffers by calling the
	 * function context_set_implicit_cmem_buffer() for each of the
	 * implicit memories they need to set (GENERIC, DMA, KEY).
	 *
	 * If a single static CMEM buffer for GENERIC is OK => then using
	 * this is ok.
	 *
	 * TODO: Could support KEY MEM fixed splti of cmem_space (e.g. fixed
	 * CMEM_KEY_MEMORY_SIZE space at end of buffer)
	 */
	static uint8_t cmem_space[ CCC_CMEM_IMPLICIT_BUFFER_SIZE ] CMEM_MEM_ALIGNED(PAGE_SIZE);

	LTRACEF("entry\n");

	ret = context_set_implicit_cmem_buffer(CMEM_TYPE_GENERIC,
					       cmem_space,
					       sizeof_u32(cmem_space));
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_USE_STATIC_BUFFER_IN_CMEM */

static status_t use_implicit_cmem_buffers_in_ctx(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_USE_STATIC_BUFFER_IN_CMEM
	{
		static uint32_t cmem_static_buffer_set = 0U;

		if (0U == cmem_static_buffer_set) {
			cmem_static_buffer_set = 1U;

			ret = use_static_cmem_buffer();
			CCC_ERROR_CHECK(ret);
		}
	}
#endif /* HAVE_USE_STATIC_BUFFER_IN_CMEM */

	if (implicit_cmem.cmem_owner_ctx_handle != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("*** ERROR CMEM page used by ctx_handle %u => new ctx_handle %u\n",
					       implicit_cmem.cmem_owner_ctx_handle,
					       ctx->ctx_handle));
	}

#if CCC_WITH_CONTEXT_KEY_MEMORY
	/* Use implicit KEY buffer unless provided by caller
	 * Use CMEM_KEY_MEMORY_SIZE at the END OF cmem_buffer
	 * and reduce the number of bytes available for GENERIC/DMA.
	 *
	 * If implicit_cmem.imp.cmem_key_buf is not set, GENERIC
	 * memory will get used.
	 */
	if ((NULL == ctx->ctx_mem.cmem_key_buf) ||
	    (0U == ctx->ctx_mem.cmem_key_size)) {
		ctx->ctx_mem.cmem_key_buf  = implicit_cmem.imp.cmem_key_buf;
		ctx->ctx_mem.cmem_key_size = implicit_cmem.imp.cmem_key_size;

		LOG_CCC_CMEM("==== CMEM KEY buffer %p set for ctx %p\n",
			 ctx->ctx_mem.cmem_key_buf, ctx);

		implicit_cmem.cmem_owner_ctx_handle = ctx->ctx_handle;
	} else {
		LOG_CCC_CMEM("+++ CMEM KEY %p provided by caller for ctx %p\n",
			 ctx->ctx_mem.cmem_buf, ctx);
	}
#endif /* CCC_WITH_CONTEXT_KEY_MEMORY */

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* Use implicit DMA buffer space unless provided by caller.
	 *
	 * If implicit_cmem.imp.cmem_dma_buf is not set, GENERIC
	 * memory will get used.
	 */
	if ((NULL == ctx->ctx_mem.cmem_dma_buf) ||
	    (0U == ctx->ctx_mem.cmem_dma_size)) {
		ctx->ctx_mem.cmem_dma_buf  = implicit_cmem.imp.cmem_dma_buf;
		ctx->ctx_mem.cmem_dma_size = implicit_cmem.imp.cmem_dma_size;

		LOG_CCC_CMEM("==== CMEM DMA buffer %p set for ctx %p\n",
			 ctx->ctx_mem.cmem_key_buf, ctx);

		implicit_cmem.cmem_owner_ctx_handle = ctx->ctx_handle;
	} else {
		LOG_CCC_CMEM("+++ CMEM DMA %p provided by caller for ctx %p\n",
			 ctx->ctx_mem.cmem_buf, ctx);
	}
#endif /* CCC_WITH_CONTEXT_DMA_MEMORY */

	/* Use implicit GENERIC buffer space unless provided by caller.
	 *
	 * If implicit_cmem.imp.cmem_buf is not set => return en error.
	 */
	if ((NULL == ctx->ctx_mem.cmem_buf) ||
	    (0U == ctx->ctx_mem.cmem_size)) {
		LOG_CCC_CMEM("==== CMEM buffer set for ctx %p\n", ctx);

		ctx->ctx_mem.cmem_buf  = implicit_cmem.imp.cmem_buf;
		ctx->ctx_mem.cmem_size = implicit_cmem.imp.cmem_size;

		implicit_cmem.cmem_owner_ctx_handle = ctx->ctx_handle;
	} else {
		LOG_CCC_CMEM("+++ CMEM %p provided by caller for ctx %p\n",
			 ctx->ctx_mem.cmem_buf, ctx);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Buf must be linear memory (contiguous) and aligned to at least CACHE_LINE
 * or 64 which ever is larger.
 *
 * If passing <NULL,0> the implicit memory variant is disabled.
 */
status_t context_set_implicit_cmem_buffer(enum cmem_type_e cmtype,
					  uint8_t *buf_arg,
					  uint32_t blen_arg)
{
	status_t ret = NO_ERROR;
	uint32_t blen = blen_arg;
	uint8_t *buf = buf_arg;
	const uint32_t alignsize = 64U;

	if ((((uintptr_t)buf & (alignsize - 1U)) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("CMEM(%u): Context memory %p not aligned\n",
					     cmtype, buf));
	}

	if (((blen & (alignsize - 1U)) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("CMEM(%u): Context memory size %u not aligned\n", cmtype,
					     blen));
	}

	if ((NULL == buf) || (0U == blen)) {
		LTRACEF("CMEM(%u): Disabling implicit context memory\n", cmtype);
		buf = NULL;
		blen = 0U;
	}

	switch (cmtype) {
	case CMEM_TYPE_GENERIC:
		implicit_cmem.imp.cmem_buf = buf;
		implicit_cmem.imp.cmem_size = blen;
		break;

#if CCC_WITH_CONTEXT_KEY_MEMORY
	case CMEM_TYPE_KEY:
		if ((blen > 0U) && (blen < CMEM_KEY_MEMORY_SIZE)) {
			ret = SE_ERROR(ERR_BAD_LEN);
			break;
		}
		implicit_cmem.imp.cmem_key_buf = buf;
		implicit_cmem.imp.cmem_key_size = blen;
		break;
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	case CMEM_TYPE_DMA:
		if ((blen > 0U) && (blen < CMEM_DMA_MAX_IMPLICIT_MEMORY_SIZE_LONGEST)) {
			ret = SE_ERROR(ERR_BAD_LEN);
			break;
		}
		implicit_cmem.imp.cmem_dma_buf = buf;
		implicit_cmem.imp.cmem_dma_size = blen;
		break;
#endif

	default:
		LTRACEF("Unsupported CMEM region type %u\n", cmtype);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_IMPLICIT_CONTEXT_MEMORY_USE */

/**
 * @brief TE_OP_INIT primitive handler
 *
 * @param ctx crypto context
 * @param arg from client crypto argument object
 *
 * @return ret NO_ERROR on success, error code otherwise
 */
static status_t handle_op_init(crypto_context_t *ctx,
			       te_crypto_args_t *arg)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	/* Require context to be in reset state before init or re-init */
	if (ctx->ctx_handle_state != TE_HANDLE_FLAG_RESET) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Init op called, object context not free (in state 0x%x)\n",
						       ctx->ctx_handle_state));
	}

	if (CRYPTO_CONTEXT_MAGIX != ctx->ctx_magic) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Context has not been set up\n"));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Context run state already set at init\n"));
	}

#if HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM
	/* This allows TE_OP_INIT to re-define what the crypto context is used for.
	 *
	 * The purpose of the object can be changed after it has been reset
	 * by doing a new TE_OP_INIT setting the algorithm and mode again.
	 *
	 * The ctx_domain (i.e. origin that created the context) can not be
	 * changed without context reset and new context setup.
	 */
	ret = crypto_context_setup_algo_handlers(ctx, arg->ca_alg_mode, arg->ca_algo);
	CCC_ERROR_CHECK(ret);
#endif

	// XXX could use select INIT here... (TODO: consider)
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

#if TEGRA_MEASURE_MCP
	measure_mcp_init();
#endif

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE
	ret = use_implicit_cmem_buffers_in_ctx(ctx);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_IMPLICIT_CONTEXT_MEMORY_USE */

	ret = CONTEXT_CALL_CLASS_INIT(ctx, &arg->ca_init);
	CCC_ERROR_CHECK(ret);

	ctx->ctx_handle_state |= TE_HANDLE_FLAG_INITIALIZED;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief TE_OP_SET_KEY primitive handler
 *
 * @param ctx crypto context
 * @param arg from client crypto argument object
 * @param opcode Vector of requested operation codes
 *
 * @return ret NO_ERROR on success, error code otherwise
 */
static status_t handle_op_set_key(crypto_context_t *ctx,
				  const te_crypto_args_t *arg,
				  te_crypto_op_t opcode)
{
	status_t ret = NO_ERROR;
	bool do_setkey = false;

	LTRACEF("Entry\n");

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_FINALIZED) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Setting keys requires init() first and then allows set_key() */
	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_INITIALIZED) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/** Context must be reset before key can be changed */
	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Key already set for the context\n"));
	}

	/* some ops do not need SET_KEY */
	do_setkey = ((ctx->ctx_funs->cfun_nosup & CFUN_NOSUP_SETKEY) == 0U);
	if (BOOL_IS_TRUE(do_setkey)) {
		ret = CONTEXT_CALL_CLASS_SET_KEY(ctx, ctx->ctx_key,
						 arg->ca_set_key.kdata);
		CCC_ERROR_CHECK(ret);

		ctx->ctx_handle_state |= TE_HANDLE_FLAG_KEY_SET;
	} else {
		/* If this is a combined op, do not report error if
		 *  set_key function is not defined (the op does not use
		 *  keys; e.g. message digests)
		 */
		if (TE_OP_SET_KEY == opcode) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief TE_OP_UPDATE primitive handler
 *
 * @param ctx crypto context
 * @param arg from client crypto argument object
 * @param opcode Vector of requested operation codes
 *
 * @return ret NO_ERROR on success, error code otherwise
 */
static status_t handle_op_update(crypto_context_t *ctx,
				 te_crypto_args_t *arg,
				 te_crypto_op_t opcode)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	/* Both UPDATE and DOFINAL in a single call is not supported.
	 * (not enough data parameters in the syscall to keep arg smaller)
	 */
	if ((opcode & TE_OP_DOFINAL) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_FINALIZED) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_INITIALIZED) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((ctx->ctx_funs->cfun_nosup & CFUN_NOSUP_UPDATE) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ret = CONTEXT_CALL_CLASS_UPDATE(ctx, &ctx->ctx_run_state,
					&arg->ca_data);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief TE_OP_DOFINAL primitive handler
 *
 * @param ctx crypto context
 * @param arg from client crypto argument object
 *
 * @return ret NO_ERROR on success, error code otherwise
 */
static status_t handle_op_dofinal(crypto_context_t *ctx,
				  te_crypto_args_t *arg)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_FINALIZED) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_INITIALIZED) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ret = CONTEXT_CALL_CLASS_DOFINAL(ctx, &ctx->ctx_run_state,
					 &arg->ca_data,
					 &arg->ca_init);
	CCC_ERROR_CHECK(ret);

	ctx->ctx_handle_state |= TE_HANDLE_FLAG_FINALIZED;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief TE_OP_RESET primitive handler
 *
 * @param ctx crypto context
 */
static void handle_op_reset(crypto_context_t *ctx)
{
	crypto_context_reset(ctx);
}

#if HAVE_SE_ASYNC
static status_t handle_op_async_sanity_check(const crypto_context_t *ctx,
						te_crypto_op_t te_op_async)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	if (0U != (ctx->ctx_handle_state & TE_HANDLE_FLAG_FINALIZED)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (0U == (ctx->ctx_handle_state & TE_HANDLE_FLAG_INITIALIZED)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	switch (te_op_async) {
	case TE_OP_ASYNC_UPDATE_START:
	case TE_OP_ASYNC_DOFINAL_START:
	case TE_OP_ASYNC_CHECK_STATE:
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		LTRACEF("Invalid async op.\n");
		break;
	}
	CCC_ERROR_CHECK(ret);

	if ((ctx->ctx_funs->cfun_nosup & CFUN_NOSUP_ASYNC) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async operation not supported for this op class\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_op_async_check_state(const crypto_context_t *ctx,
					    te_crypto_args_t *arg)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	ret = handle_op_async_sanity_check(ctx, TE_OP_ASYNC_CHECK_STATE);
	CCC_ERROR_CHECK(ret);

	if (CONTEXT_ASYNC_STATE_ACTIVE_NONE == CONTEXT_ASYNC_GET_ACTIVE(ctx)) {
		/* Not an error, return no async operation in progress
		 */
		LTRACEF("No asynchronous operation started\n");
		arg->ca_init.async_state = ASYNC_CHECK_ENGINE_NONE;
	} else {
		ret = CONTEXT_CALL_CLASS_ASYNC_CHECK_STATE(ctx, &ctx->ctx_run_state,
							   &arg->ca_init);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_op_async_update(crypto_context_t *ctx,
				       te_crypto_args_t *arg,
				       te_crypto_op_t opcode)
{
	status_t ret = NO_ERROR;
	bool start_operation = false;

	LTRACEF("Entry\n");

	ret = handle_op_async_sanity_check(ctx, TE_OP_ASYNC_UPDATE_START);
	CCC_ERROR_CHECK(ret);

	switch (opcode) {
	case TE_OP_ASYNC_UPDATE_START:
		if (CONTEXT_ASYNC_GET_ACTIVE(ctx) != CONTEXT_ASYNC_STATE_ACTIVE_NONE) {
			CCC_ERROR_MESSAGE("Async operation already in progress -- must complete that first\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		start_operation = true;
		break;

	case TE_OP_ASYNC_FINISH:
		if (CONTEXT_ASYNC_GET_ACTIVE(ctx) != CONTEXT_ASYNC_STATE_ACTIVE_UPDATE) {
			CCC_ERROR_MESSAGE("Update operation not in progress -- must start one first\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		break;

	default:
		CCC_ERROR_MESSAGE("Async update: opcode unsupported\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = CONTEXT_CALL_CLASS_ASYNC_UPDATE(ctx, &ctx->ctx_run_state,
					      &arg->ca_data, start_operation);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(start_operation)) {
		CONTEXT_ASYNC_SET_ACTIVE(ctx, CONTEXT_ASYNC_STATE_ACTIVE_UPDATE);
	} else {
		CONTEXT_ASYNC_SET_ACTIVE(ctx, CONTEXT_ASYNC_STATE_ACTIVE_NONE);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t handle_op_async_dofinal(crypto_context_t *ctx,
					te_crypto_args_t *arg,
					te_crypto_op_t opcode)
{
	status_t ret = NO_ERROR;
	bool start_operation = false;

	LTRACEF("Entry\n");

	ret = handle_op_async_sanity_check(ctx, TE_OP_ASYNC_DOFINAL_START);
	CCC_ERROR_CHECK(ret);

	switch (opcode) {
	case TE_OP_ASYNC_DOFINAL_START:
		if (CONTEXT_ASYNC_GET_ACTIVE(ctx) != CONTEXT_ASYNC_STATE_ACTIVE_NONE) {
			CCC_ERROR_MESSAGE("Async operation already in progress -- must complete that first\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		start_operation = true;
		break;

	case TE_OP_ASYNC_FINISH:
		if (CONTEXT_ASYNC_GET_ACTIVE(ctx) != CONTEXT_ASYNC_STATE_ACTIVE_DOFINAL) {
			CCC_ERROR_MESSAGE("DOFINAL operation not in progress -- must start one first\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		break;

	default:
		CCC_ERROR_MESSAGE("Async dofinal: opcode unsupported\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = CONTEXT_CALL_CLASS_ASYNC_DOFINAL(ctx, &ctx->ctx_run_state,
					       &arg->ca_data,
					       &arg->ca_init,
					       start_operation);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(start_operation)) {
		CONTEXT_ASYNC_SET_ACTIVE(ctx, CONTEXT_ASYNC_STATE_ACTIVE_DOFINAL);
	} else {
		CONTEXT_ASYNC_SET_ACTIVE(ctx, CONTEXT_ASYNC_STATE_ACTIVE_NONE);
	}

	if (TE_OP_ASYNC_FINISH == opcode) {
		ctx->ctx_handle_state |= TE_HANDLE_FLAG_FINALIZED;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t crypto_call_handle_op_async(te_crypto_op_t te_op_async,
					crypto_context_t *ctx,
					te_crypto_args_t *arg,
					bool *is_update_p,
					te_crypto_op_t *opcode_p,
					uint32_t *opcount_p)
{
	status_t ret = NO_ERROR;
	bool is_update = *is_update_p;
	te_crypto_op_t opcode = *opcode_p;
	uint32_t opcount = *opcount_p;

	LTRACEF("Entry\n");

	if (0U != ((opcode) & te_op_async)) {

		switch (te_op_async) {
		case TE_OP_ASYNC_UPDATE_START:
			ret = handle_op_async_update(ctx, arg, TE_OP_ASYNC_UPDATE_START);

			is_update = true;
			break;
		case TE_OP_ASYNC_DOFINAL_START:
			if (BOOL_IS_TRUE(is_update)) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						LTRACEF("update and dofinal can not both be set for same op\n"));
			}

			ret = handle_op_async_dofinal(ctx, arg, TE_OP_ASYNC_DOFINAL_START);
			break;
		case TE_OP_ASYNC_CHECK_STATE:
			ret = handle_op_async_check_state(ctx, arg);
			break;
		default:
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			LTRACEF("Invalid asynchronous operation.\n");
			break;
		}
		CCC_ERROR_CHECK(ret);

		CCC_SAFE_ADD_U32(opcount, opcount, 1U);
		opcode &= ~te_op_async;
	}
	ret = NO_ERROR;

	*is_update_p = is_update;
	*opcode_p  = opcode;
	*opcount_p = opcount;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Support the asynchronous SE engine calls.
 *
 * Do not swap the order of handlers; the current order allows
 * e.g. UPDATE_START and FINISH to be given in the same call. This should
 * then be (effectively) identical to the syncronous UPDATE call.
 *
 * This is because the FINISH is a blocking call, completing a started async
 * update (it waits until operation is complete).
 *
 * If you do not want to block, use the TE_OP_ASYNC_CHECK_STATE, it does not block.
 *
 * It will return the state of the lwrrently started call (code now supports
 * only one started call at a time, but should in fact support as many started calls
 * as we have parallel engines. This is more than one in some SoCs and after host1x
 * is implemented => TODO: add support later).
 */
static status_t crypto_call_handler_async(crypto_context_t *ctx,
					  te_crypto_args_t *arg,
					  te_crypto_op_t   *opcode_p,
					  uint32_t	   *opcount_p)
{
	status_t ret = NO_ERROR;
	te_crypto_op_t opcode = *opcode_p;
	uint32_t opcount = *opcount_p;
	bool is_update = false;

	ret = crypto_call_handle_op_async(TE_OP_ASYNC_UPDATE_START,
			ctx, arg, &is_update, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	ret = crypto_call_handle_op_async(TE_OP_ASYNC_DOFINAL_START,
			ctx, arg, &is_update, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	if ((opcode & TE_OP_ASYNC_FINISH) != 0U) {
		async_active_state_t active = CONTEXT_ASYNC_GET_ACTIVE(ctx);
		opcount++;

		switch (active) {
		case CONTEXT_ASYNC_STATE_ACTIVE_UPDATE:
			ret = handle_op_async_update(ctx, arg, TE_OP_ASYNC_FINISH);
			break;

		case CONTEXT_ASYNC_STATE_ACTIVE_DOFINAL:
			ret = handle_op_async_dofinal(ctx, arg, TE_OP_ASYNC_FINISH);
			break;

		case CONTEXT_ASYNC_STATE_ACTIVE_NONE:
			LTRACEF("No async operation in progress for this context\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;

		default:
			LTRACEF("Async state invalid\n");
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		CCC_ERROR_CHECK(ret);

		opcode &= ~TE_OP_ASYNC_FINISH;
	}

	ret = crypto_call_handle_op_async(TE_OP_ASYNC_CHECK_STATE,
			ctx, arg, &is_update, &opcode, &opcount);

fail:
	*opcode_p = opcode;
	*opcount_p = opcount;

	return ret;
}
#endif /* HAVE_SE_ASYNC */

static status_t crypto_call_handle_op(te_crypto_op_t te_op,
					crypto_context_t *ctx,
					te_crypto_args_t *arg,
					te_crypto_op_t *opcode_p,
					uint32_t *opcount_p)
{
	status_t ret = NO_ERROR;
	te_crypto_op_t opcode = *opcode_p;
	uint32_t opcount = *opcount_p;

	if (0U != ((opcode) & te_op)) {

		switch (te_op) {
		case TE_OP_INIT:
			ret = handle_op_init(ctx, arg);
			break;
		case TE_OP_SET_KEY:
			ret = handle_op_set_key(ctx, arg, opcode);
			break;
		case TE_OP_UPDATE:
			ret = handle_op_update(ctx, arg, opcode);
			break;
		case TE_OP_DOFINAL:
			ret = handle_op_dofinal(ctx, arg);
			break;
		case TE_OP_RESET:
			handle_op_reset(ctx);
			break;
#if HAVE_USER_MODE
		case TE_OP_RELEASE:
			ret = handle_op_release(&ctx);
			break;
#endif
		default:
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			LTRACEF("Invalid operation.\n");
			break;
		}
		CCC_ERROR_CHECK(ret);

		CCC_SAFE_ADD_U32(opcount, opcount, 1U);
		opcode &= ~te_op;
	}
	ret = NO_ERROR;

	*opcode_p  = opcode;
	*opcount_p = opcount;
fail:

	return ret;
}

static status_t crypto_call_handler(crypto_context_t *ctx,
				    te_crypto_args_t *arg,
				    te_crypto_op_t   *opcode_p,
				    uint32_t	     *opcount_p)
{
	status_t ret = NO_ERROR;
	te_crypto_op_t opcode = *opcode_p;
	uint32_t opcount = *opcount_p;

	/* XX ALLOCATE handled in TA syscall code; TOS calls do not use ALLOCATE/RELEASE,
	 * XX  they use a local context object managed by the caller.
	 */
	ret = crypto_call_handle_op(TE_OP_INIT, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	ret = crypto_call_handle_op(TE_OP_SET_KEY, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

#if HAVE_SE_ASYNC
	ret = crypto_call_handler_async(ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);
#endif

	ret = crypto_call_handle_op(TE_OP_UPDATE, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	ret = crypto_call_handle_op(TE_OP_DOFINAL, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	ret = crypto_call_handle_op(TE_OP_RESET, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

#if HAVE_USER_MODE
	ret = crypto_call_handle_op(TE_OP_RELEASE, ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	*opcode_p  = opcode;
	*opcount_p = opcount;

	return ret;
}

static void crypto_call_handle_error(crypto_context_t *ctx,
				     te_crypto_op_t *opcode_p)
{
	te_crypto_op_t opcode = *opcode_p;

	/* Mark the error to context */
	ctx->ctx_handle_state |= TE_HANDLE_FLAG_ERROR;

	/* Combined operations will execute reset/release also
	 * after a failure (kernel originated calls do not
	 * release the context, since it is not allocated by
	 * this code).
	 */
	if ((opcode & TE_OP_RESET) != 0U) {
		opcode &= ~TE_OP_RESET;

		handle_op_reset(ctx);
	}

#if HAVE_USER_MODE
	if (ctx->ctx_domain != TE_CRYPTO_DOMAIN_KERNEL) {
		if ((opcode & TE_OP_RELEASE) != 0U) {
			status_t ret = handle_op_release(&ctx);
			if (NO_ERROR != ret) {
				LTRACEF("handle_op_release failed, ignored\n");
			}
			opcode &= ~TE_OP_RELEASE;
		}
	}
#endif

	*opcode_p = opcode;
}

static status_t crypto_fixup_opcode(const crypto_context_t *ctx,
				    te_crypto_op_t   *opcode_p)
{
	status_t ret = NO_ERROR;
	te_crypto_op_t opcode = *opcode_p;

	if (TE_OP_COMBINED_OPERATION == opcode) {
		opcode = (TE_OP_INIT | TE_OP_SET_KEY | TE_OP_DOFINAL | TE_OP_RESET);

#if HAVE_USER_MODE
		if (TE_CRYPTO_DOMAIN_KERNEL != ctx->ctx_domain) {
			opcode |= TE_OP_RELEASE;
		}
#endif
	}

	/* Only reset and release primitives allowed when context
	 * is in error state.
	 */
	if ((TE_OP_RESET != opcode) && (TE_OP_RELEASE != opcode) &&
	    (0U != (ctx->ctx_handle_state & TE_HANDLE_FLAG_ERROR))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("crypto context in error state\n"));
	}

	if (0U != (opcode & TE_OP_ALLOCATE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("ALLOCATE operation not handled by caller (opcode 0x%x)\n",
						       opcode));
	}

	/* Check blocked KERNEL domain operations
	 */
	if ((TE_CRYPTO_DOMAIN_KERNEL == ctx->ctx_domain) &&
	    (0U != (opcode & TE_OP_RELEASE))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Crypto context : kernel using RELEASE operation (opcode 0x%x)\n",
						       opcode));
	}

	ret = NO_ERROR;

	*opcode_p  = opcode;
fail:

	return ret;
}

status_t crypto_handle_operation(struct crypto_context_s *ctx,
				 te_crypto_args_t *arg)
{
	status_t ret = NO_ERROR;
	te_crypto_op_t opcode = TE_OP_NOP;
	uint32_t opcount = 0U;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == arg)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("opcode 0x%x, ctx run state %p\n",
		arg->ca_opcode, ctx->ctx_run_state.rs.s_object);

	/* Process and colwert the opcode */
	opcode = arg->ca_opcode;
	ret = crypto_fixup_opcode(ctx, &opcode);
	CCC_ERROR_CHECK(ret);

	ret = crypto_call_handler(ctx, arg, &opcode, &opcount);
	CCC_ERROR_CHECK(ret);

	if (0U == opcount) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("unsupported opcode 0x%x in handler -- unknown operations\n",
						   opcode));
	}

	if (0U != opcode) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("unsupported opcode bits 0x%x detected by opcode handler\n",
						   opcode));
	}

fail:
	if ((NO_ERROR != ret) && (NULL != ctx)) {

		LTRACEF("Op 0x%x class %u algo 0x%x mode %u crypto dispatcher error: %d\n",
			((NULL != arg) ? arg->ca_opcode : 0U),
			ctx->ctx_class, ctx->ctx_algo,
			ctx->ctx_alg_mode, ret);

		crypto_call_handle_error(ctx, &opcode);
	}

	LTRACEF("exit: opcode 0x%x, ret=%d\n", opcode, ret);
	return ret;
}

status_t context_get_key_mem(struct crypto_context_s *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ctx->ctx_key) {
		struct context_mem_s *cmem = context_memory_get(&ctx->ctx_mem);
		ctx->ctx_key = CMTAG_MEM_GET_OBJECT(cmem,
						    CMTAG_KOBJECT,
						    0U,
						    te_crypto_key_t,
						    te_crypto_key_t *);
		if (NULL == ctx->ctx_key) {
			CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
