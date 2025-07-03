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

/* XXX TODO: MOVE THIS!!!
 * This layer takes care of processing and buffering data when several
 * calls feed the inbound data in parts (i.e. zero or more UPDATE()
 * calls followed by one DOFINAL() call).
 *
 * Such a sequence of multiple calls is supported for digests, macs and
 * block/stream cipher functions.
 *
 * Functions in this file are called by the crypto_<md,cipher,mac>.c
 * to process the data, possibly via the crypto_ta_api.c which is dealing
 * with the TA user space addresses (if the crypto call domain is not the
 * secure OS).
 *
 * So this module deals with kernel virtual addresses only. This calls
 * the SE engine HW functions in tegra_se.c, which maps the KV addresses
 * to phys addresses before passing them to the engine.
 */

/**
 * @file   crypto_process.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Support functions for process layer && external clients accessing
 *   process layer features and keyslot management.
 */

#include <crypto_system_config.h>
#include <crypto_ops.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if SE_RD_DEBUG
#if HAVE_NC_REGIONS
/* XXXXX TODO REMOVE =>
 * Only for testing; if set 1 forces all pages to be treated as
 * "non-adjacent" in HAVE_NC_REGIONS cases.
 *
 * note: AES is probably still not handled properly in NC cases
 * (e.g. for Trusty TA's)
 */
#ifndef TEST_FORCE_ALL_PAGES_NON_ADJACENT
#define TEST_FORCE_ALL_PAGES_NON_ADJACENT 0
#endif
#endif /* HAVE_NC_REGIONS */
#endif /* SE_RD_DEBUG */

/* Generic function for algos using digests
 */
status_t get_message_digest_size(te_crypto_algo_t algo, uint32_t *len_p)
{
	status_t ret = NO_ERROR;
	uint32_t bits = 0U;

	LTRACEF("entry\n");

	if (NULL == len_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*len_p = 0U;

	switch (algo) {
	case TE_ALG_MD5:
		bits = 128U;
		break;

	case TE_ALG_SHA1:
		bits = 160U;
		break;

	case TE_ALG_SHA512_224:
	case TE_ALG_SHA224:
	case TE_ALG_SHA3_224:
		bits = 224U;
		break;

	case TE_ALG_SHA512_256:
	case TE_ALG_SHA256:
	case TE_ALG_SHA3_256:
		bits = 256U;
		break;

	case TE_ALG_SHA384:
	case TE_ALG_SHA3_384:
		bits = 384U;
		break;

	case TE_ALG_SHA512:
	case TE_ALG_WHIRLPOOL:
	case TE_ALG_SHA3_512:
		bits = 512U;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*len_p = (bits / 8U);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_NC_REGIONS

struct nc_region_s {
	uint32_t	clength;	/**< length of contiguous bytes */
	uint32_t	remain;		/**< number of remaining buffer bytes */
	const uint8_t  *kva;		/**< current virtual address */
	uint32_t	pcount;
};

static status_t nc_find_non_contiguous_page(struct nc_region_s *ncr,
					    te_crypto_domain_t domain,
					    PHYS_ADDR_T paddr_param)
{
	status_t ret = NO_ERROR;
	bool done = false;
	PHYS_ADDR_T paddr = paddr_param;
	PHYS_ADDR_T next_paddr = (PHYS_ADDR_T)0U;

	LTRACEF("entry\n");

#if SE_RD_DEBUG
#if TEST_FORCE_ALL_PAGES_NON_ADJACENT
	/* Non-adjacent pages should not affect result.
	 * But this makes easier testing of this claim.
	 *
	 * SIMULATING FOR TEST CASE HERE: every page forced non-adjacent
	 */
	CCC_ERROR_MESSAGE("TESTING: NC region test: force all buffer pages non-adjacent\n");
	CCC_ERROR_MESSAGE("TESTING: clength=%u, remain=%u\n", ncr->clength, ncr->remain);
	done = true;
#endif
#endif /* SE_RD_DEBUG */

	/* Here the kva is always page aligned or the loop terminates
	 * (remain == 0U)
	 */
	while (!BOOL_IS_TRUE(done) && (ncr->remain > 0U)) {
		DEBUG_ASSERT(((uintptr_t)ncr->kva & CCC_PAGE_MASK) == 0U);

		next_paddr = DOMAIN_VADDR_TO_PADDR(domain, ncr->kva);
		if ((PHYS_ADDR_T)0UL == next_paddr) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			done = true;
			LTRACEF("Domain %u vaddr %p can't be mapped to physaddr (start of page %u)\n",
				domain, ncr->kva, (ncr->pcount+1));
			continue;
		}

#if !TEGRA_MEASURE_MCP
		LTRACEF("NEXT_PADDR (kva %p) => 0x%llx\n",
			ncr->kva, (unsigned long long)next_paddr);
#endif

		if ((PHYS_ADDR_T)(((uintptr_t)paddr & ~CCC_PAGE_MASK) + CCC_PAGE_SIZE) != next_paddr) {
			/* Phys pages are not adjacent */
			break;
		}

		ncr->pcount++;

		if (ncr->remain > CCC_PAGE_SIZE) {
			/* Move to next page */
			ncr->clength += CCC_PAGE_SIZE;
			ncr->kva      = &ncr->kva[CCC_PAGE_SIZE];
			ncr->remain  -= CCC_PAGE_SIZE;

			paddr	      = next_paddr;
		} else {
			/* Rest of the bytes fit in current page =>
			 * buffer was contiguous in phys mem
			 */
			ncr->clength += ncr->remain;
			ncr->kva      = &ncr->kva[ncr->remain];
			ncr->remain   = 0U;
		}
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Get contiguous phys region length from virtual address KVA in the given
 *  domain (kernel or TA).
 *
 * Loop pages forward until the physical pages are no longer
 *  contiguous; response is the number of physically contiguous
 *  bytes starting from virtual address KVA (which may be a secure app
 *  address (32 bit TA address) or a (64 bit) Kernel address).
 *
 * Need to cast a pointer to scalar for page boundary checks and
 * bytes on page callwlations when dealing with non-contiguous memory
 * objects in physical memory when crossing page boundaries. This
 * is required if the SE engine accesses the memory via physical addresses.
 */
status_t ccc_nc_region_get_length(te_crypto_domain_t domain,
				  const uint8_t *kva,
				  uint32_t remain,
				  uint32_t *clen_p)
{
	status_t ret = NO_ERROR;
	struct nc_region_s ncr = { .kva = NULL };
	const uint32_t bytes_in_first_page = CCC_PAGE_SIZE - (uint32_t)(((uintptr_t)kva & CCC_PAGE_MASK));
	PHYS_ADDR_T paddr = (PHYS_ADDR_T)0U;

	LTRACEF("entry\n");

	if ((NULL == kva) || (NULL == clen_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ncr.pcount = 1U;

	LTRACEF("get nc region starting at address %p (length %u bytes)\n", kva, remain);
	*clen_p = 0U;

	paddr = DOMAIN_VADDR_TO_PADDR(domain, kva);

	if ((PHYS_ADDR_T)0UL == paddr) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Domain %u vaddr %p can't be mapped to physaddr, first page\n",
					     domain, kva));
	}

	LTRACEF("First PADDR (kva %p) => 0x%llx\n",
		kva, (unsigned long long)paddr);

	if (remain <= bytes_in_first_page) {
		/* All bytes fit in the first page */
		ncr.clength = remain;
		ncr.remain = 0U;
	} else {
		ncr.kva     = &kva[bytes_in_first_page];	// kva of next page boundary
		ncr.clength = bytes_in_first_page;		// First page bytes
		ncr.remain  = remain - bytes_in_first_page;

		ret = nc_find_non_contiguous_page(&ncr, domain, paddr);
		CCC_ERROR_CHECK(ret);
	}

	LTRACEF("Contiguous: %u bytes on %u pages (in region %u)\n",
		ncr.clength, ncr.pcount, ncr.remain);

	*clen_p = ncr.clength;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_NC_REGIONS */

#if HAVE_IS_CONTIGUOUS_MEM_FUNCTION
/* Check if the given buffer is contiguous across pages
 * If HAVE_NC_REGIONS is 0 => this is always true.
 * If it is 1 => the buffer page boundaries verified.
 *
 * Returns NO_ERROR only if memory is contiguous but also sets
 * the boolean *is_contiguous_p TRUE if the memory is contiguous.
 *
 * is_contiguous_p can be left NULL if the return value is sufficient.
 */
status_t se_is_contiguous_memory_region(const uint8_t *kva,
					uint32_t size,
					bool *is_contiguous_p)
{
	status_t ret       = NO_ERROR;
	bool is_contiguous = false;

	LTRACEF("entry (size: %u)\n", size);

	(void)size;

	if (NULL == kva) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != is_contiguous_p) {
		*is_contiguous_p = false;
	}

#if HAVE_NC_REGIONS
	if (0U == size) {
		is_contiguous = true;
	} else {
		te_crypto_domain_t domain = TE_CRYPTO_DOMAIN_KERNEL;
		uint32_t clen = 0U;

#if HAVE_USER_MODE
		/* This enables mapping both KERNEL and TA addresses */
		domain = TE_CRYPTO_DOMAIN_TA;
#endif
		ret = ccc_nc_region_get_length(domain, kva, size, &clen);
		CCC_ERROR_CHECK(ret);

		if (clen >= size) {
			is_contiguous = true;
		}
	}
#else
	/* If only contiguous memory is used => this is always true */
	is_contiguous = true;
#endif /* HAVE_NC_REGIONS */

	if (NULL != is_contiguous_p) {
		*is_contiguous_p = is_contiguous;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_IS_CONTIGUOUS_MEM_FUNCTION */
