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

/* This file contains call mechanism into process layer functions
 * for getting rid of function pointers, e.g. from API layer calls
 * to process layer.
 */
#include <crypto_system_config.h>

#ifndef INCLUDED_CRYPTO_PROCESS_CALL_H
#define INCLUDED_CRYPTO_PROCESS_CALL_H

/* This defines the CORE HANDLER calling mechanism via the
 * CORE_PROCESS_xxx() macros.
 *
 * Call the function directly by name.
 */

/* No function pointers used in process layer calls
 */
#define CORE_PROCESS_SHA(input, pcontext)			\
	se_sha_process_input(input, pcontext) /* SHA digest and HMAC-SHA */

#if HAVE_SE_ASYNC_SHA && HAVE_SE_SHA
#define CORE_PROCESS_SHA_ASYNC(input, pcontext)			\
	se_sha_process_async_input(input, pcontext) /* SHA async */

#define CORE_PROCESS_SHA_ASYNC_POLL(pcontext, idle_p)		\
	se_sha_process_async_check(pcontext, idle_p) /* SHA poll */
#endif /* HAVE_SE_ASYNC_SHA && HAVE_SE_SHA */

#if HAVE_SE_KAC_KDF
#define CORE_PROCESS_SHA_KDF(input, pcontext)			\
	se_sha_kdf_process_input(input, pcontext) /* SHA KDF derive */
#endif /* HAVE_SE_KAC_KDF */

#if HAVE_CMAC_AES
#define CORE_PROCESS_AES_CMAC(input, pcontext)			\
	se_cmac_process_input(input, pcontext) /* AES CMAC */
#endif /* HAVE_CMAC_AES */

#if HAVE_RSA_CIPHER
#define CORE_PROCESS_RSA_CRYPTO(input, pcontext)		\
	se_rsa_process_input(input, pcontext) /* RSA crypto */
#endif /* HAVE_RSA_CIPHER */

#if CCC_WITH_RSA

#define CORE_PROCESS_RSA_SIGN(input, pcontext)			\
	se_rsa_process_sign(input, pcontext) /* RSA sign */

#define CORE_PROCESS_RSA_VERIFY(input, pcontext)		\
	se_rsa_process_verify(input, pcontext) /* RSA verify */
#endif

#if CCC_WITH_ECC
#define CORE_PROCESS_EC_SIGN(input, pcontext)			\
	se_ecc_process_sign(input, pcontext) /* EC sign */

#define CORE_PROCESS_EC_VERIFY(input, pcontext)			\
	se_ecc_process_verify(input, pcontext) /* EC verify */
#endif /* CCC_WITH_ECC */

#if CCC_WITH_ECDH || CCC_WITH_X25519
#define CORE_PROCESS_EC_DERIVE(input, pcontext)			\
	se_ecdh_process_input(input, pcontext) /* ECDH derive */
#endif /* CCC_WITH_ECDH || CCC_WITH_X25519 */

#define CORE_PROCESS_AES(input, pcontext)			\
	se_aes_process_input(input, pcontext) /* AES crypto and AES derive */

#if CCC_WITH_AES_GCM
#define CORE_PROCESS_AES_GCM(input, pcontext)			\
	se_aes_gcm_process_input(input, pcontext) /* AES GCM */
#endif /* CCC_WITH_AES_GCM */

#if HAVE_AES_CCM
#define CORE_PROCESS_AES_CCM(input, pcontext)			\
	se_aes_ccm_process_input(input, pcontext) /* AES CCM */
#endif /* HAVE_AES_CCM */

#if HAVE_SE_ASYNC_AES && HAVE_SE_AES
#define CORE_PROCESS_AES_ASYNC(input, pcontext)			\
	se_aes_process_async_input(input, pcontext) /* AES async */

#define CORE_PROCESS_AES_ASYNC_POLL(pcontext, idle_p)		\
	se_aes_process_async_check(pcontext, idle_p) /* AES poll */
#endif /* HAVE_SE_ASYNC_AES && HAVE_SE_AES */

#if HAVE_SE_KAC_KEYOP
#define CORE_PROCESS_AES_KWUW(input, pcontext)			\
	se_kwuw_process_input(input, pcontext) /* AES KWUW (t23x) */
#endif /* HAVE_SE_KAC_KEYOP */

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API
#define CORE_PROCESS_RNG(input, pcontext)			\
	se_random_process(input, pcontext) /* RNG */
#endif /* CCC_WITH_DRNG || CCC_ENABLE_TRNG_API */

#if HAVE_MD5
#define CORE_PROCESS_SW_MD5(input, pcontext)			\
	soft_md5_process_input(input, pcontext)
#endif

#if HAVE_SW_WHIRLPOOL
#define CORE_PROCESS_SW_WHIRLPOOL(input, pcontext)		\
	soft_whirlpool_process_input(input, pcontext)
#endif

#if HAVE_SW_SHA3
#define CORE_PROCESS_SW_SHA3(input, pcontext)			\
	soft_sha3_process_input(input, pcontext)
#endif

#if CCC_WITH_XMSS
#define CORE_PROCESS_XMSS_VERIFY(input, pcontext)		\
	se_xmss_process_verify(input, pcontext)
#endif

#if HAVE_USER_MODE

/* User mode aware macro versions for the PROCESS_xxx() defined
 * in the user mode include file.
 *
 * These re-enter core functions by calling the process functions
 * directly by the TA (user mode) code.
 *
 * TODO: Could rewrite the user mode calls by passing a tag
 * for the function to call from user code; this way the user code
 * could be generalized when the used code would call the process
 * function indicated by a tag instead of a fixed function as now.
 *
 * Also, use CORE_PROCESS_xxx() macros in the user mode instead of
 * directly call by name => verify before using user mode in
 * production code.
 *
 * Current method is compatible with old design, not altered for now.
 */
#include <crypto_process_user_call.h>

#else

/* Macros used in the CCC core to enter process layer functions
 * (e.g. from API layer, but also from other locations).
 *
 * The below macros do not deal with user mode calls, they are for
 * single address space cases only.
 *
 * When HAVE_USER_MODE enabled, these macros need to be replaced by user
 * space aware calls, which enter the process functions via a user
 * space handler.
 *
 * Hence, for HAVE_USER_MODE setups, these are defined in
 * crypto_process_user_call.h
 */
#if HAVE_SE_SHA
#define PROCESS_SHA(input, pcontext) CORE_PROCESS_SHA(input, pcontext)

#if HAVE_SE_ASYNC_SHA && HAVE_SE_SHA
#define PROCESS_SHA_ASYNC(input, pcontext) CORE_PROCESS_SHA_ASYNC(input, pcontext)
#define PROCESS_SHA_ASYNC_POLL(pcontext, idle_p) CORE_PROCESS_SHA_ASYNC_POLL(pcontext, idle_p)
#endif

#if HAVE_SE_KAC_KDF
#define PROCESS_SHA_KDF(input, pcontext) CORE_PROCESS_SHA_KDF(input, pcontext)
#endif
#endif /* HAVE_SE_SHA */

#if HAVE_CMAC_AES
#define PROCESS_AES_CMAC(input, pcontext) CORE_PROCESS_AES_CMAC(input, pcontext)
#endif

#if HAVE_RSA_CIPHER
#define PROCESS_RSA_CRYPTO(input, pcontext) CORE_PROCESS_RSA_CRYPTO(input, pcontext)
#endif

#if CCC_WITH_RSA
#define PROCESS_RSA_SIGN(input, pcontext) CORE_PROCESS_RSA_SIGN(input, pcontext)
#define PROCESS_RSA_VERIFY(input, pcontext) CORE_PROCESS_RSA_VERIFY(input, pcontext)
#endif

#if CCC_WITH_ECC
#define PROCESS_EC_SIGN(input, pcontext) CORE_PROCESS_EC_SIGN(input, pcontext)
#define PROCESS_EC_VERIFY(input, pcontext) CORE_PROCESS_EC_VERIFY(input, pcontext)
#endif

#if CCC_WITH_ECDH || CCC_WITH_X25519
#define PROCESS_EC_DERIVE(input, pcontext) CORE_PROCESS_EC_DERIVE(input, pcontext)
#endif

#if HAVE_SE_AES

#define PROCESS_AES(input, pcontext) CORE_PROCESS_AES(input, pcontext)

#if CCC_WITH_AES_GCM
#define PROCESS_AES_GCM(input, pcontext) CORE_PROCESS_AES_GCM(input, pcontext)
#endif

#if HAVE_AES_CCM
#define PROCESS_AES_CCM(input, pcontext) CORE_PROCESS_AES_CCM(input, pcontext)
#endif

#if HAVE_SE_ASYNC_AES
#define PROCESS_AES_ASYNC(input, pcontext) CORE_PROCESS_AES_ASYNC(input, pcontext)
#define PROCESS_AES_ASYNC_POLL(pcontext, idle_p) CORE_PROCESS_AES_ASYNC_POLL(pcontext, idle_p)
#endif

#endif /* HAVE_SE_AES */

#if HAVE_SE_KAC_KEYOP
#define PROCESS_AES_KWUW(input, pcontext) CORE_PROCESS_AES_KWUW(input, pcontext)
#endif

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API
#define PROCESS_RNG(input, pcontext) CORE_PROCESS_RNG(input, pcontext)
#endif

#if HAVE_MD5
#define PROCESS_SW_MD5(input, pcontext) CORE_PROCESS_SW_MD5(input, pcontext)
#endif

#if HAVE_SW_WHIRLPOOL
#define PROCESS_SW_WHIRLPOOL(input, pcontext) CORE_PROCESS_SW_WHIRLPOOL(input, pcontext)
#endif

#if HAVE_SW_SHA3
#define PROCESS_SW_SHA3(input, pcontext) CORE_PROCESS_SW_SHA3(input, pcontext)
#endif

#if CCC_WITH_XMSS
#define PROCESS_XMSS_VERIFY(input, pcontext) CORE_PROCESS_XMSS_VERIFY(input, pcontext)
#endif
#endif /* HAVE_USER_MODE */

#endif /* INCLUDED_CRYPTO_PROCESS_CALL_H */
