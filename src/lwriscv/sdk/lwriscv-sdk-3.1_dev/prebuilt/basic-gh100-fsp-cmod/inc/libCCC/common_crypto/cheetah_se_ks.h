/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* struct se_key_slot object definition, differs in KAC enabled systems.
 */
#ifndef INCLUDED_TEGRA_SE_KS_H
#define INCLUDED_TEGRA_SE_KS_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if HAVE_SOC_FEATURE_KAC

#include <tegra_se_kac.h>

#if HAVE_LWPKA11_SUPPORT
#include <lwpka/lwpka_ec_ks_kac.h>
#endif

#else

#if HAVE_SE_UNWRAP

#if ((CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T18X)) && \
     (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T19X)))
#error "Platform does not support T18X/T19X unwrap operation"
#endif

status_t engine_aes_unwrap_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext);
#endif

/* For SE device (AES and MAC) algorithm keys only
 *
 * If other engines added to SE device, may need to support more
 * complex key material.
 */
struct se_key_slot {
	const uint8_t *ks_keydata;	/* AES key material, read only */
	uint32_t ks_bitsize;		/* Bitsize of key material */
	uint32_t ks_slot;		/* Keyslot used by the algorithm */
	union {
		uint32_t ks_subkey_slot;	/* Multi-key operation keyslot.
						 *
						 * For XTS this is same as ks_slot
						 *  unless HAVE_SOC_FEATURE_KAC is set
						 *
						 * This field is also KDF_2KEY
						 * key2_index slot for second
						 * KDF key (i.e. the KDD key).
						 *
						 * The second key material is
						 * in ks_keydata at key size
						 * offset (as for XTS subkey).
						 */
#if HAVE_SE_KAC_KDF
		struct {
			uint32_t ks_kdd_slot;
			const uint8_t *ks_kdd_key;
		};
#endif
	};

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
	/* T1XX operations
	 */
	uint32_t ks_target_keyslot;	/* T1XX dst kslot to unwrap or derive to */
#endif
};
#endif /* HAVE_SOC_FEATURE_KAC */
#endif /* INCLUDED_TEGRA_SE_KS_H */
