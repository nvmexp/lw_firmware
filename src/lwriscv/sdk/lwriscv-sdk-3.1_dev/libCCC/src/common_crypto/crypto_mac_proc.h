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

#ifndef INCLUDED_CRYPTO_MAC_PROC_H
#define INCLUDED_CRYPTO_MAC_PROC_H

#include <crypto_ops.h>

// XXX generic ==> move this !
status_t copy_data_to_domain_address(te_crypto_domain_t domain,
				     uint8_t *domain_addr,
				     uint8_t *kv_src,
				     uint32_t len);

#endif /* INCLUDED_CRYPTO_MAC_PROC_H */
