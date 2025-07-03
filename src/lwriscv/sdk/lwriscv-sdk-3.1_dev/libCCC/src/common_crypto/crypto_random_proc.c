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

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG

#include <crypto_random_proc.h>

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t rng_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_rng_context *rng_econtext)
{
	status_t ret = NO_ERROR;
	engine_class_t eclass = CCC_ENGINE_CLASS_DRNG;

	if (NULL == rng_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
	case TE_ALG_RANDOM_DRNG: /* DRNG engines */
		eclass = CCC_ENGINE_CLASS_DRNG;
		break;

#if HAVE_PKA1_TRNG
	case TE_ALG_RANDOM_TRNG: /* TRNG engines (true random entropy) */
		eclass = CCC_ENGINE_CLASS_TRNG;
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("RNG algorithm 0x%x unknown\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = ccc_select_engine(&rng_econtext->engine, eclass, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RNG engine selection failed: %u\n", ret));

	rng_econtext->algorithm = algo;
	rng_econtext->domain = domain;
	/* No CMEM in rng_econtext for now. If added, init here. */

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	rng_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	return ret;
}

/* Does not memset zero => do it in caller */
status_t rng_context_static_init(te_crypto_domain_t domain,
				 engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct se_rng_context *rng_context)
{
	status_t ret = NO_ERROR;

	if (NULL == rng_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = rng_engine_context_static_init(domain, engine_id, algo, &rng_context->ec);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RNG engine context setup failed: %u\n", ret));
fail:
	return ret;
}

#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */
