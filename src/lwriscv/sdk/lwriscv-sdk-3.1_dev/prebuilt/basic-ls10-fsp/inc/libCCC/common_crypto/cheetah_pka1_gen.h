/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic engine : generic RSA/ECC support defs  */

#ifndef INCLUDED_TEGRA_PKA1_GEN_H
#define INCLUDED_TEGRA_PKA1_GEN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <include/tegra_pka1.h>

struct engine_s;
struct context_mem_s;

#ifndef HAVE_PKA1_BLINDING
/* RSA private key operations and EC (some) point ops add key blinding
 *  if this is defined nonzero; this provides improved protection
 *  agains TA (timing) (and (if enabled) agains DPA (differential
 *  power analysis) and SPA (simple power analysis) attacks).
 *
 * Using this requires either RNG1 or PKA1 TRNG to be enabled by
 * configuration =>
 *  define HAVE_RNG1_DRNG 1 in configuration and/or
 *  define HAVE_PKA1_TRNG 1 in configuration.
 *
 * If both are enabled, RNG1 will be used.
 *
 * Allow sustem config to turn this off. May have security concers if
 * turned off.
 */
#define HAVE_PKA1_BLINDING 1
#endif

#ifndef HAVE_PKA1_SCC_DPA
//
// XXXX TODO: test the side channel protection DPA feature.
//
// XXXX enabling SCC DPA (i.e. "not disabling" it means that
// XXXX TRNG autoreseed must also be set to 1 at the same time
// XXXX  or PKA will not work.
//
// Using PKA1 SCC requires TNRG to be enabled.
//
#define HAVE_PKA1_SCC_DPA 0
#endif

#ifndef MEASURE_PERF_ENGINE_PKA1_OPERATION
#define MEASURE_PERF_ENGINE_PKA1_OPERATION	0
#endif

static inline bool m_is_supported_rsa_modulus_byte_length(uint32_t len)
{
	return ((((512U/8U) == len)  || ((768U/8U) == len)  || ((1024U/8U) == len) ||
		 ((1536U/8U) == len)  || ((2048U/8U) == len) || ((3072U/8U) == len) ||
		 ((4096U/8U) == len)) &&
		((len) >= (RSA_MIN_KEYSIZE_BITS/8U)));
}

#define IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(len) \
		m_is_supported_rsa_modulus_byte_length(len)

/* RSA private key length matches modulus length, public key length is fixed one word.
 * Caller must zero pad keys to supported lengths.
 */
static inline bool m_is_supported_rsa_exponent_byte_length(uint32_t len)
{
	return (IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(len) || (BYTES_IN_WORD == len));
}

#define IS_SUPPORTED_RSA_EXPONENT_BYTE_LENGTH(len) \
		m_is_supported_rsa_exponent_byte_length(len)

/* P-521 not properly supported yet. Enable correct size when added.
 *
 * TODO for PKA1: Now 80U bytes (640 bits) is used for 521 bit keys even though 528
 * would be the next byte boundary or, if word size fields are used,
 * 544 bits and 68 bytes would be used (FIXME)
 *
 * Some other engines use 544 / 8 == 68 bytes for P-521 lwrve,
 * check if can use that also with PKA1 for this lwrve.
 */
static inline bool m_is_supported_ec_lwrve_byte_length(uint32_t len)
{
	return ((((160U/8U) == len) ||
		 ((192U/8U) == len)  || ((224U/8U) == len) || ((256U/8U) == len) ||
		 ((320U/8U) == len)  ||
		 ((384U/8U) == len)  || ((512U/8U) == len) ||
		 ((80U) == len)) &&
		(len >= (CCC_EC_MIN_PRIME_BITS/8U)));
}

#define IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(len)	\
		m_is_supported_ec_lwrve_byte_length(len)

// XX Where does this value come from (IAS?)
#define SE_PKA1_MUTEX_WDT_UNITS	0x600000U

#ifndef SE_PKA1_TIMEOUT

#if TEGRA_DELAY_FUNCTION_UNDEFINED
#define SE_PKA1_TIMEOUT		(40000U*200U) /* poll count, not microseconds */
#else
#define SE_PKA1_TIMEOUT		40000U	/*micro seconds*/
#endif

#endif /* !defined(SE_PKA1_TIMEOUT) */

#define MAX_PKA1_OPERAND_SIZE		(MAX_RSA_BIT_SIZE / 8U)
#define MAX_PKA1_OPERAND_WORD_SIZE	(MAX_PKA1_OPERAND_SIZE / BYTES_IN_WORD)

/* PKA Security Engine operand sizes */
enum pka1_op_mode {
	/* Supported RSA key op sizes */
	PKA1_OP_MODE_ILWALID	= 0,
	PKA1_OP_MODE_RSA512	= 1,
	PKA1_OP_MODE_RSA768	= 2,
	PKA1_OP_MODE_RSA1024	= 3,
	PKA1_OP_MODE_RSA1536	= 4,
	PKA1_OP_MODE_RSA2048	= 5,
	PKA1_OP_MODE_RSA3072	= 6,
	PKA1_OP_MODE_RSA4096	= 7,

	/* Supported ECC lwrve op sizes */
	PKA1_OP_MODE_ECC160	= 8,
	PKA1_OP_MODE_ECC192	= 9,
	PKA1_OP_MODE_ECC224	= 10,
	PKA1_OP_MODE_ECC256	= 11,
	PKA1_OP_MODE_ECC320	= 12, /* Not certain if PKA1 supports 320 bit ops? (brainpool P320r1/P320t1) */
	PKA1_OP_MODE_ECC384	= 13,
	PKA1_OP_MODE_ECC512	= 14,
	PKA1_OP_MODE_ECC521	= 15,
};

/* PKA Security Engine operation modes */
enum pka1_engine_ops {
	PKA1_OP_MODULAR_ILWALID		= 0,

	PKA1_OP_MODULAR_ADDITION	= 10,
#if CCC_WITH_MODULAR_DIVISION
	PKA1_OP_MODULAR_DIVISION	= 11,
#endif /* CCC_WITH_MODULAR_DIVISION */
	PKA1_OP_MODULAR_ILWERSION	= 12,
	PKA1_OP_MODULAR_MULTIPLICATION	= 13,
	PKA1_OP_MODULAR_SUBTRACTION	= 14,
	PKA1_OP_MODULAR_EXPONENTIATION	= 15,
	PKA1_OP_MODULAR_REDUCTION	= 16,
#if CCC_WITH_BIT_SERIAL_REDUCTION
	PKA1_OP_BIT_SERIAL_REDUCTION	= 30,
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */
	PKA1_OP_BIT_SERIAL_REDUCTION_DP	= 31,

	PKA1_OP_EC_POINT_ADD		= 40,
	PKA1_OP_EC_POINT_MULTIPLY	= 41,
	PKA1_OP_EC_POINT_DOUBLE		= 42,
	PKA1_OP_EC_POINT_VERIFY		= 43,
	PKA1_OP_EC_SHAMIR_TRICK		= 44,
#if HAVE_PKA1_ISM3
	PKA1_OP_EC_ISM3			= 45,	// XXX Does this even exist in new elliptic 20????
#endif /* HAVE_PKA1_ISM3 */

#if HAVE_ELLIPTIC_20
	/* P521 library for 2.0 FW */
	PKA1_OP_EC_P521_POINT_MULTIPLY	= 60,
	PKA1_OP_EC_P521_POINT_ADD	= 61,
	PKA1_OP_EC_P521_POINT_DOUBLE	= 62,
	PKA1_OP_EC_P521_POINT_VERIFY	= 63,
	PKA1_OP_EC_P521_SHAMIR_TRICK	= 64,

	PKA1_OP_EC_MODMULT_521		= 67,	/* x*y mod m mod p, where p=2^251-1 */
	PKA1_OP_EC_M_521_MONTMULT	= 68,	/* x * y mod m, over the modular field m
						 *
						 * Can only be used with 521 bit sized operations.
						 * Intended for use with an arbitrary modulus, typically
						 * the order-of-the-base-point (OBP).
						 *
						 * MODMULT_521 is faster, Mersenne specific purpose 521
						 * bit modular multiplication.
						 *
						 * This requires OOB callwlated Montgomery values.
						 */

	/* X25519 (ECDH with lwrve-25519) multiply
	 */
	PKA1_OP_C25519_POINT_MULTIPLY	= 70,

	PKA1_OP_C25519_MOD_EXP		= 81,
	PKA1_OP_C25519_MOD_MULT		= 82,
	PKA1_OP_C25519_MOD_SQUARE	= 83,

	/* Edwards lwrve ED25519 operations
	 */
	PKA1_OP_ED25519_POINT_MULTIPLY	= 90,
	PKA1_OP_ED25519_POINT_ADD	= 91,
	PKA1_OP_ED25519_POINT_DOUBLE	= 92,
	PKA1_OP_ED25519_POINT_VERIFY	= 93,
	PKA1_OP_ED25519_SHAMIR_TRICK	= 94,

	PKA1_OP_GEN_MULTIPLY		= 100,	/* Non modular multiplication: c=a*b) */

	PKA1_OP_GEN_MULTIPLY_MOD	= 101,	/*  c= (a*b) mod m
						 *
						 * Non modular multiplication and
						 *  immediate bit serial dp reduction
						 *  without reading the intermediate values
						 *  from the unit.
						 */
#endif /* HAVE_ELLIPTIC_20 */
};

#define	BANK_A 0U
#define	BANK_B 1U
#define	BANK_C 2U
#define	BANK_D 3U

typedef uint32_t pka1_bank_t;

#define PKA1_MAX_BANK BANK_D

/* PKA1 external entry points for LW version of the elliptic unit FW
 */
typedef enum pka1_entrypoint_e {
	PKA1_ENTRY_ILWALID =		0x00,

	PKA1_ENTRY_MONT_M =		0x10, /* m' */
	PKA1_ENTRY_MONT_RILW =		0x11, /* 1/r mod m */
	PKA1_ENTRY_MONT_R2 =		0x12, /* r^2 mod m */

	PKA1_ENTRY_CRT =		0x16, /* Chinese Remainder Theorem */
	PKA1_ENTRY_CRT_KEY_SETUP =	0x15, /* setup keys for CTR operation */

	PKA1_ENTRY_IS_LWRVE_M3 =	0x22, /* a == -3 */

	PKA1_ENTRY_MOD_ADDITION =	0x0b, /* x+y mod m */
	PKA1_ENTRY_MOD_DIVISION =	0x0d, /* y/x mod m */
	PKA1_ENTRY_MOD_EXP =		0x14, /* x^y mod m */
	PKA1_ENTRY_MOD_ILWERSION =	0x0e, /* 1/x mod m */
	PKA1_ENTRY_MOD_MULTIPLICATION =	0x0a, /* x*y mod m */
	PKA1_ENTRY_MOD_SUBTRACTION =	0x0c, /* x-y mod m */

	PKA1_ENTRY_EC_POINT_ADD =	0x1C, /* R = P + Q */
	PKA1_ENTRY_EC_POINT_DOUBLE =	0x1A, /* Q = 2P */
	PKA1_ENTRY_EC_POINT_MULT =	0x19, /* Q = kP */
	PKA1_ENTRY_EC_ECPV =		0x1E, /* y^2 == x^3 + ax + b mod p
					       * (i.e. Verify point is in lwrve)
					       */
	PKA1_ENTRY_MOD_REDUCTION =	     0x0f, /* x mod m */
	PKA1_ENTRY_BIT_SERIAL_REDUCTION =    0x18, /* x mod m */
	PKA1_ENTRY_BIT_SERIAL_REDUCTION_DP = 0x17, /* x mod m (double precision x) */

	PKA1_ENTRY_EC_SHAMIR_TRICK =	0x23, /* R = kP + lQ */

#if HAVE_ELLIPTIC_20
	/* Updated: IAS v0.71 (rc) */
	PKA1_ENTRY_GEN_MULTIPLY =	0x13, /* non-modular multiplication c = a*b
					       * (double length response)
					       */

	PKA1_ENTRY_MOD_521_MONTMULT =	0x28, /* P521: x*y mod m (operands are 521 bit values)
					       * Need OOB montgomery pre-computed values to use this!
					       */

	PKA1_ENTRY_MOD_MERSENNE_521_MULT = 0x29, /* P521: x*y mod p (modulus must be p = 2^251-1) */

	PKA1_ENTRY_EC_521_POINT_MULT =	0x24, /* ECC-521 Weierstrass Q = kP mod p */
	PKA1_ENTRY_EC_521_POINT_ADD =	0x26, /* ECC-521 Weierstrass R = P + Q mod p */
	PKA1_ENTRY_EC_521_POINT_DOUBLE =0x25, /* ECC-521 Weierstrass Q = 2P mod p*/
	PKA1_ENTRY_EC_521_PV =		0x27, /* ECC-521 Weierstrass point verify on lwrve */
	PKA1_ENTRY_EC_521_SHAMIR_TRICK =0x2A, /* ECC-521 Weierstrass R = kP + lQ */

	PKA1_ENTRY_C25519_POINT_MULT =	0x2E, /* C25519 Q = kP (Lwrve25519 ECDH) */

	PKA1_ENTRY_C25519_MOD_EXP =	0x30, /* C25519 x^y mod p */
	PKA1_ENTRY_C25519_MOD_MULT =	0x31, /* C25519 x*y mod p */
	PKA1_ENTRY_C25519_MOD_SQR =	0x32, /* C25519 x^2 mod p */

	PKA1_ENTRY_ED25519_POINT_MULT =	  0x2B, /* ED25519 Q = kP mod p */
	PKA1_ENTRY_ED25519_POINT_ADD =	  0x2C, /* ED25519 R = P + Q mod p */
	PKA1_ENTRY_ED25519_POINT_DOUBLE = 0x2D, /* ED25519 Q = 2P mod p */
	PKA1_ENTRY_ED25519_PV =		  0x2F, /* ED25519 point verify on lwrve */
	PKA1_ENTRY_ED25519_SHAMIR_TRICK = 0x33, /* ED25519 R = kP + lQ */
#endif /* HAVE_ELLIPTIC_20 */
} pka1_entrypoint_t;

/* HW defines values, specified in SE PKA IAS (table 28).
 * TEGRA_SE_PKA1_KEYSLOT_ADDR_FIELD(field)
 *
 * SW writes the field data in auto-increment mode.
 *
 * WORD_ADDR = 0..127 (some words might not be used)
 */
#define	KMAT_RSA_EXPONENT	0U
#define	KMAT_RSA_MODULUS	1U
#define	KMAT_RSA_M_PRIME	2U
#define	KMAT_RSA_R2_SQR		3U

#define	KMAT_EC_A		4U
#define	KMAT_EC_B		5U
#define	KMAT_EC_P		6U
#define	KMAT_EC_ORDER		7U
#define	KMAT_EC_PX		8U
#define	KMAT_EC_PY		9U
#define	KMAT_EC_QX	       10U
#define	KMAT_EC_QY	       11U
#define	KMAT_EC_KEY	       12U
#define	KMAT_EC_P_PRIME	       13U
#define	KMAT_EC_R2_SQR	       14U

typedef uint32_t pka1_keymat_field_t;

enum pka1_montgomery_setup_ops {
	CALC_R_NONE	= 10,
	CALC_R_ILW	= 11,
	CALC_M_PRIME	= 12,
	CALC_R_SQR	= 13,
};

#define PKA1_MONT_FLAG_NONE			0x0000U
#define PKA1_MONT_FLAG_VALUES_OK		0x0001U
#define PKA1_MONT_FLAG_M_PRIME_LITTLE_ENDIAN	0x0010U
#define PKA1_MONT_FLAG_R2_LITTLE_ENDIAN		0x0020U
#define PKA1_MONT_FLAG_NO_RELEASE		0x0100U /* Do not release the mont values */
#define PKA1_MONT_FLAG_OUT_OF_BAND		0x0200U /* Provided out-of-band by caller */
#define PKA1_MONT_FLAG_CONST_VALUES		0x0400U /* Constants in lwrve parameters */

struct pka1_engine_data {
	enum pka1_op_mode op_mode; /* for the EC operations this is same as lwrve->mode */
	enum pka1_engine_ops pka1_op;

	/* These ref to large buffers for RSA (max 512 bytes each (4096/8))
	 * and smaller for EC (max 80 bytes each). These hold the
	 * montgomery colwersion results M_PRIME and R_SQR.
	 *
	 * Since the EC lwrve montgomery values are constants allow
	 * them to be specified in lwrve parameters. If not
	 * given => code callwlates these values with PKA1 functions.
	 *
	 * These either refer to OOB callwlated values OR they
	 * refer to an allocated buffer in mont_buffer field.
	 * In that case the montgomery values are callwlated
	 * to mont_buffer and referred to by m_prime and r2 fields
	 * via const pointers.
	 *
	 * The mont_flags is used to imply the state of these.
	 */
	const uint8_t *m_prime;
	const uint8_t *r2;

	/* By default: Montgomery values are BIG ENDIAN, set the
	 * LITTLE ENDIAN flag if required.
	 */
	uint32_t mont_flags;

	/* If space for the montgomery value fields (m_prime && r2) is
	 * allocated dynamically, this buffer contains space
	 * for both of the above values.
	 *
	 * In that case mont_buffer == m_prime, otherwise NULL.
	 *
	 * This mechanism is used so that the m_prime and r2 fields
	 * above can be made const, which means that they
	 * can be assigned const value from lwrve parameters.
	 */
	uint8_t *mont_buffer;
};

#if HAVE_PKA1_LOAD
status_t pka1_keyslot_load(const struct engine_s *engine, uint32_t pka_keyslot);

status_t pka1_set_keyslot_field(const struct engine_s *engine,
				pka1_keymat_field_t field,
				uint32_t key_slot,
				const uint8_t *param,
				uint32_t key_words,
				bool data_big_endian,
				uint32_t field_word_size);
#endif /* HAVE_PKA1_LOAD */

status_t pka1_num_words(enum pka1_op_mode op_mode, uint32_t *words_p);

status_t pka1_montgomery_values_calc(struct context_mem_s *cmem,
				     const struct engine_s *engine,
				     uint32_t m_len,
				     const uint8_t *modulus,
				     struct pka1_engine_data *pka1_data,
				     bool modulus_big_endian);

status_t tegra_pka1_set_fixed_montgomery_values(struct pka1_engine_data *pka1_data,
						const uint8_t *m_prime,
						const uint8_t *r2,
						bool big_endian);

void pka1_data_release(struct context_mem_s *cmem, struct pka1_engine_data *pka1_data);

/* XXX Changed: set partial radix in the callee FIXME remove this comment once tested */
status_t pka1_go_start(const struct engine_s *engine, enum pka1_op_mode op_mode,
		       uint32_t byte_size, pka1_entrypoint_t entrypoint);

status_t pka1_wait_op_done(const struct engine_s *engine);

/* Regs defined for banks in pka1_bank_t. Not all of these regs exist for all op_modes.
 * Check Table 9 (PKA wide register map base address) for existence.
 */
#define	R0 0U
#define	R1 1U
#define	R2 2U
#define	R3 3U
#define	R4 4U
#define	R5 5U
#define	R6 6U
#define	R7 7U

#define PKA1_MAX_REG_VALUE R7

typedef uint32_t pka1_reg_id_t;

status_t pka1_bank_register_read(const struct engine_s *engine,
				 enum pka1_op_mode op_mode,
				 pka1_bank_t bank, pka1_reg_id_t breg,
				 uint8_t *data, uint32_t nwords, bool big_endian);

status_t pka1_bank_register_write(const struct engine_s *engine,
				  enum pka1_op_mode op_mode,
				  pka1_bank_t bank, pka1_reg_id_t breg,
				  const uint8_t *data, uint32_t nwords, bool big_endian,
				  uint32_t field_word_size);

#if HAVE_PKA1_TRNG
status_t pka1_trng_setup_locked(const struct engine_s *engine);

/* This is in tegra_pka1_rng.c (XXX TODO create tegra_pka1_rng.h) */
status_t pka1_trng_generate_locked(const struct engine_s *engine, uint8_t *buf, uint32_t blen);
#endif

/* Return status (in case of timeout...)
 * But what can we do if there is a timeout
 * => let the caller decide for now (terminate
 * current operation on error)
 */
status_t pka1_acquire_mutex(const struct engine_s *engine);
void pka1_release_mutex(const struct engine_s *engine);

#if HAVE_RSA_MONTGOMERY_CALC
/* LIbrary function to export PKA1 callwlated montgomery values for
 * RSA modulus.
 *
 * Not used by CCC, provided for out-of-band montgomery value callwlation.
 */
status_t se_get_rsa_montgomery_values(uint32_t rsa_size_bytes,
				      uint8_t *modulus,
				      bool is_big_endian,
				      uint8_t *m_prime,
				      uint8_t *r2);
#endif /* HAVE_RSA_MONTGOMERY_CALC */
#endif /* INCLUDED_TEGRA_PKA1_GEN_H */
