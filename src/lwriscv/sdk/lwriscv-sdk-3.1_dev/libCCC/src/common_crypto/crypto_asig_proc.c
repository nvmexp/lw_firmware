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

/* Support for asymmetric operations with SE engine (RSA, EC).
 *
 * Context initializations.
 */
#include <crypto_system_config.h>

#include <crypto_asig_proc.h>

#if CCC_WITH_ECC
/* EC context setup functions */
#include <crypto_ec.h>
#endif

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if CCC_WITH_RSA

status_t rsa_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_rsa_context *rsa_econtext,
					struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == rsa_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ccc_select_engine(&rsa_econtext->engine, CCC_ENGINE_CLASS_RSA, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA engine selection failed: %u\n",
				      ret));

	rsa_econtext->algorithm = algo;
	rsa_econtext->domain = domain;
	rsa_econtext->cmem = cmem;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	rsa_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t rsa_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				 te_crypto_algo_t algo_param,
				 struct se_rsa_context *rsa_context,
				 struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	te_crypto_algo_t hash_algorithm = TE_ALG_NONE;
	uint32_t hlen = 0U;
	te_crypto_algo_t algo = algo_param;

	LTRACEF("entry\n");

	if (NULL == rsa_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
#if HAVE_PLAIN_DH
		/* Diffie-Hellman-Merkle key agreement uses modular exponentiation,
		 *  DH implemented as "Plain RSA private key decipher".
		 */
	case TE_ALG_DH:
		algo = TE_ALG_MODEXP;
		break;
#endif

		/* RSA cipher functions */

	case TE_ALG_MODEXP:
	case TE_ALG_RSAES_PKCS1_V1_5:
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
		hash_algorithm = TE_ALG_SHA1;
		hlen = 20U;
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
		hash_algorithm = TE_ALG_SHA224;
		hlen = 28U;
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
		hash_algorithm = TE_ALG_SHA256;
		hlen = 32U;
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
		hash_algorithm = TE_ALG_SHA384;
		hlen = 48U;
		break;

	case TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
		hash_algorithm = TE_ALG_SHA512;
		hlen = 64U;
		break;

		/* RSA signature functions */

	case TE_ALG_RSASSA_PKCS1_V1_5_MD5:
		hash_algorithm = TE_ALG_MD5;
		hlen = 16U;
		break;

	case TE_ALG_RSASSA_PKCS1_V1_5_SHA1:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
		hash_algorithm = TE_ALG_SHA1;
		hlen = 20U;
		break;

	case TE_ALG_RSASSA_PKCS1_V1_5_SHA224:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
		hash_algorithm = TE_ALG_SHA224;
		hlen = 28U;
		break;

	case TE_ALG_RSASSA_PKCS1_V1_5_SHA256:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
		hash_algorithm = TE_ALG_SHA256;
		hlen = 32U;
		break;

	case TE_ALG_RSASSA_PKCS1_V1_5_SHA384:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
		hash_algorithm = TE_ALG_SHA384;
		hlen = 48U;
		break;

	case TE_ALG_RSASSA_PKCS1_V1_5_SHA512:
	case TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
		hash_algorithm = TE_ALG_SHA512;
		hlen = 64U;
		break;

	default:
		CCC_ERROR_MESSAGE("RSA algorithm 0x%x unsupported\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	// XXX  Standard allows all kind of values for slen, including zero size.
	//
	// Max supported size of sig_slen is 64 bytes == SHA-512 result size, if later
	// accepting a parameter for it...
	//
	/// XXX FYI: the salt len is not forced (sLen == hLen) now when decoding. Also MB1 code
	// parses the SALT in a "dynamic fashion".
	//
	// Allow SALT LENGTH to be dynamic; set an RSA init flag INIT_FLAG_RSA_SALT_DYNAMIC_LEN
	// for that.
	//
	rsa_context->sig_slen = hlen;	// if signature => signature hash length
	rsa_context->md_algorithm = hash_algorithm;

	ret = rsa_engine_context_static_init(domain, engine_id, algo,
					     &rsa_context->ec, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA engine context setup failed: %u\n",
				      ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECC
/* ECC specific setup functions
 */
static status_t ec_engine_context_static_init(te_crypto_domain_t domain,
					      engine_id_t engine_id,
					      te_crypto_algo_t algo,
					      te_ec_lwrve_id_t lwrve_id,
					      struct se_engine_ec_context *ec_econtext,
					      struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	const pka1_ec_lwrve_t *lwrve = NULL;
	engine_class_t cclass = 0U;

	LTRACEF("entry\n");
	if (NULL == ec_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
	case TE_ALG_ECDSA:
	case TE_ALG_ECDH:
		cclass = CCC_ENGINE_CLASS_EC;
		break;

#if CCC_WITH_X25519
	case TE_ALG_X25519:
		cclass = CCC_ENGINE_CLASS_X25519;
		break;
#endif
#if CCC_WITH_ED25519
	case TE_ALG_ED25519CTX:
	case TE_ALG_ED25519PH:
	case TE_ALG_ED25519:
		cclass = CCC_ENGINE_CLASS_ED25519;
		break;
#endif
	default:
		CCC_ERROR_MESSAGE("EC algorithm 0x%x unsupported (class)\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = ccc_select_engine(&ec_econtext->engine, cclass, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("EC engine selection failed: %u\n", ret));

	/* This may be engine specific; but not lwrrently */
	lwrve = ccc_ec_get_lwrve(lwrve_id);
	if (NULL == lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve %u not supported\n", lwrve_id));
	}
	ec_econtext->ec_lwrve  = lwrve;
	ec_econtext->algorithm = algo;
	ec_econtext->domain    = domain;
	ec_econtext->cmem      = cmem;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	ec_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t ec_context_static_init(te_crypto_domain_t domain,
				engine_id_t engine_id,
				te_crypto_algo_t algo,
				te_ec_lwrve_id_t lwrve_id,
				struct se_ec_context *ec_context,
				struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == ec_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
#if CCC_WITH_ECDH
	case TE_ALG_ECDH:
		break;
#endif
#if CCC_WITH_X25519
	case TE_ALG_X25519:
		break;
#endif
#if CCC_WITH_ECDSA
	case TE_ALG_ECDSA:
		break;
#endif
#if CCC_WITH_ED25519
	case TE_ALG_ED25519CTX:
	case TE_ALG_ED25519PH:
	case TE_ALG_ED25519:
		break;
#endif
	default:
		CCC_ERROR_MESSAGE("EC algorithm 0x%x unsupported\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = ec_engine_context_static_init(domain, engine_id, algo,
					    lwrve_id, &ec_context->ec, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("EC engine context setup failed: %u\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECC */
