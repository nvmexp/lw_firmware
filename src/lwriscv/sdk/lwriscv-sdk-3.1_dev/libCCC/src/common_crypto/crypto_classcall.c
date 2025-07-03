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

#if CCC_WITH_ECC || HAVE_CCC_KDF
#include <crypto_derive.h>
#endif

#include <crypto_classcall.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t context_call_class_init(struct crypto_context_s *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)args;

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_init(ctx, args);
		break;
#endif
#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	case TE_CLASS_MAC:
		ret = se_crypto_mac_init(ctx, args);
		break;
#endif
#if HAVE_AES_AEAD
	case TE_CLASS_AE:
		ret = se_crypto_ae_init(ctx, args);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_init(ctx, args);
		break;
#endif
#if HAVE_RSA_CIPHER
	case TE_CLASS_ASYMMETRIC_CIPHER:
		ret = se_crypto_acipher_init(ctx, args);
		break;
#endif
#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519
	case TE_CLASS_ASYMMETRIC_SIGNATURE:
		ret = se_crypto_asig_init(ctx, args);
		break;
#endif
#if HAVE_CCC_KDF
	case TE_CLASS_KEY_DERIVATION:
		ret = se_crypto_derive_init(ctx, args);
		break;
#endif
#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
	case TE_CLASS_RANDOM:
		ret = se_crypto_random_init(ctx, args);
		break;
#endif
#if HAVE_SE_KAC_KEYOP
	case TE_CLASS_KW:
		ret = se_crypto_kwuw_init(ctx, args);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_set_key(struct crypto_context_s *ctx, te_crypto_key_t *key,
				    const te_args_key_data_t *kargs)
{
	status_t ret = NO_ERROR;

	(void)key;
	(void)kargs;

	LTRACEF("entry\n");

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_set_key(ctx, key, kargs);
		break;
#endif
#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	case TE_CLASS_MAC:
		ret = se_crypto_mac_set_key(ctx, key, kargs);
		break;
#endif
#if HAVE_AES_AEAD
	case TE_CLASS_AE:
		ret = se_crypto_ae_set_key(ctx, key, kargs);
		break;
#endif
#if HAVE_RSA_CIPHER
	case TE_CLASS_ASYMMETRIC_CIPHER:
		ret = se_crypto_acipher_set_key(ctx, key, kargs);
		break;
#endif
#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519
	case TE_CLASS_ASYMMETRIC_SIGNATURE:
		ret = se_crypto_asig_set_key(ctx, key, kargs);
		break;
#endif
#if HAVE_CCC_KDF
	case TE_CLASS_KEY_DERIVATION:
		ret = se_crypto_derive_set_key(ctx, key, kargs);
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case TE_CLASS_KW:
		ret = se_crypto_kwuw_set_key(ctx, key, kargs);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_update(const struct crypto_context_s *ctx,
				   struct run_state_s *run_state,
				   te_args_data_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)run_state;
	(void)args;

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_update(ctx, run_state, args);
		break;
#endif
#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	case TE_CLASS_MAC:
		ret = se_crypto_mac_update(ctx, run_state, args);
		break;
#endif
#if HAVE_AES_AEAD_UPDATE
	case TE_CLASS_AE:
		ret = se_crypto_ae_update(ctx, run_state, args);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_update(ctx, run_state, args);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_dofinal(const struct crypto_context_s *ctx,
				    struct run_state_s *run_state,
				    te_args_data_t *args,
				    te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)run_state;
	(void)args;
	(void)init_args;

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	case TE_CLASS_MAC:
		ret = se_crypto_mac_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_AES_AEAD
	case TE_CLASS_AE:
		ret = se_crypto_ae_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_RSA_CIPHER
	case TE_CLASS_ASYMMETRIC_CIPHER:
		ret = se_crypto_acipher_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519
	case TE_CLASS_ASYMMETRIC_SIGNATURE:
		ret = se_crypto_asig_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_CCC_KDF
	case TE_CLASS_KEY_DERIVATION:
		ret = se_crypto_derive_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
	case TE_CLASS_RANDOM:
		ret = se_crypto_random_dofinal(ctx, run_state, args, init_args);
		break;
#endif
#if HAVE_SE_KAC_KEYOP
	case TE_CLASS_KW:
		ret = se_crypto_kwuw_dofinal(ctx, run_state, args, init_args);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_reset(struct crypto_context_s *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_reset(ctx);
		break;
#endif
#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5
	case TE_CLASS_MAC:
		ret = se_crypto_mac_reset(ctx);
		break;
#endif
#if HAVE_AES_AEAD
	case TE_CLASS_AE:
		ret = se_crypto_ae_reset(ctx);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_reset(ctx);
		break;
#endif
#if HAVE_RSA_CIPHER
	case TE_CLASS_ASYMMETRIC_CIPHER:
		ret = se_crypto_acipher_reset(ctx);
		break;
#endif
#if CCC_WITH_ECDSA || CCC_WITH_RSA || CCC_WITH_ED25519
	case TE_CLASS_ASYMMETRIC_SIGNATURE:
		ret = se_crypto_asig_reset(ctx);
		break;
#endif
#if HAVE_CCC_KDF
	case TE_CLASS_KEY_DERIVATION:
		ret = se_crypto_derive_reset(ctx);
		break;
#endif
#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
	case TE_CLASS_RANDOM:
		ret = se_crypto_random_reset(ctx);
		break;
#endif
#if HAVE_SE_KAC_KEYOP
	case TE_CLASS_KW:
		ret = se_crypto_kwuw_reset(ctx);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC
status_t context_call_class_async_check_state(const struct crypto_context_s *ctx,
					      const struct run_state_s *run_state,
					      te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_async_check_state(ctx, run_state, init_args);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_async_check_state(ctx, run_state, init_args);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_async_update(const struct crypto_context_s *ctx,
					 struct run_state_s *run_state,
					 te_args_data_t *args,
					 bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_async_update(ctx, run_state, args, start_operation);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_async_update(ctx, run_state, args, start_operation);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t context_call_class_async_dofinal(const struct crypto_context_s *ctx,
					  struct run_state_s *run_state,
					  te_args_data_t *args,
					  te_args_init_t *init_args,
					  bool start_operation)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ctx->ctx_class) {
#if HAVE_SE_AES
	case TE_CLASS_CIPHER:
		ret = se_crypto_cipher_async_dofinal(ctx, run_state, args,
						     init_args, start_operation);
		break;
#endif
#if HAVE_SE_SHA || HAVE_MD5 || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3
	case TE_CLASS_DIGEST:
		ret = se_crypto_md_async_dofinal(ctx, run_state, args,
						 init_args, start_operation);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC */
