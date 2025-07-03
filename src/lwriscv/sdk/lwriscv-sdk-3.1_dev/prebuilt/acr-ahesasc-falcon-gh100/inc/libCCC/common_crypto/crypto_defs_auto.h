/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_DEFS_AUTO_H
#define INCLUDED_CRYPTO_DEFS_AUTO_H


#ifndef HAVE_LW_AUTOMOTIVE_SYSTEM
#define HAVE_LW_AUTOMOTIVE_SYSTEM 0	/* Leaves the algorithms enabled by default */
#endif

#ifndef HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION
#define HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION 0
#endif

#if HAVE_LW_AUTOMOTIVE_SYSTEM
/* This is a collective config define for LW automotive. If you need to leave one
 * of these algos enabled do not use the collective define, drop things
 * using individual defines.
 *
 * The LW Automotive systems disable various algorithms and key lengths.
 *
 * Define HAVE_LW_AUTOMOTIVE_SYSTEM 1 in your makefile or crypto_system_config.h
 * file to drop the following algorithms and algorithm key lengths collectively.
 *
 * Dropped algorithms (and all other algorithms using these):
 * - SHA-1
 * - SHA-224
 * - AES-OFB
 * - All EC lwrves with prime 160-224
 * - All RSA operations with key lengths <= 2048 bits.
 *   Note: A JAMA requirement exception to this allows
 *   enabling RSA-2048 bit keys with RSASSA-PKCS1_V1_5 signature
 *   verification (specifically for that, no other purpose).
 */
#define HAVE_AES_OFB		0	/* Disables AES-OFB */
#define HAVE_SHA1		0	/* Disables SHA-1 */
#define HAVE_SHA224		0	/* Disables SHA-224 */

#if HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION
/* All automotive cases require longer RSA keys.
 *
 * Default is 3072 bit RSA keys; allows it to be overriden
 * for now for RSASSA-PKCS1v1.5 signature verification by 2048.
 *
 * The special case algorithm matching with RSA keylength is done
 * elsewhere.
 *
 * If you need to support RSA2048 bits in automotive builds define that
 * in your system config file. This will, as a special case, only
 * enable RSA2048 keys to be used in RSASSA-PKCS1v1.5 sig validations.
 */
#define RSA_MIN_KEYSIZE_BITS	2048U

#if HAVE_RSA_PKCS_VERIFY == 0
/* Need to enable this in your config file */
#error "Automotive RSA-2048 bit key use case PKCS1v1.5 signature verification not enabled"
#endif

#else

#define RSA_MIN_KEYSIZE_BITS	3072U

#endif /* HAVE_LW_AUTOMOTIVE_RSA2048_VALIDATION */

/* EC prime length is unconditional automotive requirement
 *
 * Any lwrves with small primes are disabled in the header files
 * as well.
 */
#define CCC_EC_MIN_PRIME_BITS	256U

/* Automotive setups disable AES 192 bit keys.
 */
#ifndef HAVE_AES_KEY192
#define HAVE_AES_KEY192 0
#endif

#endif /* HAVE_LW_AUTOMOTIVE_SYSTEM */

#endif /* INCLUDED_CRYPTO_DEFS_AUTO_H */
