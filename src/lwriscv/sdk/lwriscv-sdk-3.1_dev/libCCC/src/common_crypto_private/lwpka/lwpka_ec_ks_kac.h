/*
 * Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* struct se_key_slot object definition, differs in KAC enabled systems.
 */
#ifndef INCLUDED_LWPKA_EC_KS_KAC_H
#define INCLUDED_LWPKA_EC_KS_KAC_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if HAVE_SOC_FEATURE_KAC == 0
#error "KAC features not supported"
#endif

#if HAVE_LWPKA11_SUPPORT

struct ec_kac_manifest {
	uint32_t emf_origin;	/*  4 bit field, 3:0 (HW, fixed by origin) */
	uint32_t emf_user;	/*  4 bit field, bits 7:4 (Client sets this) */
	uint32_t emf_purpose;	/*  5 bit field, bits 11:8 (HW, fixed) */
	uint32_t emf_ex;	/*  1 bit field, bit  12:12 (HW, fixed) */
	/* reserved: 13 */
	uint32_t emf_size;	/*  2 bit field, bits 15:14 (CCC from op_mode) */
	uint32_t emf_sw;	/* 12 bit field, bits 27:16 (Client sets this) */
	uint32_t emf_dp_idx;	/*  2 bit field, bits 28:29 (HW, from size) */
	uint32_t emf_type;	/*  2 bit field, bits 30:31 (key type, HW, fixed) */
};

/* For engine switch calls =>
 *  eks_mf is only used by ECCKEY_PRI and ECCKEY_GEN.
 *
 *  eks_keystore is used by all EC KS operation (set this to ZERO for LWPKA1.1)
 *
 *  Private key (for ECCKEY_PRI) is in econtext->ec_pkey with length lwrve->nbytes.
 *
 *  Public key (for ECCGEN_PUB) is placed to input_params->dst_ec_pubkey point object,
 *    if NULL != input_params (if NULL, pubkey is not read from HW).
 *
 */
struct ec_ks_slot {
	uint32_t eks_keystore;		/* Keystore used by the algorithm */
	struct ec_kac_manifest eks_mf; /* Input KAC keyslot metadata fields (manifest, user, etc)
					*  for keystore operations which need this input.
					*/
};
#endif /* HAVE_LWPKA11_SUPPORT */
#endif /* INCLUDED_LWPKA_EC_KS_KAC_H */
