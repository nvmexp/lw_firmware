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

/* This file contains call mechanism into user space process layer
 * functions for handling data parameters before calling the
 * CORE_PROCESS_xxx() macros entering CCC core process layer from the
 * user space handlers.
 *
 * I.e. this adds a layer of user space data handlers between the API
 * layer (or other code with user space data) and the CCC core process
 * layer.
 *
 * These macros define prototypes for the USER MODE api by declaring
 * prototypes for the um_process_XXX() functions.
 *
 * These subsystem functions should check the domain of the engine
 * context and if it is a user mode context it needs to do any
 * additional handling of such user mode data (input and output
 * buffers) and then pass the "managed data" to the corresponding
 * CORE_PROCESS_XXX handler in CCC core.
 *
 * LWRRENTLY: the um_process_XXX() function definitons are in
 * crypto_process_user_call.c which calls the existing "ta_xxx()" api
 * in Trusty+CCC configurations.
 *
 * Not yet decided if the subsystem boundary is BEFORE or AFTER the
 * um_process_XXX() functions... (Lwrrently "AFTER"),
 */
#include <crypto_system_config.h>

#ifndef INCLUDED_CRYPTO_PROCESS_USER_CALL_H
#define INCLUDED_CRYPTO_PROCESS_USER_CALL_H

#if HAVE_USER_MODE == 0
#error "This file is for HAVE_USER_MODE compilations only"
#endif

#if HAVE_SE_SHA
status_t um_process_sha(struct se_data_params *input, struct se_sha_context *pcontext);
#define PROCESS_SHA(input, pcontext) um_process_sha(input, pcontext)

#if HAVE_SE_ASYNC_SHA && HAVE_SE_SHA
status_t um_process_sha_async(struct se_data_params *input, struct se_sha_context *pcontext);
#define PROCESS_SHA_ASYNC(input, pcontext) um_process_sha_async(input, pcontext)

status_t um_process_sha_async_poll(const struct se_sha_context *pcontext, bool *idle_p);
#define PROCESS_SHA_ASYNC_POLL(pcontext, idle_p) um_process_sha_async_poll(pcontext, idle_p)
#endif /* HAVE_SE_ASYNC_SHA && HAVE_SE_SHA */

#if HAVE_SE_KAC_KDF
status_t um_process_sha_kdf(struct se_data_params *input, struct se_sha_context *pcontext);
#define PROCESS_SHA_KDF(input, pcontext) um_process_sha_kdf(input, pcontext)
#endif /* HAVE_SE_KAC_KDF */
#endif /* HAVE_SE_SHA */

#if HAVE_CMAC_AES
status_t um_process_aes_cmac(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES_CMAC(input, pcontext) um_process_aes_cmac(input, pcontext)
#endif /* HAVE_CMAC_AES */

#if HAVE_RSA_CIPHER
status_t um_process_rsa_crypto(struct se_data_params *input, struct se_rsa_context *pcontext);
#define PROCESS_RSA_CRYPTO(input, pcontext) um_process_rsa_crypto(input, pcontext)
#endif

#if CCC_WITH_RSA
status_t um_process_rsa_verify(struct se_data_params *input, struct se_rsa_context *pcontext);
#define PROCESS_RSA_SIGN(input, pcontext) um_process_rsa_sign(input, pcontext)

status_t um_process_rsa_sign(struct se_data_params *input, struct se_rsa_context *pcontext);
#define PROCESS_RSA_VERIFY(input, pcontext) um_process_rsa_verify(input, pcontext)
#endif

#if CCC_WITH_ECC
status_t um_process_ec_sign(struct se_data_params *input, struct se_ec_context *pcontext);
#define PROCESS_EC_SIGN(input, pcontext) um_process_ec_sign(input, pcontext)

status_t um_process_ec_verify(struct se_data_params *input, struct se_ec_context *pcontext);
#define PROCESS_EC_VERIFY(input, pcontext) um_process_ec_verify(input, pcontext)

#if CCC_WITH_ECDH || CCC_WITH_X25519
status_t um_process_ec_derive(struct se_data_params *input, struct se_ec_context *pcontext);
#define PROCESS_EC_DERIVE(input, pcontext) um_process_ec_derive(input, pcontext)
#endif
#endif /* CCC_WITH_ECC */

#if HAVE_SE_AES
status_t um_process_aes(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES(input, pcontext) um_process_aes(input, pcontext)

/* CCM and GCM use the same user mode handler
 */
#if CCC_WITH_AES_GCM
status_t um_process_aes_gcm(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES_GCM(input, pcontext) um_process_aes_gcm(input, pcontext)
#endif

#if HAVE_AES_CCM
status_t um_process_aes_ccm(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES_CCM(input, pcontext) um_process_aes_ccm(input, pcontext)
#endif

#if HAVE_SE_ASYNC_AES && HAVE_SE_AES
status_t um_process_aes_async(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES_ASYNC(input, pcontext) um_process_aes_async(input, pcontext)

status_t um_process_aes_async_poll(const struct se_aes_context *pcontext, bool *idle_p);
#define PROCESS_AES_ASYNC_POLL(pcontext, idle_p) um_process_aes_async_poll(pcontext, idle_p)
#endif

#endif /* HAVE_SE_AES */

#if HAVE_SE_KAC_KEYOP
status_t um_process_aes_kwuw(struct se_data_params *input, struct se_aes_context *pcontext);
#define PROCESS_AES_KWUW(input, pcontext) um_process_aes_kwuw(input, pcontext)
#endif

#if CCC_WITH_DRNG || CCC_ENABLE_TRNG_API
status_t um_process_rng(struct se_data_params *input, struct se_rng_context *pcontext);
#define PROCESS_RNG(input, pcontext) um_process_rng(input, pcontext)
#endif

/* Fully SW versions */

#if HAVE_MD5
struct soft_md5_context;
status_t um_sw_process_md5(struct se_data_params *input, struct soft_md5_context *pcontext);
#define PROCESS_SW_MD5(input, pcontext) um_sw_process_md5(input, pcontext)
#endif

#if HAVE_SW_WHIRLPOOL
struct soft_whirlpool_context;
status_t um_sw_process_whirlpool(struct se_data_params *input, struct soft_whirlpool_context *pcontext);
#define PROCESS_SW_WHIRLPOOL(input, pcontext) um_sw_process_whirlpool(input, pcontext)
#endif

#if HAVE_SW_SHA3
struct soft_sha3_context;
status_t um_sw_process_sha3(struct se_data_params *input, struct soft_sha3_context *pcontext);
#define PROCESS_SW_SHA3(input, pcontext) um_sw_process_sha3(input, pcontext)
#endif

#if CCC_WITH_XMSS
struct se_xmss_context;
status_t um_process_xmss_verify(struct se_data_params *input, struct se_xmss_context *pcontext);
#define PROCESS_XMSS_VERIFY(input, pcontext) um_process_xmss_verify(input, pcontext)
#endif
#endif /* INCLUDED_CRYPTO_PROCESS_USER_CALL_H */
