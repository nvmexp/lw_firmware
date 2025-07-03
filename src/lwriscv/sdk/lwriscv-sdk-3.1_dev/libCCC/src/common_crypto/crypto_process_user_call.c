/*
 * Copyright (c) 2020-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* This file implementd call mechanism into user space process layer functions
 * for handling data parameters before calling the CORE_PROCESS_xxx() macros
 * entering CCC core process layer from the user space handlers.
 *
 * I.e. this adds a layer of user space data handlers between the API layer
 * (or other code with user space data) and the CCC core process layer.
 */
#include <crypto_system_config.h>

#if HAVE_USER_MODE

#include <crypto_barrier.h>
#include <crypto_util.h>
#include <tegra_se.h>
#include <crypto_process_call.h>
#include <crypto_process_user_call.h>
#include <crypto_context.h>
#include <crypto_ops_intern.h>

#if HAVE_AES_CCM
#include <aes-ccm/crypto_ops_ccm.h>
#endif

#if HAVE_AES_GCM
#include <aes-gcm/crypto_ops_gcm.h>
#endif

#if CCC_WITH_XMSS
#include <xmss/crypto_xmss.h>
#endif

#include <crypto_ta_api.h>

#define UM_FUNCTION_NORMAL(um_name, core_name, ta_name, context_type)	\
status_t um_name(struct se_data_params *input,				\
		 struct se_ ## context_type ## _context *pcontext)	\
{ status_t ret = ERR_BAD_STATE; FI_BARRIER(status, ret);		\
  LTRACEF("entry\n");							\
  if (NULL == pcontext) { CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS); }	\
  switch (pcontext->ec.domain) {					\
  case TE_CRYPTO_DOMAIN_TA: ret = ta_name(input, pcontext); break;	\
  default: ret = core_name(input, pcontext); break;			\
  }									\
fail: LTRACEF("exit: %d\n", ret); return ret;				\
}

#define UM_FUNCTION_NO_UMODE(um_name, core_name, context_type)		\
status_t um_name(struct se_data_params *input,				\
		 struct se_ ## context_type ## _context *pcontext)	\
{ status_t ret = ERR_BAD_STATE; FI_BARRIER(status, ret);		\
  LTRACEF("entry\n");							\
  if (NULL == pcontext) { CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS); }	\
  switch (pcontext->ec.domain) {					\
  case TE_CRYPTO_DOMAIN_TA: ret = SE_ERROR(ERR_NOT_SUPPORTED); break;	\
  default: ret = core_name(input, pcontext); break;			\
  }									\
fail: LTRACEF("exit: %d\n", ret); return ret;				\
}

#include <crypto_process_user_call.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_SHA
UM_FUNCTION_NORMAL(um_process_sha, CORE_PROCESS_SHA, ta_se_sha_process_input, sha)

#if HAVE_SE_ASYNC_SHA
UM_FUNCTION_NO_UMODE(um_process_sha_async, CORE_PROCESS_SHA_ASYNC, sha)

/* Async SHA poll is not using normalized arguments, define function here.
 */
status_t um_process_sha_async_poll(const struct se_sha_context *pcontext,
				   bool *idle_p)
{
	status_t ret = ERR_BAD_STATE;
	FI_BARRIER(status, ret);

	LTRACEF("entry\n");

	if (NULL == pcontext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (pcontext->ec.domain) {
	case TE_CRYPTO_DOMAIN_TA:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	default:
		ret = CORE_PROCESS_SHA_ASYNC_POLL(pcontext, idle_p);
		break;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_SHA */

#if HAVE_SE_KAC_KDF
UM_FUNCTION_NO_UMODE(um_process_sha_kdf, CORE_PROCESS_SHA_KDF, sha)
#endif /* HAVE_SE_KAC_KDF */
#endif /* HAVE_SE_SHA */

#if HAVE_CMAC_AES
UM_FUNCTION_NORMAL(um_process_aes_cmac, CORE_PROCESS_AES_CMAC, ta_se_cmac_process_input, aes)
#endif /* HAVE_CMAC_AES */

#if HAVE_RSA_CIPHER
UM_FUNCTION_NORMAL(um_process_rsa_crypto, CORE_PROCESS_RSA_CRYPTO, ta_se_rsa_process_input, rsa)
#endif

#if CCC_WITH_RSA
UM_FUNCTION_NORMAL(um_process_rsa_sign, CORE_PROCESS_RSA_SIGN, ta_se_rsa_process_sign, rsa)

UM_FUNCTION_NORMAL(um_process_rsa_verify, CORE_PROCESS_RSA_VERIFY, ta_se_rsa_process_verify, rsa)
#endif

#if CCC_WITH_ECC
UM_FUNCTION_NORMAL(um_process_ec_sign, CORE_PROCESS_EC_SIGN, ta_se_ecc_process_sign, ec)

UM_FUNCTION_NORMAL(um_process_ec_verify, CORE_PROCESS_EC_VERIFY, ta_se_ecc_process_verify, ec)

#if CCC_WITH_ECDH || CCC_WITH_X25519
UM_FUNCTION_NORMAL(um_process_ec_derive, CORE_PROCESS_EC_DERIVE, ta_se_ecdh_process_input, ec)
#endif
#endif /* CCC_WITH_ECC */

#if CCC_WITH_XMSS
UM_FUNCTION_NORMAL(um_process_xmss_verify, CORE_PROCESS_XMSS_VERIFY, ta_se_xmss_process_verify, xmss)
#endif

#if HAVE_SE_AES

UM_FUNCTION_NORMAL(um_process_aes, CORE_PROCESS_AES, ta_se_aes_process_input, aes)

/* CCM and GCM use the same user mode handler
 */
#if CCC_WITH_AES_GCM
UM_FUNCTION_NORMAL(um_process_aes_gcm, CORE_PROCESS_AES_GCM, ta_se_ae_process_input, aes)
#endif

#if HAVE_AES_CCM
UM_FUNCTION_NORMAL(um_process_aes_ccm, CORE_PROCESS_AES_CCM, ta_se_ae_process_input, aes)
#endif

#if HAVE_SE_ASYNC_AES
UM_FUNCTION_NO_UMODE(um_process_aes_async, CORE_PROCESS_AES_ASYNC, aes)

/* Async AES poll is not using normalized arguments, define function here.
 */
status_t um_process_aes_async_poll(const struct se_aes_context *pcontext,
				   bool *idle_p)
{
	status_t ret = ERR_BAD_STATE;
	FI_BARRIER(status, ret);

	LTRACEF("entry\n");

	if (NULL == pcontext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (pcontext->ec.domain) {
	case TE_CRYPTO_DOMAIN_TA:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	default:
		ret = CORE_PROCESS_AES_ASYNC_POLL(pcontext, idle_p);
		break;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif

#endif /* HAVE_SE_AES */

#if HAVE_SE_KAC_KEYOP
UM_FUNCTION_NO_UMODE(um_process_aes_kwuw, CORE_PROCESS_AES_KWUW, aes)
#endif

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API
UM_FUNCTION_NORMAL(um_process_rng, CORE_PROCESS_RNG, ta_se_random_process, rng)
#endif

#endif /* HAVE_USER_MODE */
