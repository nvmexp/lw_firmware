/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/*
 * Support CheetAh Security Engine Elliptic crypto algorithm ECDH
 *  key agreement.
 *
 * Note: plain Diffie-Hellman-Merkle (DH algorithm) is implemented as
 *  plain RSA private key decipher (in crypto_acipher.c). So it is not
 *  here even though it is also a key derivation algorithm. The crypto
 *  context dispatcher passes TE_ALG_DH directly to acipher
 *  functions. This avoids adding the RSA context to the
 *  se_derive_state_t (the RSA context would grow this state with
 *  about one kilobyte and make it DMA sensitive).
 */

#include <crypto_system_config.h>

#if HAVE_CCC_KDF

#include <crypto_derive_proc.h>
#include <crypto_asig_proc.h>

#if HAVE_SE_KAC_KDF
#include <crypto_md_proc.h>
#endif

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_KAC_KDF
status_t kdf_context_static_init(te_crypto_domain_t domain,
				 engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct se_sha_context *sha_context,
				 struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = sha_engine_context_static_init_cm(domain, engine_id, algo,
						&sha_context->ec, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("KDF engine context setup failed: %u\n", ret));

	sha_context->used = 0U;
	sha_context->byte_count = 0U;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KDF */
#endif /* HAVE_CCC_KDF */
