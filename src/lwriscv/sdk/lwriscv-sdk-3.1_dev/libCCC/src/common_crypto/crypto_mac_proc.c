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

/* CMAC-AES-*, HMAC-SHA-* and HMAC-MD5 support.
 *
 * => supports MD5, SHA1 and SHA2 digests for HMACs and AES block cipher for CMAC.
 *
 * If other digests are added (e.g. sha-3) code requires
 * some changes to call those different digests and the mac_state_t object
 * then needs a new digest contexts for those algorithms.
 */

#include <crypto_system_config.h>

#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5

#include <crypto_mac_proc.h>

#if HAVE_CMAC_AES
#include <crypto_cipher_proc.h>
#endif

#if HAVE_HMAC_SHA
#include <crypto_md_proc.h>
#endif

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* XXX TODO: Move this, generic => use for copying results to clients.
 *
 * kv_src cound be const, but some subsystem data copy functions do not
 * support const functions.
 */
status_t copy_data_to_domain_address(te_crypto_domain_t domain,
				     uint8_t *domain_addr,
				     uint8_t *kv_src,
				     uint32_t len)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	if ((NULL == domain_addr) || (NULL == kv_src) || (0U == len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (domain) {
	case TE_CRYPTO_DOMAIN_KERNEL:
		se_util_mem_move(domain_addr, kv_src, len);
		break;
#if HAVE_USER_MODE
	case TE_CRYPTO_DOMAIN_TA:
		ret = ta_copy_buffer_to_ta(GET_ADDR32(domain_addr), kv_src, len);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: ret=%d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5 */
