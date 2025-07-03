/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_DEFS_H
#define INCLUDED_CRYPTO_DEFS_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <crypto_soc.h>

#ifndef USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
/* This will improve code coverage, but makes relevant macro
 * statements a "compound statement" instead of a "statement".
 *
 * This does not matter in CCC due to selected C-coding style,
 * but is NOT a generic solution.
 *
 * The generic solution is to make macros look like a statement, like
 * it is if this is defined non-zero and like it was before and
 * has always been in C coding styles.
 *
 * Defining this zero affects code coverage in error handler macros
 * since they contain the "goto fail;" error handler statement which
 * blocks reach of the "while(false)" lowering the code coverage.
 */
#define USE_DO_WHILE_IN_ERROR_HANDLING_MACROS 0
#endif

#if !defined(CACHE_LINE) || (CACHE_LINE == 0) || ((CACHE_LINE % 16) != 0)
#error "Please define CACHE_LINE to 16U or a multiple of 16U bytes"
#error "If this is not valid for your system, please file a CCC bug"
#endif

/* Default config options to reduce complexity of the config file
 */
#ifndef HAVE_NC_REGIONS
#define HAVE_NC_REGIONS 0
#endif

#ifndef HAVE_USER_MODE
#define HAVE_USER_MODE 0
#endif

#ifndef HAVE_SE_MUTEX
#define HAVE_SE_MUTEX 1
#endif

/* This is zero to use backwards compatible version of
 * macro HW_MUTEX_TAKE_ENGINE in tegra_se.h
 *
 * This is only relevant for subsystems using low level
 * CCC functions directly and the subsystem then needs to
 * lock the HW mutex before calling such functions (asynchronous
 * code or engine level function direct access).
 *
 * Some subsystems using the async algorithms or directly access CCC
 * engine functions do use there macros in their subsystem code for
 * locking the mutexes. In case they do not have compatible error
 * handling compilation would fail.
 *
 * This can be fixed by (please prefer #1):
 *
 * 1) adding compatible error handling (i.e. fail: label handling
 *    errors and status_t ret variable).
 *
 * or
 *
 * 2) define HAVE_MUTEX_LOCK_STATUS 0 which ignores the status code
 *    returned from mutex acquire function.
 */
#ifndef HAVE_MUTEX_LOCK_STATUS
#define HAVE_MUTEX_LOCK_STATUS 0
#endif

#ifndef TEGRA_DELAY_FUNCTION_UNDEFINED
#define TEGRA_DELAY_FUNCTION_UNDEFINED 1
#endif

#if TEGRA_DELAY_FUNCTION_UNDEFINED

#define TEGRA_UDELAY_VALUE_SE0_OP_INITIAL   0
#define TEGRA_UDELAY_VALUE_SE0_OP	    0
#define TEGRA_UDELAY_VALUE_SE0_MUTEX	    0

#define TEGRA_UDELAY_VALUE_PKA1_OP_INITIAL  0
#define TEGRA_UDELAY_VALUE_PKA1_OP   	    0
#define TEGRA_UDELAY_VALUE_PKA1_MUTEX	    0

#endif /* TEGRA_DELAY_FUNCTION_UNDEFINED */

/* Cert-C grunt adjustment
 */
#if defined(EC_MIN_PRIME_BITS)

#if defined(CCC_EC_MIN_PRIME_BITS)
/* Acceptable if the values match, but just keep it simple */
#error "EC_MIN_PRIME_BITS and CCC_EC_MIN_PRIME_BITS are both defined"
#endif

#define CCC_EC_MIN_PRIME_BITS EC_MIN_PRIME_BITS
#endif /* defined(EC_MIN_PRIME_BITS) */

#include <crypto_defs_auto.h>

/******/

#include <crypto_defs_mem.h>

/******/

#ifndef HAVE_AES_OFB
#define HAVE_AES_OFB		1
#endif

#ifndef HAVE_SE_RSA
#define HAVE_SE_RSA		0
#endif

#ifndef HAVE_PKA1_RSA
#define HAVE_PKA1_RSA		0
#endif

#ifndef HAVE_SHA1
#define HAVE_SHA1		1
#endif

#ifndef HAVE_SHA224
#define HAVE_SHA224		1
#endif

#ifndef HAVE_RSA_PKCS_SIGN
#define HAVE_RSA_PKCS_SIGN	0
#endif

#ifndef HAVE_RSA_PSS_SIGN
#define HAVE_RSA_PSS_SIGN	0
#endif

#ifndef HAVE_RSA_OAEP
#define HAVE_RSA_OAEP		0
#endif

#ifndef HAVE_OAEP_LABEL
#define HAVE_OAEP_LABEL		0
#endif

#ifndef HAVE_PLAIN_DH
#define HAVE_PLAIN_DH		0
#endif

/* Min bit sizes supported by HW/SW */
#define RSA_SHORTEST_KEYSIZE_BITS	512U
#define CCC_EC_SHORTEST_PRIME_BITS	160U

#if !defined(RSA_MIN_KEYSIZE_BITS)
/* Minimum allowed RSA keylength.
 *
 * Note that all real applications should use key lengths >= 2048
 * Shorter keys should only be used for testing purposes.
 */
#if SE_RD_DEBUG
#define RSA_MIN_KEYSIZE_BITS RSA_SHORTEST_KEYSIZE_BITS
#else
/* Use of shorter RSA keys is unsafe and must not be used
 * in production code. This is the default unless R&D or
 * subsystem restricts the minimum size to even longer keys.
 *
 * See above example for automotive requirements.
 */
#define RSA_MIN_KEYSIZE_BITS 2048U
#endif
#endif /* !defined(RSA_MIN_KEYSIZE_BITS) */

/* PKA0 (SE RSA) engine supports only keys up to 2048 bits.
 *
 * Need to define HAVE_SE_RSA 0 to disable PKA0 if it is unusable for anything.
 */
#if RSA_MIN_KEYSIZE_BITS > 2048U
#if HAVE_SE_RSA
#error "Do not enable PKA0 with HAVE_SE_RSA when RSA key length must be over 2048"
#endif
#endif

/* Minimum allowed EC lwrve prime bits.
 */
#ifndef CCC_EC_MIN_PRIME_BITS
#define CCC_EC_MIN_PRIME_BITS CCC_EC_SHORTEST_PRIME_BITS
#endif

/* Subsystem specific exec speed randomizing call */
#ifndef CCC_RANDOMIZE_EXELWTION_TIME
#define CCC_RANDOMIZE_EXELWTION_TIME
#endif

/*
 * Disable GSCID clearing by default.
 * Changed as requested in Jira COCC-903
 */
#ifndef CCC_CLEAR_GSCID_REG
#define CCC_CLEAR_GSCID_REG 0
#endif

/* Expose low level asynchronous engine calls if nonzero */
#ifndef HAVE_SE_ASYNC
#define HAVE_SE_ASYNC 0
#endif

#if HAVE_SE_ASYNC

#ifndef HAVE_SE_ASYNC_AES
#define HAVE_SE_ASYNC_AES 1
#endif

#ifndef HAVE_SE_ASYNC_SHA
#define HAVE_SE_ASYNC_SHA 1
#endif

#endif /* HAVE_SE_ASYNC */

#ifndef HAVE_SE_ASYNC_AES
#define HAVE_SE_ASYNC_AES 0
#endif

#ifndef HAVE_SE_ASYNC_SHA
#define HAVE_SE_ASYNC_SHA 0
#endif

#ifndef HAVE_IS_CONTIGUOUS_MEM_FUNCTION
/* Function se_is_contiguous_memory_region() exists only if this
 * is set.
 */
#define HAVE_IS_CONTIGUOUS_MEM_FUNCTION (HAVE_SE_ASYNC_AES || HAVE_SE_ASYNC_SHA)
#endif

/* CCC optional feature: runtime keyslot setup for dynamic key
 * operations. If you need that define this nonzero and add
 * crypto_kconfig.c to source file list in build.
 * See crypto_kconfig.h.
 */
#ifndef HAVE_CRYPTO_KCONFIG
#define HAVE_CRYPTO_KCONFIG 0
#endif

#ifndef HAVE_RNG1_DRNG
#define HAVE_RNG1_DRNG 0
#endif

#ifndef HAVE_LWRNG_DRNG
#define HAVE_LWRNG_DRNG 0
#endif

#ifndef HAVE_PKA1_TRNG
#define HAVE_PKA1_TRNG 0
#endif

#ifndef HAVE_SE_RANDOM
#define HAVE_SE_RANDOM 0
#endif

#ifndef HAVE_SE_GENRND
#define HAVE_SE_GENRND 0
#endif

#ifndef HAVE_AES_XTS
#define HAVE_AES_XTS 0
#endif

#ifndef HAVE_ELLIPTIC_20
/* T19X runs newer Elliptic FW in PKA1 */
#define HAVE_ELLIPTIC_20 \
	(CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) == CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T19X))
#endif /* HAVE_ELLIPTIC_20 */

/* Define this zero if SHAMIR'S TRICK CAN NOT BE used.
 * (Value of R = u1xP1 + u2xP2 can be callwlated with Shamir's trick).
 *
 * NOTE for T18X:
 * T18X can't use shamir's trick because if u1xP1 and u2xP2
 * are reflected points the unit may hang.
 *
 * For newer units: define this nonzero because it is faster.
 */
#ifndef HAVE_SHAMIR_TRICK
#define HAVE_SHAMIR_TRICK (HAVE_ELLIPTIC_20 || CCC_SOC_WITH_LWPKA)
#endif

#ifndef HAVE_ED25519_SIGN
#define HAVE_ED25519_SIGN 0
#endif

#if defined(HAVE_ECDSA_SIGN) && defined(HAVE_ECDSA_SIGNING)
#error "Please remove the obsolete HAVE_ECDSA_SIGNING config option"
#endif

#if defined(HAVE_ECDSA_SIGNING) && !defined(HAVE_ECDSA_SIGN)
#define HAVE_ECDSA_SIGN HAVE_ECDSA_SIGNING
#endif

#ifndef HAVE_ECDSA_SIGN
#define HAVE_ECDSA_SIGN 0
#endif

#ifndef POINT_OP_RESULT_VERIFY
/* Verify EC point on lwrve by default, after point multiply operation.
 * Not all point results are checked; could add more checks.
 *
 * If HW does this check or you do not want these checks: set 0
 */
#define POINT_OP_RESULT_VERIFY (CCC_SOC_WITH_LWPKA == 0)
#endif

#ifndef HAVE_GEN_MULTIPLY
/*
 * Generic multiply available only if HAVE_PKA1_MODULAR && HAVE_ELLIPTIC_20
 */
#define HAVE_GEN_MULTIPLY						\
	(HAVE_PKA1_MODULAR &&						\
	 (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) == CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T19X)))
#endif

#ifndef HAVE_PKA1_ECC_POINT_ADD
#define HAVE_PKA1_ECC_POINT_ADD (HAVE_SHAMIR_TRICK == 0)
#endif

#ifndef HAVE_PKA1_ECC_POINT_DOUBLE
#define HAVE_PKA1_ECC_POINT_DOUBLE 0
#endif

#ifndef HAVE_PKA1_ED25519_POINT_ADD
#define HAVE_PKA1_ED25519_POINT_ADD (HAVE_SHAMIR_TRICK == 0)
#endif

#ifndef HAVE_PKA1_ED25519_POINT_DOUBLE
#define HAVE_PKA1_ED25519_POINT_DOUBLE 0
#endif

#ifndef CCC_WITH_MODULAR_DIVISION
#define CCC_WITH_MODULAR_DIVISION 0
#endif /* CCC_WITH_MODULAR_DIVISION */

#ifndef PKA_MODULAR_OPERATIONS_PRESERVE_MODULUS
#define PKA_MODULAR_OPERATIONS_PRESERVE_MODULUS CCC_SOC_WITH_PKA1
#endif

#ifndef CCC_WITH_BIT_SERIAL_REDUCTION
#define CCC_WITH_BIT_SERIAL_REDUCTION 0
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */

#ifndef CCC_WITH_ECC_POINT_DOUBLE
#define CCC_WITH_ECC_POINT_DOUBLE HAVE_PKA1_ECC_POINT_DOUBLE
#endif /* CCC_WITH_ECC_POINT_DOUBLE */

#ifndef CCC_WITH_ED25519_POINT_DOUBLE
#define CCC_WITH_ED25519_POINT_DOUBLE HAVE_PKA1_ED25519_POINT_DOUBLE
#endif /* CCC_WITH_ED25519_POINT_DOUBLE */

#ifndef CCC_WITH_ECC_POINT_ADD
#if !HAVE_SHAMIR_TRICK
#define CCC_WITH_ECC_POINT_ADD HAVE_PKA1_ECC_POINT_ADD /* required in this case */
#else
#define CCC_WITH_ECC_POINT_ADD 0
#endif
#endif /* CCC_WITH_ECC_POINT_ADD */

#ifndef HAVE_PKA1_ED25519
#define HAVE_PKA1_ED25519 0
#endif

/* Use the long form group equation when possible.
 *
 * ED25519 requires HAVE_GEN_MULTIPLY, it can be
 * used for other lwrves without that.
 *
 * LWPKA sets this elsewhere.
 */
#ifndef VERIFY_GROUP_EQUATION_LONG_FORM
#define VERIFY_GROUP_EQUATION_LONG_FORM ((HAVE_PKA1_ED25519 == 0) || HAVE_GEN_MULTIPLY)
#endif

#ifndef CCC_WITH_ED25519_POINT_ADD
#if !VERIFY_GROUP_EQUATION_LONG_FORM || !HAVE_SHAMIR_TRICK
 /* required in this case; error if not set */
#define CCC_WITH_ED25519_POINT_ADD HAVE_PKA1_ED25519_POINT_ADD
#else
#define CCC_WITH_ED25519_POINT_ADD 0
#endif
#endif /* CCC_WITH_ED25519_POINT_ADD */

#ifndef CCC_WITH_P521_POINT_DOUBLE
#define CCC_WITH_P521_POINT_DOUBLE 0
#endif /* CCC_WITH_P521_POINT_DOUBLE */

#ifndef CCC_WITH_P521_POINT_ADD
#define CCC_WITH_P521_POINT_ADD 0
#endif /* CCC_WITH_P521_POINT_ADD */

#define X_EC_POINT_DOUBLE CCC_WITH_ECC_POINT_DOUBLE || CCC_WITH_ED25519_POINT_DOUBLE || \
			  CCC_WITH_P521_POINT_DOUBLE

#define X_EC_POINT_ADD CCC_WITH_ECC_POINT_ADD || CCC_WITH_ED25519_POINT_ADD || \
			CCC_WITH_P521_POINT_ADD

#ifndef HAVE_P521
#define HAVE_P521 0

#if CCC_WITH_P521_POINT_DOUBLE || CCC_WITH_P521_POINT_ADD
#error "NIST-P521 primitives enabled without NIST-P521 lwrve support"
#endif
#endif

#ifndef HAVE_PKA1_SCALAR_MULTIPLIER_CHECK
#define HAVE_PKA1_SCALAR_MULTIPLIER_CHECK 0
#endif /* HAVE_PKA1_SCALAR_MULTIPLIER_CHECK */

#ifndef HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY
#define HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY 0
#endif

#ifndef HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY
#define HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY 0
#endif

#ifndef HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY
#define HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY 0
#endif

#ifndef HAVE_PKA1_ISM3
#define HAVE_PKA1_ISM3 0
#endif /* HAVE_PKA1_ISM3 */

#ifndef HAVE_PKA1_ECDH
#define HAVE_PKA1_ECDH 0
#endif

#ifndef HAVE_PKA1_X25519
#define HAVE_PKA1_X25519 0
#endif

/* Some functionality in CCC requires safe random numbers.
 *
 * SE HW supports three different RNGs:
 * - RNG1 device DRNG (prefer this)
 * - SE AES0 DRNG
 * - PKA1 TRNG (should not use this for general purpose random needs,
 *     but if nothing else is available...)
 *
 * Note: Should not use this macro in cases caller holds a mutex and
 * this call also grabs a mutex (e.g. TRNG) in case it would cause a deadlock
 * or timeout. In systems not using mutex protection (LWRNG) or when a dedicated
 * mutex exists for the macro call (e.g. RNG1): no restrictions.
 *
 * If a mutex issue exists => deal with random number generator code locally, there
 * are also function versions that can be called with a mutex held (RNG1, LWRNG).
 */
#if defined(CCC_GENERATE_RANDOM)

#if SE_RD_DEBUG == 0
#error "Redefining the random number generator only allowed with SE_RD_DEBUG"
#endif

#else

#ifndef CCC_ERROR_IF_NO_RANDOM_GENERATOR
#define CCC_ERROR_IF_NO_RANDOM_GENERATOR 0
#endif

#if HAVE_SE_GENRND

#define CCC_GENERATE_RANDOM(rbuf, rbuf_len) se_crypto_kernel_generate_random(rbuf, rbuf_len)
/* T23X SE mutex */
#define NEED_SE_CRYPTO_GENERATE_RANDOM

#elif HAVE_LWRNG_DRNG
/* no mutex */
#define CCC_GENERATE_RANDOM(rbuf, rbuf_len) lwrng_generate_drng(rbuf, rbuf_len)

#elif HAVE_RNG1_DRNG
/* DRNG mutex */
#define CCC_GENERATE_RANDOM(rbuf, rbuf_len) rng1_generate_drng(rbuf, rbuf_len)

#ifndef HAVE_RNG1_USE_AES256
/* RNG1 selects SEC_ALG AES-256 by default when set nonzero.
 *
 * Uses AES-128 when this is 0.
 */
#define HAVE_RNG1_USE_AES256 1
#endif /* HAVE_RNG1_USE_AES256 */

#elif HAVE_SE_RANDOM
/* RNG0 SE mutex */
#define CCC_GENERATE_RANDOM(rbuf, rbuf_len) se_crypto_kernel_generate_random(rbuf, rbuf_len)
#define NEED_SE_CRYPTO_GENERATE_RANDOM

#elif HAVE_PKA1_TRNG
	/* TRNG PKA1 mutex */
	/* Should not use this, but if nothing else is available */
#define CCC_GENERATE_RANDOM(rbuf, rbuf_len) pka1_trng_generate_random(rbuf, rbuf_len)
#define NEED_PKA1_TRNG_GENERATE_RANDOM

#else

#if CCC_ERROR_IF_NO_RANDOM_GENERATOR
// Do not define CCC_GENERATE_RANDOM if there is no random number
//  generator defined in the system config file.
//
// This will lead to compilation #error failures with features that
//  require it => subsystem needs to define which RNG to enable.
//
#error "CCC configuration needs a random number generator"
#endif

#endif /* Auto-select random number generator */
#endif /* CCC_GENERATE_RANDOM */

/* Extra setting now required to enable CCC API access to TRNG.
 * TRNG access should be restricted for testing purposes.
 */
#if HAVE_PKA1_TRNG

#ifndef TRNG_API_DEFAULT
/* Enable PKA1 TRNG CCC API access when HAVE_PKA1_TRNG.
 * For backwards compatibility only.
 */
#define TRNG_API_DEFAULT 1
#endif

#if TRNG_API_DEFAULT
#define CCC_ENABLE_TRNG_API 1
#else
#define CCC_ENABLE_TRNG_API 0
#endif

#else

#define CCC_ENABLE_TRNG_API 0

#endif /* HAVE_PKA1_TRNG */

#ifndef HAVE_KOBLITZ_LWRVES
#define HAVE_KOBLITZ_LWRVES 0
#endif

#ifndef HAVE_BRAINPOOL_LWRVES
#define HAVE_BRAINPOOL_LWRVES 0
#endif

#ifndef HAVE_BRAINPOOL_TWISTED_LWRVES
#define HAVE_BRAINPOOL_TWISTED_LWRVES 0
#endif

#ifndef HAVE_NIST_LWRVES
#define HAVE_NIST_LWRVES 0
#endif

#ifndef HAVE_AES_CTS
#define HAVE_AES_CTS 0
#endif

#ifndef HAVE_AES_GCM
#define HAVE_AES_GCM 0
#endif

#ifndef HAVE_GMAC_AES
#define HAVE_GMAC_AES 0
#endif

#if HAVE_GMAC_AES && !HAVE_AES_GCM
#error "GMAC-AES requires HAVE_AES_GCM"
#endif

#ifndef HAVE_AES_CCM
#define HAVE_AES_CCM 0
#endif

#if HAVE_AES_GCM || HAVE_AES_CCM
#define HAVE_AES_AEAD 1
#else
#define HAVE_AES_AEAD 0
#endif

#if HAVE_AES_GCM
#define HAVE_AES_AEAD_UPDATE 1
#else
#define HAVE_AES_AEAD_UPDATE 0
#endif

#ifndef SE_NULL_SHA_DIGEST_FIXED_RESULTS
/* When defined zero the SHA HW engine can callwlate result of empty
 * input.
 *
 * If used HW does not support it => setting this to nonzero value
 * uses pre-callwlated fixed results in SW.
 */
#if ((CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) == CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T18X)))

/* T18X HW does not support NULL SHA input, newer engines do.
 */
#define SE_NULL_SHA_DIGEST_FIXED_RESULTS 1

#else

/* HW can callwlate SHA from zero size input */
#define SE_NULL_SHA_DIGEST_FIXED_RESULTS 0

#endif/* SOC FAMILY T18X */

#endif /* SE_NULL_SHA_DIGEST_FIXED_RESULTS */

#ifndef HAVE_NIST_TRUNCATED_SHA2
#define HAVE_NIST_TRUNCATED_SHA2 0
#endif

#ifndef HAVE_RSA_MONTGOMERY_CALC
#define HAVE_RSA_MONTGOMERY_CALC 0
#endif

#ifndef HAVE_SW_WHIRLPOOL
#define HAVE_SW_WHIRLPOOL	0
#endif

#if !defined(HAVE_SE_SHA)
/* Note HAVE_SE_SHA this is turned on unless explicitly disabled in
 * config.
 *
 * SHA digest is implicitly used by RSA-PSS signature validation mask
 * generation even in cases when SHA digests are not explictly
 * enabled.
 */
#define HAVE_SE_SHA		1
#endif

/* Allow configs which need SHA but do not enable it.
 * E.g. if using external SW sha or external HW in CCC lite.
 */
#ifndef HAVE_EXTERNAL_SHA
#define HAVE_EXTERNAL_SHA 0
#endif

#ifndef HAVE_SW_SHA3
#define HAVE_SW_SHA3	0
#endif

#ifndef HAVE_HW_SHA3
#define HAVE_HW_SHA3	0
#endif

#if HAVE_HW_SHA3 && HAVE_SW_SHA3
#error "Both HW and SW setup for SHA-3 is not supported, disable one of these"
#endif

#if HAVE_HW_SHA3
/* Default SHAKE XOF output bit lengths when not specified by caller.
 */
#ifndef SHAKE128_DEFAULT_XOF_SIZE
#define SHAKE128_DEFAULT_XOF_SIZE 128U
#endif

#ifndef SHAKE256_DEFAULT_XOF_SIZE
#define SHAKE256_DEFAULT_XOF_SIZE 256U
#endif
#endif /* HAVE_HW_SHA3 */

#ifndef HAVE_MD5
#define HAVE_MD5 0
#endif

#ifndef HAVE_HMAC_MD5
#define HAVE_HMAC_MD5 0
#endif

#if HAVE_MD5 && !SE_RD_DEBUG
#error "MD5 can be enabled for R&D configurations only"
#endif

#ifndef SHA_DST_MEMORY	/* WIP feature */
#define SHA_DST_MEMORY 0
#endif

#ifndef CMAC_DST_MEMORY	/* WIP feature */
#define CMAC_DST_MEMORY 0
#endif

#ifndef HMAC_DST_MEMORY	/* WIP feature */
#define HMAC_DST_MEMORY 0
#endif

#ifndef HAVE_SE_CMAC_KDF /* [T19X] Key derivation with CMAC-AES and NIST 800-108 counter */
#define HAVE_SE_CMAC_KDF 0
#endif

#ifndef HAVE_CMAC_DST_KEYTABLE
#define HAVE_CMAC_DST_KEYTABLE HAVE_SE_CMAC_KDF
#endif

#ifndef HAVE_SE_AES_KDF /* [T19X] Key derivation with supported AES modes */
#define HAVE_SE_AES_KDF 0
#endif

/* Support AES keyslot IV preset value when key is also preset.
 */
#ifndef HAVE_AES_IV_PRESET
#define HAVE_AES_IV_PRESET 0
#endif

/* If EC private key generation not yet used by any code => disable for now */
#ifndef HAVE_GENERATE_EC_PRIVATE_KEY
#define HAVE_GENERATE_EC_PRIVATE_KEY 0
#endif

#if HAVE_RSA_CIPHER
/* For backwards compatibility: HAVE_RSA_CIPHER used to mean this, but
 * is also required for OAEP. Now it is possible to exclude PKCS1v1_5
 * encoding code by defining it 0 in the config file.
 */
#ifndef HAVE_RSA_PKCS1V15_CIPHER
#define HAVE_RSA_PKCS1V15_CIPHER 1
#endif
#endif

#ifndef HAVE_RSA_PKCS1V15_CIPHER
#define HAVE_RSA_PKCS1V15_CIPHER 0
#endif

#if HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER
#define HAVE_RSA_CIPHER 1
#endif

#if HAVE_RSA_CIPHER && !(HAVE_RSA_OAEP || HAVE_RSA_PKCS1V15_CIPHER || HAVE_PLAIN_DH )
#error "RSA cipher enabled but no padding modes selected (OAEP or PKCS1#v1.5) or plain DH"
#endif

#ifndef HAVE_SE_AES
#define HAVE_SE_AES 0
#endif

#ifndef HAVE_SE_AES_ENGINE_AES0
#define HAVE_SE_AES_ENGINE_AES0 HAVE_SE_AES
#endif

#ifndef HAVE_SE_AES_ENGINE_AES1
#define HAVE_SE_AES_ENGINE_AES1 HAVE_SE_AES
#endif

#ifndef HAVE_AES_PADDING
/* AES-CBC and AES-ECB padding modes supported by default */
#define HAVE_AES_PADDING HAVE_SE_AES
#endif

#ifndef HAVE_AES_CTR
/* AES-CTR enabled by default with AES support */
#define HAVE_AES_CTR HAVE_SE_AES
#endif

/* This is enabled unless disabled by configuration.
 *
 * Automotive config DISABLE this by default (see above).
 *
 * To disable: define HAVE_AES_KEY192 0
 */
#ifndef HAVE_AES_KEY192
#define HAVE_AES_KEY192 1
#endif

#ifndef HAVE_CMAC_AES
#define HAVE_CMAC_AES 0
#endif

#if HAVE_SE_AES == 0
#if HAVE_CMAC_AES || HAVE_AES_AEAD
#error "CMAC-AES and AES AE-AD modes require HAVE_SE_AES (AES engine)"
#endif

#if HAVE_SE_AES_ENGINE_AES0 || HAVE_SE_AES_ENGINE_AES1
#error "Need HAVE_SE_AES for enabling HAVE_SE_AES_ENGINE_AES*"
#endif

#else

#if (HAVE_SE_AES_ENGINE_AES0 || HAVE_SE_AES_ENGINE_AES1) == 0
#error "Need at least one of HAVE_SE_AES_ENGINE_AES* for HAVE_SE_AES"
#endif

#endif /* HAVE_SE_AES == 0 */

#if HAVE_AES_CCM && !HAVE_CMAC_AES
#error "HAVE_AES_CCM requires HAVE_CMAC_AES engine code"
#endif

#if HAVE_SE_CMAC_KDF
#if !HAVE_CMAC_AES
#error "HAVE_SE_CMAC_KDF required HAVE_CMAC_AES"
#endif

#if !HAVE_CMAC_DST_KEYTABLE
#error "HAVE_SE_CMAC_KDF required HAVE_CMAC_DST_KEYTABLE"
#endif
#endif /* HAVE_SE_CMAC_KDF */

#ifndef HAVE_SE_SCC
/* By default enable SE SCC for all AES operations
 * Override by defining HAVE_SE_SCC 0 => disables SE AES SCC
 */
#define HAVE_SE_SCC 1
#endif /* HAVE_SE_SCC */

#ifndef HAVE_AES_COUNTER_OVERFLOW_CHECK
/* Check if the AES-CTR mode 8 byte counter
 * would overflow in the next AES-CTR call.
 */
#define HAVE_AES_COUNTER_OVERFLOW_CHECK (HAVE_AES_CTR || HAVE_AES_CCM || HAVE_AES_GCM)
#endif

#ifndef HAVE_CCC_LIBRARY_MODE_CALLS
/* Include CCC provided library mode calls not used by CCC core.
 *
 * Included by default for backwards compatibility.
 *
 * Define 0 in config file if not needed.
 */
#define HAVE_CCC_LIBRARY_MODE_CALLS 1
#endif

/* AES modes including AES-CTR, GCM and CCM use 64
 * bit data colwerters for counter increment.
 */
#ifndef HAVE_CCC_64_BITS_DATA_COLWERSION
#define HAVE_CCC_64_BITS_DATA_COLWERSION (HAVE_AES_CTR || HAVE_AES_CCM || HAVE_AES_GCM)
#endif

/* T18X and T19X key unwrapping */
#ifndef HAVE_SE_UNWRAP
#define HAVE_SE_UNWRAP 0
#endif

#ifndef HAVE_SE_KAC_KEYOP
#define HAVE_SE_KAC_KEYOP 0
#endif

#ifndef HAVE_SE_KAC_GENKEY
#define HAVE_SE_KAC_GENKEY 0
#endif

#ifndef HAVE_SE_KAC_CLONE
#define HAVE_SE_KAC_CLONE 0
#endif

#ifndef HAVE_SE_KAC_LOCK
#define HAVE_SE_KAC_LOCK 0
#endif

/* For backwards compatibility only */
#ifdef HAVE_HMAC_SHA
#define HAVE_SW_HMAC_SHA HAVE_HMAC_SHA
#endif

/* For backward compatibility.
 *
 * So, to DISABLE HAVE_SW_HMAC_SHA it needs to be set 0
 * in the config file if HAVE_HW_HMAC_SHA is defined.
 *
 * HW HMAC defines also SW/HYBRID HMAC identically
 * in case SW HMAC is not defined.
 */
#if defined(HAVE_HW_HMAC_SHA) && !defined(HAVE_SW_HMAC_SHA)
#define HAVE_SW_HMAC_SHA HAVE_HW_HMAC_SHA
#endif

#ifndef HAVE_HW_HMAC_SHA
#define HAVE_HW_HMAC_SHA 0
#endif

#ifndef HAVE_SW_HMAC_SHA
#define HAVE_SW_HMAC_SHA 0
#endif

#if HAVE_HW_HMAC_SHA || HAVE_SW_HMAC_SHA
#define HAVE_HMAC_SHA 1
#else
#define HAVE_HMAC_SHA 0
#endif

#ifndef HAVE_SE_KAC_KDF /* [T23X] Key derivation */
#define HAVE_SE_KAC_KDF 0
#endif

/* NIST 800-108 KDF in Counter Mode data encoder
 * for [T23X] KDF/KDD
 */
#ifndef HAVE_SE_KAC_KDF_NIST_ENCODER
#define HAVE_SE_KAC_KDF_NIST_ENCODER 0
#endif

#ifndef HAVE_KAC_KEYSLOT_LOCK
#define HAVE_KAC_KEYSLOT_LOCK 0
#endif

#ifdef CCC_SET_SE_APERTURE
/* T23X memory apertures */
#define HAVE_SE_APERTURE 1
#else
#define HAVE_SE_APERTURE 0
#endif /* CCC_SET_SE_APERTURE */

/* Collective settings to enable shared code resources
 */
#define HAVE_RSA_SIGN (HAVE_RSA_PSS_SIGN || HAVE_RSA_PKCS_SIGN)

#define HAVE_CCC_KDF \
	(HAVE_PKA1_ECDH || HAVE_PKA1_X25519 || HAVE_SE_KAC_KDF || HAVE_SE_AES_KDF || HAVE_SE_CMAC_KDF)

#ifndef HAVE_EXPORT_NIST_COUNTER_MODE_ENCODER
#define HAVE_EXPORT_NIST_COUNTER_MODE_ENCODER 0
#endif

#if !defined(TEGRA_READ32) && !defined(TEGRA_WRITE32)
#define CCC_MEM32(addr) ((volatile uint32_t *)(uintptr_t)(addr))
#define TEGRA_WRITE32(a, v) (*CCC_MEM32(a) = (v))
#define TEGRA_READ32(a) (*CCC_MEM32(a))
#endif

/* Invalid keyslot index (for undefining key indexes).
 * SE AES/SHA invalid keyslot index
 */
#define ILWALID_KEYSLOT_INDEX 255U

#if !defined(CCC_STACK_PROTECTOR)
#define CCC_STACK_PROTECTOR 0
#endif

/* Force some selections; these are ancient configs nobody uses.
 */
#define HAVE_DEPRECATED 0		// No longer support any ancient depercated features

#if !defined(CCC_ERROR_MESSAGE)
#if !defined(ERROR_MESSAGE)
#error "Please define CCC_ERROR_MESSAGE in client configuration file"
#endif
/* Allow backwards compatible define use if not strictly Cert-C */
#define CCC_ERROR_MESSAGE	ERROR_MESSAGE
#endif /* !defined(CCC_ERROR_MESSAGE) */

#if !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES)
/* These are defined only for backwards compatibility,
 * no longer used by CCC. The CCC_E* names are used by
 * CCC code for Cert-C compatibility.
 */
#if !defined(EC_MIN_PRIME_BITS)
#define EC_MIN_PRIME_BITS CCC_EC_MIN_PRIME_BITS
#endif

#if !defined(ERROR_MESSAGE)
#define ERROR_MESSAGE CCC_ERROR_MESSAGE
#endif
#endif /* !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES) */

#ifndef HAVE_ERROR_CAPTURE
#define HAVE_ERROR_CAPTURE 0
#endif

#ifndef HAVE_SE_ENGINE_TIMEOUT
#define HAVE_SE_ENGINE_TIMEOUT 0
#endif /* HAVE_SE_ENGINE_TIMEOUT */

#ifndef HAVE_NO_STATIC_DATA_INITIALIZER
/* Some systems do not initialize file global static variables
 * to nonzero values. In such systems the polling loop timeout boundaries
 * (with runtime control) are left zero and the polling loop will poll
 * only once before it times out. This is not correct, and to fix such a system
 * define HAVE_NO_STATIC_DATA_INITIALIZER nonzero so CCC knows to
 * initialize the "static initializers" at runtime.
 */
#define HAVE_NO_STATIC_DATA_INITIALIZER 0
#endif

#ifndef HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM
#define HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM 0
#endif

#ifndef TEGRA_MEASURE_MEMORY_USAGE
#define TEGRA_MEASURE_MEMORY_USAGE 0
#endif

#ifndef HAVE_SE1
#define HAVE_SE1 0
#endif

#ifndef LTRACEF
#define LTRACEF(...)
#endif

#ifndef TRACE_REG_OPS
#define TRACE_REG_OPS 0
#endif

#ifndef MODULE_TRACE
#define MODULE_TRACE 0
#endif

#ifndef HAVE_PKA1_RESET
#define  HAVE_PKA1_RESET 1 /* Enable PKA1 device reset on timeouts */
#endif

#ifndef HAVE_PKA1_LOAD
#define HAVE_PKA1_LOAD 0
#endif

#ifndef HAVE_PKA1_ECC
#define HAVE_PKA1_ECC 0
#endif

#ifndef HAVE_XMSS_VERIFY
#define HAVE_XMSS_VERIFY 0
#endif

#ifndef HAVE_XMSS_SIGN
#define HAVE_XMSS_SIGN 0
#endif

/* XMSS signing is not implemented.
 */
#if HAVE_XMSS_SIGN
#error "XMSS signing is not supported"
#endif

#ifndef HAVE_XMSS_USE_TARGET_OPTIONS
#define HAVE_XMSS_USE_TARGET_OPTIONS 0
#endif

#ifndef HAVE_LWPKA11_SUPPORT
#define HAVE_LWPKA11_SUPPORT 0
#endif

/* If the subsystem does not need to manipulate the ECC keystore keys
 * do not define the functions for doing that by setting this zero.
 *
 * Such a system can anyway use the keystore for ECDSA but has no
 * mechanisms to operate on the keystore keys.
 */
#ifndef HAVE_LWPKA11_ECKS_KEYOPS
#define HAVE_LWPKA11_ECKS_KEYOPS HAVE_LWPKA11_SUPPORT
#endif

#ifndef HAVE_LWPKA11_FORCE_ECDSA_SIGN_KEYSTORE
#define HAVE_LWPKA11_FORCE_ECDSA_SIGN_KEYSTORE 0
#endif

/* CCC_PAGE_SIZE and CCC_PAGE_MASK lwrrently used only when
 * HAVE_NC_REGIONS is nonzero (for finding non-contiguous data region
 * boundaries in physical memory).
 */
#if !defined(CCC_PAGE_SIZE)

#if !defined(PAGE_SIZE) && HAVE_NC_REGIONS
#error "PAGE_SIZE not defined"
#endif

#define CCC_PAGE_SIZE ((uint32_t)PAGE_SIZE)

#endif /* !defined(CCC_PAGE_SIZE) */

#if !defined(CCC_PAGE_MASK)
#define CCC_PAGE_MASK ((uint32_t)(CCC_PAGE_SIZE - 1U))
#endif /* !defined(CCC_PAGE_MASK) */

#ifndef CCC_USE_DMA_MEM_BUFFER_ALLOC
#define CCC_USE_DMA_MEM_BUFFER_ALLOC 0
#endif

#ifndef CCC_DMA_MEM_ALIGN
#define CCC_DMA_MEM_ALIGN CACHE_LINE
#endif

#if !defined(CCC_BASE_WHEN_DEVICE_NOT_PRESENT)

#define CCC_BASE_WHEN_DEVICE_NOT_PRESENT NULL
#define CCC_DEVICE_BASE_CAN_BE_NULL 0

#else

/* Subsystem must define CCC_BASE_WHEN_DEVICE_NOT_PRESENT
 * value as a "uint8_t *" compatible address which is returned
 * from subsystem when CCC polls a device for it's base address
 * in case the device is not accessible.
 *
 * Normally this address is NULL, but some subsystems use NULL
 * as a valid base address for device so some other address need to
 * be used.
 */
#define CCC_DEVICE_BASE_CAN_BE_NULL 1

#endif /* !defined(CCC_BASE_WHEN_DEVICE_NOT_PRESENT) */

#ifndef CCC_INLINE_REGISTER_ACCESS
/* By default, use inline functions for register access
 * Define 0 to use normal functions instead to save space.
 */
#define CCC_INLINE_REGISTER_ACCESS 1
#endif

/* Runtime controllable CCC R&D logger when SE_RD_DEBUG is set.
 *
 * CCC_VPRINTF_AP macro must be defined to be a vprintf-like
 * function accepting va_list ap argument.
 */
#ifdef CCC_VPRINTF_AP
#define HAVE_SE_UTIL_VPRINTF_AP SE_RD_DEBUG
#endif

#ifndef HAVE_SE_UTIL_VPRINTF_AP
#define HAVE_SE_UTIL_VPRINTF_AP 0
#endif

#if SE_RD_DEBUG == 0

#if TRACE_REG_OPS || MODULE_TRACE
#error "Tracing without SE_RD_DEBUG"
#endif

#if TEGRA_MEASURE_MEMORY_USAGE
#error "Measuring memory without SE_RD_DEBUG"
#endif

#if HAVE_SE_UTIL_VPRINTF_AP
#error "R&D runtime log control without SE_RD_DEBUG"
#endif

#endif /* SE_RD_DEBUG == 0 */

/* Enable CCC_WITH_xxx multi-engine features based on the HAVE_XXX
 * engine/feature enablers
 */
#ifndef CCC_WITH_SHA3
#define CCC_WITH_SHA3 (HAVE_SW_SHA3 || HAVE_HW_SHA3)
#endif

#ifndef CCC_WITH_RSA
#define CCC_WITH_RSA (HAVE_SE_RSA || HAVE_PKA1_RSA)
#endif

#ifndef CCC_WITH_RSA4096
#define CCC_WITH_RSA4096 HAVE_PKA1_RSA
#endif

#ifndef CCC_WITH_ECC
#define CCC_WITH_ECC HAVE_PKA1_ECC
#endif

#ifndef CCC_WITH_ED25519
#define CCC_WITH_ED25519 HAVE_PKA1_ED25519
#endif

#ifndef CCC_WITH_X25519
#define CCC_WITH_X25519 HAVE_PKA1_X25519
#endif

#ifndef CCC_WITH_ECDSA
#define CCC_WITH_ECDSA HAVE_PKA1_ECDSA
#endif

#ifndef CCC_WITH_ECDH
#define CCC_WITH_ECDH HAVE_PKA1_ECDH
#endif

#ifndef CCC_WITH_MODULAR
#define CCC_WITH_MODULAR HAVE_PKA1_MODULAR
#endif

#ifndef CCC_WITH_PKA_KEYLOAD
#define CCC_WITH_PKA_KEYLOAD HAVE_PKA1_LOAD
#endif

#ifndef CCC_WITH_SE_RANDOM
#define CCC_WITH_SE_RANDOM (HAVE_SE_RANDOM || HAVE_SE_GENRND)
#endif

#ifndef CCC_WITH_DRNG
#define CCC_WITH_DRNG (CCC_WITH_SE_RANDOM || HAVE_RNG1_DRNG || HAVE_LWRNG_DRNG)
#endif

#define CCC_WITH_AES_GCM (HAVE_AES_GCM || HAVE_GMAC_AES)

#define CCC_WITH_NIST_KDF_COUNTER_ENCODER \
	(HAVE_SE_KAC_KDF_NIST_ENCODER || HAVE_SE_CMAC_KDF)

#define CCC_WITH_XMSS (HAVE_XMSS_VERIFY || HAVE_XMSS_SIGN)

/* Due to large XMSS work buffer need to relocate
 * the XMSS root_seed to engine context as a work around
 * when CMEM is used.
 *
 * In this case there is no need to allocate a key
 * object for XMSS the root_seed is held in engine context.
 *
 * This can not be zero when (CCC_WITH_XMSS && HAVE_CONTEXT_MEMORY)
 * Set this always when XMSS is enabled now. No real reasons to use
 * different settings and code paths for mempool clients.
 */
#define CCC_WITH_XMSS_MINIMAL_CMEM CCC_WITH_XMSS

/* CCC supports passing EC public key in keyslot.
 * The corresponding pubkey must have these point_flags set:
 *  CCC_EC_POINT_FLAG_UNDEFINED and CCC_EC_POINT_FLAG_IN_KEYSLOT
 */
#ifndef CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT
#define CCC_WITH_PASS_EC_PUBKEY_IN_KEYSLOT HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY
#endif

/* Some sanity checks
 * TODO: Consider moving config sanity checks to crypto_conf_sanity.h
 */
#if HAVE_RSA_CIPHER && !CCC_WITH_RSA
#error "HAVE_RSA_CIPHER requires RSA enabled"
#endif

#if CCC_WITH_RSA && !(HAVE_SE_SHA || HAVE_EXTERNAL_SHA)
#error "RSA operations require HAVE_SE_SHA for masks"
#endif

#endif /* INCLUDED_CRYPTO_DEFS_H */
