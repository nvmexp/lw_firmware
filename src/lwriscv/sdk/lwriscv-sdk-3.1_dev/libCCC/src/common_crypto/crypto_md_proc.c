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

#if HAVE_SE_SHA || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3

#include <crypto_md_proc.h>

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t sha_engine_context_static_init_cm(te_crypto_domain_t domain,
					   engine_id_t engine_id,
					   te_crypto_algo_t algo,
					   struct se_engine_sha_context *sha_econtext,
					   struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	uint32_t block_bits = 0U;
	uint32_t hash_bits = 0U;

	LTRACEF("entry\n");

	if (NULL == sha_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
	case TE_ALG_SHA1:
		block_bits = 512U;
		hash_bits =  160U;
		break;

	case TE_ALG_SHA224:
		block_bits = 512U;
		hash_bits =  224U;
		break;

	case TE_ALG_SHA256:
		block_bits = 512U;
		hash_bits =  256U;
		break;

	case TE_ALG_SHA384:
		block_bits = 1024U;
		hash_bits =  384U;
		break;

#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
	case TE_ALG_SHA512_256:
		/* The split digest operates internally with SHA-512 parameters */
#endif
	case TE_ALG_SHA512:
		block_bits = 1024U;
		hash_bits =  512U;
		break;

#if HAVE_HW_SHA3
	case TE_ALG_SHA3_224:
		/* 1152 = 1600 - 448 */
		block_bits = 1152U;
		hash_bits =  224U;
		break;
	case TE_ALG_SHA3_256:
		/* 1088 = 1600 - 512 */
		block_bits = 1088U;
		hash_bits =  256U;
		break;
	case TE_ALG_SHA3_384:
		/* 832 = 1600 - 768 */
		block_bits = 832U;
		hash_bits =  384U;
		break;
	case TE_ALG_SHA3_512:
		/* 576 = 1600 - 1024 */
		block_bits = 576U;
		hash_bits =  512U;
		break;
	case TE_ALG_SHAKE128:
		/* 1344 = 1600 - 256
		 *
		 * SHAKE128 is a sponge function with variable output size.
		 * It is defined by the caller and set to hash_size when defined.
		 * This is the default XOF bitlen if not specified by the caller.
		 */
		block_bits = 1344U;
		hash_bits =  SHAKE128_DEFAULT_XOF_SIZE;
		sha_econtext->shake_xof_bitlen = hash_bits;
		break;
	case TE_ALG_SHAKE256:
		/* 1088 = 1600 - 512
		 *
		 * SHAKE256 is a sponge function with variable output size.
		 * It is defined by the caller and set to hash_size when defined.
		 * This is the default XOF bitlen if not specified by the caller.
		 */
		block_bits = 1088U;
		hash_bits =  SHAKE256_DEFAULT_XOF_SIZE;
		sha_econtext->shake_xof_bitlen = hash_bits;
		break;
#endif /* HAVE_HW_SHA3 */

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
		/* Special purpose setup for KDF using HMAC-SHA256 */
		block_bits = 512U;
		hash_bits =  256U;
		break;
#endif /* HAVE_SE_KAC_KDF */

	default:
		CCC_ERROR_MESSAGE("Digest algorithm 0x%x unknown\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	sha_econtext->block_size = block_bits / 8U;

	/* SHAKE hash_size result size is set by the caller based on init arguments
	 * If not set, the defaults are used.
	 */
	sha_econtext->hash_size =  hash_bits / 8U;

	ret = ccc_select_engine(&sha_econtext->engine, CCC_ENGINE_CLASS_SHA, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("SHA engine selection failed: %u\n", ret));

	sha_econtext->algorithm = algo;
	sha_econtext->hash_algorithm = algo;
	sha_econtext->is_first = true;
	sha_econtext->is_last  = false;
	sha_econtext->domain = domain;
	sha_econtext->cmem = cmem;

#if TEGRA_MEASURE_PERFORMANCE
	sha_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* This is a backwards compat function, call
 * sha_engine_context_static_init_cm() instead.
 */
status_t sha_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_sha_context *sha_econtext)
{
	return sha_engine_context_static_init_cm(domain, engine_id,
						 algo, sha_econtext,
						 NULL);
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
static status_t sha_context_dma_buf_alloc(struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;
	uint8_t *buf = NULL;

	LTRACEF("entry\n");

	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != sha_context->buf) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	buf = CMTAG_MEM_GET_BUFFER(sha_context->ec.cmem,
				   CMTAG_ALIGNED_DMA_BUFFER,
				   CCC_DMA_MEM_ALIGN,
				   CCC_DMA_MEM_SHA_BUFFER_SIZE);
	if (NULL == buf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("SHA context dma memory alloc failed: %u bytes\n",
					     CCC_DMA_MEM_SHA_BUFFER_SIZE));
	}
	sha_context->buf = buf;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void sha_context_dma_buf_release(struct se_sha_context *sha_context)
{
	LTRACEF("entry\n");
	if ((NULL != sha_context) && (NULL != sha_context->buf)) {
		se_util_mem_set(sha_context->buf, 0U, CCC_DMA_MEM_SHA_BUFFER_SIZE);
		CMTAG_MEM_RELEASE(sha_context->ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  sha_context->buf);
	}
	LTRACEF("exit\n");
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

/* Does not memset zero => do it yourself */
status_t sha_context_static_init(te_crypto_domain_t domain,
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
			CCC_ERROR_MESSAGE("SHA engine context setup failed: %u\n", ret));

	sha_context->used = 0U;
	sha_context->byte_count = 0U;

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
	/* HW engines for SHA access intermediate buffers
	 * with a DMA engine.
	 */
	ret = sha_context_dma_buf_alloc(sha_context);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#endif /* HAVE_SE_SHA || HAVE_SW_WHIRLPOOL || CCC_WITH_SHA3 */
