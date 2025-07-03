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
#ifndef INCLUDED_CRYPTO_EC_H
#define INCLUDED_CRYPTO_EC_H

#include <crypto_ops.h>
#include <tegra_pka1_ec_gen.h>

#if CCC_SOC_WITH_PKA1
#include <tegra_pka1_ec.h>
#include <tegra_pka1_mod.h>
#else
#include <lwpka/tegra_lwpka_ec.h>
#include <lwpka/tegra_lwpka_mod.h>
#endif

typedef enum te_ec_lwrve_type_e {
	PKA1_LWRVE_TYPE_NONE = 0,	/* unspecified */
	PKA1_LWRVE_TYPE_WEIERSTRASS,	/* weierstrass */
	PKA1_LWRVE_TYPE_LWRVE25519,	/* Specific lwrve25519 (montgomery lwrve) */
	PKA1_LWRVE_TYPE_ED25519,	/* Specific lwrve ed25519 (twisted edwards) */
	PKA1_LWRVE_TYPE_LWRVE448,	/* Specific lwrve448 (montgomery lwrve) */
	PKA1_LWRVE_TYPE_ED448,		/* Specific lwrve ed448 (twisted edwards) */
} te_ec_lwrve_type_t;

#define PKA1_LWRVE_FLAG_NONE		0x0000U
#define PKA1_LWRVE_FLAG_FIPS		0x0001U	/* FIPS lwrve (XXX TODO) */
#define PKA1_LWRVE_FLAG_LITTLE_ENDIAN	0x0002U	/* Lwrve params are in little endian order */
#define PKA1_LWRVE_FLAG_MONTGOMERY_OK	0x0004U /* If montgomery residues are in lwrve parameters */
#define PKA1_LWRVE_FLAG_A_IS_M3		0x0008U	/* Only set this if lwrve parameter
						 * know to be A==-3
						 */
#define PKA1_LWRVE_FLAG_SPECIAL		0x0010U /* Special property lwrve (depends on lwrve) */
#define PKA1_LWRVE_FLAG_NO_MONTGOMERY	0x0020U /* Does not use montgomery */

typedef struct {
	/* Montgomery constants for lwrve prime */
	const uint8_t *m_prime;
	const uint8_t *r2;
} ec_montgomery_values_t;

typedef struct pka1_ec_lwrve_s {
	const te_ec_lwrve_id_t id;
	const uint32_t      flags;	/* flags */
	const char	   *name;	/* name string */
	const struct {
		const uint8_t *gen_x;
		const uint8_t *gen_y;
	} g;	/* generator point */
	const ec_montgomery_values_t mg; /* EC lwrves can use fixed montgomery values (optional) */
	const uint8_t      *p;		/* prime */
	const uint8_t      *n;		/* order */
	const uint8_t      *a;		/* parameter a */
	const uint8_t      *b;		/* parameter b */
	const uint8_t      *d;		/* parameter d (NULL for other lwrves than ED25519) */
	const uint32_t      nbytes;	/* each parameter length in bytes */
	const enum pka1_op_mode mode;
	const te_ec_lwrve_type_t type;
} pka1_ec_lwrve_t;

#endif /* INCLUDED_CRYPTO_EC_H */
