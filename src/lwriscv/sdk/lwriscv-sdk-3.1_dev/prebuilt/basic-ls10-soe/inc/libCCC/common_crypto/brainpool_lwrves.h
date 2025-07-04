/*
 * Header file for CheetAh Security Elliptic Engine
 *
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 */

#ifndef INCLUDED_BRAINPOOL_LWRVES_H
#define INCLUDED_BRAINPOOL_LWRVES_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <tegra_pka1_ec_param.h>

#ifndef HAVE_BRAINPOOL_LWRVES_ALL
#define HAVE_BRAINPOOL_LWRVES_ALL 0
#endif

#ifndef HAVE_BRAINPOOL_TWISTED_LWRVES_ALL
#define HAVE_BRAINPOOL_TWISTED_LWRVES_ALL 0
#endif

#ifndef BRAINPOOL_EC_USE_FIXED_MONTGOMERY
#define BRAINPOOL_EC_USE_FIXED_MONTGOMERY 1
#endif

#if BRAINPOOL_EC_USE_FIXED_MONTGOMERY

#define BP_R1_MONT_VALUE_OR_NULL(size, field)				\
	((0 == bp_p ## size ## r1_mont_valid) ? NULL : ec_bp_p ## size ## r1_mont.field)

#define BP_R1_MONT_FLAG(size)						\
	((0 == bp_p ## size ## r1_mont_valid) ? PKA1_LWRVE_FLAG_NONE : PKA1_LWRVE_FLAG_MONTGOMERY_OK)

#define BP_T1_MONT_VALUE_OR_NULL(size, field)				\
	((0 == bp_p ## size ## t1_mont_valid) ? NULL : ec_bp_p ## size ## t1_mont.field)

#define BP_T1_MONT_FLAG(size)						\
	((0 == bp_p ## size ## t1_mont_valid) ? PKA1_LWRVE_FLAG_NONE : PKA1_LWRVE_FLAG_MONTGOMERY_OK)

#define BP_LWRVE_MONT_NONE(size)   GENERIC_LWRVE_MONT_NONE(bp_p ## size ## r1)
#define BP_TWISTED_LWRVE_MONT_NONE(size)   GENERIC_LWRVE_MONT_NONE(bp_p ## size ## t1)

#else

/* Pre-computed montgomery values never used */
#define BP_R1_MONT_VALUE_OR_NULL(size, field) NULL
#define BP_R1_MONT_FLAG(size) PKA1_LWRVE_FLAG_NONE

#define BP_T1_MONT_VALUE_OR_NULL(size, field) NULL
#define BP_T1_MONT_FLAG(size) PKA1_LWRVE_FLAG_NONE

#define BP_LWRVE_MONT_NONE(size)
#define BP_TWISTED_LWRVE_MONT_NONE(size)

#endif /* BRAINPOOL_EC_USE_FIXED_MONTGOMERY */

#if HAVE_BRAINPOOL_LWRVES_ALL
#if CCC_EC_MIN_PRIME_BITS <= 160U
#define HAVE_BP_160 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 192U
#define HAVE_BP_192 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 224U
#define HAVE_BP_224 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 256U
#define HAVE_BP_256 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 320U
#define HAVE_BP_320 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 384U
#define HAVE_BP_384 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 512U
#define HAVE_BP_512 1
#endif
#endif /* HAVE_BRAINPOOL_LWRVES_ALL */

#if HAVE_BRAINPOOL_TWISTED_LWRVES_ALL
#if CCC_EC_MIN_PRIME_BITS <= 160U
#define HAVE_BPT_160 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 192U
#define HAVE_BPT_192 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 224U
#define HAVE_BPT_224 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 256U
#define HAVE_BPT_256 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 320U
#define HAVE_BPT_320 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 384U
#define HAVE_BPT_384 1
#endif
#if CCC_EC_MIN_PRIME_BITS <= 512U
#define HAVE_BPT_512 1
#endif
#endif /* HAVE_BRAINPOOL_TWISTED_LWRVES_ALL */

#ifndef HAVE_BP_160
#define HAVE_BP_160 0
#endif
#ifndef HAVE_BP_192
#define HAVE_BP_192 0
#endif
#ifndef HAVE_BP_224
#define HAVE_BP_224 0
#endif
#ifndef HAVE_BP_256
#define HAVE_BP_256 0
#endif
#ifndef HAVE_BP_320
#define HAVE_BP_320 0
#endif
#ifndef HAVE_BP_384
#define HAVE_BP_384 0
#endif
#ifndef HAVE_BP_512
#define HAVE_BP_512 0
#endif

#ifndef HAVE_BPT_160
#define HAVE_BPT_160 0
#endif
#ifndef HAVE_BPT_192
#define HAVE_BPT_192 0
#endif
#ifndef HAVE_BPT_224
#define HAVE_BPT_224 0
#endif
#ifndef HAVE_BPT_256
#define HAVE_BPT_256 0
#endif
#ifndef HAVE_BPT_320
#define HAVE_BPT_320 0
#endif
#ifndef HAVE_BPT_384
#define HAVE_BPT_384 0
#endif
#ifndef HAVE_BPT_512
#define HAVE_BPT_512 0
#endif

#if HAVE_LW_AUTOMOTIVE_SYSTEM
#if HAVE_BP_160 || HAVE_BP_192 || HAVE_BP_224 || HAVE_BPT_160 || HAVE_BPT_192 || HAVE_BPT_224
#error "LW automotive system must not enable BP[t]_160 .. BP[t]_224 lwrves"
#endif
#endif

/* Brainpool EC lwrve parameters from www.ecc-brainpool.org/download/Domain-parameters.pdf
 * [ Also in text form at www.ietf.org/rfc/rfc5639.txt ]
 *
 * Brainpool lwrves use short Weierstrass equation y^2 = x^3 + ax + b, where 4a^3+27b^2 != 0 in F_p
 *
 * Field Type: prime-field
 *
 * These values are in big endian byte arrays.
 *
 * Keep these word aligned; they are written to PKA1 bank registers as words.
 *
 * Twisted lwrve values from (https) safelwrves.cr.yp.to/ and from rfc5639
 *
 * PKA1 value L (ceiling(log2(n))) for these lwrves is identical to (lwrve->nbytes*8)
 */

/* Brainpool regular lwrve parameters (twisted lwrves defined at end of file)
 *
 * Brainpool lwrve type definitions using the generic lwrve macros
 *
 * size: lwrve bit length
 */
#define BP_LWRVE_TYPE(size)	   GENERIC_LWRVE_TYPE(bp_p ## size ## r1, (size / 8))

#define BP_LWRVE_MONT_TYPE(size)   GENERIC_LWRVE_MONT_TYPE(bp_p ## size ## r1, (size / 8))
#define BP_LWRVE_MONT_VALUES(size) GENERIC_LWRVE_MONT_VALUES(bp_p ## size ## r1)

#define BP_LWRVE(size)		   GENERIC_LWRVE(bp_p ## size ## r1)

#if HAVE_BP_160
/* brainpoolP160r1
 */
BP_LWRVE_TYPE(160);

BP_LWRVE(160) = {
   .p = { 0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC,
	  0x60, 0xDF, 0xC7, 0xAD, 0x95, 0xB3, 0xD8, 0x13,
	  0x95, 0x15, 0x62, 0x0F },
   .a = { 0x34, 0x0E, 0x7B, 0xE2, 0xA2, 0x80, 0xEB, 0x74,
	  0xE2, 0xBE, 0x61, 0xBA, 0xDA, 0x74, 0x5D, 0x97,
	  0xE8, 0xF7, 0xC3, 0x00 },
   .b = { 0x1E, 0x58, 0x9A, 0x85, 0x95, 0x42, 0x34, 0x12,
	  0x13, 0x4F, 0xAA, 0x2D, 0xBD, 0xEC, 0x95, 0xC8,
	  0xD8, 0x67, 0x5E, 0x58 },
   .x = { 0xBE, 0xD5, 0xAF, 0x16, 0xEA, 0x3F, 0x6A, 0x4F,
	  0x62, 0x93, 0x8C, 0x46, 0x31, 0xEB, 0x5A, 0xF7,
	  0xBD, 0xBC, 0xDB, 0xC3 },
   .y = { 0x16, 0x67, 0xCB, 0x47, 0x7A, 0x1A, 0x8E, 0xC3,
	  0x38, 0xF9, 0x47, 0x41, 0x66, 0x9C, 0x97, 0x63,
	  0x16, 0xDA, 0x63, 0x21 },
   .n = { 0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC,
	  0x60, 0xDF, 0x59, 0x91, 0xD4, 0x50, 0x29, 0x40,
	  0x9E, 0x60, 0xFC, 0x09 },
};

#define bp_p160r1_mont_valid 0
BP_LWRVE_MONT_NONE(160);

#endif /* HAVE_BP_160 */

#if HAVE_BP_192

/* brainpoolP192r1 */

BP_LWRVE_TYPE(192);

BP_LWRVE(192) = {
   .p = { 0xC3, 0x02, 0xF4, 0x1D, 0x93, 0x2A, 0x36, 0xCD,
	  0xA7, 0xA3, 0x46, 0x30, 0x93, 0xD1, 0x8D, 0xB7,
	  0x8F, 0xCE, 0x47, 0x6D, 0xE1, 0xA8, 0x62, 0x97, },
   .a = { 0x6A, 0x91, 0x17, 0x40, 0x76, 0xB1, 0xE0, 0xE1,
	  0x9C, 0x39, 0xC0, 0x31, 0xFE, 0x86, 0x85, 0xC1,
	  0xCA, 0xE0, 0x40, 0xE5, 0xC6, 0x9A, 0x28, 0xEF, },
   .b = { 0x46, 0x9A, 0x28, 0xEF, 0x7C, 0x28, 0xCC, 0xA3,
	  0xDC, 0x72, 0x1D, 0x04, 0x4F, 0x44, 0x96, 0xBC,
	  0xCA, 0x7E, 0xF4, 0x14, 0x6F, 0xBF, 0x25, 0xC9, },
   .x = { 0xC0, 0xA0, 0x64, 0x7E, 0xAA, 0xB6, 0xA4, 0x87,
	  0x53, 0xB0, 0x33, 0xC5, 0x6C, 0xB0, 0xF0, 0x90,
	  0x0A, 0x2F, 0x5C, 0x48, 0x53, 0x37, 0x5F, 0xD6, },
   .y = { 0x14, 0xB6, 0x90, 0x86, 0x6A, 0xBD, 0x5B, 0xB8,
	  0x8B, 0x5F, 0x48, 0x28, 0xC1, 0x49, 0x00, 0x02,
	  0xE6, 0x77, 0x3F, 0xA2, 0xFA, 0x29, 0x9B, 0x8F, },
   .n = { 0xC3, 0x02, 0xF4, 0x1D, 0x93, 0x2A, 0x36, 0xCD,
	  0xA7, 0xA3, 0x46, 0x2F, 0x9E, 0x9E, 0x91, 0x6B,
	  0x5B, 0xE8, 0xF1, 0x02, 0x9A, 0xC4, 0xAC, 0xC1, },
};

#define bp_p192r1_mont_valid 0
BP_LWRVE_MONT_NONE(192);

#endif /* HAVE_BP_192 */

#if HAVE_BP_224

/* brainpoolP224r1 */

BP_LWRVE_TYPE(224);

BP_LWRVE(224) = {
   .p = { 0xD7, 0xC1, 0x34, 0xAA, 0x26, 0x43, 0x66, 0x86,
	  0x2A, 0x18, 0x30, 0x25, 0x75, 0xD1, 0xD7, 0x87,
	  0xB0, 0x9F, 0x07, 0x57, 0x97, 0xDA, 0x89, 0xF5,
	  0x7E, 0xC8, 0xC0, 0xFF, },
   .a = { 0x68, 0xA5, 0xE6, 0x2C, 0xA9, 0xCE, 0x6C, 0x1C,
	  0x29, 0x98, 0x03, 0xA6, 0xC1, 0x53, 0x0B, 0x51,
	  0x4E, 0x18, 0x2A, 0xD8, 0xB0, 0x04, 0x2A, 0x59,
	  0xCA, 0xD2, 0x9F, 0x43, },
   .b = { 0x25, 0x80, 0xF6, 0x3C, 0xCF, 0xE4, 0x41, 0x38,
	  0x87, 0x07, 0x13, 0xB1, 0xA9, 0x23, 0x69, 0xE3,
	  0x3E, 0x21, 0x35, 0xD2, 0x66, 0xDB, 0xB3, 0x72,
	  0x38, 0x6C, 0x40, 0x0B, },
   .x = { 0x0D, 0x90, 0x29, 0xAD, 0x2C, 0x7E, 0x5C, 0xF4,
	  0x34, 0x08, 0x23, 0xB2, 0xA8, 0x7D, 0xC6, 0x8C,
	  0x9E, 0x4C, 0xE3, 0x17, 0x4C, 0x1E, 0x6E, 0xFD,
	  0xEE, 0x12, 0xC0, 0x7D, },
   .y = { 0x58, 0xAA, 0x56, 0xF7, 0x72, 0xC0, 0x72, 0x6F,
	  0x24, 0xC6, 0xB8, 0x9E, 0x4E, 0xCD, 0xAC, 0x24,
	  0x35, 0x4B, 0x9E, 0x99, 0xCA, 0xA3, 0xF6, 0xD3,
	  0x76, 0x14, 0x02, 0xCD, },
   .n = { 0xD7, 0xC1, 0x34, 0xAA, 0x26, 0x43, 0x66, 0x86,
	  0x2A, 0x18, 0x30, 0x25, 0x75, 0xD0, 0xFB, 0x98,
	  0xD1, 0x16, 0xBC, 0x4B, 0x6D, 0xDE, 0xBC, 0xA3,
	  0xA5, 0xA7, 0x93, 0x9F, },
};

#define bp_p224r1_mont_valid 0
BP_LWRVE_MONT_NONE(224);

#endif /* HAVE_BP_224 */

#if HAVE_BP_256

/* brainpoolP256r1
 * Cofactor: (i = 1)
 */
BP_LWRVE_TYPE(256);

BP_LWRVE(256) = {
   .p = { 0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	  0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x72,
	  0x6E, 0x3B, 0xF6, 0x23, 0xD5, 0x26, 0x20, 0x28,
	  0x20, 0x13, 0x48, 0x1D, 0x1F, 0x6E, 0x53, 0x77, },
   .a = { 0x7D, 0x5A, 0x09, 0x75, 0xFC, 0x2C, 0x30, 0x57,
	  0xEE, 0xF6, 0x75, 0x30, 0x41, 0x7A, 0xFF, 0xE7,
	  0xFB, 0x80, 0x55, 0xC1, 0x26, 0xDC, 0x5C, 0x6C,
	  0xE9, 0x4A, 0x4B, 0x44, 0xF3, 0x30, 0xB5, 0xD9, },
   .b = { 0x26, 0xDC, 0x5C, 0x6C, 0xE9, 0x4A, 0x4B, 0x44,
	  0xF3, 0x30, 0xB5, 0xD9, 0xBB, 0xD7, 0x7C, 0xBF,
	  0x95, 0x84, 0x16, 0x29, 0x5C, 0xF7, 0xE1, 0xCE,
	  0x6B, 0xCC, 0xDC, 0x18, 0xFF, 0x8C, 0x07, 0xB6, },
   .x = { 0x8B, 0xD2, 0xAE, 0xB9, 0xCB, 0x7E, 0x57, 0xCB,
	  0x2C, 0x4B, 0x48, 0x2F, 0xFC, 0x81, 0xB7, 0xAF,
	  0xB9, 0xDE, 0x27, 0xE1, 0xE3, 0xBD, 0x23, 0xC2,
	  0x3A, 0x44, 0x53, 0xBD, 0x9A, 0xCE, 0x32, 0x62, },
   .y = { 0x54, 0x7E, 0xF8, 0x35, 0xC3, 0xDA, 0xC4, 0xFD,
	  0x97, 0xF8, 0x46, 0x1A, 0x14, 0x61, 0x1D, 0xC9,
	  0xC2, 0x77, 0x45, 0x13, 0x2D, 0xED, 0x8E, 0x54,
	  0x5C, 0x1D, 0x54, 0xC7, 0x2F, 0x04, 0x69, 0x97, },
   .n = { 0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	  0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x71,
	  0x8C, 0x39, 0x7A, 0xA3, 0xB5, 0x61, 0xA6, 0xF7,
	  0x90, 0x1E, 0x0E, 0x82, 0x97, 0x48, 0x56, 0xA7, },
};

#if BRAINPOOL_EC_USE_FIXED_MONTGOMERY

BP_LWRVE_MONT_TYPE(256);

#define bp_p256r1_mont_valid 1

BP_LWRVE_MONT_VALUES(256) = {
   .m_prime = { 0xDB, 0x43, 0xF8, 0xE6, 0xD0, 0xBD, 0x5A, 0xF5,
		0xFD, 0xEC, 0xCE, 0xFB, 0x8A, 0xB1, 0xE8, 0x38,
		0xDA, 0xB9, 0xE6, 0xA2, 0x27, 0x73, 0xCA, 0x1F,
		0xC6, 0xA7, 0x55, 0x90, 0xCE, 0xFD, 0x89, 0xB9},
   .r2 = { 0x47, 0x17, 0xAA, 0x21, 0xE5, 0x95, 0x7F, 0xA8,
	   0xA1, 0xEC, 0xDA, 0xCD, 0x6B, 0x1A, 0xC8, 0x07,
	   0x5C, 0xCE, 0x4C, 0x26, 0x61, 0x4D, 0x4F, 0x4D,
	   0x8C, 0xFE, 0xDF, 0x7B, 0xA6, 0x46, 0x5B, 0x6C},
};

#else

#define bp_p256r1_mont_valid 0

#endif /* BRAINPOOL_EC_USE_FIXED_MONTGOMERY */

#endif /* HAVE_BP_256*/

#if HAVE_BP_320

/* brainpoolP320r1 */

BP_LWRVE_TYPE(320);

BP_LWRVE(320) = {
   .p = { 0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7,
	  0xE1, 0x3C, 0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65,
	  0xF9, 0x8F, 0xCF, 0xA6, 0xF6, 0xF4, 0x0D, 0xEF,
	  0x4F, 0x92, 0xB9, 0xEC, 0x78, 0x93, 0xEC, 0x28,
	  0xFC, 0xD4, 0x12, 0xB1, 0xF1, 0xB3, 0x2E, 0x27, },
   .a = { 0x3E, 0xE3, 0x0B, 0x56, 0x8F, 0xBA, 0xB0, 0xF8,
	  0x83, 0xCC, 0xEB, 0xD4, 0x6D, 0x3F, 0x3B, 0xB8,
	  0xA2, 0xA7, 0x35, 0x13, 0xF5, 0xEB, 0x79, 0xDA,
	  0x66, 0x19, 0x0E, 0xB0, 0x85, 0xFF, 0xA9, 0xF4,
	  0x92, 0xF3, 0x75, 0xA9, 0x7D, 0x86, 0x0E, 0xB4, },
   .b = { 0x52, 0x08, 0x83, 0x94, 0x9D, 0xFD, 0xBC, 0x42,
	  0xD3, 0xAD, 0x19, 0x86, 0x40, 0x68, 0x8A, 0x6F,
	  0xE1, 0x3F, 0x41, 0x34, 0x95, 0x54, 0xB4, 0x9A,
	  0xCC, 0x31, 0xDC, 0xCD, 0x88, 0x45, 0x39, 0x81,
	  0x6F, 0x5E, 0xB4, 0xAC, 0x8F, 0xB1, 0xF1, 0xA6, },
   .x = { 0x43, 0xBD, 0x7E, 0x9A, 0xFB, 0x53, 0xD8, 0xB8,
	  0x52, 0x89, 0xBC, 0xC4, 0x8E, 0xE5, 0xBF, 0xE6,
	  0xF2, 0x01, 0x37, 0xD1, 0x0A, 0x08, 0x7E, 0xB6,
	  0xE7, 0x87, 0x1E, 0x2A, 0x10, 0xA5, 0x99, 0xC7,
	  0x10, 0xAF, 0x8D, 0x0D, 0x39, 0xE2, 0x06, 0x11, },
   .y = { 0x14, 0xFD, 0xD0, 0x55, 0x45, 0xEC, 0x1C, 0xC8,
	  0xAB, 0x40, 0x93, 0x24, 0x7F, 0x77, 0x27, 0x5E,
	  0x07, 0x43, 0xFF, 0xED, 0x11, 0x71, 0x82, 0xEA,
	  0xA9, 0xC7, 0x78, 0x77, 0xAA, 0xAC, 0x6A, 0xC7,
	  0xD3, 0x52, 0x45, 0xD1, 0x69, 0x2E, 0x8E, 0xE1, },
   .n = { 0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7,
	  0xE1, 0x3C, 0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65,
	  0xF9, 0x8F, 0xCF, 0xA5, 0xB6, 0x8F, 0x12, 0xA3,
	  0x2D, 0x48, 0x2E, 0xC7, 0xEE, 0x86, 0x58, 0xE9,
	  0x86, 0x91, 0x55, 0x5B, 0x44, 0xC5, 0x93, 0x11, },
};

#define bp_p320r1_mont_valid 0
BP_LWRVE_MONT_NONE(320);

#endif /* HAVE_BP_320 */

#if HAVE_BP_384

/* brainpoolP384r1 */

BP_LWRVE_TYPE(384);

BP_LWRVE(384) = {
   .p = { 0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28,
	  0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF,
	  0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB4,
	  0x12, 0xB1, 0xDA, 0x19, 0x7F, 0xB7, 0x11, 0x23,
	  0xAC, 0xD3, 0xA7, 0x29, 0x90, 0x1D, 0x1A, 0x71,
	  0x87, 0x47, 0x00, 0x13, 0x31, 0x07, 0xEC, 0x53, },
   .a = { 0x7B, 0xC3, 0x82, 0xC6, 0x3D, 0x8C, 0x15, 0x0C,
	  0x3C, 0x72, 0x08, 0x0A, 0xCE, 0x05, 0xAF, 0xA0,
	  0xC2, 0xBE, 0xA2, 0x8E, 0x4F, 0xB2, 0x27, 0x87,
	  0x13, 0x91, 0x65, 0xEF, 0xBA, 0x91, 0xF9, 0x0F,
	  0x8A, 0xA5, 0x81, 0x4A, 0x50, 0x3A, 0xD4, 0xEB,
	  0x04, 0xA8, 0xC7, 0xDD, 0x22, 0xCE, 0x28, 0x26, },
   .b = { 0x04, 0xA8, 0xC7, 0xDD, 0x22, 0xCE, 0x28, 0x26,
	  0x8B, 0x39, 0xB5, 0x54, 0x16, 0xF0, 0x44, 0x7C,
	  0x2F, 0xB7, 0x7D, 0xE1, 0x07, 0xDC, 0xD2, 0xA6,
	  0x2E, 0x88, 0x0E, 0xA5, 0x3E, 0xEB, 0x62, 0xD5,
	  0x7C, 0xB4, 0x39, 0x02, 0x95, 0xDB, 0xC9, 0x94,
	  0x3A, 0xB7, 0x86, 0x96, 0xFA, 0x50, 0x4C, 0x11, },
   .x = { 0x1D, 0x1C, 0x64, 0xF0, 0x68, 0xCF, 0x45, 0xFF,
	  0xA2, 0xA6, 0x3A, 0x81, 0xB7, 0xC1, 0x3F, 0x6B,
	  0x88, 0x47, 0xA3, 0xE7, 0x7E, 0xF1, 0x4F, 0xE3,
	  0xDB, 0x7F, 0xCA, 0xFE, 0x0C, 0xBD, 0x10, 0xE8,
	  0xE8, 0x26, 0xE0, 0x34, 0x36, 0xD6, 0x46, 0xAA,
	  0xEF, 0x87, 0xB2, 0xE2, 0x47, 0xD4, 0xAF, 0x1E, },
   .y = { 0x8A, 0xBE, 0x1D, 0x75, 0x20, 0xF9, 0xC2, 0xA4,
	  0x5C, 0xB1, 0xEB, 0x8E, 0x95, 0xCF, 0xD5, 0x52,
	  0x62, 0xB7, 0x0B, 0x29, 0xFE, 0xEC, 0x58, 0x64,
	  0xE1, 0x9C, 0x05, 0x4F, 0xF9, 0x91, 0x29, 0x28,
	  0x0E, 0x46, 0x46, 0x21, 0x77, 0x91, 0x81, 0x11,
	  0x42, 0x82, 0x03, 0x41, 0x26, 0x3C, 0x53, 0x15, },
   .n = { 0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28,
	  0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF,
	  0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB3,
	  0x1F, 0x16, 0x6E, 0x6C, 0xAC, 0x04, 0x25, 0xA7,
	  0xCF, 0x3A, 0xB6, 0xAF, 0x6B, 0x7F, 0xC3, 0x10,
	  0x3B, 0x88, 0x32, 0x02, 0xE9, 0x04, 0x65, 0x65, },
};

#define bp_p384r1_mont_valid 0
BP_LWRVE_MONT_NONE(384);

#endif /* HAVE_BP_384 */

#if HAVE_BP_512

/* brainpoolP512r1 */

BP_LWRVE_TYPE(512);

BP_LWRVE(512) = {
   .p = { 0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B,
	  0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07,
	  0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E,
	  0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x71,
	  0x7D, 0x4D, 0x9B, 0x00, 0x9B, 0xC6, 0x68, 0x42,
	  0xAE, 0xCD, 0xA1, 0x2A, 0xE6, 0xA3, 0x80, 0xE6,
	  0x28, 0x81, 0xFF, 0x2F, 0x2D, 0x82, 0xC6, 0x85,
	  0x28, 0xAA, 0x60, 0x56, 0x58, 0x3A, 0x48, 0xF3, },
   .a = { 0x78, 0x30, 0xA3, 0x31, 0x8B, 0x60, 0x3B, 0x89,
	  0xE2, 0x32, 0x71, 0x45, 0xAC, 0x23, 0x4C, 0xC5,
	  0x94, 0xCB, 0xDD, 0x8D, 0x3D, 0xF9, 0x16, 0x10,
	  0xA8, 0x34, 0x41, 0xCA, 0xEA, 0x98, 0x63, 0xBC,
	  0x2D, 0xED, 0x5D, 0x5A, 0xA8, 0x25, 0x3A, 0xA1,
	  0x0A, 0x2E, 0xF1, 0xC9, 0x8B, 0x9A, 0xC8, 0xB5,
	  0x7F, 0x11, 0x17, 0xA7, 0x2B, 0xF2, 0xC7, 0xB9,
	  0xE7, 0xC1, 0xAC, 0x4D, 0x77, 0xFC, 0x94, 0xCA, },
   .b = { 0x3D, 0xF9, 0x16, 0x10, 0xA8, 0x34, 0x41, 0xCA,
	  0xEA, 0x98, 0x63, 0xBC, 0x2D, 0xED, 0x5D, 0x5A,
	  0xA8, 0x25, 0x3A, 0xA1, 0x0A, 0x2E, 0xF1, 0xC9,
	  0x8B, 0x9A, 0xC8, 0xB5, 0x7F, 0x11, 0x17, 0xA7,
	  0x2B, 0xF2, 0xC7, 0xB9, 0xE7, 0xC1, 0xAC, 0x4D,
	  0x77, 0xFC, 0x94, 0xCA, 0xDC, 0x08, 0x3E, 0x67,
	  0x98, 0x40, 0x50, 0xB7, 0x5E, 0xBA, 0xE5, 0xDD,
	  0x28, 0x09, 0xBD, 0x63, 0x80, 0x16, 0xF7, 0x23, },
   .x = { 0x81, 0xAE, 0xE4, 0xBD, 0xD8, 0x2E, 0xD9, 0x64,
	  0x5A, 0x21, 0x32, 0x2E, 0x9C, 0x4C, 0x6A, 0x93,
	  0x85, 0xED, 0x9F, 0x70, 0xB5, 0xD9, 0x16, 0xC1,
	  0xB4, 0x3B, 0x62, 0xEE, 0xF4, 0xD0, 0x09, 0x8E,
	  0xFF, 0x3B, 0x1F, 0x78, 0xE2, 0xD0, 0xD4, 0x8D,
	  0x50, 0xD1, 0x68, 0x7B, 0x93, 0xB9, 0x7D, 0x5F,
	  0x7C, 0x6D, 0x50, 0x47, 0x40, 0x6A, 0x5E, 0x68,
	  0x8B, 0x35, 0x22, 0x09, 0xBC, 0xB9, 0xF8, 0x22, },
   .y = { 0x7D, 0xDE, 0x38, 0x5D, 0x56, 0x63, 0x32, 0xEC,
	  0xC0, 0xEA, 0xBF, 0xA9, 0xCF, 0x78, 0x22, 0xFD,
	  0xF2, 0x09, 0xF7, 0x00, 0x24, 0xA5, 0x7B, 0x1A,
	  0xA0, 0x00, 0xC5, 0x5B, 0x88, 0x1F, 0x81, 0x11,
	  0xB2, 0xDC, 0xDE, 0x49, 0x4A, 0x5F, 0x48, 0x5E,
	  0x5B, 0xCA, 0x4B, 0xD8, 0x8A, 0x27, 0x63, 0xAE,
	  0xD1, 0xCA, 0x2B, 0x2F, 0xA8, 0xF0, 0x54, 0x06,
	  0x78, 0xCD, 0x1E, 0x0F, 0x3A, 0xD8, 0x08, 0x92, },
   .n = { 0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B,
	  0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07,
	  0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E,
	  0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x70,
	  0x55, 0x3E, 0x5C, 0x41, 0x4C, 0xA9, 0x26, 0x19,
	  0x41, 0x86, 0x61, 0x19, 0x7F, 0xAC, 0x10, 0x47,
	  0x1D, 0xB1, 0xD3, 0x81, 0x08, 0x5D, 0xDA, 0xDD,
	  0xB5, 0x87, 0x96, 0x82, 0x9C, 0xA9, 0x00, 0x69, },
};

#define bp_p512r1_mont_valid 0
BP_LWRVE_MONT_NONE(512);

#endif /* HAVE_BP_512 */

/*
 * Brainpool twisted lwrve parameters
 */
#if HAVE_BRAINPOOL_TWISTED_LWRVES

#define BP_TWISTED_LWRVE_TYPE(size)	   GENERIC_LWRVE_TYPE(bp_p ## size ## t1, (size / 8))
#define BP_TWISTED_LWRVE_MONT_TYPE(size)   GENERIC_LWRVE_MONT_TYPE(bp_p ## size ## t1, (size / 8))
#define BP_TWISTED_LWRVE(size)		   GENERIC_LWRVE(bp_p ## size ## t1)
#define BP_TWISTED_LWRVE_MONT_VALUES(size) GENERIC_LWRVE_MONT_VALUES(bp_p ## size ## t1)

#if HAVE_BPT_160

/* brainpoolP160t1 (twisted lwrve)
 *
 * Z: 24DBFF5DEC9B986BBFE5295A29BFBAE45E0F5D0B
 */
BP_TWISTED_LWRVE_TYPE(160);

BP_TWISTED_LWRVE(160) = {
   .p = { 0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC,
	  0x60, 0xDF, 0xC7, 0xAD, 0x95, 0xB3, 0xD8, 0x13,
	  0x95, 0x15, 0x62, 0x0F },
   .a = { 0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC,
	  0x60, 0xDF, 0xC7, 0xAD, 0x95, 0xB3, 0xD8, 0x13,
	  0x95, 0x15, 0x62, 0x0C, },
   .b = { 0x7A, 0x55, 0x6B, 0x6D, 0xAE, 0x53, 0x5B, 0x7B,
	  0x51, 0xED, 0x2C, 0x4D, 0x7D, 0xAA, 0x7A, 0x0B,
	  0x5C, 0x55, 0xF3, 0x80, },
   .x = { 0xB1, 0x99, 0xB1, 0x3B, 0x9B, 0x34, 0xEF, 0xC1,
	  0x39, 0x7E, 0x64, 0xBA, 0xEB, 0x05, 0xAC, 0xC2,
	  0x65, 0xFF, 0x23, 0x78, },
   .y = { 0xAD, 0xD6, 0x71, 0x8B, 0x7C, 0x7C, 0x19, 0x61,
	  0xF0, 0x99, 0x1B, 0x84, 0x24, 0x43, 0x77, 0x21,
	  0x52, 0xC9, 0xE0, 0xAD, },
   .n = { 0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC,
	  0x60, 0xDF, 0x59, 0x91, 0xD4, 0x50, 0x29, 0x40,
	  0x9E, 0x60, 0xFC, 0x09 },
};

#define bp_p160t1_mont_valid 0
BP_TWISTED_LWRVE_MONT_NONE(160);

#endif /* HAVE_BPT_160 */

#if HAVE_BPT_192
/* brainpoolP192t1
 * Z = 1B6F5CC8DB4DC7AF19458A9CB80DC2295E5EB9C3732104CB
 */
BP_TWISTED_LWRVE_TYPE(192);

BP_TWISTED_LWRVE(192) = {
   .p = { 0xC3, 0x02, 0xF4, 0x1D, 0x93, 0x2A, 0x36, 0xCD,
	  0xA7, 0xA3, 0x46, 0x30, 0x93, 0xD1, 0x8D, 0xB7,
	  0x8F, 0xCE, 0x47, 0x6D, 0xE1, 0xA8, 0x62, 0x97, },
   .a = { 0xC3, 0x02, 0xF4, 0x1D, 0x93, 0x2A, 0x36, 0xCD,
	  0xA7, 0xA3, 0x46, 0x30, 0x93, 0xD1, 0x8D, 0xB7,
	  0x8F, 0xCE, 0x47, 0x6D, 0xE1, 0xA8, 0x62, 0x94, },
   .b = { 0x13, 0xD5, 0x6F, 0xFA, 0xEC, 0x78, 0x68, 0x1E,
	  0x68, 0xF9, 0xDE, 0xB4, 0x3B, 0x35, 0xBE, 0xC2,
	  0xFB, 0x68, 0x54, 0x2E, 0x27, 0x89, 0x7B, 0x79, },
   .x = { 0x3A, 0xE9, 0xE5, 0x8C, 0x82, 0xF6, 0x3C, 0x30,
	  0x28, 0x2E, 0x1F, 0xE7, 0xBB, 0xF4, 0x3F, 0xA7,
	  0x2C, 0x44, 0x6A, 0xF6, 0xF4, 0x61, 0x81, 0x29, },
   .y = { 0x09, 0x7E, 0x2C, 0x56, 0x67, 0xC2, 0x22, 0x3A,
	  0x90, 0x2A, 0xB5, 0xCA, 0x44, 0x9D, 0x00, 0x84,
	  0xB7, 0xE5, 0xB3, 0xDE, 0x7C, 0xCC, 0x01, 0xC9, },
   .n = { 0xC3, 0x02, 0xF4, 0x1D, 0x93, 0x2A, 0x36, 0xCD,
	  0xA7, 0xA3, 0x46, 0x2F, 0x9E, 0x9E, 0x91, 0x6B,
	  0x5B, 0xE8, 0xF1, 0x02, 0x9A, 0xC4, 0xAC, 0xC1, },
};

#define bp_p192t1_mont_valid 0
BP_TWISTED_LWRVE_MONT_NONE(192);

#endif /* HAVE_BPT_192 */

#if HAVE_BPT_224
/*
 * Lwrve-ID: brainpoolP224t1 (twisted lwrve)
 * Cofactor: (i = 1)
 *
 * Z = 2DF271E14427A346910CF7A2E6CFA7B3F484E5C2CCE1C8B730E28B3F
 */
BP_TWISTED_LWRVE_TYPE(224);

BP_TWISTED_LWRVE(224) = {
   .p = { 0xD7, 0xC1, 0x34, 0xAA, 0x26, 0x43, 0x66, 0x86,
	  0x2A, 0x18, 0x30, 0x25, 0x75, 0xD1, 0xD7, 0x87,
	  0xB0, 0x9F, 0x07, 0x57, 0x97, 0xDA, 0x89, 0xF5,
	  0x7E, 0xC8, 0xC0, 0xFF, },
   .a = { 0xD7, 0xC1, 0x34, 0xAA, 0x26, 0x43, 0x66, 0x86,
	  0x2A, 0x18, 0x30, 0x25, 0x75, 0xD1, 0xD7, 0x87,
	  0xB0, 0x9F, 0x07, 0x57, 0x97, 0xDA, 0x89, 0xF5,
	  0x7E, 0xC8, 0xC0, 0xFC, },
   .b = { 0x4B, 0x33, 0x7D, 0x93, 0x41, 0x04, 0xCD, 0x7B,
	  0xEF, 0x27, 0x1B, 0xF6, 0x0C, 0xED, 0x1E, 0xD2,
	  0x0D, 0xA1, 0x4C, 0x08, 0xB3, 0xBB, 0x64, 0xF1,
	  0x8A, 0x60, 0x88, 0x8D, },
   .x = { 0x6A, 0xB1, 0xE3, 0x44, 0xCE, 0x25, 0xFF, 0x38,
	  0x96, 0x42, 0x4E, 0x7F, 0xFE, 0x14, 0x76, 0x2E,
	  0xCB, 0x49, 0xF8, 0x92, 0x8A, 0xC0, 0xC7, 0x60,
	  0x29, 0xB4, 0xD5, 0x80, },
   .y = { 0x03, 0x74, 0xE9, 0xF5, 0x14, 0x3E, 0x56, 0x8C,
	  0xD2, 0x3F, 0x3F, 0x4D, 0x7C, 0x0D, 0x4B, 0x1E,
	  0x41, 0xC8, 0xCC, 0x0D, 0x1C, 0x6A, 0xBD, 0x5F,
	  0x1A, 0x46, 0xDB, 0x4C, },
   .n = { 0xD7, 0xC1, 0x34, 0xAA, 0x26, 0x43, 0x66, 0x86,
	  0x2A, 0x18, 0x30, 0x25, 0x75, 0xD0, 0xFB, 0x98,
	  0xD1, 0x16, 0xBC, 0x4B, 0x6D, 0xDE, 0xBC, 0xA3,
	  0xA5, 0xA7, 0x93, 0x9F, },
};

#define bp_p224t1_mont_valid 0
BP_TWISTED_LWRVE_MONT_NONE(224);

#endif /* HAVE_BPT_224 */

#if HAVE_BPT_256
/* brainpoolP256t1 (twisted lwrve)
 *
 * Z: 3E2D4BD9597B58639AE7AA669CAB9837CF5CF20A2C852D10F655668DFC150EF0
 */

BP_TWISTED_LWRVE_TYPE(256);

BP_TWISTED_LWRVE(256) = {
   .p = { 0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	  0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x72,
	  0x6E, 0x3B, 0xF6, 0x23, 0xD5, 0x26, 0x20, 0x28,
	  0x20, 0x13, 0x48, 0x1D, 0x1F, 0x6E, 0x53, 0x77, },
   .a = { 0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	  0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x72,
	  0x6E, 0x3B, 0xF6, 0x23, 0xD5, 0x26, 0x20, 0x28,
	  0x20, 0x13, 0x48, 0x1D, 0x1F, 0x6E, 0x53, 0x74, },
   .b = { 0x66, 0x2C, 0x61, 0xC4, 0x30, 0xD8, 0x4E, 0xA4,
	  0xFE, 0x66, 0xA7, 0x73, 0x3D, 0x0B, 0x76, 0xB7,
	  0xBF, 0x93, 0xEB, 0xC4, 0xAF, 0x2F, 0x49, 0x25,
	  0x6A, 0xE5, 0x81, 0x01, 0xFE, 0xE9, 0x2B, 0x04, },
   .x = { 0xA3, 0xE8, 0xEB, 0x3C, 0xC1, 0xCF, 0xE7, 0xB7,
	  0x73, 0x22, 0x13, 0xB2, 0x3A, 0x65, 0x61, 0x49,
	  0xAF, 0xA1, 0x42, 0xC4, 0x7A, 0xAF, 0xBC, 0x2B,
	  0x79, 0xA1, 0x91, 0x56, 0x2E, 0x13, 0x05, 0xF4, },
   .y = { 0x2D, 0x99, 0x6C, 0x82, 0x34, 0x39, 0xC5, 0x6D,
	  0x7F, 0x7B, 0x22, 0xE1, 0x46, 0x44, 0x41, 0x7E,
	  0x69, 0xBC, 0xB6, 0xDE, 0x39, 0xD0, 0x27, 0x00,
	  0x1D, 0xAB, 0xE8, 0xF3, 0x5B, 0x25, 0xC9, 0xBE, },
   .n = { 0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	  0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x71,
	  0x8C, 0x39, 0x7A, 0xA3, 0xB5, 0x61, 0xA6, 0xF7,
	  0x90, 0x1E, 0x0E, 0x82, 0x97, 0x48, 0x56, 0xA7, },
};

#if BRAINPOOL_EC_USE_FIXED_MONTGOMERY

BP_TWISTED_LWRVE_MONT_TYPE(256);

#define bp_p256t1_mont_valid 1

BP_TWISTED_LWRVE_MONT_VALUES(256) = {
   .m_prime = { 0xDB, 0x43, 0xF8, 0xE6, 0xD0, 0xBD, 0x5A, 0xF5,
		0xFD, 0xEC, 0xCE, 0xFB, 0x8A, 0xB1, 0xE8, 0x38,
		0xDA, 0xB9, 0xE6, 0xA2, 0x27, 0x73, 0xCA, 0x1F,
		0xC6, 0xA7, 0x55, 0x90, 0xCE, 0xFD, 0x89, 0xB9, },
   .r2 = { 0x47, 0x17, 0xAA, 0x21, 0xE5, 0x95, 0x7F, 0xA8,
	   0xA1, 0xEC, 0xDA, 0xCD, 0x6B, 0x1A, 0xC8, 0x07,
	   0x5C, 0xCE, 0x4C, 0x26, 0x61, 0x4D, 0x4F, 0x4D,
	   0x8C, 0xFE, 0xDF, 0x7B, 0xA6, 0x46, 0x5B, 0x6C, },
};

#else

#define bp_p256t1_mont_valid 0

#endif /* BRAINPOOL_EC_USE_FIXED_MONTGOMERY */
#endif /* HAVE_BPT_256 */

#if HAVE_BPT_320
/* brainpoolP320t1 (Twisted lwrve)
 * Z: 15F75CAF668077F7E85B42EB01F0A81FF56ECD6191D55CB82B7D861458A18FEF
 *    C3E5AB7496F3C7B1
 */
BP_TWISTED_LWRVE_TYPE(320);

BP_TWISTED_LWRVE(320) = {
   .p = { 0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7,
	  0xE1, 0x3C, 0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65,
	  0xF9, 0x8F, 0xCF, 0xA6, 0xF6, 0xF4, 0x0D, 0xEF,
	  0x4F, 0x92, 0xB9, 0xEC, 0x78, 0x93, 0xEC, 0x28,
	  0xFC, 0xD4, 0x12, 0xB1, 0xF1, 0xB3, 0x2E, 0x27, },
   .a = { 0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7,
	  0xE1, 0x3C, 0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65,
	  0xF9, 0x8F, 0xCF, 0xA6, 0xF6, 0xF4, 0x0D, 0xEF,
	  0x4F, 0x92, 0xB9, 0xEC, 0x78, 0x93, 0xEC, 0x28,
	  0xFC, 0xD4, 0x12, 0xB1, 0xF1, 0xB3, 0x2E, 0x24, },
   .b = { 0xA7, 0xF5, 0x61, 0xE0, 0x38, 0xEB, 0x1E, 0xD5,
	  0x60, 0xB3, 0xD1, 0x47, 0xDB, 0x78, 0x20, 0x13,
	  0x06, 0x4C, 0x19, 0xF2, 0x7E, 0xD2, 0x7C, 0x67,
	  0x80, 0xAA, 0xF7, 0x7F, 0xB8, 0xA5, 0x47, 0xCE,
	  0xB5, 0xB4, 0xFE, 0xF4, 0x22, 0x34, 0x03, 0x53, },
   .x = { 0x92, 0x5B, 0xE9, 0xFB, 0x01, 0xAF, 0xC6, 0xFB,
	  0x4D, 0x3E, 0x7D, 0x49, 0x90, 0x01, 0x0F, 0x81,
	  0x34, 0x08, 0xAB, 0x10, 0x6C, 0x4F, 0x09, 0xCB,
	  0x7E, 0xE0, 0x78, 0x68, 0xCC, 0x13, 0x6F, 0xFF,
	  0x33, 0x57, 0xF6, 0x24, 0xA2, 0x1B, 0xED, 0x52, },
   .y = { 0x63, 0xBA, 0x3A, 0x7A, 0x27, 0x48, 0x3E, 0xBF,
	  0x66, 0x71, 0xDB, 0xEF, 0x7A, 0xBB, 0x30, 0xEB,
	  0xEE, 0x08, 0x4E, 0x58, 0xA0, 0xB0, 0x77, 0xAD,
	  0x42, 0xA5, 0xA0, 0x98, 0x9D, 0x1E, 0xE7, 0x1B,
	  0x1B, 0x9B, 0xC0, 0x45, 0x5F, 0xB0, 0xD2, 0xC3, },
   .n = { 0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7,
	  0xE1, 0x3C, 0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65,
	  0xF9, 0x8F, 0xCF, 0xA5, 0xB6, 0x8F, 0x12, 0xA3,
	  0x2D, 0x48, 0x2E, 0xC7, 0xEE, 0x86, 0x58, 0xE9,
	  0x86, 0x91, 0x55, 0x5B, 0x44, 0xC5, 0x93, 0x11, },
};

#define bp_p320t1_mont_valid 0
BP_TWISTED_LWRVE_MONT_NONE(320);

#endif /* HAVE_BPT_320 */

#if HAVE_BPT_384
/* brainpoolP384t1 (Twisted lwrve)
 * Z: 41DFE8DD399331F7166A66076734A89CD0D2BCDB7D068E44E1F378F41ECBAE97
 *    D2D63DBC87BCCDDCCC5DA39E8589291C
 */
BP_TWISTED_LWRVE_TYPE(384);

BP_TWISTED_LWRVE(384) = {
   .p = { 0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28,
	  0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF,
	  0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB4,
	  0x12, 0xB1, 0xDA, 0x19, 0x7F, 0xB7, 0x11, 0x23,
	  0xAC, 0xD3, 0xA7, 0x29, 0x90, 0x1D, 0x1A, 0x71,
	  0x87, 0x47, 0x00, 0x13, 0x31, 0x07, 0xEC, 0x53, },
   .a = { 0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28,
	  0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF,
	  0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB4,
	  0x12, 0xB1, 0xDA, 0x19, 0x7F, 0xB7, 0x11, 0x23,
	  0xAC, 0xD3, 0xA7, 0x29, 0x90, 0x1D, 0x1A, 0x71,
	  0x87, 0x47, 0x00, 0x13, 0x31, 0x07, 0xEC, 0x50, },
   .b = { 0x7F, 0x51, 0x9E, 0xAD, 0xA7, 0xBD, 0xA8, 0x1B,
	  0xD8, 0x26, 0xDB, 0xA6, 0x47, 0x91, 0x0F, 0x8C,
	  0x4B, 0x93, 0x46, 0xED, 0x8C, 0xCD, 0xC6, 0x4E,
	  0x4B, 0x1A, 0xBD, 0x11, 0x75, 0x6D, 0xCE, 0x1D,
	  0x20, 0x74, 0xAA, 0x26, 0x3B, 0x88, 0x80, 0x5C,
	  0xED, 0x70, 0x35, 0x5A, 0x33, 0xB4, 0x71, 0xEE, },
   .x = { 0x18, 0xDE, 0x98, 0xB0, 0x2D, 0xB9, 0xA3, 0x06,
	  0xF2, 0xAF, 0xCD, 0x72, 0x35, 0xF7, 0x2A, 0x81,
	  0x9B, 0x80, 0xAB, 0x12, 0xEB, 0xD6, 0x53, 0x17,
	  0x24, 0x76, 0xFE, 0xCD, 0x46, 0x2A, 0xAB, 0xFF,
	  0xC4, 0xFF, 0x19, 0x1B, 0x94, 0x6A, 0x5F, 0x54,
	  0xD8, 0xD0, 0xAA, 0x2F, 0x41, 0x88, 0x08, 0xCC, },
   .y = { 0x25, 0xAB, 0x05, 0x69, 0x62, 0xD3, 0x06, 0x51,
	  0xA1, 0x14, 0xAF, 0xD2, 0x75, 0x5A, 0xD3, 0x36,
	  0x74, 0x7F, 0x93, 0x47, 0x5B, 0x7A, 0x1F, 0xCA,
	  0x3B, 0x88, 0xF2, 0xB6, 0xA2, 0x08, 0xCC, 0xFE,
	  0x46, 0x94, 0x08, 0x58, 0x4D, 0xC2, 0xB2, 0x91,
	  0x26, 0x75, 0xBF, 0x5B, 0x9E, 0x58, 0x29, 0x28, },
   .n = { 0x8C, 0xB9, 0x1E, 0x82, 0xA3, 0x38, 0x6D, 0x28,
	  0x0F, 0x5D, 0x6F, 0x7E, 0x50, 0xE6, 0x41, 0xDF,
	  0x15, 0x2F, 0x71, 0x09, 0xED, 0x54, 0x56, 0xB3,
	  0x1F, 0x16, 0x6E, 0x6C, 0xAC, 0x04, 0x25, 0xA7,
	  0xCF, 0x3A, 0xB6, 0xAF, 0x6B, 0x7F, 0xC3, 0x10,
	  0x3B, 0x88, 0x32, 0x02, 0xE9, 0x04, 0x65, 0x65, },
};

#define bp_p384t1_mont_valid 0
BP_TWISTED_LWRVE_MONT_NONE(384);

#endif /* HAVE_BPT_384 */

#if HAVE_BPT_512
/* brainpoolP512r1 (Twisted lwrve)
 * Z: 12EE58E6764838B69782136F0F2D3BA06E27695716054092E60A80BEDB212B64
 *    E585D90BCE13761F85C3F1D2A64E3BE8FEA2220F01EBA5EEB0F35DBD29D922AB
 */
BP_TWISTED_LWRVE_TYPE(512);

BP_TWISTED_LWRVE(512) = {
   .p = { 0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B,
	  0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07,
	  0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E,
	  0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x71,
	  0x7D, 0x4D, 0x9B, 0x00, 0x9B, 0xC6, 0x68, 0x42,
	  0xAE, 0xCD, 0xA1, 0x2A, 0xE6, 0xA3, 0x80, 0xE6,
	  0x28, 0x81, 0xFF, 0x2F, 0x2D, 0x82, 0xC6, 0x85,
	  0x28, 0xAA, 0x60, 0x56, 0x58, 0x3A, 0x48, 0xF3, },
   .a = { 0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B,
	  0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07,
	  0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E,
	  0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x71,
	  0x7D, 0x4D, 0x9B, 0x00, 0x9B, 0xC6, 0x68, 0x42,
	  0xAE, 0xCD, 0xA1, 0x2A, 0xE6, 0xA3, 0x80, 0xE6,
	  0x28, 0x81, 0xFF, 0x2F, 0x2D, 0x82, 0xC6, 0x85,
	  0x28, 0xAA, 0x60, 0x56, 0x58, 0x3A, 0x48, 0xF0, },
   .b = { 0x7C, 0xBB, 0xBC, 0xF9, 0x44, 0x1C, 0xFA, 0xB7,
	  0x6E, 0x18, 0x90, 0xE4, 0x68, 0x84, 0xEA, 0xE3,
	  0x21, 0xF7, 0x0C, 0x0B, 0xCB, 0x49, 0x81, 0x52,
	  0x78, 0x97, 0x50, 0x4B, 0xEC, 0x3E, 0x36, 0xA6,
	  0x2B, 0xCD, 0xFA, 0x23, 0x04, 0x97, 0x65, 0x40,
	  0xF6, 0x45, 0x00, 0x85, 0xF2, 0xDA, 0xE1, 0x45,
	  0xC2, 0x25, 0x53, 0xB4, 0x65, 0x76, 0x36, 0x89,
	  0x18, 0x0E, 0xA2, 0x57, 0x18, 0x67, 0x42, 0x3E, },
   .x = { 0x64, 0x0E, 0xCE, 0x5C, 0x12, 0x78, 0x87, 0x17,
	  0xB9, 0xC1, 0xBA, 0x06, 0xCB, 0xC2, 0xA6, 0xFE,
	  0xBA, 0x85, 0x84, 0x24, 0x58, 0xC5, 0x6D, 0xDE,
	  0x9D, 0xB1, 0x75, 0x8D, 0x39, 0xC0, 0x31, 0x3D,
	  0x82, 0xBA, 0x51, 0x73, 0x5C, 0xDB, 0x3E, 0xA4,
	  0x99, 0xAA, 0x77, 0xA7, 0xD6, 0x94, 0x3A, 0x64,
	  0xF7, 0xA3, 0xF2, 0x5F, 0xE2, 0x6F, 0x06, 0xB5,
	  0x1B, 0xAA, 0x26, 0x96, 0xFA, 0x90, 0x35, 0xDA, },
   .y = { 0x5B, 0x53, 0x4B, 0xD5, 0x95, 0xF5, 0xAF, 0x0F,
	  0xA2, 0xC8, 0x92, 0x37, 0x6C, 0x84, 0xAC, 0xE1,
	  0xBB, 0x4E, 0x30, 0x19, 0xB7, 0x16, 0x34, 0xC0,
	  0x11, 0x31, 0x15, 0x9C, 0xAE, 0x03, 0xCE, 0xE9,
	  0xD9, 0x93, 0x21, 0x84, 0xBE, 0xEF, 0x21, 0x6B,
	  0xD7, 0x1D, 0xF2, 0xDA, 0xDF, 0x86, 0xA6, 0x27,
	  0x30, 0x6E, 0xCF, 0xF9, 0x6D, 0xBB, 0x8B, 0xAC,
	  0xE1, 0x98, 0xB6, 0x1E, 0x00, 0xF8, 0xB3, 0x32, },
   .n = { 0xAA, 0xDD, 0x9D, 0xB8, 0xDB, 0xE9, 0xC4, 0x8B,
	  0x3F, 0xD4, 0xE6, 0xAE, 0x33, 0xC9, 0xFC, 0x07,
	  0xCB, 0x30, 0x8D, 0xB3, 0xB3, 0xC9, 0xD2, 0x0E,
	  0xD6, 0x63, 0x9C, 0xCA, 0x70, 0x33, 0x08, 0x70,
	  0x55, 0x3E, 0x5C, 0x41, 0x4C, 0xA9, 0x26, 0x19,
	  0x41, 0x86, 0x61, 0x19, 0x7F, 0xAC, 0x10, 0x47,
	  0x1D, 0xB1, 0xD3, 0x81, 0x08, 0x5D, 0xDA, 0xDD,
	  0xB5, 0x87, 0x96, 0x82, 0x9C, 0xA9, 0x00, 0x69, },
};

#if BRAINPOOL_EC_USE_FIXED_MONTGOMERY

BP_TWISTED_LWRVE_MONT_TYPE(512);

#define bp_p512t1_mont_valid 1

BP_TWISTED_LWRVE_MONT_VALUES(512) = {
   .m_prime = { 0x4C, 0x39, 0x8E, 0x09, 0x22, 0x96, 0x3F, 0x73,
		0xC8, 0x2B, 0x46, 0xFB, 0xFA, 0xAE, 0xA0, 0x06,
		0x36, 0x82, 0xA0, 0x85, 0xD4, 0xF0, 0x55, 0x03,
		0x16, 0x09, 0x8B, 0x3E, 0x75, 0xC2, 0x05, 0x72,
		0xAE, 0xF7, 0x52, 0x8B, 0xC5, 0xA7, 0xE2, 0x43,
		0x50, 0x65, 0x2E, 0xC0, 0x81, 0x5C, 0xFB, 0xCD,
		0xED, 0xAA, 0x1D, 0x85, 0xA2, 0xCD, 0x1E, 0x7C,
		0x83, 0x9B, 0x32, 0x20, 0x7D, 0x89, 0xEF, 0xC5, },
   .r2 = { 0x3C, 0x4C, 0x9D, 0x05, 0xA9, 0xFF, 0x64, 0x50,
	   0x20, 0x2E, 0x19, 0x40, 0x20, 0x56, 0xEE, 0xCC,
	   0xA1, 0x6D, 0xAA, 0x5F, 0xD4, 0x2B, 0xFF, 0x83,
	   0x19, 0x48, 0x6F, 0xD8, 0xD5, 0x89, 0x80, 0x57,
	   0xE0, 0xC1, 0x9A, 0x77, 0x83, 0x51, 0x4A, 0x25,
	   0x53, 0xB7, 0xF9, 0xBC, 0x90, 0x5A, 0xFF, 0xD3,
	   0x79, 0x3F, 0xB1, 0x30, 0x27, 0x15, 0x79, 0x05,
	   0x49, 0xAD, 0x14, 0x4A, 0x61, 0x58, 0xF2, 0x05, },
};

#else

#define bp_p512t1_mont_valid 0

#endif /* BRAINPOOL_EC_USE_FIXED_MONTGOMERY */
#endif /* HAVE_BPT_512 */

#endif /* HAVE_BRAINPOOL_TWISTED_LWRVES */

#endif /* INCLUDED_BRAINPOOL_LWRVES_H */
