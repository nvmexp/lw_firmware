/*
 * Header file for CheetAh Security Elliptic Engine
 *
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 */

#ifndef INCLUDED_C25519_LWRVES_H
#define INCLUDED_C25519_LWRVES_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <tegra_pka1_ec_param.h>

#ifndef HAVE_C25519_LWRVES
#define HAVE_C25519_LWRVES 1
#endif

/* C25519 Montgomery lwrve (EC on prime field Fp) =>
 *
 * y^2 mod p = x^3 + ax + x mod p
 *
 * where p=2^255 - 19 and a = 486662
 *
 * p = 2^255-19 =
 *  0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED
 *
 * The order-of-the-base-point (OBP) for this lwrve is:
 * 2^252 + 27742317777372353535851937790883648493 =
 *  0x1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed
 *
 * Only point multiplication is supported for this lwrve (e.g. for
 * "ECDH with C25519" which is also called X25519).
 *
 * R = key*Px mod p where (1 < key < OBP) and "Px is on the lwrve" (???)
 *
 * X25519 is used in situations where only the X coordinate is required,
 * hence the function only computes against X (Y is not required) and
 * only produces an output of X.
 *
 * If "key" is out of specs the op will fail with status of 66
 * (ILWALID_KEY).
 *
 * The "key" must be zero extended to the full power of 2 aligned (256
 * bit, 32 byte) register size.
 *
 * PKA1 value L (ceiling(log2(n))) for these lwrves is 253 !!!!!
 * (Special case: NIST and BP lwrves have value (nbytes*8), this is ((nbytes*8)+1) !
 */

/* lwrve25519 type definitions using the generic lwrve macros
 */
#define C25519_LWRVE_TYPE GENERIC_LWRVE_TYPE(c25519, 32)
#define C25519_LWRVE	  GENERIC_LWRVE(c25519)

C25519_LWRVE_TYPE;

/*
 * Field B does not exist for C25519 => not set here.
 * (Unset fields will be left zero by init)
 */
C25519_LWRVE = {
	.p = { 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed, },

	/* 486662 (XXX Is this field ever used?) */
	.a = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x6D, 0x06, },
	.x = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, },
	.y = { 0x20, 0xae, 0x19, 0xa1, 0xb8, 0xa0, 0x86, 0xb4,
	       0xe0, 0x1e, 0xdd, 0x2c, 0x77, 0x48, 0xd1, 0x4c,
	       0x92, 0x3d, 0x4d, 0x7e, 0x6d, 0x7c, 0x61, 0xb2,
	       0x29, 0xe9, 0xc5, 0xa2, 0x7e, 0xce, 0xd3, 0xd9, },

	/* (OBP) order-of-the-base-point set to n =>
	 * 2^252 + 27742317777372353535851937790883648493 =
	 *  7237005577332262213973186563042994240857116359379907606001950938285454250989 =
	 *  0x1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed
	 *
	 * ceiling(log2(n)) == 253
	 */
	.n = { 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x14, 0xDE, 0xF9, 0xDE, 0xA2, 0xF7, 0x9C, 0xD6,
	       0x58, 0x12, 0x63, 0x1A, 0x5C, 0xF5, 0xD3, 0xED, },
};

#endif /* INCLUDED_C25519_LWRVES_H */
