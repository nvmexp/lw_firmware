/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/* NVPKA feature configurations for T23X
 */
#ifndef INCLUDED_LWPKA_DEFS_H
#define INCLUDED_LWPKA_DEFS_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if !defined(CCC_SOC_WITH_LWPKA) || CCC_SOC_WITH_LWPKA == 0
#error "Incorrect NVPKA setup"
#endif

#define HAVE_LWPKA_SCC	  1	/* NVPKA SCC features (Default:1) */

/* HAVE_GEN_MULTPLY is optional; it makes CCC use GEN_MULT/MOD_REDUCTION
 * instead of MOD_MULTIPLICATION.
 *
 * It is recommended to set this 0 at least for now.
 */
#ifndef HAVE_GEN_MULTIPLY
#define HAVE_GEN_MULTIPLY 0	/* NVPKA supports generic multiply/reduction.
				 * If 0, use MOD_MULTIPLY instead (faster in NVPKA HW)
				 * and does not need pre-set montgomery values.
				 */
#endif

#define NVPKA_MAX_KEYSLOTS 4U

#ifndef HAVE_LWPKA_RESET
#define HAVE_LWPKA_RESET 0
#endif

/* Config features from PKA1 config file mapped to NVPKA
 * automatically when enabled with this.
 */
#define MAP_PKA1_CONFIG_TO_LWPKA 1

/* By default use NVPKA HW mutex locks (mutexes are optional)
 *
 * Define HAVE_LWPKA_MUTEX in case the mutex is not required,
 * e.g. for PSC ROM code where the accesses are serialized.
 */
#if !defined(HAVE_LWPKA_MUTEX)
#define HAVE_LWPKA_MUTEX 1
#endif

/* NVPKA now uses the LONG FORM group equation for
 * e.g. ED25519 verification.
 *
 * Short form support is possible but not implemented
 * at this point. Would require minor tweaks to generic
 * code parts using shamir's tricks for this.
 */
#define VERIFY_GROUP_EQUATION_LONG_FORM 1

/* MODULAR operations preserve the LOR_C0 register
 * up to operations size bytes if the operations succeeds.
 *
 * On any error or other anomaly also LOR_C0 gets cleared.
 */
#define PKA_MODULAR_OPERATIONS_PRESERVE_MODULUS 1

#if defined(MAP_PKA1_CONFIG_TO_LWPKA)

/* Key loading feature dup */
#if defined(HAVE_PKA1_LOAD)
#define HAVE_LWPKA_LOAD HAVE_PKA1_LOAD
#endif

/* Algorithms, on the other hand, get mapped directly to NVPKA.
 * Use either PKA1 or NVPKA defines for algorithm support.
 */
#if defined(HAVE_PKA1_RSA)
#define HAVE_LWPKA_RSA HAVE_PKA1_RSA
#endif

#if defined(HAVE_PKA1_ECC)
#define HAVE_LWPKA_ECC HAVE_PKA1_ECC
#endif

#if defined(HAVE_PKA1_ECDH)
#define HAVE_LWPKA_ECDH HAVE_PKA1_ECDH
#endif

#if defined(HAVE_PKA1_ECDSA)
#define HAVE_LWPKA_ECDSA HAVE_PKA1_ECDSA
#endif

#if defined(HAVE_PKA1_ED25519)
#define HAVE_LWPKA_ED25519 HAVE_PKA1_ED25519
#endif

#if defined(HAVE_PKA1_X25519)
#define HAVE_LWPKA_X25519 HAVE_PKA1_X25519
#endif

#if defined(HAVE_RNG1_DRNG)
#define HAVE_LWRNG_DRNG HAVE_RNG1_DRNG
#endif

#endif /* defined(MAP_PKA1_CONFIG_TO_LWPKA) */

/*****************/

#ifndef HAVE_LWPKA_LOAD
#define HAVE_LWPKA_LOAD 0
#endif

#ifndef HAVE_LWPKA_RSA
#define HAVE_LWPKA_RSA 0
#endif

#ifndef HAVE_LWPKA_ECC
#define HAVE_LWPKA_ECC 0
#endif

#ifndef HAVE_LWPKA_ECDH
#define HAVE_LWPKA_ECDH 0
#endif

#ifndef HAVE_LWPKA_ECDSA
#define HAVE_LWPKA_ECDSA 0
#endif

#ifndef HAVE_LWPKA_ED25519
#define HAVE_LWPKA_ED25519 0
#endif

#ifndef HAVE_LWPKA_ED448
#define HAVE_LWPKA_ED448 0
#endif

#ifndef HAVE_LWPKA_X25519
#define HAVE_LWPKA_X25519 0
#endif

#ifndef HAVE_LWPKA_MODULAR_DIVISION
#define HAVE_LWPKA_MODULAR_DIVISION 0
#endif

#ifndef TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL
#define TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL 0
#endif

#ifndef TEGRA_UDELAY_VALUE_LWPKA_OP
#define TEGRA_UDELAY_VALUE_LWPKA_OP 0
#endif

#ifndef TEGRA_UDELAY_VALUE_LWPKA_MUTEX
#define TEGRA_UDELAY_VALUE_LWPKA_MUTEX 0
#endif

/* CCC feature settings for NVPKA
 * These could be unconditional.
 */
#ifndef CCC_WITH_RSA
#define CCC_WITH_RSA HAVE_LWPKA_RSA
#endif

#ifndef CCC_WITH_RSA4096
#define CCC_WITH_RSA4096 HAVE_LWPKA_RSA
#endif

#ifndef CCC_WITH_ECC
#define CCC_WITH_ECC HAVE_LWPKA_ECC
#endif

#ifndef CCC_WITH_ED25519
#define CCC_WITH_ED25519 HAVE_LWPKA_ED25519
#endif

#ifndef CCC_WITH_ED448
#define CCC_WITH_ED448 HAVE_LWPKA_ED448
#endif

#ifndef CCC_WITH_EDWARDS
#define CCC_WITH_EDWARDS (CCC_WITH_ED25519 || CCC_WITH_ED448)
#endif

#ifndef CCC_WITH_X25519
#define CCC_WITH_X25519 HAVE_LWPKA_X25519
#endif

#ifndef CCC_WITH_MONTGOMERY_ECDH
#define CCC_WITH_MONTGOMERY_ECDH (CCC_WITH_X25519)
#endif

#ifndef CCC_WITH_ECDSA
#define CCC_WITH_ECDSA HAVE_LWPKA_ECDSA
#endif

#ifndef CCC_WITH_ECDH
#define CCC_WITH_ECDH HAVE_LWPKA_ECDH
#endif

#ifndef CCC_WITH_MODULAR
#define CCC_WITH_MODULAR HAVE_LWPKA_MODULAR
#endif

#ifndef CCC_WITH_PKA_KEYLOAD
#define CCC_WITH_PKA_KEYLOAD HAVE_LWPKA_LOAD
#endif

#if HAVE_LWPKA_LOAD

/* Severe performance hit for public key operations
 * using keyslot values.
 */
#ifndef HAVE_LWPKA_KEYSLOT_FAIL_PUBLIC
#define HAVE_LWPKA_KEYSLOT_FAIL_PUBLIC 0
#endif

/* Force SCC_EN in case this is 1 and keyslot is used.
 * HW locked feature normally, also with values that would not need this.
 */
#ifndef HAVE_LWPKA_KEYSLOT_ENABLE_SC_ALWAYS
#define HAVE_LWPKA_KEYSLOT_ENABLE_SC_ALWAYS 1
#endif

#endif /* HAVE_LWPKA_LOAD */

#endif /* INCLUDED_LWPKA_DEFS_H */
